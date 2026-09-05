import sqlite3
import pandas as pd
import streamlit as st

DB_PATH = "database/reconciliation.db"

st.set_page_config(
    page_title="Razorpay AI Finance Controller",
    page_icon="⚡",
    layout="wide"
)


def query(sql, params=()):
    conn = sqlite3.connect(DB_PATH)

    try:
        return pd.read_sql_query(
            sql,
            conn,
            params=params
        )
    finally:
        conn.close()


st.title("Razorpay AI Finance Controller")
st.caption(
    "Deterministic C++ reconciliation + Gemini exception diagnostics"
)

st.markdown("---")

batches = query(
    """
    SELECT *
    FROM batches
    ORDER BY started_at DESC
    """
)

if batches.empty:
    st.warning(
        "No batches found. Run:\n\n"
        "`python3 scripts/run_demo.py`"
    )
    st.stop()

batch_ids = batches["batch_id"].tolist()

selected_batch = st.selectbox(
    "Select Batch",
    batch_ids
)

batch = batches[
    batches["batch_id"] == selected_batch
].iloc[0]

results_df = query(
    """
    SELECT *
    FROM reconciliation_results
    WHERE batch_id = ?
    ORDER BY transaction_id
    """,
    (selected_batch,)
)

exceptions_df = query(
    """
    SELECT *
    FROM exceptions
    WHERE batch_id = ?
    ORDER BY transaction_id
    """,
    (selected_batch,)
)

audit_df = query(
    """
    SELECT *
    FROM audit_logs
    WHERE batch_id = ?
    ORDER BY timestamp DESC, id DESC
    """,
    (selected_batch,)
)

total = int(batch["total_records"])
matched = int(batch["matched_records"])
exceptions = int(batch["exception_records"])

match_rate = (
    matched / total * 100
    if total
    else 0
)

st.subheader(f"Batch: `{selected_batch}`")

if batch["status"] == "COMPLETED":
    st.success(
        f"Batch Status: {batch['status']}"
    )
else:
    st.error(
        f"Batch Status: {batch['status']}"
    )

c1, c2, c3, c4 = st.columns(4)

c1.metric(
    "Internal Records",
    total
)

c2.metric(
    "Matched",
    matched
)

c3.metric(
    "Match Rate",
    f"{match_rate:.2f}%"
)

c4.metric(
    "Exceptions",
    exceptions
)

st.markdown("---")

tab1, tab2, tab3 = st.tabs(
    [
        "📊 Reconciliation",
        "🤖 AI Diagnostics",
        "📜 Audit Trail"
    ]
)

with tab1:

    st.subheader("Exception Breakdown")

    if not results_df.empty:

        breakdown = (
            results_df["result"]
            .value_counts()
            .rename_axis("Result")
            .reset_index(name="Count")
        )

        st.dataframe(
            breakdown,
            use_container_width=True,
            hide_index=True
        )

        st.subheader("Reconciliation Results")

        st.dataframe(
            results_df,
            use_container_width=True,
            hide_index=True
        )

with tab2:

    st.subheader("AI Diagnostic Status")

    if not exceptions_df.empty:

        ai_counts = (
            exceptions_df["ai_status"]
            .value_counts()
            .rename_axis("Status")
            .reset_index(name="Count")
        )

        st.dataframe(
            ai_counts,
            use_container_width=True,
            hide_index=True
        )

        st.subheader("AI Exception Diagnostics")

        columns = [
            "transaction_id",
            "detected_status",
            "reconciliation_reason",
            "ai_status",
            "ai_classification",
            "ai_confidence",
            "ai_reason",
            "ai_recommended_action"
        ]

        columns = [
            c for c in columns
            if c in exceptions_df.columns
        ]

        st.dataframe(
            exceptions_df[columns],
            use_container_width=True,
            hide_index=True
        )

    else:
        st.success("No exceptions.")

with tab3:

    st.subheader("Audit Trail")

    if not audit_df.empty:

        st.dataframe(
            audit_df,
            use_container_width=True,
            hide_index=True
        )

    else:
        st.info("No audit events found.")

st.markdown("---")

st.caption(
    "Financial truth is established by the deterministic C++ "
    "reconciliation engine. Gemini is advisory only."
)