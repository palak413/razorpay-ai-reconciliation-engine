import unittest
import sqlite3
import os
import tempfile
from unittest.mock import MagicMock, patch
from ai.diagnostic_agent import DiagnosticAgent

class TestDiagnosticAgent(unittest.TestCase):
    def setUp(self):
        self.db_fd, self.db_path = tempfile.mkstemp(suffix=".db")
        os.close(self.db_fd)
        
        conn = sqlite3.connect(self.db_path)
        conn.execute("""
            CREATE TABLE batches (
                batch_id TEXT PRIMARY KEY,
                status TEXT NOT NULL,
                started_at INTEGER NOT NULL,
                completed_at INTEGER,
                engine_version TEXT NOT NULL,
                total_records INTEGER DEFAULT 0,
                matched_records INTEGER DEFAULT 0,
                exception_records INTEGER DEFAULT 0
            );
        """)
        conn.execute("""
            CREATE TABLE exceptions (
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
                created_at INTEGER NOT NULL,
                updated_at INTEGER,
                PRIMARY KEY (batch_id, transaction_id)
            );
        """)
        conn.execute("""
            CREATE TABLE reconciliation_results (
                batch_id TEXT NOT NULL,
                transaction_id TEXT NOT NULL,
                result TEXT NOT NULL,
                PRIMARY KEY (batch_id, transaction_id)
            );
        """)
        conn.commit()
        conn.close()

        self.agent = DiagnosticAgent(db_path=self.db_path)

    def tearDown(self):
        if os.path.exists(self.db_path):
            os.remove(self.db_path)

    def test_valid_response(self):
        valid_data = {
            "classification": "AMOUNT_DISCREPANCY",
            "reason": "Amount differs by 100 minor units.",
            "recommended_action": "Review invoice.",
            "confidence": 0.95
        }
        res = self.agent.validate_response(valid_data)
        self.assertEqual(res["classification"], "AMOUNT_DISCREPANCY")

    @patch("ai.diagnostic_agent.genai.Client")
    def test_malformed_json_triggers_retry_path(self, mock_client_cls):
        mock_client = MagicMock()
        bad_response = MagicMock()
        bad_response.text = "this is not valid json {"
        mock_client.models.generate_content.return_value = bad_response
        mock_client_cls.return_value = mock_client
        self.agent.client = mock_client

        with self.assertRaises(Exception):
            self.agent.diagnose_exception({"transaction_id": "TX_BAD_JSON"}, max_retries=2)
        
        self.assertEqual(mock_client.models.generate_content.call_count, 2)

    def test_missing_required_field(self):
        invalid_data = {
            "classification": "AMOUNT_DISCREPANCY",
            "reason": "Reason string.",
            "confidence": 0.8
        }
        with self.assertRaises(ValueError):
            self.agent.validate_response(invalid_data)

    def test_invalid_classification(self):
        invalid_data = {
            "classification": "INVALID_CLASS",
            "reason": "Reason string.",
            "recommended_action": "Action string.",
            "confidence": 0.5
        }
        with self.assertRaises(ValueError):
            self.agent.validate_response(invalid_data)

    def test_confidence_zero_and_one(self):
        res0 = self.agent.validate_response({
            "classification": "UNKNOWN", "reason": "r", "recommended_action": "a", "confidence": 0.0
        })
        self.assertEqual(res0["confidence"], 0.0)

        res1 = self.agent.validate_response({
            "classification": "UNKNOWN", "reason": "r", "recommended_action": "a", "confidence": 1.0
        })
        self.assertEqual(res1["confidence"], 1.0)

    def test_confidence_boolean_rejection(self):
        with self.assertRaises(ValueError):
            self.agent.validate_response({
                "classification": "UNKNOWN", "reason": "r", "recommended_action": "a", "confidence": True
            })
        with self.assertRaises(ValueError):
            self.agent.validate_response({
                "classification": "UNKNOWN", "reason": "r", "recommended_action": "a", "confidence": False
            })

    def test_confidence_out_of_bounds(self):
        with self.assertRaises(ValueError):
            self.agent.validate_response({
                "classification": "UNKNOWN", "reason": "r", "recommended_action": "a", "confidence": -0.1
            })
        with self.assertRaises(ValueError):
            self.agent.validate_response({
                "classification": "UNKNOWN", "reason": "r", "recommended_action": "a", "confidence": 1.1
            })

    @patch("ai.diagnostic_agent.genai.Client")
    def test_retry_after_transient_failure_and_call_count(self, mock_client_cls):
        mock_client = MagicMock()
        success_response = MagicMock()
        success_response.text = '{"classification": "BANK_DELAY", "reason": "Delayed settlement", "recommended_action": "Wait", "confidence": 0.9}'
        
        mock_client.models.generate_content.side_effect = [Exception("Timeout"), Exception("Connection Error"), success_response]
        mock_client_cls.return_value = mock_client
        self.agent.client = mock_client

        res = self.agent.diagnose_exception({"transaction_id": "TX_RETRY"}, max_retries=3)
        self.assertEqual(res["classification"], "BANK_DELAY")
        self.assertEqual(mock_client.models.generate_content.call_count, 3)

    @patch("ai.diagnostic_agent.genai.Client")
    def test_retry_exhaustion_exact_count(self, mock_client_cls):
        mock_client = MagicMock()
        mock_client.models.generate_content.side_effect = Exception("Persistent API Failure")
        mock_client_cls.return_value = mock_client
        self.agent.client = mock_client

        with self.assertRaises(Exception):
            self.agent.diagnose_exception({"transaction_id": "TX_FAIL"}, max_retries=3)
        
        self.assertEqual(mock_client.models.generate_content.call_count, 3)

    @patch("ai.diagnostic_agent.genai.Client")
    def test_successful_ai_database_update_and_preservation(self, mock_client_cls):
        conn = sqlite3.connect(self.db_path)
        conn.execute("INSERT INTO batches (batch_id, status, started_at, engine_version) VALUES ('B1', 'COMPLETED', 1000, 'v1')")
        conn.execute("""
            INSERT INTO exceptions (batch_id, transaction_id, detected_status, internal_amount, external_amount, internal_tax, external_tax, currency, timestamp_diff_seconds, reconciliation_reason, ai_status, created_at)
            VALUES ('B1', 'TX_100', 'AMOUNT_MISMATCH', 10000, 9500, 1800, 1800, 'INR', 10, 'Amount mismatch reason', 'PENDING', 1000)
        """)
        conn.execute("INSERT INTO reconciliation_results (batch_id, transaction_id, result) VALUES ('B1', 'TX_100', 'AMOUNT_MISMATCH')")
        conn.commit()
        conn.close()

        mock_client = MagicMock()
        success_response = MagicMock()
        success_response.text = '{"classification": "AMOUNT_DISCREPANCY", "reason": "Amount diff", "recommended_action": "Check ledger", "confidence": 0.95}'
        mock_client.models.generate_content.return_value = success_response
        self.agent.client = mock_client

        self.agent.process_pending_exceptions(batch_id='B1')

        conn = sqlite3.connect(self.db_path)
        row = conn.execute("SELECT ai_status, ai_classification, detected_status, internal_amount FROM exceptions WHERE batch_id='B1' AND transaction_id='TX_100'").fetchone()
        res_row = conn.execute("SELECT result FROM reconciliation_results WHERE batch_id='B1' AND transaction_id='TX_100'").fetchone()
        conn.close()

        self.assertEqual(row[0], 'COMPLETED')
        self.assertEqual(row[1], 'AMOUNT_DISCREPANCY')
        self.assertEqual(row[2], 'AMOUNT_MISMATCH')
        self.assertEqual(row[3], 10000)
        self.assertEqual(res_row[0], 'AMOUNT_MISMATCH')

    @patch("ai.diagnostic_agent.genai.Client")
    def test_ai_failure_sets_failed_and_preserves_deterministic_fields(self, mock_client_cls):
        conn = sqlite3.connect(self.db_path)
        conn.execute("INSERT INTO batches (batch_id, status, started_at, engine_version) VALUES ('B2', 'COMPLETED', 1000, 'v1')")
        conn.execute("""
            INSERT INTO exceptions (batch_id, transaction_id, detected_status, internal_amount, external_amount, internal_tax, external_tax, currency, timestamp_diff_seconds, reconciliation_reason, ai_status, created_at)
            VALUES ('B2', 'TX_200', 'TAX_MISMATCH', 5000, 5000, 900, 800, 'INR', 5, 'Tax mismatch reason', 'PENDING', 1000)
        """)
        conn.commit()
        conn.close()

        mock_client = MagicMock()
        mock_client.models.generate_content.side_effect = Exception("API Error")
        self.agent.client = mock_client

        self.agent.process_pending_exceptions(batch_id='B2')

        conn = sqlite3.connect(self.db_path)
        row = conn.execute("SELECT ai_status, detected_status, internal_amount, internal_tax, external_tax FROM exceptions WHERE batch_id='B2' AND transaction_id='TX_200'").fetchone()
        conn.close()

        self.assertEqual(row[0], 'FAILED')
        self.assertEqual(row[1], 'TAX_MISMATCH')
        self.assertEqual(row[2], 5000)
        self.assertEqual(row[3], 900)
        self.assertEqual(row[4], 800)

    @patch("ai.diagnostic_agent.genai.Client")
    def test_batch_scoped_and_transaction_isolation(self, mock_client_cls):
        conn = sqlite3.connect(self.db_path)
        conn.execute("INSERT INTO batches (batch_id, status, started_at, engine_version) VALUES ('B3', 'COMPLETED', 1000, 'v1')")
        conn.execute("INSERT INTO batches (batch_id, status, started_at, engine_version) VALUES ('B4', 'COMPLETED', 1000, 'v1')")
        
        conn.execute("""
            INSERT INTO exceptions (batch_id, transaction_id, detected_status, ai_status, created_at)
            VALUES ('B3', 'TX_B3', 'MISSING_EXTERNAL', 'PENDING', 1000)
        """)
        conn.execute("""
            INSERT INTO exceptions (batch_id, transaction_id, detected_status, ai_status, created_at)
            VALUES ('B4', 'TX_B4', 'MISSING_EXTERNAL', 'PENDING', 1000)
        """)
        conn.commit()
        conn.close()

        mock_client = MagicMock()
        success_response = MagicMock()
        success_response.text = '{"classification": "MISSING_SETTLEMENT", "reason": "Missing", "recommended_action": "Follow up", "confidence": 0.85}'
        mock_client.models.generate_content.return_value = success_response
        self.agent.client = mock_client

        self.agent.process_pending_exceptions(batch_id='B3')

        conn = sqlite3.connect(self.db_path)
        b3_status = conn.execute("SELECT ai_status FROM exceptions WHERE batch_id='B3' AND transaction_id='TX_B3'").fetchone()[0]
        b4_status = conn.execute("SELECT ai_status FROM exceptions WHERE batch_id='B4' AND transaction_id='TX_B4'").fetchone()[0]
        conn.close()

        self.assertEqual(b3_status, 'COMPLETED')
        self.assertEqual(b4_status, 'PENDING')

if __name__ == "__main__":
    unittest.main()