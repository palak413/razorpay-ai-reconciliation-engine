#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <optional>
#include <sqlite3.h>
#include "models.h"

class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database();

    bool connect();
    void disconnect();

    bool begin_transaction();
    bool commit();
    bool rollback();

    bool save_batch(const Batch& batch);
    bool save_internal_transactions(const std::vector<InternalTransaction>& txs);
    bool save_bank_settlements(const std::vector<BankSettlement>& settlements);
    bool save_reconciliation_results(const std::vector<ReconciliationResult>& results);
    bool save_exceptions(const std::vector<ExceptionRecord>& exceptions);
    
    bool update_exception_ai(
        const std::string& batch_id,
        const std::string& transaction_id,
        const std::string& ai_status,
        const std::optional<std::string>& ai_classification,
        const std::optional<double>& ai_confidence,
        const std::optional<std::string>& ai_reason,
        const std::optional<std::string>& ai_recommended_action,
        int64_t updated_at
    );

    bool save_audit_log(int64_t timestamp,
                        const std::string& batch_id,
                        const std::optional<std::string>& transaction_id,
                        const std::string& event_type,
                        const std::string& component,
                        const std::string& status,
                        const std::string& message);

private:
    std::string db_path_;
    sqlite3* db_ = nullptr;

    void log_error(const std::string& operation);
};

#endif // DATABASE_H