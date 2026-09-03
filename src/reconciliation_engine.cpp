#include "reconciliation_engine.h"
#include <cmath>
#include <ctime>
#include <algorithm>

ReconciliationEngine::ReconciliationEngine(const std::string& engine_version) : version_(engine_version) {}

void ReconciliationEngine::process(
    const std::vector<InternalTransaction>& internal_txs,
    const std::vector<BankSettlement>& bank_settlements,
    std::vector<ReconciliationResult>& out_results,
    std::vector<ExceptionRecord>& out_exceptions
) {
    long long now = std::time(nullptr);

    std::unordered_map<std::string, BankSettlement> bank_map;
    std::unordered_map<std::string, int> bank_tx_count;

    for (const auto& bs : bank_settlements) {
        if (!bs.transaction_id.empty()) {
            bank_map[bs.transaction_id] = bs;
            bank_tx_count[bs.transaction_id]++;
        }
    }

    std::unordered_map<std::string, bool> processed_bank_ids;

    for (const auto& tx : internal_txs) {
        ReconciliationResult res;
        res.transaction_id = tx.transaction_id;
        res.internal_amount = tx.amount;
        res.processed_at = now;
        res.engine_version = version_;

        auto it = bank_map.find(tx.transaction_id);

        if (it == bank_map.end()) {
            res.result = "MISSING_EXTERNAL";
            res.reason = "No settlement record found in bank file";
            res.external_amount = 0.0;
            res.timestamp_diff_seconds = 0;
            out_results.push_back(res);

            out_exceptions.push_back({tx.transaction_id, res.result, tx.amount, 0.0, tx.currency, "N/A", 0});
            continue;
        }

        const auto& bs = it->second;
        processed_bank_ids[bs.settlement_id] = true;
        res.external_amount = bs.amount;
        long long diff = std::abs(bs.settlement_timestamp - tx.timestamp);
        res.timestamp_diff_seconds = diff;

        if (bank_tx_count[tx.transaction_id] > 1) {
            res.result = "DUPLICATE";
            res.reason = "Multiple bank settlements reference this transaction ID";
            out_results.push_back(res);
            out_exceptions.push_back({tx.transaction_id, res.result, tx.amount, bs.amount, tx.currency, bs.currency, diff});
            continue;
        }

        if (tx.currency != bs.currency) {
            res.result = "CURRENCY_MISMATCH";
            res.reason = "Internal currency differs from bank currency";
            out_results.push_back(res);
            out_exceptions.push_back({tx.transaction_id, res.result, tx.amount, bs.amount, tx.currency, bs.currency, diff});
            continue;
        }

        if (std::abs(tx.amount - bs.amount) > 0.01) {
            res.result = "AMOUNT_MISMATCH";
            res.reason = "Amount discrepancy detected";
            out_results.push_back(res);
            out_exceptions.push_back({tx.transaction_id, res.result, tx.amount, bs.amount, tx.currency, bs.currency, diff});
            continue;
        }

        if (diff > 86400) {
            res.result = "TIMESTAMP_MISMATCH";
            res.reason = "Settlement time gap exceeds 24-hour window";
            out_results.push_back(res);
            out_exceptions.push_back({tx.transaction_id, res.result, tx.amount, bs.amount, tx.currency, bs.currency, diff});
            continue;
        }

        res.result = "MATCHED";
        res.reason = "Exact match on transaction ID, amount, currency, and timestamp window";
        out_results.push_back(res);
    }

    for (const auto& bs : bank_settlements) {
        if (bs.transaction_id.empty() || processed_bank_ids.find(bs.settlement_id) == processed_bank_ids.end()) {
            std::string orphan_id = bs.transaction_id.empty() ? bs.settlement_id : bs.transaction_id;
            ReconciliationResult res;
            res.transaction_id = orphan_id;
            res.result = "MISSING_INTERNAL";
            res.reason = "Bank settlement exists without corresponding internal transaction";
            res.internal_amount = 0.0;
            res.external_amount = bs.amount;
            res.timestamp_diff_seconds = 0;
            res.processed_at = now;
            res.engine_version = version_;
            out_results.push_back(res);

            out_exceptions.push_back({orphan_id, res.result, 0.0, bs.amount, "N/A", bs.currency, 0});
        }
    }
}