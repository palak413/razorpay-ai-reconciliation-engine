import csv
import random
import time

SEED = 42
random.seed(SEED)

base_time = 1756896000 # Fixed epoch timestamp for reproducibility

internal_records = []
bank_records = []

# Generate 100 Base Transactions
for i in range(1, 101):
    tx_id = f"TX10{i:02d}"
    pay_id = f"PAY_{i:04d}"
    ts = base_time + (i * 300)
    amt = round(random.uniform(100.0, 5000.0), 2)
    curr = "INR"
    tax = round(amt * 0.18, 2)
    status = "SUCCESS"

    internal_records.append({
        "transaction_id": tx_id,
        "payment_id": pay_id,
        "timestamp": ts,
        "amount": amt,
        "currency": curr,
        "tax": tax,
        "status": status
    })

    # Bank Settlement mapping (1:1 standard baseline)
    bank_records.append({
        "settlement_id": f"SETTL_{i:04d}",
        "transaction_id": tx_id,
        "settlement_timestamp": ts + random.randint(5, 120),
        "amount": amt,
        "currency": curr,
        "tax": tax,
        "bank_reference": f"BANK_REF_{i:04d}",
        "status": "SETTLED"
    })

# Controlled Discrepancy Injections
# 1. Amount Mismatch
bank_records[10]["amount"] = bank_records[10]["amount"] + 150.00 

# 2. Currency Mismatch
bank_records[20]["currency"] = "USD"

# 3. Tax Mismatch
bank_records[30]["tax"] = round(bank_records[30]["tax"] + 25.00, 2)

# 4. Timestamp Mismatch (>24 hrs gap)
bank_records[40]["settlement_timestamp"] = internal_records[40]["timestamp"] + 90000 

# 5. Missing External (Remove bank record)
bank_records.pop(50)

# 6. Missing Internal (Add orphan bank record)
bank_records.append({
    "settlement_id": "SETTL_9999",
    "transaction_id": "TX9999",
    "settlement_timestamp": base_time + 1000,
    "amount": 750.00,
    "currency": "INR",
    "tax": 135.00,
    "bank_reference": "BANK_REF_9999",
    "status": "SETTLED"
})

# 7. Duplicate Settlement
bank_records.append({
    "settlement_id": "SETTL_DUP_001",
    "transaction_id": internal_records[60]["transaction_id"],
    "settlement_timestamp": internal_records[60]["timestamp"] + 10,
    "amount": internal_records[60]["amount"],
    "currency": "INR",
    "tax": internal_records[60]["tax"],
    "bank_reference": "BANK_REF_DUP_60",
    "status": "SETTLED"
})

# Write Internal CSV
with open("data/internal_ledger.csv", "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=internal_records[0].keys())
    writer.writeheader()
    writer.writerows(internal_records)

# Write Bank Settlement CSV
with open("data/bank_settlement.csv", "w", newline="") as f:
    writer = csv.DictWriter(f, fieldnames=bank_records[0].keys())
    writer.writeheader()
    writer.writerows(bank_records)

print(f"Data generated: {len(internal_records)} internal records, {len(bank_records)} bank records.")