import sqlite3
import json
import os
import time
from google import genai
from google.genai import types
from prompts import DIAGNOSTIC_PROMPT

API_KEY = os.environ.get("GEMINI_API_KEY")
if not API_KEY:
    print("ERROR: GEMINI_API_KEY environment variable not set.")
    exit(1)

client = genai.Client(api_key=API_KEY)
DB_PATH = "database/reconciliation.db"

def process_exceptions():
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    cursor = conn.cursor()

    cursor.execute("SELECT * FROM exceptions WHERE ai_status = 'PENDING'")
    exceptions = cursor.fetchall()

    if not exceptions:
        print("No pending exceptions to process.")
        return

    print(f"Processing {len(exceptions)} exceptions via AI Diagnostic Layer...")

    for exc in exceptions:
        payload = {
            "transaction_id": exc["transaction_id"],
            "system_status": exc["status"],
            "internal_amount": exc["internal_amount"],
            "external_amount": exc["external_amount"],
            "internal_currency": exc["internal_currency"],
            "external_currency": exc["external_currency"],
            "timestamp_difference_seconds": exc["timestamp_diff_seconds"]
        }

        prompt = f"{DIAGNOSTIC_PROMPT}\n\nPayload:\n{json.dumps(payload, indent=2)}"
        
        try:
            response = client.models.generate_content(
                model='gemini-3.6-flash',
                contents=prompt,
            )
            raw_text = response.text.strip().replace("```json", "").replace("```", "")
            ai_data = json.loads(raw_text)
            
            cursor.execute("""
                UPDATE exceptions 
                SET ai_classification = ?, ai_confidence = ?, ai_reason = ?, ai_recommended_action = ?, ai_status = 'COMPLETED'
                WHERE exception_id = ?
            """, (
                ai_data.get("classification", "UNKNOWN"),
                float(ai_data.get("confidence", 0.0)),
                ai_data.get("reason", "No reason provided"),
                ai_data.get("recommended_action", "Manual review required"),
                exc["exception_id"]
            ))
            print(f"✅ Diagnosed {exc['transaction_id']}: {ai_data.get('classification')}")
            
        except Exception as e:
            print(f"❌ AI Validation Failed for {exc['transaction_id']}: {e}")
            cursor.execute("""
                UPDATE exceptions 
                SET ai_status = 'FAILED', ai_reason = ? 
                WHERE exception_id = ?
            """, (str(e), exc["exception_id"]))
            
        cursor.execute("""
            INSERT INTO audit_logs (timestamp, event_type, transaction_id, component, message)
            VALUES (?, ?, ?, ?, ?)
        """, (int(time.time()), "AI_DIAGNOSTIC_COMPLETED", exc["transaction_id"], "AI_AGENT", "Processed exception payload"))
        
        time.sleep(1)

    conn.commit()
    conn.close()
    print("AI Diagnostic processing complete.")

if __name__ == "__main__":
    process_exceptions()