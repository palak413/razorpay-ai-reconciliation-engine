#ifndef RECONCILIATION_ENGINE_H
#define RECONCILIATION_ENGINE_H

#include <string>
#include <vector>

// Represented in minor units (e.g., paise/cents) to eliminate floating point drift
struct InternalTransaction {
    std::string transaction_id;
    int64_t amount; 
    int64_t tax;    
    std::string currency;
    long long timestamp;
    std::string status;
};

struct BankSettlement {
    std::string settlement_id;
    std::string transaction_id;
    int64_t amount; 
    int64_t tax;    
    std::string currency;
    long long settlement_timestamp;
};

struct ReconciliationResult {
    std::string transaction_id;
    std::string result;
    std::string reason;
    int64_t internal_amount = 0;
    int64_t external_amount = 0;
    int64_t internal_tax = 0;
    int64_t external_tax = 0;
    long long timestamp_diff_seconds = 0;
    long long processed_at = 0;
    std::string engine_version;
};

struct ExceptionRecord {
    std::string transaction_id;
    std::string detected_status;
    int64_t internal_amount = 0;
    int64_t external_amount = 0;
    int64_t internal_tax = 0;
    int64_t external_tax = 0;
    std::string internal_currency;
    std::string external_currency;
    long long timestamp_diff_seconds = 0;
};

class ReconciliationEngine {
public:
    ReconciliationEngine(const std::string& engine_version);

    void process(
        const std::vector<InternalTransaction>& internal_txs,
        const std::vector<BankSettlement>& bank_settlements,
        std::vector<ReconciliationResult>& out_results,
        std::vector<ExceptionRecord>& out_exceptions
    );

private:
    std::string version_;
};

#endif // RECONCILIATION_ENGINE_H