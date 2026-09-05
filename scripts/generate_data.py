import csv
import random

SEED = 42
random.seed(SEED)

BASE_TIME = 1756896000

internal_records = []
bank_records = []

for i in range(1, 101):
    tx_id = f"TX10{i:02d}"
    ts = int(BASE_TIME + i * 300)

    amount = round(random.uniform(100.0, 5000.0), 2)
    tax = round(amount * 0.18, 2)

    internal_records.append({
        "transaction_id": tx_id,
        "amount": amount,
        "tax": tax,
        "currency": "INR",
        "timestamp": str(ts),
        "status": "SUCCESS"
    })

    bank_records.append({
        "settlement_id": f"SET_{i:04d}",
        "transaction_id": tx_id,
        "amount": amount,
        "tax": tax,
        "currency": "INR",
        "timestamp": str(ts + random.randint(5, 120))
    })


# Amount mismatch - 2
bank_records[10]["amount"] += 150.00
bank_records[11]["amount"] += 200.00


# Currency mismatch - 2
bank_records[20]["currency"] = "USD"
bank_records[21]["currency"] = "USD"


# Tax mismatch - 2
bank_records[30]["tax"] += 25.00
bank_records[31]["tax"] += 30.00


# Timestamp mismatch - 2
bank_records[40]["timestamp"] = str(
    int(internal_records[40]["timestamp"]) + 90000
)

bank_records[41]["timestamp"] = str(
    int(internal_records[41]["timestamp"]) + 100000
)


# Missing external - 2
missing_ids = {"TX1051", "TX1052"}

bank_records = [
    x for x in bank_records
    if x["transaction_id"] not in missing_ids
]


# Missing internal - 4
bank_records.extend([
    {
        "settlement_id": "SET_ORPHAN_01",
        "transaction_id": "TX9991",
        "amount": 750.00,
        "tax": 135.00,
        "currency": "INR",
        "timestamp": str(BASE_TIME + 1000)
    },
    {
        "settlement_id": "SET_ORPHAN_02",
        "transaction_id": "TX9992",
        "amount": 850.00,
        "tax": 153.00,
        "currency": "INR",
        "timestamp": str(BASE_TIME + 2000)
    },
    {
        "settlement_id": "SET_NULL_01",
        "transaction_id": "",
        "amount": 950.00,
        "tax": 171.00,
        "currency": "INR",
        "timestamp": str(BASE_TIME + 3000)
    },
    {
        "settlement_id": "SET_NULL_02",
        "transaction_id": "",
        "amount": 1050.00,
        "tax": 189.00,
        "currency": "INR",
        "timestamp": str(BASE_TIME + 4000)
    }
])


# Duplicate settlement - 2
for idx, suffix in [(60, "A"), (61, "B")]:
    bank_records.append({
        "settlement_id": f"SET_DUP_{suffix}",
        "transaction_id": internal_records[idx]["transaction_id"],
        "amount": internal_records[idx]["amount"],
        "tax": internal_records[idx]["tax"],
        "currency": internal_records[idx]["currency"],
        "timestamp": str(
            int(internal_records[idx]["timestamp"]) + 10
        )
    })


# Internal CSV
with open(
    "data/internal_ledger.csv",
    "w",
    newline="",
    encoding="utf-8"
) as f:

    fields = [
        "transaction_id",
        "amount",
        "tax",
        "currency",
        "timestamp",
        "status"
    ]

    writer = csv.DictWriter(
        f,
        fieldnames=fields,
        lineterminator="\n"
    )

    writer.writeheader()
    writer.writerows(internal_records)


# Bank CSV
with open(
    "data/bank_settlement.csv",
    "w",
    newline="",
    encoding="utf-8"
) as f:

    fields = [
        "settlement_id",
        "transaction_id",
        "amount",
        "tax",
        "currency",
        "timestamp"
    ]

    writer = csv.DictWriter(
        f,
        fieldnames=fields,
        lineterminator="\n"
    )

    writer.writeheader()
    writer.writerows(bank_records)


print(
    f"Data generated: "
    f"{len(internal_records)} internal records, "
    f"{len(bank_records)} bank records."
)