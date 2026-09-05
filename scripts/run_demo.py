import sqlite3
import subprocess
import sys

DB_PATH = "database/reconciliation.db"
ENGINE = "./build/reconciliation_engine"


def run_command(cmd, description):
    print(f"\n[Demo] {description}")

    result = subprocess.run(cmd)

    if result.returncode != 0:
        print(f"[Demo Error] {description} failed.")
        sys.exit(result.returncode)


def get_batch_id():
    conn = sqlite3.connect(DB_PATH)

    try:
        row = conn.execute(
            """
            SELECT batch_id
            FROM batches
            ORDER BY started_at DESC, rowid DESC
            LIMIT 1
            """
        ).fetchone()

        if not row:
            raise RuntimeError(
                "No batch found after reconciliation."
            )

        return row[0]

    finally:
        conn.close()


def run_ai(batch_id):
    print(
        f"\n[Demo] Running AI diagnostics for batch: {batch_id}"
    )

    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "ai.diagnostic_agent",
            batch_id
        ]
    )

    if result.returncode != 0:
        print(
            "[Demo Warning] AI diagnostic process returned "
            "a non-zero status."
        )
        print(
            "Deterministic reconciliation remains completed "
            "independently of AI."
        )


def print_summary(batch_id):
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row

    try:
        batch = conn.execute(
            """
            SELECT *
            FROM batches
            WHERE batch_id = ?
            """,
            (batch_id,)
        ).fetchone()

        results = conn.execute(
            """
            SELECT result, COUNT(*) AS cnt
            FROM reconciliation_results
            WHERE batch_id = ?
            GROUP BY result
            ORDER BY result
            """,
            (batch_id,)
        ).fetchall()

        exceptions = conn.execute(
            """
            SELECT detected_status, COUNT(*) AS cnt
            FROM exceptions
            WHERE batch_id = ?
            GROUP BY detected_status
            ORDER BY detected_status
            """,
            (batch_id,)
        ).fetchall()

        ai_status = conn.execute(
            """
            SELECT ai_status, COUNT(*) AS cnt
            FROM exceptions
            WHERE batch_id = ?
            GROUP BY ai_status
            """,
            (batch_id,)
        ).fetchall()

        total = batch["total_records"]
        matched = batch["matched_records"]
        exception_count = batch["exception_records"]

        match_rate = (
            matched / total * 100.0
            if total > 0
            else 0.0
        )

        print("\n========================================")
        print("FINAL OPERATIONAL SUMMARY")
        print("========================================")
        print(f"Batch ID: {batch_id}")
        print(f"Status: {batch['status']}")
        print(f"Internal Records: {total}")
        print(
            f"Matched Records: "
            f"{matched} ({match_rate:.2f}%)"
        )
        print(f"Exception Records: {exception_count}")

        print("\nReconciliation Breakdown:")

        for row in results:
            print(
                f"  - {row['result']}: {row['cnt']}"
            )

        print("\nException Breakdown:")

        for row in exceptions:
            print(
                f"  - {row['detected_status']}: {row['cnt']}"
            )

        counts = {
            row["ai_status"]: row["cnt"]
            for row in ai_status
        }

        print("\nAI Diagnostic Status:")
        print(
            f"  - COMPLETED: "
            f"{counts.get('COMPLETED', 0)}"
        )
        print(
            f"  - FAILED: "
            f"{counts.get('FAILED', 0)}"
        )
        print(
            f"  - PENDING: "
            f"{counts.get('PENDING', 0)}"
        )

        print("========================================")

    finally:
        conn.close()


def main():

    print("========================================")
    print("RAZORPAY AI FINANCE CONTROLLER")
    print("END-TO-END DEMO")
    print("========================================")

    run_command(
        [
            sys.executable,
            "scripts/generate_data.py"
        ],
        "Generating synthetic datasets"
    )

    run_command(
        [
            sys.executable,
            "scripts/setup_db.py"
        ],
        "Initializing SQLite database"
    )

    run_command(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            "build"
        ],
        "Configuring CMake"
    )

    run_command(
        [
            "cmake",
            "--build",
            "build"
        ],
        "Building C++ reconciliation engine"
    )

    run_command(
        [ENGINE],
        "Running deterministic C++ reconciliation"
    )

    batch_id = get_batch_id()

    print(
        f"\n[Demo] Active batch: {batch_id}"
    )

    run_ai(batch_id)

    print_summary(batch_id)


if __name__ == "__main__":
    main()