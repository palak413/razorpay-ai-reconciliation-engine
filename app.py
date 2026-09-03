import streamlit as st
import sqlite3
import pandas as pd

st.set_page_config(
    page_title="Razorpay AI Finance Controller",
    page_icon="⚡",
    layout="wide"
)

st.title("Razorpay AI Finance Controller & Reconciliation Engine")
st.markdown("High-throughput deterministic C++ reconciliation core paired with a Gemini AI diagnostic layer.")
st.markdown("---")

DB_PATH = "database/reconciliation.db"

def load_data(query):
    conn = sqlite3.connect(DB_PATH)
    df = pd.read_sql(query, conn)
    conn.close()
    return df

# Top metrics cards
try:
    df_res = load_data("SELECT * FROM reconciliation_results")
    df_exc = load_data("SELECT * FROM exceptions")
    
    total_matched = len(df_res[df_res['result'] == 'MATCHED']) if not df_res.empty else 0
    total_exceptions = len(df_exc) if not df_exc.empty else 0
    ai_completed = len(df_exc[df_exc['ai_status'] == 'COMPLETED']) if not df_exc.empty else 0

    col1, col2, col3, col4 = st.columns(4)
    col1.metric("Engine Status", "ONLINE (WAL Mode)")
    col2.metric("Total Processed", len(df_res) if not df_res.empty else 0)
    col3.metric("Successful Matches", total_matched)
    col4.metric("AI Diagnosed Exceptions", f"{ai_completed} / {total_exceptions}")
except Exception:
    st.warning("Database tables are empty. Run your demo script first!")

st.markdown("---")

tab1, tab2, tab3 = st.tabs(["📊 Reconciliation Results", "🤖 AI Exceptions & Diagnostics", "📜 Immutable Audit Logs"])

with tab1:
    st.subheader("Deterministic Match Ledger (C++ Engine Output)")
    if 'df_res' in locals() and not df_res.empty:
        st.dataframe(df_res, use_container_width=True)
    else:
        st.info("No data available.")

with tab2:
    st.subheader("Exception Queue & Gemini Diagnostic Classifications")
    if 'df_exc' in locals() and not df_exc.empty:
        st.dataframe(df_exc, use_container_width=True)
    else:
        st.info("No exceptions found.")

with tab3:
    st.subheader("System Audit Trail")
    try:
        df_audit = load_data("SELECT * FROM audit_logs")
        st.dataframe(df_audit, use_container_width=True)
    except Exception:
        st.info("No audit logs found.")