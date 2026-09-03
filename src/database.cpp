#include "database.h"
#include <iostream>
#include <ctime>

Database::Database(const std::string& db_path) : db_path_(db_path), db_(nullptr) {}

Database::~Database() {
    close();
}

bool Database::open() {
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db_) << std::endl;
        return false;
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, nullptr);
    return true;
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

std::vector<InternalTransaction> Database::get_internal_transactions() {
    std::vector<InternalTransaction> list;
    const char* sql = "SELECT transaction_id, payment_id, timestamp, amount, currency, tax, status FROM internal_transactions;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            InternalTransaction tx;
            tx.transaction_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            tx.payment_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            tx.timestamp = sqlite3_column_int64(stmt, 2);
            tx.amount = sqlite3_column_double(stmt, 3);
            tx.currency = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            tx.tax = sqlite3_column_double(stmt, 5);
            tx.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            list.push_back(tx);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

std::vector<BankSettlement> Database::get_bank_settlements() {
    std::vector<BankSettlement> list;
    const char* sql = "SELECT settlement_id, transaction_id, settlement_timestamp, amount, currency, tax, bank_reference, status FROM bank_settlements;";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            BankSettlement bs;
            bs.settlement_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            const unsigned char* tx_text = sqlite3_column_text(stmt, 1);
            bs.transaction_id = tx_text ? reinterpret_cast<const char*>(tx_text) : "";
            bs.settlement_timestamp = sqlite3_column_int64(stmt, 2);
            bs.amount = sqlite3_column_double(stmt, 3);
            bs.currency = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            bs.tax = sqlite3_column_double(stmt, 5);
            bs.bank_reference = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            bs.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            list.push_back(bs);
        }
    }
    sqlite3_finalize(stmt);
    return list;
}

bool Database::save_results_and_exceptions(
    const std::vector<ReconciliationResult>& results,
    const std::vector<ExceptionRecord>& exceptions,
    const std::string& batch_id
) {
    sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

    const char* res_sql = "INSERT OR REPLACE INTO reconciliation_results "
                          "(transaction_id, result, reason, internal_amount, external_amount, timestamp_diff_seconds, processed_at, engine_version) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* res_stmt;
    sqlite3_prepare_v2(db_, res_sql, -1, &res_stmt, nullptr);

    for (const auto& r : results) {
        sqlite3_bind_text(res_stmt, 1, r.transaction_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(res_stmt, 2, r.result.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(res_stmt, 3, r.reason.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(res_stmt, 4, r.internal_amount);
        sqlite3_bind_double(res_stmt, 5, r.external_amount);
        sqlite3_bind_int64(res_stmt, 6, r.timestamp_diff_seconds);
        sqlite3_bind_int64(res_stmt, 7, r.processed_at);
        sqlite3_bind_text(res_stmt, 8, r.engine_version.c_str(), -1, SQLITE_TRANSIENT);

        if (sqlite3_step(res_stmt) != SQLITE_DONE) {
            sqlite3_finalize(res_stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            log_audit("DB_ERROR", r.transaction_id, batch_id, "ENGINE", "Failed to insert reconciliation result");
            return false;
        }
        sqlite3_reset(res_stmt);
    }
    sqlite3_finalize(res_stmt);

    const char* exc_sql = "INSERT OR REPLACE INTO exceptions "
                          "(transaction_id, status, internal_amount, external_amount, internal_currency, external_currency, timestamp_diff_seconds, ai_status) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, 'PENDING');";
    sqlite3_stmt* exc_stmt;
    sqlite3_prepare_v2(db_, exc_sql, -1, &exc_stmt, nullptr);

    for (const auto& e : exceptions) {
        sqlite3_bind_text(exc_stmt, 1, e.transaction_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(exc_stmt, 2, e.status.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_double(exc_stmt, 3, e.internal_amount);
        sqlite3_bind_double(exc_stmt, 4, e.external_amount);
        sqlite3_bind_text(exc_stmt, 5, e.internal_currency.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(exc_stmt, 6, e.external_currency.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(exc_stmt, 7, e.timestamp_diff_seconds);

        if (sqlite3_step(exc_stmt) != SQLITE_DONE) {
            sqlite3_finalize(exc_stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            log_audit("DB_ERROR", e.transaction_id, batch_id, "ENGINE", "Failed to insert exception");
            return false;
        }
        sqlite3_reset(exc_stmt);
    }
    sqlite3_finalize(exc_stmt);

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

void Database::log_audit(const std::string& event_type, const std::string& tx_id, const std::string& batch_id, const std::string& component, const std::string& message) {
    const char* sql = "INSERT INTO audit_logs (timestamp, event_type, transaction_id, batch_id, component, message) VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, std::time(nullptr));
        sqlite3_bind_text(stmt, 2, event_type.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, tx_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, batch_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, component.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, message.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
}