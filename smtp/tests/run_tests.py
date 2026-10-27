#!/usr/bin/env python3
"""Integration tests for smtp_client; no external SMTP server is contacted."""

import socket
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path


CLIENT = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None


class FakeSmtpServer:
    def __init__(self, script):
        self.script = script
        self.error = None
        self.thread = None
        self.received_data = None
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("127.0.0.1", 0))
        self.listener.listen(1)
        self.port = self.listener.getsockname()[1]

    def start(self):
        def run():
            try:
                connection, _ = self.listener.accept()
                with connection:
                    connection.settimeout(5)
                    self.script(self, connection)
            except Exception as error:  # retained for the test assertion
                self.error = error
            finally:
                self.listener.close()

        self.thread = threading.Thread(target=run, daemon=True)
        self.thread.start()

    def finish(self):
        self.thread.join(timeout=6)
        if self.thread.is_alive():
            raise AssertionError("fake SMTP server did not finish")
        if self.error:
            raise self.error

    @staticmethod
    def line(connection):
        result = bytearray()
        while not result.endswith(b"\r\n"):
            chunk = connection.recv(1)
            if not chunk:
                raise AssertionError("client closed the connection unexpectedly")
            result += chunk
        return bytes(result)

    @classmethod
    def command(cls, connection, expected):
        actual = cls.line(connection)
        if actual != expected:
            raise AssertionError(f"expected {expected!r}, got {actual!r}")


class SmtpClientTests(unittest.TestCase):
    def run_client(self, port, body, subject="Test subject"):
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False) as message:
            message.write(body)
            message_path = message.name
        try:
            return subprocess.run(
                [str(CLIENT), "127.0.0.1", str(port), "alice@example.test",
                 "bob@example.test", subject, message_path],
                text=True, capture_output=True, timeout=8, check=False,
            )
        finally:
            Path(message_path).unlink()

    def test_complete_dialogue_and_dot_stuffing(self):
        def script(server, connection):
            connection.sendall(b"220-local test server\r\n220 ready\r\n")
            server.command(connection, b"EHLO localhost\r\n")
            connection.sendall(b"250-localhost\r\n250 SIZE 100000\r\n")
            server.command(connection, b"MAIL FROM:<alice@example.test>\r\n")
            connection.sendall(b"250 sender accepted\r\n")
            server.command(connection, b"RCPT TO:<bob@example.test>\r\n")
            connection.sendall(b"251 recipient will be forwarded\r\n")
            server.command(connection, b"DATA\r\n")
            connection.sendall(b"354 end data with <CR><LF>.<CR><LF>\r\n")
            data = bytearray()
            while not data.endswith(b"\r\n.\r\n"):
                data.extend(connection.recv(1024))
            server.received_data = bytes(data)
            connection.sendall(b"250 message queued\r\n")
            server.command(connection, b"QUIT\r\n")
            connection.sendall(b"221 bye\r\n")

        server = FakeSmtpServer(script)
        server.start()
        result = self.run_client(server.port, "First line\n.leading dot\nLast line")
        server.finish()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Message accepted", result.stdout)
        self.assertEqual(
            server.received_data,
            b"From: alice@example.test\r\nTo: bob@example.test\r\n"
            b"Subject: Test subject\r\n\r\nFirst line\r\n..leading dot\r\n"
            b"Last line\r\n.\r\n",
        )

    def test_rejects_unexpected_smtp_reply(self):
        def script(server, connection):
            connection.sendall(b"220 ready\r\n")
            server.command(connection, b"EHLO localhost\r\n")
            connection.sendall(b"250 hello\r\n")
            server.command(connection, b"MAIL FROM:<alice@example.test>\r\n")
            connection.sendall(b"550 sender rejected\r\n")

        server = FakeSmtpServer(script)
        server.start()
        result = self.run_client(server.port, "body")
        server.finish()

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unexpected SMTP reply 550", result.stderr)

    def test_rejects_invalid_port_before_connecting(self):
        result = subprocess.run(
            [str(CLIENT), "127.0.0.1", "0", "a@b.test", "c@d.test", "subject", "none"],
            text=True, capture_output=True, timeout=5, check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Usage:", result.stderr)


if __name__ == "__main__":
    if CLIENT is None:
        raise SystemExit("Usage: run_tests.py <path-to-smtp_client>")
    unittest.main(argv=[sys.argv[0]])
