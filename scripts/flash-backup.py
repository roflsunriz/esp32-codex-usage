"""ESP32の全フラッシュを退避、またはハッシュ確認後に復元する。"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def digest(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("backup", "restore"))
    parser.add_argument("--port", required=True)
    parser.add_argument("--file", type=Path, required=True)
    args = parser.parse_args()
    destination = args.file.resolve()
    metadata = destination.with_suffix(destination.suffix + ".json")
    base = [sys.executable, "-m", "esptool", "--chip", "esp32", "--port", args.port, "--baud", "460800"]
    if args.action == "backup":
        if destination.exists() or metadata.exists():
            parser.error("既存のバックアップを上書きしません。別のファイル名を指定してください。")
        destination.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(base + ["--after", "no-reset", "read-flash", "0", "ALL", str(destination)], check=True)
        subprocess.run(base + ["verify-flash", "0", str(destination)], check=True)
        metadata.write_text(json.dumps({"sha256": digest(destination), "bytes": destination.stat().st_size}, indent=2) + "\n", encoding="utf-8")
        print("全フラッシュの退避と実機との照合が完了しました。バックアップは非公開で保管してください。")
    else:
        recorded = json.loads(metadata.read_text(encoding="utf-8"))
        if recorded["sha256"] != digest(destination) or recorded["bytes"] != destination.stat().st_size:
            parser.error("バックアップのサイズまたはSHA-256が一致しません。書き込みを中止しました。")
        subprocess.run(base + ["--after", "no-reset", "write-flash", "0", str(destination)], check=True)
        subprocess.run(base + ["verify-flash", "0", str(destination)], check=True)
        print("元のフラッシュ内容の書き戻しと照合が完了しました。本体の通常起動を確認してください。")


if __name__ == "__main__":
    main()
