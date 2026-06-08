#!/usr/bin/env python3
"""
run_matrix.py — 對單一檔案、多個 thread 數各跑 N 輪，結果全部附加到同一個 CSV。

用法：
    python3 run_matrix.py -f hello.txt              # 用 config.json 的 num_threads list，跑 3 輪
    python3 run_matrix.py -f hello.txt -r 5         # 跑 5 輪
    python3 run_matrix.py -f hello.txt -r 5 -t 1 4 8  # 覆蓋 thread 數清單

執行順序：
    for round in 1..R:
        for t in thread_list:
            run_once.py -f FILE -t t -r round
"""
import argparse, json, subprocess, sys
from datetime import datetime
from pathlib import Path

CONFIG_PATH = Path(__file__).with_name("config.json")


def load_thread_list() -> list[int]:
    with open(CONFIG_PATH, encoding="utf-8") as f:
        cfg = json.load(f)
    t = cfg.get("num_threads", [4])
    return t if isinstance(t, list) else [t]


def main():
    default_threads = load_thread_list()

    parser = argparse.ArgumentParser(
        description="Matrix benchmark: one file × multiple thread counts × N rounds."
    )
    parser.add_argument("-f", "--file",    required=True,
                        help="filename to test (under sample_dir in config.json)")
    parser.add_argument("-r", "--rounds",  type=int, default=3,
                        help="number of rounds per thread count (default: 3)")
    parser.add_argument("-t", "--threads", type=int, nargs="+", default=default_threads,
                        metavar="T",
                        help=f"thread counts to test (default from config: {default_threads})")
    args = parser.parse_args()

    measure_dir = Path(__file__).parent.resolve()
    run_once    = measure_dir / "run_once.py"
    if not run_once.exists():
        print(f"run_once.py not found: {run_once}", file=sys.stderr)
        sys.exit(1)

    py = sys.executable
    total = args.rounds * len(args.threads)
    done  = 0

    print(f"\nFile     : {args.file}")
    print(f"Threads  : {args.threads}")
    print(f"Rounds   : {args.rounds}")
    print(f"Total runs: {total}")

    for rnd in range(1, args.rounds + 1):
        for t in args.threads:
            done += 1
            ts = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            print(f"\n{'='*60}")
            print(f"  [{done}/{total}]  Round {rnd}  |  threads={t}  @  {ts}")
            print(f"{'='*60}")

            proc = subprocess.run(
                [py, str(run_once), "-f", args.file, "-t", str(t), "-r", str(rnd)]
            )
            if proc.returncode != 0:
                print(f"run_once.py failed (round={rnd}, threads={t})", file=sys.stderr)
                sys.exit(1)

    print(f"\n{'='*60}")
    print(f"  All {total} runs completed ({args.rounds} rounds × {len(args.threads)} thread counts).")
    print(f"{'='*60}")


if __name__ == "__main__":
    main()
