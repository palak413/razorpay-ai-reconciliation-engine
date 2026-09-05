#include "database.h"
#include <iostream>

Database::Database(const std::string& db_path) : db_path_(db_path) {}

Database::~Database() {
    disconnect();
}

void Database::log_error(const std::string& operation) {
    if (db_) {
        std::cerr << "[Database Error] " << operation << ": " << sqlite3_errmsg(db_) << std::endl;
    } else {
        std::cerr << "[Database Error] " << operation << ": Database not connected." << std::endl;
    }
}

bool Database::connect() {
    if (sqlite3_open(db_path_.c_str(), &db_) != SQLITE_OK) {
        log_error("sqlite3_open");
        return false;
    }

    char* err_msg = nullptr;
    if (sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "Failed to enable WAL: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    
    if (sqlite3_exec(db_, "PRAGMA foreign_keys=ON;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
        std::cerr << "Failed to enable foreign keys: " << err_msg << std::endl;
        sqlite3_free(err_msg);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    return true;
}

void Database::disconnect() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool Database::begin_transaction() {
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, "BEGIN TRANSACTION;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
        log_error("begin_transaction");
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool Database::commit() {
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
        log_error("commit");
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

bool Database::rollback() {
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, &err_msg) != SQLITE_OK) {
        log_error("rollback");
        if (err_msg) sqlite3_free(err_msg);
        return false;
    }
    return true;
}

// Strict bind helper functions with return code validation
inline bool safe_bind_text(sqlite3_stmt* stmt, int idx, const std::string& val) {
    return sqlite3_bind_text(stmt, idx, val.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
}

inline bool safe_bind_opt_text(sqlite3_stmt* stmt, int idx, const std::optional<std::string>& val) {
    if (val) {
        return sqlite3_bind_text(stmt, idx, val->c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
    } else {
        return sqlite3_bind_null(stmt, idx) == SQLITE_OK;
    }
}

inline bool safe_bind_int64(sqlite3_stmt* stmt, int idx, int64_t val) {
    return sqlite3_bind_int64(stmt, idx, val) == SQLITE_OK;
}

inline bool safe_bind_opt_int64(sqlite3_stmt* stmt, int idx, const std::optional<int64_t>& val) {
    if (val) {
        return sqlite3_bind_int64(stmt, idx, *val) == SQLITE_OK;
    } else {
        return sqlite3_bind_null(stmt, idx) == SQLITE_OK;
    }
}

inline bool safe_bind_opt_double(sqlite3_stmt* stmt, int idx, const std::optional<double>& val) {
    if (val) {
        return sqlite3_bind_double(stmt, idx, *val) == SQLITE_OK;
    } else {
        return sqlite3_bind_null(stmt, idx) == SQLITE_OK;
    }
}

bool Database::save_batch(const Batch& batch) {
    const char* sql = "INSERT INTO batches (batch_id, status, started_at, completed_at, engine_version, total_records, matched_records, exception_records) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(batch_id) DO UPDATE SET "
                      "status=excluded.status, completed_at=excluded.completed_at, total_records=excluded.total_records, "
                      "matched_records=excluded.matched_records, exception_records=excluded.exception_records;";
                      
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_error("prepare save_batch");
        return false;
    }

    if (!safe_bind_text(stmt, 1, batch.batch_id) ||
        !safe_bind_text(stmt, 2, batch.status) ||
        !safe_bind_int64(stmt, 3, batch.started_at) ||
        !safe_bind_opt_int64(stmt, 4, batch.completed_at) ||
        !safe_bind_text(stmt, 5, batch.engine_version) ||
        !safe_bind_int64(stmt, 6, batch.total_records) ||
        !safe_bind_int64(stmt, 7, batch.matched_records) ||
        !safe_bind_int64(stmt, 8, batch.exception_records)) {
        log_error("bind save_batch");
        sqlite3_finalize(stmt);
        return false;
    }

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) log_error("step save_batch");
    
    sqlite3_finalize(stmt);
    return success;
}

bool Database::save_internal_transactions(const std::vector<InternalTransaction>& txs) {
    if (txs.empty()) return true;
    const char* sql = "INSERT INTO internal_transactions (batch_id, transaction_id, amount, tax, currency, timestamp, status) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(batch_id, transaction_id) DO UPDATE SET "
                      "amount=excluded.amount, tax=excluded.tax, currency=excluded.currency, timestamp=excluded.timestamp, status=excluded.status;";
                      
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_error("prepare save_internal_transactions");
        return false;
    }

    for (const auto& tx : txs) {
        if (!safe_bind_text(stmt, 1, tx.batch_id) ||
            !safe_bind_text(stmt, 2, tx.transaction_id) ||
            !safe_bind_int64(stmt, 3, tx.amount) ||
            !safe_bind_int64(stmt, 4, tx.tax) ||
            !safe_bind_text(stmt, 5, tx.currency) ||
            !safe_bind_int64(stmt, 6, tx.timestamp) ||
            !safe_bind_text(stmt, 7, tx.status)) {
            log_error("bind save_internal_transactions");
            sqlite3_finalize(stmt);
            return false;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            log_error("step save_internal_transactions");
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool Database::save_bank_settlements(const std::vector<BankSettlement>& settlements) {
    if (settlements.empty()) return true;
    const char* sql = "INSERT INTO bank_settlements (batch_id, settlement_id, transaction_id, amount, tax, currency, timestamp) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(batch_id, settlement_id) DO UPDATE SET "
                      "transaction_id=excluded.transaction_id, amount=excluded.amount, tax=excluded.tax, currency=excluded.currency, timestamp=excluded.timestamp;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_error("prepare save_bank_settlements");
        return false;
    }

    for (const auto& bs : settlements) {
        if (!safe_bind_text(stmt, 1, bs.batch_id) ||
            !safe_bind_text(stmt, 2, bs.settlement_id) ||
            !safe_bind_opt_text(stmt, 3, bs.transaction_id) ||
            !safe_bind_int64(stmt, 4, bs.amount) ||
            !safe_bind_int64(stmt, 5, bs.tax) ||
            !safe_bind_text(stmt, 6, bs.currency) ||
            !safe_bind_int64(stmt, 7, bs.timestamp)) {
            log_error("bind save_bank_settlements");
            sqlite3_finalize(stmt);
            return false;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            log_error("step save_bank_settlements");
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool Database::save_reconciliation_results(const std::vector<ReconciliationResult>& results) {
    if (results.empty()) return true;
    const char* sql = "INSERT INTO reconciliation_results (batch_id, transaction_id, settlement_id, result, internal_amount, external_amount, internal_tax, external_tax, currency, timestamp_diff_seconds, reason) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(batch_id, transaction_id) DO UPDATE SET "
                      "settlement_id=excluded.settlement_id, result=excluded.result, internal_amount=excluded.internal_amount, "
                      "external_amount=excluded.external_amount, internal_tax=excluded.internal_tax, external_tax=excluded.external_tax, "
                      "currency=excluded.currency, timestamp_diff_seconds=excluded.timestamp_diff_seconds, reason=excluded.reason;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_error("prepare save_reconciliation_results");
        return false;
    }

    for (const auto& r : results) {
        if (!safe_bind_text(stmt, 1, r.batch_id) ||
            !safe_bind_text(stmt, 2, r.transaction_id) ||
            !safe_bind_opt_text(stmt, 3, r.settlement_id) ||
            !safe_bind_text(stmt, 4, r.result) ||
            !safe_bind_opt_int64(stmt, 5, r.internal_amount) ||
            !safe_bind_opt_int64(stmt, 6, r.external_amount) ||
            !safe_bind_opt_int64(stmt, 7, r.internal_tax) ||
            !safe_bind_opt_int64(stmt, 8, r.external_tax) ||
            !safe_bind_opt_text(stmt, 9, r.currency) ||
            !safe_bind_opt_int64(stmt, 10, r.timestamp_diff_seconds) ||
            !safe_bind_opt_text(stmt, 11, r.reason)) {
            log_error("bind save_reconciliation_results");
            sqlite3_finalize(stmt);
            return false;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            log_error("step save_reconciliation_results");
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool Database::save_exceptions(const std::vector<ExceptionRecord>& exceptions) {
    if (exceptions.empty()) return true;
    
    const char* sql = "INSERT INTO exceptions (batch_id, transaction_id, detected_status, internal_amount, external_amount, internal_tax, external_tax, currency, timestamp_diff_seconds, reconciliation_reason, ai_status, created_at, updated_at) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(batch_id, transaction_id) DO UPDATE SET "
                      "detected_status=excluded.detected_status, internal_amount=excluded.internal_amount, external_amount=excluded.external_amount, "
                      "internal_tax=excluded.internal_tax, external_tax=excluded.external_tax, currency=excluded.currency, "
                      "timestamp_diff_seconds=excluded.timestamp_diff_seconds, reconciliation_reason=excluded.reconciliation_reason, updated_at=excluded.updated_at;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_error("prepare save_exceptions");
        return false;
    }

    for (const auto& e : exceptions) {
        if (!safe_bind_text(stmt, 1, e.batch_id) ||
            !safe_bind_text(stmt, 2, e.transaction_id) ||
            !safe_bind_text(stmt, 3, e.detected_status) ||
            !safe_bind_opt_int64(stmt, 4, e.internal_amount) ||
            !safe_bind_opt_int64(stmt, 5, e.external_amount) ||
            !safe_bind_opt_int64(stmt, 6, e.internal_tax) ||
            !safe_bind_opt_int64(stmt, 7, e.external_tax) ||
            !safe_bind_opt_text(stmt, 8, e.currency) ||
            !safe_bind_opt_int64(stmt, 9, e.timestamp_diff_seconds) ||
            !safe_bind_opt_text(stmt, 10, e.reconciliation_reason) ||
            !safe_bind_text(stmt, 11, e.ai_status) ||
            !safe_bind_int64(stmt, 12, e.created_at) ||
            !safe_bind_opt_int64(stmt, 13, e.updated_at)) {
            log_error("bind save_exceptions");
            sqlite3_finalize(stmt);
            return false;
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            log_error("step save_exceptions");
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    return true;
}

bool Database::update_exception_ai(
    const std::string& batch_id,
    const std::string& transaction_id,
    const std::string& ai_status,
    const std::optional<std::string>& ai_classification,
    const std::optional<double>& ai_confidence,
    const std::optional<std::string>& ai_reason,
    const std::optional<std::string>& ai_recommended_action,
    int64_t updated_at
) {
    const char* sql = "UPDATE exceptions SET "
                      "ai_status = ?, "
                      "ai_classification = ?, "
                      "ai_confidence = ?, "
                      "ai_reason = ?, "
                      "ai_recommended_action = ?, "
                      "updated_at = ? "
                      "WHERE batch_id = ? AND transaction_id = ?;";

    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_error("prepare update_exception_ai");
        return false;
    }

    if (!safe_bind_text(stmt, 1, ai_status) ||
        !safe_bind_opt_text(stmt, 2, ai_classification) ||
        !safe_bind_opt_double(stmt, 3, ai_confidence) ||
        !safe_bind_opt_text(stmt, 4, ai_reason) ||
        !safe_bind_opt_text(stmt, 5, ai_recommended_action) ||
        !safe_bind_int64(stmt, 6, updated_at) ||
        !safe_bind_text(stmt, 7, batch_id) ||
        !safe_bind_text(stmt, 8, transaction_id)) {
        log_error("bind update_exception_ai");
        sqlite3_finalize(stmt);
        return false;
    }

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        log_error("step update_exception_ai");
        sqlite3_finalize(stmt);
        return false;
    }

    if (sqlite3_changes(db_) != 1) {
        std::cerr << "[Database Error] update_exception_ai: exception not found."
                  << std::endl;
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}

bool Database::save_audit_log(int64_t timestamp, const std::string& batch_id, const std::optional<std::string>& transaction_id, 
                              const std::string& event_type, const std::string& component, const std::string& status, const std::string& message) {
    const char* sql = "INSERT INTO audit_logs (timestamp, batch_id, transaction_id, event_type, component, status, message) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?);";
                      
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        log_error("prepare save_audit_log");
        return false;
    }

    if (!safe_bind_int64(stmt, 1, timestamp) ||
        !safe_bind_text(stmt, 2, batch_id) ||
        !safe_bind_opt_text(stmt, 3, transaction_id) ||
        !safe_bind_text(stmt, 4, event_type) ||
        !safe_bind_text(stmt, 5, component) ||
        !safe_bind_text(stmt, 6, status) ||
        !safe_bind_text(stmt, 7, message)) {
        log_error("bind save_audit_log");
        sqlite3_finalize(stmt);
        return false;
    }

    bool success = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!success) log_error("step save_audit_log");
    
    sqlite3_finalize(stmt);
    return success;
}