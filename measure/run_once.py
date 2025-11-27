#!/usr/bin/env python3
import json, subprocess, os, re, sys
from pathlib import Path
from datetime import datetime, timezone, timedelta

CONFIG_PATH = Path(__file__).with_name("config.json")
TZ_TPE = timezone(timedelta(hours=8))
NUM_RE = re.compile('([0-9]*) (ms)')
 
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

def to_ms(text: str) -> int:
    m = NUM_RE.search(text)
    return int(m.group(1))

def main():
    # load config
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        cfg = json.load(f)

    fmt = cfg.get("format", {})
    COL_FILE = fmt.get("col_file", 18)
    COL_SIZE = fmt.get("col_size", 14)
    COL_SND  = fmt.get("col_snd", 14)
    COL_RCV  = fmt.get("col_rcv", 16)

    config_files = cfg.get("files", [])

    sample_dir = Path(cfg["paths"]["sample_dir"]).resolve()
    result_dir = Path(cfg["paths"]["result_dir"]).resolve()
    sender = Path(cfg["paths"]["sender_cpp"]).resolve()
    receiver = Path(cfg["paths"]["receiver_cpp"]).resolve()
    dns = cfg["dns_server"]

    # python3 run_once.py           -> 用 config.json 裡面的 files
    # python3 run_once.py filename  -> 只跑 filename
    if len(sys.argv) == 2:
        files = [sys.argv[1]]
    else:
        files = config_files
    if not files:
        print("No file to run (config.json: files is empty, and no filename argument).")
        sys.exit(1)

    # ensure results dir
    if not result_dir.exists():
        result_dir.mkdir(parents=True, exist_ok=True)
        print(f"Create path: {result_path}")

    out_txt = result_dir / "measure_result.txt"

    for name in files:
        fpath = sample_dir / name
        if not fpath.is_file():
            print(f"File not found: {fpath}")
            continue
        
        nxdomain = gen_nxdomain()
        print(f"\n--- Running {name} with NXDOMAIN {nxdomain} ---")

        # 1) Sender
        send_cmd = [str(sender), str(fpath), nxdomain, dns]
        rc, so, se = run(send_cmd)
        if rc != 0:
            print(f"sender failed rc={rc}")
            print(so)
            print(se, file=sys.stderr)
            sys.exit(rc)
        sender_ms = so

        # 2) Receiver
        recv_cmd = [str(receiver), nxdomain, dns]
        rc, so, se = run(recv_cmd)
        if rc != 0:
            print(f"receiver failed rc={rc}")
            print(so)
            print(se, file=sys.stderr)
            sys.exit(rc)
        receiver_ms = so

        size_bytes = fpath.stat().st_size

        # append one line
        line = (
            f'{name:<{COL_FILE}}'
            f'{size_bytes:>{COL_SIZE},}'
            f'{to_ms(sender_ms):>{COL_SND}}'
            f'{to_ms(receiver_ms):>{COL_RCV}}\n'
        )

        with open(out_txt, "a", encoding="utf-8") as fw:
            fw.write(line)

        print(line, end="")

if __name__ == "__main__":
    main()
        
