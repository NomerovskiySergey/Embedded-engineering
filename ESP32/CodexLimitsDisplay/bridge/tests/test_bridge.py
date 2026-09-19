import io
import json
import os
import threading
import time
import unittest

from bridge.codex_limits_bridge import (
    AppServerSession,
    BridgeApplication,
    JsonRpcTransport,
    ProtocolError,
    normalize_rate_limits,
    resolve_shared_secret,
)


class FakeTransport:
    def __init__(self, result=None, error=None):
        self.result = result
        self.error = error
        self.calls = []

    def request(self, method, params=None):
        self.calls.append(("request", method, params))
        if method == "initialize":
            return {"userAgent": "test"}
        if self.error:
            raise self.error
        return self.result

    def notify(self, method, params=None):
        self.calls.append(("notify", method, params))


def sample_result():
    return {
        "rateLimits": {
            "limitId": "codex",
            "limitName": "Codex",
            "primary": {
                "usedPercent": 25,
                "windowDurationMins": 300,
                "resetsAt": 1789783600,
            },
            "secondary": {
                "usedPercent": 80,
                "windowDurationMins": 10080,
                "resetsAt": 1790384800,
            },
            "planType": "pro",
            "credits": {"balance": 99},
        },
        "account": {"accessToken": "must-not-leak"},
    }


class NormalizeTests(unittest.TestCase):
    def test_normalizes_two_windows_without_leaking_account_fields(self):
        payload = normalize_rate_limits(sample_result(), now=1789780000)
        self.assertEqual(payload["version"], 1)
        self.assertEqual(payload["generated_at"], 1789780000)
        self.assertEqual([window["label"] for window in payload["windows"]], ["5H", "7D"])
        self.assertEqual(payload["windows"][0]["used_percent"], 25)
        self.assertNotIn("account", payload)
        self.assertNotIn("credits", json.dumps(payload))
        self.assertNotIn("accessToken", json.dumps(payload))

    def test_uses_codex_multi_bucket_when_compatibility_view_is_missing(self):
        result = {"rateLimitsByLimitId": {"other": {}, "codex": sample_result()["rateLimits"]}}
        payload = normalize_rate_limits(result, now=1)
        self.assertEqual(len(payload["windows"]), 2)

    def test_rejects_malformed_service_data(self):
        bad_values = [None, {}, {"rateLimits": {"primary": {"usedPercent": 101}}}]
        for value in bad_values:
            with self.subTest(value=value):
                with self.assertRaises(ProtocolError):
                    normalize_rate_limits(value, now=1)


class SessionTests(unittest.TestCase):
    def test_initializes_then_reads_only_rate_limits(self):
        transport = FakeTransport(sample_result())
        session = AppServerSession(transport, now=lambda: 1789780000)
        payload = session.read_limits()
        self.assertEqual(payload["windows"][0]["used_percent"], 25)
        self.assertEqual(
            transport.calls,
            [
                ("request", "initialize", {"clientInfo": {"name": "codex_limits_bridge", "title": "Codex Limits Bridge", "version": "1.0.0"}}),
                ("notify", "initialized", None),
                ("request", "account/rateLimits/read", None),
            ],
        )

    def test_protocol_errors_become_explicit_unavailable_responses(self):
        transport = FakeTransport(error=ProtocolError("not authenticated"))
        app = BridgeApplication(AppServerSession(transport), "secret")
        status, payload = app.handle("GET", "/v1/limits", "Bearer secret")
        self.assertEqual(status, 503)
        self.assertEqual(payload, {"available": False, "error": "codex_unavailable"})


class ApplicationTests(unittest.TestCase):
    def setUp(self):
        self.app = BridgeApplication(AppServerSession(FakeTransport(sample_result()), now=lambda: 1), "correct")

    def test_rejects_missing_or_incorrect_secret(self):
        for header in (None, "", "Bearer wrong", "Basic correct"):
            with self.subTest(header=header):
                status, payload = self.app.handle("GET", "/v1/limits", header)
                self.assertEqual(status, 401)
                self.assertEqual(payload, {"available": False, "error": "unauthorized"})

    def test_exposes_no_write_or_proxy_route(self):
        for method, path in (("POST", "/v1/limits"), ("GET", "/rpc"), ("DELETE", "/v1/limits")):
            with self.subTest(method=method, path=path):
                status, _ = self.app.handle(method, path, "Bearer correct")
                self.assertEqual(status, 404)

    def test_shared_secret_comes_from_environment(self):
        self.assertEqual(
            resolve_shared_secret({"CODEX_LIMITS_SHARED_SECRET": "from-env"}),
            "from-env",
        )
        for environment in ({}, {"CODEX_LIMITS_SHARED_SECRET": ""}):
            with self.subTest(environment=environment):
                with self.assertRaises(ValueError):
                    resolve_shared_secret(environment)


class TransportTests(unittest.TestCase):
    def test_json_rpc_transport_rejects_error_response(self):
        incoming = io.StringIO('{"id":1,"error":{"message":"unauthenticated"}}\n')
        outgoing = io.StringIO()
        transport = JsonRpcTransport(incoming, outgoing)
        with self.assertRaises(ProtocolError):
            transport.request("account/rateLimits/read")

    def test_json_rpc_transport_times_out(self):
        read_fd, write_fd = os.pipe()
        incoming = os.fdopen(read_fd)
        try:
            transport = JsonRpcTransport(
                incoming, io.StringIO(), timeout_seconds=0.01
            )
            with self.assertRaises(ProtocolError):
                transport.request("account/rateLimits/read")
        finally:
            incoming.close()
            os.close(write_fd)

    def test_notification_and_response_in_one_pipe_write_are_both_read(self):
        read_fd, write_fd = os.pipe()
        incoming = os.fdopen(read_fd)
        try:
            os.write(
                write_fd,
                b'{"method":"account/rateLimits/updated","params":{}}\n'
                b'{"id":1,"result":{"ok":true}}\n',
            )
            transport = JsonRpcTransport(
                incoming, io.StringIO(), timeout_seconds=0.05
            )
            self.assertEqual(
                transport.request("account/rateLimits/read"), {"ok": True}
            )
        finally:
            incoming.close()
            os.close(write_fd)

    def test_notifications_do_not_extend_request_deadline(self):
        read_fd, write_fd = os.pipe()
        incoming = os.fdopen(read_fd)

        def send_notifications():
            try:
                for _ in range(12):
                    os.write(write_fd, b'{"method":"tick","params":{}}\n')
                    time.sleep(0.01)
                os.write(write_fd, b'{"id":1,"result":{"late":true}}\n')
            except OSError:
                pass
            finally:
                os.close(write_fd)

        writer = threading.Thread(target=send_notifications)
        writer.start()
        try:
            transport = JsonRpcTransport(
                incoming, io.StringIO(), timeout_seconds=0.04
            )
            started = time.monotonic()
            with self.assertRaises(ProtocolError):
                transport.request("account/rateLimits/read")
            self.assertLess(time.monotonic() - started, 0.10)
        finally:
            incoming.close()
            writer.join()


if __name__ == "__main__":
    unittest.main()
