# Multi-Source Reconciliation Engine Architecture

## Overview
This engine reconciles internal merchant payment ledgers against external bank settlement streams with zero tolerance for floating-point drift or false-positive fuzziness.

```mermaid
graph TD
    A[Internal Ledger CSV] --> B(SQLite Database WAL Mode)
    C[Bank Settlement CSV] --> B
    B --> D[C++ Deterministic Engine O(n)]
    D -- Exact Match --> E[Reconciliation Results]
    D -- Discrepancy --> F[Exception Queue]
    F --> G[Gemini 2.5 Flash Diagnostic Agent]
    G --> H[Structured JSON Audit & Report]