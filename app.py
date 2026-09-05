import streamlit as st
import sqlite3
import pandas as pd

st.set_page_config(page_title="Razorpay AI Finance Controller", page_icon="⚡", layout="wide")

st.title("⚡ Razorpay AI Finance Controller & Reconciliation Engine")
st.markdown("Deterministic C++17 reconciliation core paired with a bounded Gemini AI diagnostic agent.")
st.markdown("---")

DB_PATH = "database/reconciliation.db"

def run_query(query, params=()):
    try:
        conn = sqlite3.connect(DB_PATH)
        df = pd.read_sql(query, conn, params=params)
        conn.close()
        return df
    except Exception:
        return pd.DataFrame()

# System status checks
wal_df = run_query("PRAGMA journal_mode;")
wal_mode = wal_df.iloc[0, 0].upper() if not wal_df.empty else "UNKNOWN"

batches_df = run_query("SELECT * FROM batches ORDER BY started_at DESC LIMIT 1;")
last_batch = batches_df.iloc[0].to_dict() if not batches_df.empty else {}

results_df = run_query("SELECT * FROM reconciliation_results;")
exceptions_df = run_query("SELECT * FROM exceptions;")

total_proc = len(results_df) + len(exceptions_df)
matched_count = len(results_df[results_df['result'] == 'MATCHED']) if not results_df.empty else 0
match_rate = (matched_count / total_proc * 100) if total_proc > 0 else 0.0

col1, col2, col3, col4, col5 = st.columns(5)
col1.metric("Engine Status", "ONLINE")
col2.metric("WAL Mode", wal_mode)
col3.metric("Total Processed", total_proc)
col4.metric("Match Rate", f"{match_rate:.1f}%")
col5.metric("Exceptions", len(exceptions_df))

st.markdown("---")
tab1, tab2, tab3 = st.tabs(["📊 Reconciliation Results", "🤖 Exception & AI Diagnostics", "📜 Audit Trail"])

with tab1:
    st.subheader("Deterministic Match Ledger")
    if not results_df.empty:
        st.dataframe(results_df, use_container_width=True)
    else:
        st.info("No reconciliation results found.")

with tab2:
    st.subheader("Exception Queue & AI Diagnostics")
    if not exceptions_df.empty:
        st.dataframe(exceptions_df, use_container_width=True)
    else:
        st.info("No exceptions recorded.")

with tab3:
    st.subheader("Append-only Audit Trail")
    audit_df = run_query("SELECT * FROM audit_logs ORDER BY timestamp DESC LIMIT 50;")
    if not audit_df.empty:
        st.dataframe(audit_df, use_container_width=True)
    else:
        st.info("No audit events found.")