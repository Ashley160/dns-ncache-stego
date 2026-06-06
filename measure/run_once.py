#!/usr/bin/env python3
import json, subprocess, os, re, sys, csv
from pathlib import Path
from datetime import datetime, timezone, timedelta

CONFIG_PATH = Path(__file__).with_name("config.json")
TZ_TPE = timezone(timedelta(hours=8))

# ── Sender patterns ──────────────────────────────────────────────────────────
RE_SND_ENCAP  = re.compile(r'Encapsulation time\s*:\s*([\d.]+)\s*ms')
RE_SND_TRANS  = re.compile(r'Transformation time\s*:\s*([\d.]+)\s*ms')
RE_SND_DNS    = re.compile(r'DNS query time\s*:\s*([\d.]+)\s*ms')
RE_SND_TOTAL  = re.compile(r'Sender total time\s*:\s*([\d.]+)\s*ms')

# ── Receiver patterns ────────────────────────────────────────────────────────
RE_RCV_NEG    = re.compile(r'Negativity Detection time\s*:\s*([\d.]+)\s*ms')
RE_RCV_DNS    = re.compile(r'DNS query time\s*:\s*([\d.]+)\s*ms')
RE_RCV_DECAP  = re.compile(r'Decapsulation time\s*:\s*([\d.]+)\s*ms')
RE_RCV_TOTAL  = re.compile(r'Receiver total time\s*:\s*([\d.]+)\s*ms')

CSV_HEADER = [
    "round", "timestamp", "dns_server", "filename", "file_size_bytes",
    "snd_encap_ms", "snd_trans_ms", "snd_dns_ms", "snd_total_ms",
    "rcv_neg_ms",   "rcv_dns_ms",   "rcv_decap_ms", "rcv_total_ms",
]
 
def gen_nxdomain(suffix: str = ".guanling") -> str:
    """
    產生 "test月日時分秒.guanling", 例如: "test1015155030.guanling",
    """
    now = datetime.now(TZ_TPE)
    base = now.strftime("test%m%d%H%M%S")
    return f"{base}{suffix}"

def run(cmd: list[str]) -> tuple[int, str, str]:
    r = subprocess.run(cmd, text=True, capture_output=True)
    return r.returncode, r.stdout, r.stderr

#def to_ms(text: str) -> int:
#    m = NUM_RE.search(text)
#    return int(m.group(1))

def parse_float(pattern, text: str) -> str:
    m = pattern.search(text)
    return m.group(1) if m else ""

def main(round_num: int = 1):
    # load config
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        cfg = json.load(f)

    config_files    = cfg.get("files", [])
    sample_dir      = Path(cfg["paths"]["sample_dir"]).resolve()
    result_dir      = Path(cfg["paths"]["result_dir"]).resolve()
    sender          = Path(cfg["paths"]["sender_cpp"]).resolve()
    receiver        = Path(cfg["paths"]["receiver_cpp"]).resolve()
    dns             = cfg["dns_server"]
    threads         = str(cfg.get("num_threads", 4))

    # python3 run_once.py           -> 用 config.json 裡面的 files
    # python3 run_once.py filename  -> 只跑 filename
    if len(sys.argv) == 2:
        files = [sys.argv[1]]
    else:
        files = config_files
    if not files:
        print("No file to run (config.json: files is empty, and no filename argument).")
        sys.exit(1)

    result_dir.mkdir(parents=True, exist_ok=True)

    csv_path = result_dir / "measure_result.csv"
    write_header = not csv_path.exists()

    with open(csv_path, "a", newline="", encoding="utf-8") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=CSV_HEADER)
        if write_header:
            writer.writeheader()

        for name in files:
            fpath = sample_dir / name
            if not fpath.is_file():
                print(f"File not found: {fpath}")
                continue

            nxdomain  = gen_nxdomain()
            timestamp = datetime.now(TZ_TPE).strftime("%Y-%m-%d %H:%M:%S")
            print(f"\n--- Round {round_num} | {name} | NXDOMAIN {nxdomain} ---")

            # 1) Sender
            send_cmd = [str(sender), str(fpath), nxdomain, dns, threads]
            rc, so, se = run(send_cmd)
            if rc != 0:
                print(f"sender failed rc={rc}\n{so}\n{se}", file=sys.stderr)
                sys.exit(rc)
            print(so, end="")

            # 2) Receiver
            recv_cmd = [str(receiver), nxdomain, dns, threads]
            rc, so2, se2 = run(recv_cmd)
            if rc != 0:
                print(f"receiver failed rc={rc}\n{so2}\n{se2}", file=sys.stderr)
                sys.exit(rc)
            print(so2, end="")

            row = {
                "round":          round_num,
                "timestamp":      timestamp,
                "dns_server":     dns,
                "filename":       name,
                "file_size_bytes": fpath.stat().st_size,
                # sender fields
                "snd_encap_ms":   parse_float(RE_SND_ENCAP,  so),
                "snd_trans_ms":   parse_float(RE_SND_TRANS,  so),
                "snd_dns_ms":     parse_float(RE_SND_DNS,    so),
                "snd_total_ms":   parse_float(RE_SND_TOTAL,  so),
                # receiver fields
                "rcv_neg_ms":     parse_float(RE_RCV_NEG,    so2),
                "rcv_dns_ms":     parse_float(RE_RCV_DNS,    so2),
                "rcv_decap_ms":   parse_float(RE_RCV_DECAP,  so2),
                "rcv_total_ms":   parse_float(RE_RCV_TOTAL,  so2),
            }
            writer.writerow(row)
            csvfile.flush()

            # human-readable summary
            print(
                f"  → CSV row written | "
                f"snd_total={row['snd_total_ms']} ms  "
                f"rcv_total={row['rcv_total_ms']} ms"
            )

    print(f"\nResults appended to: {csv_path}")



if __name__ == "__main__":
    #round number may be injected by run_matrix.py via argv
    rnum = int(sys.argv[-1]) if len(sys.argv) >= 2 and sys.argv[-1].isdigit() else 1
    main()
        
