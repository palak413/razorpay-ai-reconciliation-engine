#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <ctime>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <optional>
#include <algorithm>
#include <climits>
#include "database.h"
#include "reconciliation_engine.h"
#include "models.h"

std::vector<std::string> parse_csv_line(const std::string& line) {
    std::vector<std::string> tokens;
    std::string token;
    bool in_quotes = false;
    for (char c : line) {
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ',' && !in_quotes) {
            tokens.push_back(token);
            token.clear();
        } else {
            token += c;
        }
    }
    tokens.push_back(token);
    return tokens;
}

std::optional<int64_t> parse_minor_units_strict(const std::string& amount_str) {
    if (amount_str.empty()) return std::nullopt;

    std::string clean_str = amount_str;
    clean_str.erase(std::remove(clean_str.begin(), clean_str.end(), '"'), clean_str.end());
    clean_str.erase(std::remove(clean_str.begin(), clean_str.end(), ' '), clean_str.end());

    if (clean_str.empty()) return std::nullopt;

    bool negative = false;
    size_t start_idx = 0;
    if (clean_str[0] == '-') {
        negative = true;
        start_idx = 1;
    } else if (clean_str[0] == '+') {
        start_idx = 1;
    }

    if (start_idx >= clean_str.length()) return std::nullopt;

    size_t dot_pos = clean_str.find('.', start_idx);
    std::string int_part, frac_part;

    if (dot_pos == std::string::npos) {
        int_part = clean_str.substr(start_idx);
        frac_part = "00";
    } else {
        int_part = clean_str.substr(start_idx, dot_pos - start_idx);
        frac_part = clean_str.substr(dot_pos + 1);
        if (frac_part.length() > 2) return std::nullopt;
        if (frac_part.length() == 1) frac_part += "0";
        if (frac_part.length() == 0) frac_part = "00";
    }

    for (char c : int_part) {
        if (!std::isdigit(c)) return std::nullopt;
    }
    for (char c : frac_part) {
        if (!std::isdigit(c)) return std::nullopt;
    }

    try {
        int64_t d_val = int_part.empty() ? 0 : std::stoll(int_part);
        int64_t c_val = std::stoll(frac_part);

        if (d_val > (LLONG_MAX - c_val) / 100) {
            return std::nullopt;
        }

        int64_t total = (d_val * 100) + c_val;
        if (total < 0) return std::nullopt;

        return negative ? -total : total;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int64_t> parse_strict_int(const std::string& str) {
    if (str.empty()) return std::nullopt;
    std::string clean_str = str;
    clean_str.erase(std::remove(clean_str.begin(), clean_str.end(), '"'), clean_str.end());
    clean_str.erase(std::remove(clean_str.begin(), clean_str.end(), ' '), clean_str.end());

    for (size_t i = (clean_str[0] == '-' || clean_str[0] == '+' ? 1 : 0); i < clean_str.length(); ++i) {
        if (!std::isdigit(clean_str[i])) return std::nullopt;
    }

    try {
        return std::stoll(clean_str);
    } catch (...) {
        return std::nullopt;
    }
}

int main(int argc, char* argv[]) {
    std::string db_path = "database/reconciliation.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    Database db(db_path);
    if (!db.connect()) {
        std::cerr << "[Main Error] Failed to connect to database at " << db_path << std::endl;
        return 1;
    }

    std::ifstream internal_file("data/internal_ledger.csv");
    if (!internal_file.is_open()) {
        std::cerr << "[Main Error] Fatal: Cannot open data/internal_ledger.csv." << std::endl;
        db.disconnect();
        return 1;
    }

    std::ifstream bank_file("data/bank_settlement.csv");
    if (!bank_file.is_open()) {
        std::cerr << "[Main Error] Fatal: Cannot open data/bank_settlement.csv." << std::endl;
        internal_file.close();
        db.disconnect();
        return 1;
    }

    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    int64_t now_sec = now_ms / 1000;
    std::string batch_id = "BATCH_" + std::to_string(now_ms);
    std::string engine_version = "v1.0.0";

    std::vector<InternalTransaction> internal_txs;
    std::vector<BankSettlement> bank_settlements;

    {
        std::string line;
        bool header = true;
        size_t row_num = 0;
        while (std::getline(internal_file, line)) {
            row_num++;
            if (line.empty()) continue;
            if (header) { header = false; continue; }

            auto cols = parse_csv_line(line);
            if (cols.size() != 6) {
                std::cerr << "[Main Error] Fatal: Malformed CSV row in data/internal_ledger.csv at line " << row_num << " (expected exactly 6 columns, got " << cols.size() << ")." << std::endl;
                internal_file.close(); bank_file.close(); db.disconnect();
                return 1;
            }

            std::string tx_id = cols[0];
            if (tx_id.empty()) {
                std::cerr << "[Main Error] Fatal: Empty transaction_id in data/internal_ledger.csv at line " << row_num << "." << std::endl;
                internal_file.close(); bank_file.close(); db.disconnect();
                return 1;
            }

            auto amt = parse_minor_units_strict(cols[1]);
            if (!amt.has_value()) {
                std::cerr << "[Main Error] Fatal: Invalid amount format in data/internal_ledger.csv at line " << row_num << " ('" << cols[1] << "')." << std::endl;
                internal_file.close(); bank_file.close(); db.disconnect();
                return 1;
            }

            auto tax = parse_minor_units_strict(cols[2]);
            if (!tax.has_value()) {
                std::cerr << "[Main Error] Fatal: Invalid tax format in data/internal_ledger.csv at line " << row_num << " ('" << cols[2] << "')." << std::endl;
                internal_file.close(); bank_file.close(); db.disconnect();
                return 1;
            }

            std::string curr = cols[3];
            if (curr.empty()) {
                std::cerr << "[Main Error] Fatal: Empty currency in data/internal_ledger.csv at line " << row_num << "." << std::endl;
                internal_file.close(); bank_file.close(); db.disconnect();
                return 1;
            }

            auto ts = parse_strict_int(cols[4]);
            if (!ts.has_value()) {
                std::cerr << "[Main Error] Fatal: Invalid timestamp in data/internal_ledger.csv at line " << row_num << "." << std::endl;
                internal_file.close(); bank_file.close(); db.disconnect();
                return 1;
            }

            std::string status = cols[5];

            InternalTransaction tx;
            tx.batch_id = batch_id;
            tx.transaction_id = tx_id;
            tx.amount = *amt;
            tx.tax = *tax;
            tx.currency = curr;
            tx.timestamp = *ts;
            tx.status = status;
            internal_txs.push_back(tx);
        }
        internal_file.close();
    }

    {
        std::string line;
        bool header = true;
        size_t row_num = 0;
        while (std::getline(bank_file, line)) {
            row_num++;
            if (line.empty()) continue;
            if (header) { header = false; continue; }

            auto cols = parse_csv_line(line);
            if (cols.size() != 6) {
                std::cerr << "[Main Error] Fatal: Malformed CSV row in data/bank_settlement.csv at line " << row_num << " (expected exactly 6 columns, got " << cols.size() << ")." << std::endl;
                bank_file.close(); db.disconnect();
                return 1;
            }

            std::string settlement_id = cols[0];
            if (settlement_id.empty()) {
                std::cerr << "[Main Error] Fatal: Empty settlement_id in data/bank_settlement.csv at line " << row_num << "." << std::endl;
                bank_file.close(); db.disconnect();
                return 1;
            }

            std::string tx_id_col = cols[1];
            std::optional<std::string> opt_tx_id = std::nullopt;
            if (!tx_id_col.empty() && tx_id_col != "NULL" && tx_id_col != "null") {
                opt_tx_id = tx_id_col;
            }

            auto amt = parse_minor_units_strict(cols[2]);
            if (!amt.has_value()) {
                std::cerr << "[Main Error] Fatal: Invalid amount format in data/bank_settlement.csv at line " << row_num << "." << std::endl;
                bank_file.close(); db.disconnect();
                return 1;
            }

            auto tax = parse_minor_units_strict(cols[3]);
            if (!tax.has_value()) {
                std::cerr << "[Main Error] Fatal: Invalid tax format in data/bank_settlement.csv at line " << row_num << "." << std::endl;
                bank_file.close(); db.disconnect();
                return 1;
            }

            std::string curr = cols[4];
            if (curr.empty()) {
                std::cerr << "[Main Error] Fatal: Empty currency in data/bank_settlement.csv at line " << row_num << "." << std::endl;
                bank_file.close(); db.disconnect();
                return 1;
            }

            auto ts = parse_strict_int(cols[5]);
            if (!ts.has_value()) {
                std::cerr << "[Main Error] Fatal: Invalid timestamp in data/bank_settlement.csv at line " << row_num << "." << std::endl;
                bank_file.close(); db.disconnect();
                return 1;
            }

            BankSettlement bs;
            bs.batch_id = batch_id;
            bs.settlement_id = settlement_id;
            bs.transaction_id = opt_tx_id;
            bs.amount = *amt;
            bs.tax = *tax;
            bs.currency = curr;
            bs.timestamp = *ts;
            bank_settlements.push_back(bs);
        }
        bank_file.close();
    }

    if (internal_txs.empty()) {
        std::cerr << "[Main Error] Fatal: Internal transaction dataset is empty." << std::endl;
        db.disconnect();
        return 1;
    }

    for (const auto& tx : internal_txs) {
        if (tx.batch_id != batch_id) {
            std::cerr << "[Main Error] Fatal: Batch ID mismatch in internal transactions." << std::endl;
            db.disconnect();
            return 1;
        }
    }
    for (const auto& bs : bank_settlements) {
        if (bs.batch_id != batch_id) {
            std::cerr << "[Main Error] Fatal: Batch ID mismatch in bank settlements." << std::endl;
            db.disconnect();
            return 1;
        }
    }

    Batch batch;
    batch.batch_id = batch_id;
    batch.status = "RUNNING";
    batch.started_at = now_sec;
    batch.engine_version = engine_version;
    batch.total_records = internal_txs.size();
    batch.matched_records = 0;
    batch.exception_records = 0;

    if (!db.begin_transaction()) {
        std::cerr << "[Main Error] Failed to begin database transaction." << std::endl;
        db.disconnect();
        return 1;
    }

    if (!db.save_batch(batch)) {
        std::cerr << "[Main Error] Failed to save batch. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    if (!db.save_audit_log(now_sec, batch_id, std::nullopt, "BATCH_STARTED", "MAIN", "RUNNING", "Batch execution started.")) {
        std::cerr << "[Main Error] Failed to save BATCH_STARTED audit log. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    if (!db.save_internal_transactions(internal_txs) ||
        !db.save_bank_settlements(bank_settlements)) {
        std::cerr << "[Main Error] Failed to save input records. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    if (!db.save_audit_log(now_sec, batch_id, std::nullopt, "INPUT_LOADED", "MAIN", "RUNNING", "Loaded internal and bank CSV records.")) {
        std::cerr << "[Main Error] Failed to save INPUT_LOADED audit log. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    ReconciliationEngine engine(engine_version);
    std::vector<ReconciliationResult> results;
    std::vector<ExceptionRecord> exceptions;

    engine.process(internal_txs, bank_settlements, results, exceptions);

    if (!internal_txs.empty() && results.empty()) {
        std::cerr << "[Main Error] Fatal: Reconciliation produced zero results for non-empty input. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    int64_t matched_count = 0;
    for (const auto& r : results) {
        if (r.result == "MATCHED") {
            matched_count++;
        }
    }
    int64_t exception_count = exceptions.size();

    if (!db.save_reconciliation_results(results) || !db.save_exceptions(exceptions)) {
        std::cerr << "[Main Error] Failed to save reconciliation results/exceptions. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    batch.matched_records = matched_count;
    batch.exception_records = exception_count;
    batch.status = "COMPLETED";
    batch.completed_at = std::time(nullptr);

    if (!db.save_batch(batch)) {
        std::cerr << "[Main Error] Failed to update final batch state. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    if (!db.save_audit_log(batch.completed_at.value(), batch_id, std::nullopt, "BATCH_COMPLETED", "MAIN", "COMPLETED", "Batch successfully processed.")) {
        std::cerr << "[Main Error] Failed to save BATCH_COMPLETED audit log. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    if (!db.commit()) {
        std::cerr << "[Main Error] Failed to commit transaction. Rolling back." << std::endl;
        db.rollback();
        db.disconnect();
        return 1;
    }

    int64_t amt_mismatch = 0, tax_mismatch = 0, curr_mismatch = 0, ts_mismatch = 0;
    int64_t missing_ext = 0, missing_int = 0, dup_settlement = 0;
    for (const auto& exc : exceptions) {
        if (exc.detected_status == "AMOUNT_MISMATCH") amt_mismatch++;
        else if (exc.detected_status == "TAX_MISMATCH") tax_mismatch++;
        else if (exc.detected_status == "CURRENCY_MISMATCH") curr_mismatch++;
        else if (exc.detected_status == "TIMESTAMP_MISMATCH") ts_mismatch++;
        else if (exc.detected_status == "MISSING_EXTERNAL") missing_ext++;
        else if (exc.detected_status == "MISSING_INTERNAL") missing_int++;
        else if (exc.detected_status == "DUPLICATE_SETTLEMENT") dup_settlement++;
    }

    std::cout << "========================================\n";
    std::cout << "AI FINANCE CONTROLLER — BATCH COMPLETE\n";
    std::cout << "========================================\n";
    std::cout << "Batch ID: " << batch_id << "\n";
    std::cout << "Internal Row Count: " << internal_txs.size() << "\n";
    std::cout << "Bank Settlement Row Count: " << bank_settlements.size() << "\n";
    std::cout << "Reconciliation Result Count: " << results.size() << "\n";
    std::cout << "MATCHED Count: " << matched_count << "\n";
    std::cout << "Exception Count: " << exception_count << "\n";
    std::cout << "  - Amount Mismatches: " << amt_mismatch << "\n";
    std::cout << "  - Tax Mismatches: " << tax_mismatch << "\n";
    std::cout << "  - Currency Mismatches: " << curr_mismatch << "\n";
    std::cout << "  - Timestamp Mismatches: " << ts_mismatch << "\n";
    std::cout << "  - Missing External: " << missing_ext << "\n";
    std::cout << "  - Missing Internal: " << missing_int << "\n";
    std::cout << "  - Duplicate Settlements: " << dup_settlement << "\n";
    std::cout << "Status: COMPLETED\n";
    std::cout << "========================================\n";

    db.disconnect();
    return 0;
}