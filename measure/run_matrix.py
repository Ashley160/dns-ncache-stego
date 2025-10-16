#!/usr/bini/env python3
import argparse, subprocess, sys, time
from datetime import datetime
from pathlib import Path

def main():
    parser = argparse.ArgumentParser(description="Run run_once.py multiple rounds.")
    parser.add_argument("-r", "--rounds", type=int, default=3, dest="rounds", help="number of rounds to run (default: 3)")
    args = parser.parse_args()

    measure_dir = Path(__file__).parent.resolve()
    run_once = measure_dir / "run_once.py"
    if not run_once.exists():
        print(f"run_once.py not found: {run_once}", file=sys.stderr)
        sys.exit(1)


    py = sys.executable

    for i in range(1, args.rounds + 1):
        ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        print(f"\n=== Round {i}/{args.rounds} @ {ts} ===")

        proc = subprocess.run([py, str(run_once)])
        if proc.returncode != 0:
            print(f"something going wrong: {proc.stderr} (round {i})")
            sys.exit(1)

    
    print("\nAll rounds completed.")


if __name__ == "__main__":
    main()
