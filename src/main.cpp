#include <iostream>
#include <string>
#include "database.h"
#include "reconciliation_engine.h"

int main(int argc, char* argv[]) {
    std::string db_path = "database/reconciliation.db";
    std::string batch_id = "BATCH_20260903_001";

    if (argc > 1) {
        db_path = argv[1];
    }

    Database db(db_path);
    if (!db.open()) {
        std::cerr << "Error: Could not open database." << std::endl;
        return 1;
    }

    db.log_audit("BATCH_STARTED", "", batch_id, "ENGINE", "Reconciliation batch started");

    auto internal_txs = db.get_internal_transactions();
    auto bank_settlements = db.get_bank_settlements();

    std::cout << "Loaded " << internal_txs.size() << " internal records and " 
              << bank_settlements.size() << " bank records." << std::endl;

    ReconciliationEngine engine("v1.0.0");
    std::vector<ReconciliationResult> results;
    std::vector<ExceptionRecord> exceptions;

    engine.process(internal_txs, bank_settlements, results, exceptions);

    if (db.save_results_and_exceptions(results, exceptions, batch_id)) {
        db.log_audit("BATCH_COMPLETED", "", batch_id, "ENGINE", "Reconciliation batch completed successfully");
        std::cout << "Reconciliation finished. Results: " << results.size() 
                  << " | Exceptions queued: " << exceptions.size() << std::endl;
    } else {
        db.log_audit("BATCH_FAILED", "", batch_id, "ENGINE", "Failed to save reconciliation batch results");
        std::cerr << "Error: Transaction failed while persisting results." << std::endl;
        return 1;
    }

    return 0;
}