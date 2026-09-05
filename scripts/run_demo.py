import subprocess
import sys

def run_command(cmd):
    print(f"Running: {cmd}")
    res = subprocess.run(cmd, shell=True)
    if res.returncode != 0:
        print(f"Error executing {cmd}")
        sys.exit(1)

def main():
    print("========================================")
    print("AI FINANCE CONTROLLER — DEMO RUN")
    print("========================================")
    
    run_command("python3 scripts/setup_db.py")
    run_command("python3 scripts/generate_data.py")
    run_command("./build/reconciliation_engine database/reconciliation.db")
    run_command("python3 ai/diagnostic_agent.py")
    
    print("\n✅ DEMO COMPLETED SUCCESSFULLY WITH ACTUAL DB EXECUTION.")

if __name__ == "__main__":
    main()