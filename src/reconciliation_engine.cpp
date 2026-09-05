#include "reconciliation_engine.h"
#include <cmath>
#include <ctime>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <sstream>
#include <iostream>

ReconciliationEngine::ReconciliationEngine(const std::string& engine_version) : version_(engine_version) {}

void ReconciliationEngine::process(
    const std::vector<InternalTransaction>& internal_txs,
    const std::vector<BankSettlement>& bank_settlements,
    std::vector<ReconciliationResult>& out_results,
    std::vector<ExceptionRecord>& out_exceptions
) {
    // Ensure repeated calls on the same engine object do not append stale results
    out_results.clear();
    out_exceptions.clear();

    // Validate Batch Consistency across vectors
    std::string expected_batch_id = "";
    
    for (const auto& tx : internal_txs) {
        if (expected_batch_id.empty()) {
            expected_batch_id = tx.batch_id;
        } else if (tx.batch_id != expected_batch_id) {
            std::cerr << "[Reconciliation Error] Mixed batch IDs detected in internal transactions." << std::endl;
            return;
        }
    }

    for (const auto& bs : bank_settlements) {
        if (expected_batch_id.empty()) {
            expected_batch_id = bs.batch_id;
        } else if (bs.batch_id != expected_batch_id) {
            std::cerr << "[Reconciliation Error] Mixed batch IDs detected between internal transactions and bank settlements." << std::endl;
            return;
        }
    }

    int64_t now = std::time(nullptr);

    // Map transaction_id to all bank settlements to handle duplicates without overwriting
    std::unordered_map<std::string, std::vector<BankSettlement>> bank_map;

    for (const auto& bs : bank_settlements) {
        if (bs.transaction_id.has_value() && !bs.transaction_id->empty()) {
            bank_map[*bs.transaction_id].push_back(bs);
        }
    }

    // Batch-aware processed settlement tracking using composite keys (batch_id | settlement_id)
    std::unordered_set<std::string> processed_settlement_keys;

    for (const auto& tx : internal_txs) {
        ReconciliationResult res;
        res.batch_id = tx.batch_id;
        res.transaction_id = tx.transaction_id;
        res.internal_amount = tx.amount;
        res.internal_tax = tx.tax;
        res.currency = tx.currency;

        auto it = bank_map.find(tx.transaction_id);

        if (it == bank_map.end() || it->second.empty()) {
            res.result = "MISSING_EXTERNAL";
            res.reason = "No settlement record found in bank file";
            res.external_amount = std::nullopt;
            res.external_tax = std::nullopt;
            res.settlement_id = std::nullopt;
            res.timestamp_diff_seconds = std::nullopt;

            out_results.push_back(res);
            out_exceptions.push_back({
                tx.batch_id,
                tx.transaction_id,
                res.result,
                tx.amount,
                std::nullopt,
                tx.tax,
                std::nullopt,
                tx.currency,
                std::nullopt,
                res.reason,
                "PENDING",
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                now,
                std::nullopt
            });
            continue;
        }

        auto settlements = it->second;

        if (settlements.size() > 1) {
            // Sort duplicate settlements deterministically by settlement_id
            std::sort(settlements.begin(), settlements.end(), [](const BankSettlement& a, const BankSettlement& b) {
                return a.settlement_id < b.settlement_id;
            });

            std::vector<std::string> s_ids;
            for (const auto& bs : settlements) {
                s_ids.push_back(bs.settlement_id);
            }

            std::ostringstream reason_ss;
            reason_ss << "Multiple bank settlements reference this transaction ID: ";
            for (size_t i = 0; i < s_ids.size(); ++i) {
                reason_ss << s_ids[i];
                if (i + 1 < s_ids.size()) reason_ss << ", ";
            }

            res.result = "DUPLICATE_SETTLEMENT";
            res.reason = reason_ss.str();
            
            // Use the deterministically sorted first element as primary reference
            res.settlement_id = settlements[0].settlement_id;
            res.external_amount = settlements[0].amount;
            res.external_tax = settlements[0].tax;
            res.timestamp_diff_seconds = std::nullopt; // Duplicate settlements do not have a meaningful single timestamp comparison

            out_results.push_back(res);
            out_exceptions.push_back({
                tx.batch_id,
                tx.transaction_id,
                res.result,
                tx.amount,
                settlements[0].amount,
                tx.tax,
                settlements[0].tax,
                tx.currency,
                std::nullopt,
                res.reason,
                "PENDING",
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                now,
                std::nullopt
            });

            for (const auto& bs : settlements) {
                processed_settlement_keys.insert(bs.batch_id + "|" + bs.settlement_id);
            }
            continue;
        }

        // Exactly one settlement exists
        const auto& bs = settlements[0];
        processed_settlement_keys.insert(bs.batch_id + "|" + bs.settlement_id);
        res.settlement_id = bs.settlement_id;
        res.external_amount = bs.amount;
        res.external_tax = bs.tax;

        int64_t diff = (bs.timestamp >= tx.timestamp) ? (bs.timestamp - tx.timestamp) : (tx.timestamp - bs.timestamp);
        res.timestamp_diff_seconds = diff;

        // Priority validation order: Currency -> Amount -> Tax -> Timestamp -> MATCHED
        if (tx.currency != bs.currency) {
            res.result = "CURRENCY_MISMATCH";
            res.reason = "Internal currency differs from bank currency";
        } else if (tx.amount != bs.amount) {
            res.result = "AMOUNT_MISMATCH";
            res.reason = "Amount discrepancy detected";
        } else if (tx.tax != bs.tax) {
            res.result = "TAX_MISMATCH";
            res.reason = "Tax differs between ledger and settlement";
        } else if (diff > TIMESTAMP_TOLERANCE_SECONDS) {
            res.result = "TIMESTAMP_MISMATCH";
            res.reason = "Settlement time gap exceeds tolerance window";
        } else {
            res.result = "MATCHED";
            res.reason = "Exact match on transaction ID, amount, tax, currency, and timestamp window";
        }

        out_results.push_back(res);

        if (res.result != "MATCHED") {
            out_exceptions.push_back({
                tx.batch_id,
                tx.transaction_id,
                res.result,
                tx.amount,
                bs.amount,
                tx.tax,
                bs.tax,
                tx.currency,
                diff,
                res.reason,
                "PENDING",
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                now,
                std::nullopt
            });
        }
    }

    // Scan for orphan / missing internal settlements (including NULL transaction IDs)
    for (const auto& bs : bank_settlements) {
        std::string composite_key = bs.batch_id + "|" + bs.settlement_id;
        if (processed_settlement_keys.find(composite_key) == processed_settlement_keys.end()) {
            std::string orphan_id = (bs.transaction_id.has_value() && !bs.transaction_id->empty()) ? *bs.transaction_id : bs.settlement_id;

            ReconciliationResult res;
            res.batch_id = bs.batch_id;
            res.transaction_id = orphan_id;
            res.settlement_id = bs.settlement_id;
            res.result = "MISSING_INTERNAL";
            res.reason = "Bank settlement exists without corresponding internal transaction";
            res.internal_amount = std::nullopt;
            res.external_amount = bs.amount;
            res.internal_tax = std::nullopt;
            res.external_tax = bs.tax;
            res.currency = bs.currency;
            res.timestamp_diff_seconds = std::nullopt;

            out_results.push_back(res);
            out_exceptions.push_back({
                bs.batch_id,
                orphan_id,
                res.result,
                std::nullopt,
                bs.amount,
                std::nullopt,
                bs.tax,
                bs.currency,
                std::nullopt,
                res.reason,
                "PENDING",
                std::nullopt,
                std::nullopt,
                std::nullopt,
                std::nullopt,
                now,
                std::nullopt
            });
        }
    }
}