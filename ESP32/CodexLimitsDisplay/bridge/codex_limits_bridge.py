"""Read-only LAN bridge from Codex app-server to the ESP32 display."""

from __future__ import annotations

import argparse
import hmac
import json
import os
import select
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Callable, Mapping, TextIO


class ProtocolError(RuntimeError):
    """The app-server response was missing or unsafe for the display."""


class JsonRpcTransport:
    def __init__(
        self, incoming: TextIO, outgoing: TextIO, timeout_seconds: float = 10.0
    ):
        self._incoming = incoming
        self._outgoing = outgoing
        self._timeout_seconds = timeout_seconds
        self._next_id = 1
        self._lock = threading.Lock()
        self._read_buffer = bytearray()

    def request(self, method: str, params: dict[str, Any] | None = None) -> Any:
        with self._lock:
            request_id = self._next_id
            self._next_id += 1
            message: dict[str, Any] = {"method": method, "id": request_id}
            if params is not None:
                message["params"] = params
            self._write(message)
            deadline = time.monotonic() + self._timeout_seconds
            while True:
                line = self._readline(deadline)
                if not line:
                    raise ProtocolError("app-server closed the connection")
                try:
                    response = json.loads(line)
                except json.JSONDecodeError as error:
                    raise ProtocolError("invalid app-server JSON") from error
                if response.get("id") != request_id:
                    continue
                if "error" in response:
                    raise ProtocolError("app-server request failed")
                if "result" not in response:
                    raise ProtocolError("app-server response has no result")
                return response["result"]

    def _readline(self, deadline: float) -> str:
        try:
            file_descriptor = self._incoming.fileno()
        except (AttributeError, OSError, ValueError):
            return self._incoming.readline()
        while True:
            newline = self._read_buffer.find(b"\n")
            if newline >= 0:
                line = bytes(self._read_buffer[: newline + 1])
                del self._read_buffer[: newline + 1]
                try:
                    return line.decode("utf-8")
                except UnicodeDecodeError as error:
                    raise ProtocolError("invalid app-server UTF-8") from error
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise ProtocolError("app-server request timed out")
            readable, _, _ = select.select([file_descriptor], [], [], remaining)
            if not readable:
                raise ProtocolError("app-server request timed out")
            chunk = os.read(file_descriptor, 4096)
            if not chunk:
                return ""
            self._read_buffer.extend(chunk)

    def notify(self, method: str, params: dict[str, Any] | None = None) -> None:
        message: dict[str, Any] = {"method": method}
        if params is not None:
            message["params"] = params
        with self._lock:
            self._write(message)

    def _write(self, message: dict[str, Any]) -> None:
        self._outgoing.write(json.dumps(message, separators=(",", ":")) + "\n")
        self._outgoing.flush()


def _window_label(minutes: int) -> str:
    if minutes % 1440 == 0:
        return f"{minutes // 1440}D"
    if minutes % 60 == 0:
        return f"{minutes // 60}H"
    return f"{minutes}M"


def _normalize_window(value: Any) -> dict[str, int | str]:
    if not isinstance(value, dict):
        raise ProtocolError("limit window is missing")
    used = value.get("usedPercent")
    minutes = value.get("windowDurationMins")
    resets_at = value.get("resetsAt")
    if isinstance(used, bool) or not isinstance(used, (int, float)):
        raise ProtocolError("invalid used percentage")
    if int(used) != used or not 0 <= int(used) <= 100:
        raise ProtocolError("used percentage is out of range")
    if isinstance(minutes, bool) or not isinstance(minutes, int) or minutes <= 0:
        raise ProtocolError("invalid window duration")
    if isinstance(resets_at, bool) or not isinstance(resets_at, int) or resets_at <= 0:
        raise ProtocolError("invalid reset timestamp")
    label = _window_label(minutes)
    if len(label) > 11:
        raise ProtocolError("window label is too long")
    return {
        "label": label,
        "used_percent": int(used),
        "window_minutes": minutes,
        "resets_at": resets_at,
    }


def normalize_rate_limits(result: Any, now: int | None = None) -> dict[str, Any]:
    if not isinstance(result, dict):
        raise ProtocolError("rate-limit result is not an object")
    bucket = result.get("rateLimits")
    if not isinstance(bucket, dict):
        buckets = result.get("rateLimitsByLimitId")
        if not isinstance(buckets, dict):
            raise ProtocolError("rate-limit bucket is missing")
        bucket = buckets.get("codex")
    if not isinstance(bucket, dict):
        raise ProtocolError("Codex rate-limit bucket is missing")
    windows = [_normalize_window(bucket.get("primary"))]
    if bucket.get("secondary") is not None:
        windows.append(_normalize_window(bucket["secondary"]))
    return {
        "version": 1,
        "generated_at": int(time.time() if now is None else now),
        "windows": windows[:2],
        "truncated": len(windows) > 2,
    }


def resolve_shared_secret(environment: Mapping[str, str]) -> str:
    secret = environment.get("CODEX_LIMITS_SHARED_SECRET", "")
    if not secret:
        raise ValueError("CODEX_LIMITS_SHARED_SECRET must not be empty")
    return secret


class AppServerSession:
    def __init__(self, transport: Any, now: Callable[[], int] | None = None):
        self._transport = transport
        self._now = now or (lambda: int(time.time()))
        self._initialized = False

    def read_limits(self) -> dict[str, Any]:
        if not self._initialized:
            self._transport.request(
                "initialize",
                {
                    "clientInfo": {
                        "name": "codex_limits_bridge",
                        "title": "Codex Limits Bridge",
                        "version": "1.0.0",
                    }
                },
            )
            self._transport.notify("initialized")
            self._initialized = True
        result = self._transport.request("account/rateLimits/read")
        return normalize_rate_limits(result, self._now())


class BridgeApplication:
    def __init__(self, session: AppServerSession, shared_secret: str):
        if not shared_secret:
            raise ValueError("shared secret must not be empty")
        self._session = session
        self._expected = f"Bearer {shared_secret}"
        self._lock = threading.Lock()

    def handle(
        self, method: str, path: str, authorization: str | None
    ) -> tuple[int, dict[str, Any]]:
        if method != "GET" or path != "/v1/limits":
            return 404, {"available": False, "error": "not_found"}
        if authorization is None or not hmac.compare_digest(
            authorization, self._expected
        ):
            return 401, {"available": False, "error": "unauthorized"}
        try:
            with self._lock:
                return 200, self._session.read_limits()
        except (ProtocolError, OSError, ValueError):
            return 503, {"available": False, "error": "codex_unavailable"}


def _handler_for(app: BridgeApplication):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self) -> None:
            self._serve("GET")

        def do_POST(self) -> None:
            self._serve("POST")

        def do_DELETE(self) -> None:
            self._serve("DELETE")

        def _serve(self, method: str) -> None:
            status, payload = app.handle(
                method, self.path, self.headers.get("Authorization")
            )
            encoded = json.dumps(payload, separators=(",", ":")).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(encoded)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(encoded)

        def log_message(self, format: str, *args: Any) -> None:
            return

    return Handler


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--listen", required=True, help="Private LAN address to bind")
    parser.add_argument("--port", type=int, default=8765)
    args = parser.parse_args()
    shared_secret = resolve_shared_secret(os.environ)
    process = subprocess.Popen(
        ["codex", "app-server"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=None,
        text=True,
        bufsize=1,
    )
    if process.stdin is None or process.stdout is None:
        raise RuntimeError("failed to open Codex app-server pipes")
    transport = JsonRpcTransport(process.stdout, process.stdin)
    app = BridgeApplication(AppServerSession(transport), shared_secret)
    server = ThreadingHTTPServer((args.listen, args.port), _handler_for(app))
    try:
        server.serve_forever()
    finally:
        server.server_close()
        process.terminate()


if __name__ == "__main__":
    main()
