#!/usr/bin/env python3
"""Local integration tests for dns_resolver; no public DNS server is queried."""

import ipaddress
import socket
import struct
import subprocess
import sys
import threading
import unittest
from pathlib import Path


CLIENT = Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else None


def encode_name(name):
    return b"".join(bytes([len(label)]) + label.encode("ascii") for label in name.split(".")) + b"\0"


def parse_question(packet):
    identifier, flags, questions, _, _, _ = struct.unpack("!HHHHHH", packet[:12])
    if questions != 1:
        raise AssertionError("resolver must send exactly one DNS question")
    offset = 12
    labels = []
    while packet[offset] != 0:
        length = packet[offset]
        labels.append(packet[offset + 1:offset + 1 + length].decode("ascii"))
        offset += length + 1
    offset += 1
    question_type, question_class = struct.unpack("!HH", packet[offset:offset + 4])
    return identifier, flags, ".".join(labels), question_type, question_class, packet[12:offset + 4]


def record(name, record_type, data):
    return encode_name(name) + struct.pack("!HHIH", record_type, 1, 60, len(data)) + data


class FakeDnsServer:
    def __init__(self, handler):
        self.handler = handler
        self.error = None
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.socket.bind(("127.0.0.1", 0))
        self.port = self.socket.getsockname()[1]
        self.thread = threading.Thread(target=self.run, daemon=True)

    def start(self):
        self.thread.start()

    def run(self):
        try:
            self.socket.settimeout(5)
            self.handler(self.socket)
        except Exception as error:
            self.error = error
        finally:
            self.socket.close()

    def finish(self):
        self.thread.join(timeout=6)
        if self.thread.is_alive():
            raise AssertionError("fake DNS server did not finish")
        if self.error:
            raise self.error


class DnsResolverTests(unittest.TestCase):
    def run_client(self, port, *extra):
        return subprocess.run(
            [str(CLIENT), "www.example.test", "A", *extra, "--root", "127.0.0.1", "--port", str(port)],
            text=True, capture_output=True, timeout=8, check=False,
        )

    def test_iterative_referral_and_compressed_answer(self):
        def handler(server):
            first, peer = server.recvfrom(2048)
            identifier, flags, name, query_type, query_class, question = parse_question(first)
            self.assertEqual(name, "www.example.test")
            self.assertEqual(query_type, 1)
            self.assertEqual(query_class, 1)
            self.assertEqual(flags & 0x0100, 0, "iterative query must not request recursion")
            ns_name = "ns.example.test"
            referral = (
                struct.pack("!HHHHHH", identifier, 0x8000, 1, 0, 1, 1)
                + question
                + record("example.test", 2, encode_name(ns_name))
                + record(ns_name, 1, ipaddress.ip_address("127.0.0.1").packed)
            )
            server.sendto(referral, peer)

            second, peer = server.recvfrom(2048)
            identifier, _, name, query_type, _, question = parse_question(second)
            self.assertEqual(name, "www.example.test")
            self.assertEqual(query_type, 1)
            answer = b"\xc0\x0c" + struct.pack("!HHIH", 1, 1, 60, 4) + ipaddress.ip_address("203.0.113.7").packed
            server.sendto(struct.pack("!HHHHHH", identifier, 0x8000, 1, 1, 0, 0) + question + answer, peer)

        server = FakeDnsServer(handler)
        server.start()
        result = self.run_client(server.port)
        server.finish()

        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, "203.0.113.7\n")
        self.assertEqual(result.stderr, "")

    def test_debug_mode_and_nxdomain(self):
        def handler(server):
            request, peer = server.recvfrom(2048)
            identifier, _, _, _, _, question = parse_question(request)
            server.sendto(struct.pack("!HHHHHH", identifier, 0x8003, 1, 0, 0, 0) + question, peer)

        server = FakeDnsServer(handler)
        server.start()
        result = self.run_client(server.port, "-d")
        server.finish()

        self.assertNotEqual(result.returncode, 0)
        self.assertIn("host not found (NXDOMAIN)", result.stderr)
        self.assertIn("[debug] query", result.stderr)

    def test_rejects_unknown_record_type(self):
        result = subprocess.run(
            [str(CLIENT), "example.test", "NOT_A_TYPE"],
            text=True, capture_output=True, timeout=5, check=False,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("unsupported record type", result.stderr)


if __name__ == "__main__":
    if CLIENT is None:
        raise SystemExit("Usage: run_tests.py <path-to-dns_resolver>")
    unittest.main(argv=[sys.argv[0]])
