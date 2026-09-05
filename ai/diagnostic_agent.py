import os
import json
import sqlite3
import time
import logging
from typing import Dict, Any, Optional
from google import genai
from ai.prompts import SYSTEM_PROMPT, build_diagnostic_prompt

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("DiagnosticAgent")

ALLOWED_CLASSIFICATIONS = {
    "DATA_ERROR",
    "BANK_DELAY",
    "DUPLICATE_SETTLEMENT",
    "MISSING_SETTLEMENT",
    "AMOUNT_DISCREPANCY",
    "TAX_DISCREPANCY",
    "CURRENCY_DISCREPANCY",
    "TIMESTAMP_DISCREPANCY",
    "UNKNOWN"
}

class DiagnosticAgent:
    def __init__(self, db_path: str = "database/reconciliation.db"):
        self.db_path = db_path
        api_key = os.getenv("GEMINI_API_KEY") or os.getenv("GOOGLE_API_KEY")
        self.client = genai.Client(api_key=api_key) if api_key else None

    def validate_response(self, data: Any) -> Dict[str, Any]:
        if not isinstance(data, dict):
            raise ValueError("Response is not a JSON object.")
        
        required_fields = ["classification", "reason", "recommended_action", "confidence"]
        for field in required_fields:
            if field not in data:
                raise ValueError(f"Missing required field: {field}")

        classification = data["classification"]
        if not isinstance(classification, str) or classification not in ALLOWED_CLASSIFICATIONS:
            raise ValueError(f"Invalid classification: {classification}")

        reason = data["reason"]
        if not isinstance(reason, str) or not reason.strip():
            raise ValueError("Reason must be a non-empty string.")

        action = data["recommended_action"]
        if not isinstance(action, str) or not action.strip():
            raise ValueError("Recommended action must be a non-empty string.")

        confidence = data["confidence"]
        if isinstance(confidence, bool) or not isinstance(confidence, (int, float)) or not (0.0 <= confidence <= 1.0):
            raise ValueError(f"Confidence out of bounds [0, 1] or invalid type: {confidence}")

        return data

    def diagnose_exception(self, exception_data: Dict[str, Any], max_retries: int = 3) -> Dict[str, Any]:
        if not self.client:
            raise RuntimeError("Gemini API client not initialized.")

        prompt = build_diagnostic_prompt(exception_data)
        last_exception = None

        for attempt in range(1, max_retries + 1):
            try:
                response = self.client.models.generate_content(
                    model="gemini-2.5-flash",
                    contents=prompt,
                    config={
                        "system_instruction": SYSTEM_PROMPT,
                        "response_mime_type": "application/json",
                        "temperature": 0.1
                    }
                )
                
                raw_text = response.text.strip()
                if raw_text.startswith("```json"):
                    raw_text = raw_text[7:]
                if raw_text.endswith("```"):
                    raw_text = raw_text[:-3]
                raw_text = raw_text.strip()

                parsed = json.loads(raw_text)
                validated = self.validate_response(parsed)
                return validated

            except (json.JSONDecodeError, ValueError, Exception) as e:
                last_exception = e
                logger.warning(f"Attempt {attempt}/{max_retries} failed for tx {exception_data.get('transaction_id')}: {e}")
                if attempt == max_retries:
                    raise last_exception
                time.sleep(0.01 * (2 ** (attempt - 1)))

        raise last_exception or RuntimeError("Retry exhaustion without valid response.")

    def process_pending_exceptions(self, batch_id: Optional[str] = None):
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        cursor = conn.cursor()

        query = "SELECT * FROM exceptions WHERE ai_status = 'PENDING'"
        params = []
        if batch_id:
            query += " AND batch_id = ?"
            params.append(batch_id)

        rows = cursor.execute(query, params).fetchall()

        for row in rows:
            exc = dict(row)
            b_id = exc["batch_id"]
            t_id = exc["transaction_id"]
            
            logger.info(f"Processing AI diagnosis for batch {b_id}, transaction {t_id}")
            
            now = int(time.time())
            cursor.execute(
                "UPDATE exceptions SET ai_status = 'PROCESSING', updated_at = ? WHERE batch_id = ? AND transaction_id = ?",
                (now, b_id, t_id)
            )
            conn.commit()

            try:
                result = self.diagnose_exception(exc)
                cursor.execute(
                    """UPDATE exceptions SET 
                       ai_status = 'COMPLETED', 
                       ai_classification = ?, 
                       ai_confidence = ?, 
                       ai_reason = ?, 
                       ai_recommended_action = ?, 
                       updated_at = ? 
                       WHERE batch_id = ? AND transaction_id = ?""",
                    (result["classification"], result["confidence"], result["reason"], result["recommended_action"], int(time.time()), b_id, t_id)
                )
                conn.commit()
            except Exception as e:
                logger.error(f"AI diagnostic failed permanently for {t_id}: {e}")
                cursor.execute(
                    "UPDATE exceptions SET ai_status = 'FAILED', updated_at = ? WHERE batch_id = ? AND transaction_id = ?",
                    (int(time.time()), b_id, t_id)
                )
                conn.commit()

        conn.close()