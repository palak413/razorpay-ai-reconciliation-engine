PRAGMA journal_mode=WAL;
PRAGMA synchronous=NORMAL;
PRAGMA foreign_keys=ON;

CREATE TABLE IF NOT EXISTS internal_transactions (
    transaction_id TEXT PRIMARY KEY,
    payment_id TEXT UNIQUE,
    timestamp INTEGER NOT NULL,
    amount REAL NOT NULL,
    currency TEXT NOT NULL,
    tax REAL NOT NULL,
    status TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS bank_settlements (
    settlement_id TEXT PRIMARY KEY,
    transaction_id TEXT,
    settlement_timestamp INTEGER NOT NULL,
    amount REAL NOT NULL,
    currency TEXT NOT NULL,
    tax REAL NOT NULL,
    bank_reference TEXT UNIQUE,
    status TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS reconciliation_results (
    transaction_id TEXT PRIMARY KEY,
    result TEXT NOT NULL,
    reason TEXT,
    internal_amount REAL,
    external_amount REAL,
    timestamp_diff_seconds INTEGER,
    processed_at INTEGER NOT NULL,
    engine_version TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS exceptions (
    exception_id INTEGER PRIMARY KEY AUTOINCREMENT,
    transaction_id TEXT UNIQUE,
    status TEXT NOT NULL,
    internal_amount REAL,
    external_amount REAL,
    internal_currency TEXT,
    external_currency TEXT,
    timestamp_diff_seconds INTEGER,
    ai_classification TEXT,
    ai_confidence REAL,
    ai_reason TEXT,
    ai_recommended_action TEXT,
    ai_status TEXT DEFAULT 'PENDING'
);

CREATE TABLE IF NOT EXISTS audit_logs (
    log_id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER NOT NULL,
    event_type TEXT NOT NULL,
    transaction_id TEXT,
    batch_id TEXT,
    component TEXT NOT NULL,
    message TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_bank_settlement_tx ON bank_settlements(transaction_id);
CREATE INDEX IF NOT EXISTS idx_internal_tx_amount ON internal_transactions(amount, currency);