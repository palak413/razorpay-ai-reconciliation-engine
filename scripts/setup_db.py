import os
import sqlite3

DB_PATH = "database/reconciliation.db"
SCHEMA_PATH = "database/schema.sql"


def setup_database(db_path=DB_PATH, schema_path=SCHEMA_PATH):
    os.makedirs(os.path.dirname(db_path), exist_ok=True)

    if os.path.exists(db_path):
        os.remove(db_path)

    if not os.path.exists(schema_path):
        raise FileNotFoundError(
            f"Schema file not found: {schema_path}"
        )

    conn = sqlite3.connect(db_path)

    try:
        with open(schema_path, "r", encoding="utf-8") as f:
            schema_sql = f.read()

        conn.executescript(schema_sql)
        conn.commit()

        print(f"[Setup] Database initialized: {db_path}")

    finally:
        conn.close()


if __name__ == "__main__":
    setup_database()