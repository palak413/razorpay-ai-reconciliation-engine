import sqlite3
import subprocess

DB_PATH = "database/reconciliation.db"

def test_idempotency():
    print("Running Idempotency & Duplicate Execution Test...")
    
    # First Run
    subprocess.run(f"./build/reconciliation_engine {DB_PATH}", shell=True, capture_output=True)
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM reconciliation_results;")
    count_after_first = cursor.fetchone()[0]
    conn.close()

    # Second Run (Reprocessing the exact same batch)
    subprocess.run(f"./build/reconciliation_engine {DB_PATH}", shell=True, capture_output=True)
    
    conn = sqlite3.connect(DB_PATH)
    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM reconciliation_results;")
    count_after_second = cursor.fetchone()[0]
    conn.close()
    
    assert count_after_first == count_after_second, "Idempotency violated: Duplicate results created on re-run!"
    print(f"✅ Idempotency Verified: Count remained stable at {count_after_second} records after re-execution.")

if __name__ == "__main__":
    test_idempotency()