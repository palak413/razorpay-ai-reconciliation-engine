#ifndef RECONCILIATION_ENGINE_H
#define RECONCILIATION_ENGINE_H

#include <string>
#include <vector>
#include "models.h"

class ReconciliationEngine {
public:
    explicit ReconciliationEngine(const std::string& engine_version);

    void process(
        const std::vector<InternalTransaction>& internal_txs,
        const std::vector<BankSettlement>& bank_settlements,
        std::vector<ReconciliationResult>& out_results,
        std::vector<ExceptionRecord>& out_exceptions
    );

private:
    std::string version_;
    static constexpr int64_t TIMESTAMP_TOLERANCE_SECONDS = 86400; // 24-hour window
};

#endif // RECONCILIATION_ENGINE_H