# Razorpay AI Reconciliation Engine

## 1. Problem
Finance operations at scale involve reconciling internal ledger transactions against bank settlement files. Discrepancies routinely arise due to amount mismatches, tax rounding differences, currency variations, timestamp drift, missing records on either side, and duplicate bank settlements. Achieving zero financial discrepancy while maintaining strict auditability and repeatable processing is critical for financial health and compliance.

## 2. Solution
This project implements an enterprise-grade, deterministic financial reconciliation and diagnostic pipeline:
- **Deterministic C++ Engine**: Performs strict, precise financial matching using integer minor units (`int64_t`).
- **Atomic SQLite Storage**: WAL-mode database preserving batches, input records, reconciliation results, exceptions, and audit logs.
- **Gemini AI Diagnostic Agent**: Advisory-only LLM layer that diagnoses unresolved exceptions, explains causes, and recommends operational next steps.

> **Core Principle:** *"AI does not decide financial truth. The deterministic reconciliation engine is the source of truth; AI only diagnoses unresolved exceptions."*

---

## 3. Architecture

```text
CSV Inputs (Internal Ledger & Bank Settlements)
   │
   ▼
C++ Deterministic Reconciliation Engine
   │
   ▼
Atomic SQLite Batch Storage
   ├── batches
   ├── internal_transactions
   ├── bank_settlements
   ├── reconciliation_results
   ├── exceptions
   └── audit_logs
   │
   ▼
Python Gemini AI Diagnostic Agent (Advisory Only)
   │
   ▼
Persisted AI Metadata (exceptions.ai_*)
   │
   ▼
Final Operational Summary