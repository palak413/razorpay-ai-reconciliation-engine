#ifndef MODELS_H
#define MODELS_H

#include <string>
#include <cstdint>
#include <optional>

struct Batch {
    std::string batch_id;
    std::string status;
    int64_t started_at;
    std::optional<int64_t> completed_at;
    std::string engine_version;
    int64_t total_records = 0;
    int64_t matched_records = 0;
    int64_t exception_records = 0;
};

struct InternalTransaction {
    std::string batch_id;
    std::string transaction_id;
    int64_t amount;
    int64_t tax;
    std::string currency;
    int64_t timestamp;
    std::string status;
};

struct BankSettlement {
    std::string batch_id;
    std::string settlement_id;
    std::optional<std::string> transaction_id;
    int64_t amount;
    int64_t tax;
    std::string currency;
    int64_t timestamp;
};

struct ReconciliationResult {
    std::string batch_id;
    std::string transaction_id;
    std::optional<std::string> settlement_id;
    std::string result;
    std::optional<int64_t> internal_amount;
    std::optional<int64_t> external_amount;
    std::optional<int64_t> internal_tax;
    std::optional<int64_t> external_tax;
    std::optional<std::string> currency;
    std::optional<int64_t> timestamp_diff_seconds;
    std::optional<std::string> reason;
};

struct ExceptionRecord {
    std::string batch_id;
    std::string transaction_id;
    std::string detected_status;
    std::optional<int64_t> internal_amount;
    std::optional<int64_t> external_amount;
    std::optional<int64_t> internal_tax;
    std::optional<int64_t> external_tax;
    std::optional<std::string> currency;
    std::optional<int64_t> timestamp_diff_seconds;
    std::optional<std::string> reconciliation_reason;
    std::string ai_status = "PENDING";
    std::optional<std::string> ai_classification;
    std::optional<double> ai_confidence;
    std::optional<std::string> ai_reason;
    std::optional<std::string> ai_recommended_action;
    int64_t created_at;
    std::optional<int64_t> updated_at;
};

#endif // MODELS_H