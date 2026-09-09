#!/usr/bin/env python3
"""Run the setup-page browser checks with an isolated local fixture."""

from __future__ import annotations

import argparse
import json
import os
import shutil
import signal
import socket
import subprocess
import tempfile
import threading
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import parse_qs


ALLOWED_TIMEOUTS = {
    15000,
    30000,
    60000,
    120000,
    300000,
    600000,
    1800000,
    3600000,
    7200000,
}
SETUP_NONCE = "setup-ui-test-nonce"


@dataclass
class FixtureState:
    """Small in-memory model matching the setup API contract."""

    connected: bool = False
    authenticated: bool = False
    status: str = "初期状態"
    device_code: str = ""
    timeout_ms: int = 60000

    def snapshot(self) -> dict[str, Any]:
        return {
            "connected": self.connected,
            "authenticated": self.authenticated,
            "status": self.status,
            "deviceCode": self.device_code,
            "timeoutMs": self.timeout_ms,
        }


class SetupFixtureServer(ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True

    def __init__(self, address: tuple[str, int], page: bytes, nonce: str):
        super().__init__(address, SetupFixtureHandler)
        self.fixture = FixtureState()
        self.page = page.replace(b"__SETUP_NONCE__", nonce.encode("ascii"))
        self.nonce = nonce
        self.state_lock = threading.Lock()


class SetupFixtureHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, _format: str, *_args: object) -> None:
        # Credentials and device codes must never reach test logs.
        return

    @property
    def fixture_server(self) -> SetupFixtureServer:
        return self.server  # type: ignore[return-value]

    def _local_only(self) -> bool:
        if self.client_address[0] == "127.0.0.1":
            return True
        self._json(403, {"ok": False, "error": "local test client required"})
        return False

    def _json(self, status: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode(
            "utf-8"
        )
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def _page(self) -> None:
        body = self.fixture_server.page
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.close_connection = True

    def _post_fields(self) -> dict[str, str] | None:
        if self.headers.get("X-Setup-Key") != self.fixture_server.nonce:
            self._json(403, {"ok": False, "error": "invalid setup key"})
            return None
        raw_length = self.headers.get("Content-Length", "0")
        try:
            content_length = int(raw_length)
        except ValueError:
            self._json(400, {"ok": False, "error": "invalid request body"})
            return None
        if content_length < 0 or content_length > 4096:
            self._json(400, {"ok": False, "error": "invalid request body"})
            return None
        content_type = self.headers.get("Content-Type", "").lower()
        if content_length and not content_type.startswith(
            "application/x-www-form-urlencoded"
        ):
            self._json(400, {"ok": False, "error": "form body required"})
            return None
        body = self.rfile.read(content_length).decode("utf-8")
        return {
            key: values[-1]
            for key, values in parse_qs(body, keep_blank_values=True).items()
            if values
        }

    def do_GET(self) -> None:  # noqa: N802 - stdlib handler API
        if not self._local_only():
            return
        path = self.path.split("?", 1)[0]
        if path == "/":
            self._page()
            return
        if path == "/api/state":
            with self.fixture_server.state_lock:
                self._json(200, self.fixture_server.fixture.snapshot())
            return
        self._json(404, {"ok": False, "error": "not found"})

    def do_POST(self) -> None:  # noqa: N802 - stdlib handler API
        if not self._local_only():
            return
        fields = self._post_fields()
        if fields is None:
            return
        path = self.path.split("?", 1)[0]
        fixture = self.fixture_server.fixture
        with self.fixture_server.state_lock:
            if path == "/api/wifi":
                ssid = fields.get("ssid", "")
                password = fields.get("password", "")
                if (
                    not ssid
                    or len(ssid) > 32
                    or len(password) > 64
                    or (password and len(password) < 8)
                ):
                    self._json(400, {"ok": False, "error": "invalid wifi settings"})
                    return
                fixture.connected = True
                fixture.status = "Wi-Fi接続済み"
                self._json(200, {"ok": True})
                return
            if path == "/api/login":
                if fields:
                    self._json(400, {"ok": False, "error": "login body must be empty"})
                    return
                if not fixture.connected:
                    self._json(409, {"ok": False, "error": "wifi required"})
                    return
                fixture.authenticated = True
                fixture.device_code = "MOCK-CODE"
                fixture.status = "ブラウザで認証コードを入力"
                self._json(200, {"ok": True})
                return
            if path == "/api/timeout":
                raw_value = fields.get("value", "")
                try:
                    value = int(raw_value)
                except ValueError:
                    value = -1
                if set(fields) != {"value"} or value not in ALLOWED_TIMEOUTS:
                    self._json(400, {"ok": False, "error": "invalid timeout"})
                    return
                fixture.timeout_ms = value
                fixture.status = "設定を保存しました"
                self._json(200, {"ok": True})
                return
        self._json(404, {"ok": False, "error": "not found"})


def free_local_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def find_executable(name: str, candidates: list[Path]) -> Path:
    on_path = shutil.which(name)
    if on_path:
        return Path(on_path)
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise RuntimeError(f"{name} was not found; install it or pass --{name}")


def wait_for_cdp(port: int, process: subprocess.Popen[bytes]) -> None:
    endpoint = f"http://127.0.0.1:{port}/json/version"
    deadline = time.monotonic() + 20
    last_error: Exception | None = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"Chrome exited before CDP became ready: {process.returncode}")
        try:
            with urllib.request.urlopen(endpoint, timeout=1) as response:
                version = json.loads(response.read().decode("utf-8"))
            websocket_url = version.get("webSocketDebuggerUrl", "")
            if websocket_url.startswith(f"ws://127.0.0.1:{port}/"):
                return
            last_error = RuntimeError("CDP endpoint was not bound to 127.0.0.1")
        except (OSError, urllib.error.URLError, json.JSONDecodeError) as error:
            last_error = error
        time.sleep(0.1)
    raise RuntimeError(f"CDP did not become ready: {last_error}")


def hidden_process_kwargs() -> dict[str, Any]:
    if os.name != "nt":
        return {}
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    return {
        "creationflags": subprocess.CREATE_NO_WINDOW,
        "startupinfo": startup,
    }


def terminate_process_tree(process: subprocess.Popen[bytes] | None) -> None:
    if process is None or process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    else:
        try:
            os.kill(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            return
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def parse_args() -> argparse.Namespace:
    repo = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--chrome",
        type=Path,
        default=None,
        help="Chrome executable (auto-detected when omitted)",
    )
    parser.add_argument(
        "--node",
        type=Path,
        default=None,
        help="Node executable (auto-detected when omitted)",
    )
    parser.add_argument(
        "--screenshot-dir",
        type=Path,
        default=repo / ".local" / "setup-ui-qa",
        help="Directory for the two QA screenshots",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo = Path(__file__).resolve().parents[1]
    page_path = repo / "web" / "setup.html"
    runner_path = repo / "scripts" / "test-setup-ui.mjs"
    if not page_path.is_file() or not runner_path.is_file():
        raise RuntimeError("web/setup.html and scripts/test-setup-ui.mjs are required")

    chrome_candidates = [
        Path("/usr/bin/google-chrome"),
        Path("/usr/bin/chromium"),
        Path("/usr/bin/chromium-browser"),
        Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"),
        Path(os.environ.get("PROGRAMFILES", "C:/Program Files"))
        / "Google/Chrome/Application/chrome.exe",
        Path(os.environ.get("PROGRAMFILES(X86)", "C:/Program Files (x86)"))
        / "Google/Chrome/Application/chrome.exe",
        Path(os.environ.get("LOCALAPPDATA", ""))
        / "Google/Chrome/Application/chrome.exe",
    ]
    node_candidates = [
        Path(os.environ.get("PROGRAMFILES", "C:/Program Files"))
        / "nodejs/node.exe",
    ]
    chrome = args.chrome or find_executable("chrome", chrome_candidates)
    node = args.node or find_executable("node", node_candidates)
    page = page_path.read_bytes()
    server = SetupFixtureServer(("127.0.0.1", 0), page, SETUP_NONCE)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()

    chrome_process: subprocess.Popen[bytes] | None = None
    chrome_log = tempfile.TemporaryFile()
    try:
        fixture_port = int(server.server_address[1])
        cdp_port = free_local_port()
        base_url = f"http://127.0.0.1:{fixture_port}/"
        profile = tempfile.TemporaryDirectory(prefix="codex-setup-ui-chrome-")
        chrome_args = [
            "--headless=new",
            "--no-sandbox",
            "--disable-dev-shm-usage",
            "--disable-gpu",
            "--mute-audio",
            "--no-first-run",
            "--no-default-browser-check",
            "--disable-default-apps",
            "--remote-allow-origins=*",
            "--remote-debugging-address=127.0.0.1",
            f"--remote-debugging-port={cdp_port}",
            f"--user-data-dir={profile.name}",
            "about:blank",
        ]
        if os.name == "nt":
            chrome_args[1:1] = [
                "--disable-gpu-compositing", "--disable-gpu-sandbox",
                "--in-process-gpu", "--use-angle=swiftshader",
            ]
        chrome_process = subprocess.Popen(
            [str(chrome), *chrome_args],
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=chrome_log,
            **hidden_process_kwargs(),
        )
        try:
            wait_for_cdp(cdp_port, chrome_process)
        except RuntimeError as error:
            chrome_log.seek(0)
            detail = chrome_log.read().decode("utf-8", errors="replace")[-4000:]
            raise RuntimeError(f"{error}\nChrome startup log:\n{detail}") from error
        environment = os.environ.copy()
        environment["SETUP_BASE_URL"] = base_url
        environment["SETUP_CDP_PORT"] = str(cdp_port)
        environment["SETUP_SCREENSHOT_DIR"] = str(args.screenshot_dir.resolve())
        result = subprocess.run(
            [str(node), str(runner_path)],
            cwd=repo,
            env=environment,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            **hidden_process_kwargs(),
        )
        if result.stdout:
            print(result.stdout, end="")
        if result.returncode != 0:
            raise RuntimeError(f"setup UI checks failed with exit code {result.returncode}")
        print(
            json.dumps(
                {
                    "status": "passed",
                    "screenshots": [
                        str(args.screenshot_dir.resolve() / "setup-desktop.png"),
                        str(args.screenshot_dir.resolve() / "setup-320.png"),
                    ],
                },
                ensure_ascii=False,
            )
        )
        return 0
    finally:
        terminate_process_tree(chrome_process)
        chrome_log.close()
        if "profile" in locals():
            profile.cleanup()
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=2)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        raise SystemExit(130)
