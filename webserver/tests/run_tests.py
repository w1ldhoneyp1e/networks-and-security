#!/usr/bin/env python3
"""Integration tests for the laboratory web server, using loopback only."""

import socket
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path


CLIENT = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None


def receive_all(connection):
    data = bytearray()
    while True:
        chunk = connection.recv(4096)
        if not chunk:
            return bytes(data)
        data.extend(chunk)


class WebServerTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tempdir = tempfile.TemporaryDirectory()
        root = Path(cls.tempdir.name)
        (root / "index.html").write_text("<h1>Hello</h1>", encoding="utf-8")
        (root / "note.txt").write_text("plain text", encoding="utf-8")
        cls.process = subprocess.Popen(
            [str(CLIENT), "0", str(root)], text=True, stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        deadline = time.monotonic() + 5
        line = ""
        while time.monotonic() < deadline:
            line = cls.process.stdout.readline()
            if line.startswith("Listening on port "):
                break
        else:
            cls.process.terminate()
            stdout, stderr = cls.process.communicate(timeout=3)
            raise AssertionError(f"server did not start; stdout={stdout!r}, stderr={stderr!r}")
        cls.port = int(line.split()[3].rstrip(","))

    @classmethod
    def tearDownClass(cls):
        cls.process.terminate()
        try:
            cls.process.wait(timeout=3)
        except subprocess.TimeoutExpired:
            cls.process.kill()
            cls.process.wait(timeout=3)
        cls.tempdir.cleanup()

    def request(self, raw_request):
        with socket.create_connection(("127.0.0.1", self.port), timeout=3) as connection:
            connection.sendall(raw_request)
            return receive_all(connection)

    def test_returns_file_with_200(self):
        response = self.request(b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n")
        header, body = response.split(b"\r\n\r\n", 1)
        self.assertIn(b"HTTP/1.1 200 OK", header)
        self.assertIn(b"Content-Type: text/html; charset=utf-8", header)
        self.assertIn(b"Content-Length: 14", header)
        self.assertEqual(body, b"<h1>Hello</h1>")

    def test_returns_404_for_missing_or_unsafe_path(self):
        for target in (b"/missing.html", b"/../outside.txt"):
            response = self.request(b"GET " + target + b" HTTP/1.1\r\nHost: localhost\r\n\r\n")
            self.assertTrue(response.startswith(b"HTTP/1.1 404 Not Found"), response)
            self.assertTrue(response.endswith(b"File Not Found\n"), response)

    def test_rejects_non_get_method(self):
        response = self.request(b"POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 0\r\n\r\n")
        self.assertTrue(response.startswith(b"HTTP/1.1 405 Method Not Allowed"), response)


if __name__ == "__main__":
    if CLIENT is None:
        raise SystemExit("Usage: run_tests.py <path-to-webserver>")
    unittest.main(argv=[sys.argv[0]])
