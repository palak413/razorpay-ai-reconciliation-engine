PRAGMA journal_mode=WAL;
PRAGMA foreign_keys=ON;

-- 1. Batch Execution Tracking
CREATE TABLE IF NOT EXISTS batches (
    batch_id TEXT PRIMARY KEY,
    status TEXT NOT NULL CHECK(status IN ('PENDING', 'RUNNING', 'COMPLETED', 'FAILED', 'ROLLED_BACK')),
    started_at INTEGER NOT NULL,
    completed_at INTEGER,
    engine_version TEXT NOT NULL,
    total_records INTEGER DEFAULT 0,
    matched_records INTEGER DEFAULT 0,
    exception_records INTEGER DEFAULT 0
);

-- 2. Raw Ingestion: Internal Ledger
CREATE TABLE IF NOT EXISTS internal_transactions (
    batch_id TEXT NOT NULL,
    transaction_id TEXT NOT NULL,
    amount INTEGER NOT NULL,
    tax INTEGER NOT NULL,
    currency TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    status TEXT NOT NULL,
    PRIMARY KEY (batch_id, transaction_id),
    FOREIGN KEY (batch_id) REFERENCES batches(batch_id)
);

-- 3. Raw Ingestion: External Bank Settlements
CREATE TABLE IF NOT EXISTS bank_settlements (
    batch_id TEXT NOT NULL,
    settlement_id TEXT NOT NULL,
    transaction_id TEXT,
    amount INTEGER NOT NULL,
    tax INTEGER NOT NULL,
    currency TEXT NOT NULL,
    timestamp INTEGER NOT NULL,
    PRIMARY KEY (batch_id, settlement_id),
    FOREIGN KEY (batch_id) REFERENCES batches(batch_id)
);

-- 4. Deterministic Results
CREATE TABLE IF NOT EXISTS reconciliation_results (
    batch_id TEXT NOT NULL,
    transaction_id TEXT NOT NULL,
    settlement_id TEXT,
    result TEXT NOT NULL,
    internal_amount INTEGER,
    external_amount INTEGER,
    internal_tax INTEGER,
    external_tax INTEGER,
    currency TEXT,
    timestamp_diff_seconds INTEGER,
    reason TEXT,
    PRIMARY KEY (batch_id, transaction_id),
    FOREIGN KEY (batch_id) REFERENCES batches(batch_id)
);

-- 5. Exception Queue & AI Diagnostics
CREATE TABLE IF NOT EXISTS exceptions (
    batch_id TEXT NOT NULL,
    transaction_id TEXT NOT NULL,
    detected_status TEXT NOT NULL,
    internal_amount INTEGER,
    external_amount INTEGER,
    internal_tax INTEGER,
    external_tax INTEGER,
    currency TEXT,
    timestamp_diff_seconds INTEGER,
    reconciliation_reason TEXT,
    ai_status TEXT DEFAULT 'PENDING' CHECK(ai_status IN ('PENDING', 'PROCESSING', 'COMPLETED', 'FAILED')),
    ai_classification TEXT,
    ai_confidence REAL,
    ai_reason TEXT,
    ai_recommended_action TEXT,
    created_at INTEGER NOT NULL,
    updated_at INTEGER,
    PRIMARY KEY (batch_id, transaction_id),
    FOREIGN KEY (batch_id) REFERENCES batches(batch_id)
);

-- 6. Append-Only Audit Trail
CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    batch_id TEXT NOT NULL,
    transaction_id TEXT,
    event_type TEXT NOT NULL,
    component TEXT NOT NULL,
    status TEXT NOT NULL,
    message TEXT NOT NULL,
    FOREIGN KEY (batch_id) REFERENCES batches(batch_id)
);

-- 7. Indexes
CREATE INDEX IF NOT EXISTS idx_reconciliation_tx_id ON reconciliation_results(transaction_id);
CREATE INDEX IF NOT EXISTS idx_exceptions_tx_id ON exceptions(transaction_id);
CREATE INDEX IF NOT EXISTS idx_exceptions_detected_status ON exceptions(detected_status);
CREATE INDEX IF NOT EXISTS idx_exceptions_ai_status ON exceptions(ai_status);
CREATE INDEX IF NOT EXISTS idx_audit_logs_batch_id ON audit_logs(batch_id);