#ifndef RECONCILIATION_ENGINE_H
#define RECONCILIATION_ENGINE_H

#include <vector>
#include <unordered_map>
#include <string>
#include "models.h"

class ReconciliationEngine {
public:
    ReconciliationEngine(const std::string& engine_version = "v1.0.0");

    void process(
        const std::vector<InternalTransaction>& internal_txs,
        const std::vector<BankSettlement>& bank_settlements,
        std::vector<ReconciliationResult>& out_results,
        std::vector<ExceptionRecord>& out_exceptions
    );

private:
    std::string version_;
};

#endif