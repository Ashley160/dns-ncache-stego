#!/usr/bin/env python3
"""
run_once.py — 對單一檔案、單一 thread 數跑一輪 sender + receiver，結果寫入 CSV。

直接執行：
    python3 run_once.py -f hello.txt -t 8
    python3 run_once.py -f hello.txt -t 8 -r 2   # round label（由 run_matrix 傳入）

"""
import argparse, json, subprocess, re, sys, csv
from pathlib import Path
from datetime import datetime, timezone, timedelta

CONFIG_PATH = Path(__file__).with_name("config.json")
TZ_TPE = timezone(timedelta(hours=8))

# ── Sender patterns ──────────────────────────────────────────────────────────
RE_SND_ENCAP = re.compile(r'Encapsulation time\s*:\s*([\d.]+)\s*ms')
RE_SND_TRANS = re.compile(r'Transformation time\s*:\s*([\d.]+)\s*ms')
RE_SND_DNS   = re.compile(r'DNS query time\s*:\s*([\d.]+)\s*ms')
RE_SND_TOTAL = re.compile(r'Sender total time\s*:\s*([\d.]+)\s*ms')

# ── Receiver patterns ────────────────────────────────────────────────────────
RE_RCV_NEG   = re.compile(r'Negativity Detection time\s*:\s*([\d.]+)\s*ms')
RE_RCV_DNS   = re.compile(r'DNS query time\s*:\s*([\d.]+)\s*ms')
RE_RCV_DECAP = re.compile(r'Decapsulation time\s*:\s*([\d.]+)\s*ms')
RE_RCV_TOTAL = re.compile(r'Receiver total time\s*:\s*([\d.]+)\s*ms')

CSV_HEADER = [
    "round", "timestamp", "dns_server", "filename", "file_size_bytes",
    "num_threads",
    "snd_encap_ms", "snd_trans_ms", "snd_dns_ms", "snd_total_ms",
    "rcv_neg_ms",   "rcv_dns_ms",   "rcv_decap_ms", "rcv_total_ms",
]


def gen_nxdomain(suffix: str = ".guanling") -> str:
    now = datetime.now(TZ_TPE)
    return now.strftime(f"test%m%d%H%M%S{suffix}")


def run(cmd: list[str]) -> tuple[int, str, str]:
    r = subprocess.run(cmd, text=True, capture_output=True)
    return r.returncode, r.stdout, r.stderr


def parse_float(pattern, text: str) -> str:
    m = pattern.search(text)
    return m.group(1) if m else ""


def main():
    parser = argparse.ArgumentParser(description="Run one sender+receiver round.")
    parser.add_argument("-f", "--file",    required=True, help="filename (under sample_dir)")
    parser.add_argument("-t", "--threads", required=True, type=int, help="number of threads")
    parser.add_argument("-r", "--round",   type=int, default=1, help="round label (for CSV)")
    args = parser.parse_args()

    with open(CONFIG_PATH, encoding="utf-8") as f:
        cfg = json.load(f)

    sample_dir = Path(cfg["paths"]["sample_dir"]).resolve()
    result_dir = Path(cfg["paths"]["result_dir"]).resolve()
    sender     = Path(cfg["paths"]["sender_cpp"]).resolve()
    receiver   = Path(cfg["paths"]["receiver_cpp"]).resolve()
    dns        = cfg["dns_server"]

    fpath = sample_dir / args.file
    if not fpath.is_file():
        print(f"File not found: {fpath}", file=sys.stderr)
        sys.exit(1)

    result_dir.mkdir(parents=True, exist_ok=True)
    csv_path = result_dir / "measure_result.csv"
    write_header = not csv_path.exists()

    nxdomain  = gen_nxdomain()
    timestamp = datetime.now(TZ_TPE).strftime("%Y-%m-%d %H:%M:%S")
    threads_s = str(args.threads)

    print(f"\n--- Round {args.round} | threads={args.threads} | {args.file} | {nxdomain} ---")

    # 1) Sender
    rc, so, se = run([str(sender), str(fpath), nxdomain, dns, threads_s])
    if rc != 0:
        print(f"sender failed rc={rc}\n{so}\n{se}", file=sys.stderr)
        sys.exit(rc)
    print(so, end="")

    # 2) Receiver
    rc, so2, se2 = run([str(receiver), nxdomain, dns, threads_s])
    if rc != 0:
        print(f"receiver failed rc={rc}\n{so2}\n{se2}", file=sys.stderr)
        sys.exit(rc)
    print(so2, end="")

    row = {
        "round":           args.round,
        "timestamp":       timestamp,
        "dns_server":      dns,
        "filename":        args.file,
        "file_size_bytes": fpath.stat().st_size,
        "num_threads":     args.threads,
        "snd_encap_ms":    parse_float(RE_SND_ENCAP, so),
        "snd_trans_ms":    parse_float(RE_SND_TRANS, so),
        "snd_dns_ms":      parse_float(RE_SND_DNS,   so),
        "snd_total_ms":    parse_float(RE_SND_TOTAL, so),
        "rcv_neg_ms":      parse_float(RE_RCV_NEG,   so2),
        "rcv_dns_ms":      parse_float(RE_RCV_DNS,   so2),
        "rcv_decap_ms":    parse_float(RE_RCV_DECAP, so2),
        "rcv_total_ms":    parse_float(RE_RCV_TOTAL, so2),
    }

    with open(csv_path, "a", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_HEADER)
        if write_header:
            writer.writeheader()
        writer.writerow(row)

    print(
        f"  → CSV row written | threads={args.threads} | "
        f"snd_total={row['snd_total_ms']} ms  rcv_total={row['rcv_total_ms']} ms"
    )
    print(f"  → {csv_path}")


if __name__ == "__main__":
    main()
