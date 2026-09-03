#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>
#include <sqlite3.h>
#include "models.h"

class Database {
public:
    Database(const std::string& db_path);
    ~Database();

    bool open();
    void close();

    std::vector<InternalTransaction> get_internal_transactions();
    std::vector<BankSettlement> get_bank_settlements();

    bool save_results_and_exceptions(
        const std::vector<ReconciliationResult>& results,
        const std::vector<ExceptionRecord>& exceptions,
        const std::string& batch_id
    );

    void log_audit(const std::string& event_type, const std::string& tx_id, const std::string& batch_id, const std::string& component, const std::string& message);

private:
    std::string db_path_;
    sqlite3* db_;
};

#endif