#!/usr/bin/env python3
"""Exercise the production CLI with a pipe-backed transport."""
import os
import selectors
import subprocess
import sys
import unittest

BINARY = os.path.abspath(sys.argv.pop(1))


class StreamingTests(unittest.TestCase):
    def test_event_arrives_before_peer_disconnects(self):
        for args, event in [
            (["subscribe"], b"event workspace workspace=2\n"),
            (["--json", "subscribe"], b'{"event":"workspace","workspace":2}\n'),
        ]:
            with self.subTest(args=args):
                proc = subprocess.Popen(
                    [BINARY, "-s", "/unused", *args],
                    stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                )
                try:
                    # Deliver a partial record as well as a complete one.
                    for chunk in (event[:7], event[7:]):
                        proc.stdin.write(chunk)
                        proc.stdin.flush()
                        received = b""
                        with selectors.DefaultSelector() as selector:
                            selector.register(proc.stdout, selectors.EVENT_READ)
                            while len(received) < len(chunk):
                                self.assertTrue(selector.select(3),
                                                "CLI buffered a live event")
                                part = os.read(proc.stdout.fileno(), len(chunk) - len(received))
                                self.assertTrue(part, "CLI closed a live subscription")
                                received += part
                        self.assertEqual(received, chunk)
                        self.assertIsNone(proc.poll())
                    proc.stdin.close()
                    self.assertEqual(proc.wait(timeout=3), 0)
                finally:
                    if proc.poll() is None:
                        proc.kill()
                    proc.wait()
                    for stream in (proc.stdin, proc.stdout, proc.stderr):
                        stream.close()

    def test_subscription_survives_old_buffer_limit(self):
        payload = b"event workspace workspace=2\n" * 5000
        result = subprocess.run(
            [BINARY, "-s", "/unused", "subscribe"], input=payload,
            capture_output=True, timeout=5,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, payload)

    def test_regular_reply_exit_status(self):
        for args, reply, expected in [
            (["ping"], b"ok pong\n", 0),
            (["ping"], b"error rejected\n", 2),
            (["--json", "ping"], b'{"ok":true}\n', 0),
            (["--json", "ping"], b'{"ok":false}\n', 2),
            (["ping"], b"", 1),
        ]:
            with self.subTest(args=args, reply=reply):
                result = subprocess.run(
                    [BINARY, "-s", "/unused", *args], input=reply,
                    capture_output=True, timeout=5,
                )
                self.assertEqual(result.returncode, expected, result.stderr)
                self.assertEqual(result.stdout, reply)


if __name__ == "__main__":
    unittest.main()
