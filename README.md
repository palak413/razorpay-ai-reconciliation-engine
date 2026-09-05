# Multi-Source Reconciliation Engine (Razorpay AI Buildathon)

> **Performance Metric:** Processed 100K+ ledger records in <0.04s with zero floating-point drift.

High-throughput, deterministic financial reconciliation engine built with **C++17**, **SQLite WAL**, and a bounded **Gemini AI Diagnostic Layer**.

## Architecture Highlights
- **Deterministic Core:** C++17 hash-mapped engine providing absolute zero math hallucination risk.
- **ACID Safety:** SQLite WAL mode ensures crash recovery and data integrity.
- **AI Firewalled:** Gemini is restricted strictly to diagnostic classification of exceptions.

## Quick Start & Demo
Run the complete unified demo script:
```bash
python3 scripts/run_demo.py
