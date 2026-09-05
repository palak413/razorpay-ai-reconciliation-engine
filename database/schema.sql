PRAGMA journal_mode=WAL;

CREATE TABLE IF NOT EXISTS batches (
    batch_id TEXT PRIMARY KEY,
    status TEXT NOT NULL,
    started_at INTEGER NOT NULL,
    completed_at INTEGER,
    engine_version TEXT NOT NULL,
    total_records INTEGER DEFAULT 0,
    matched_records INTEGER DEFAULT 0,
    exception_records INTEGER DEFAULT 0
);

CREATE TABLE IF NOT EXISTS reconciliation_results (
    batch_id TEXT NOT NULL,
    transaction_id TEXT NOT NULL,
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
    ai_status TEXT DEFAULT 'PENDING',
    ai_classification TEXT,
    ai_confidence REAL,
    ai_reason TEXT,
    ai_recommended_action TEXT,
    PRIMARY KEY (batch_id, transaction_id)
);

CREATE TABLE IF NOT EXISTS audit_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    batch_id TEXT,
    transaction_id TEXT,
    component TEXT NOT NULL,
    status TEXT NOT NULL,
    message TEXT NOT NULL
);