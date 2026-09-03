#ifndef MODELS_H
#define MODELS_H

#include <string>

struct InternalTransaction {
    std::string transaction_id;
    std::string payment_id;
    long long timestamp;
    double amount;
    std::string currency;
    double tax;
    std::string status;
};

struct BankSettlement {
    std::string settlement_id;
    std::string transaction_id;
    long long settlement_timestamp;
    double amount;
    std::string currency;
    double tax;
    std::string bank_reference;
    std::string status;
};

struct ReconciliationResult {
    std::string transaction_id;
    std::string result;
    std::string reason;
    double internal_amount;
    double external_amount;
    long long timestamp_diff_seconds;
    long long processed_at;
    std::string engine_version;
};

struct ExceptionRecord {
    std::string transaction_id;
    std::string status;
    double internal_amount;
    double external_amount;
    std::string internal_currency;
    std::string external_currency;
    long long timestamp_diff_seconds;
};

#endif