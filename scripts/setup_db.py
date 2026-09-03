import sqlite3
import csv
import os

DB_PATH = "database/reconciliation.db"
SCHEMA_PATH = "database/schema.sql"

if os.path.exists(DB_PATH):
    os.remove(DB_PATH)

conn = sqlite3.connect(DB_PATH)
cursor = conn.cursor()

with open(SCHEMA_PATH, "r") as f:
    cursor.executescript(f.read())

# Load Internal Ledger
with open("data/internal_ledger.csv", "r") as f:
    reader = csv.DictReader(f)
    for row in reader:
        cursor.execute("""
            INSERT INTO internal_transactions 
            (transaction_id, payment_id, timestamp, amount, currency, tax, status)
            VALUES (?, ?, ?, ?, ?, ?, ?)
        """, (row["transaction_id"], row["payment_id"], int(row["timestamp"]), 
              float(row["amount"]), row["currency"], float(row["tax"]), row["status"]))

# Load Bank Settlements
with open("data/bank_settlement.csv", "r") as f:
    reader = csv.DictReader(f)
    for row in reader:
        cursor.execute("""
            INSERT INTO bank_settlements 
            (settlement_id, transaction_id, settlement_timestamp, amount, currency, tax, bank_reference, status)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, (row["settlement_id"], row["transaction_id"], int(row["settlement_timestamp"]), 
              float(row["amount"]), row["currency"], float(row["tax"]), row["bank_reference"], row["status"]))

conn.commit()
conn.close()

print(f"Database initialized successfully at {DB_PATH} with WAL mode enabled.")