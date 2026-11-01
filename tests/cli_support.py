"""Shared process/protocol helpers for compiler integration tests."""
import json
import re
import subprocess
import unittest

URI = "file:///test/main.fuse"

def notification(method, params=None):
    return {"jsonrpc": "2.0", "method": method, "params": params or {}}


def request(method, request_id, params=None):
    return {**notification(method, params), "id": request_id}


def opened(text, language_id="fusegl", uri=URI):
    return notification("textDocument/didOpen", {
        "textDocument": {
            "uri": uri, "languageId": language_id, "version": 1, "text": text,
        },
    })


def changed(text, version, uri=URI):
    return notification("textDocument/didChange", {
        "textDocument": {"uri": uri, "version": version},
        "contentChanges": [{"text": text}],
    })


def at(source, uri=URI):
    return {
        "textDocument": {"uri": uri},
        "position": {
            "line": source.count("\n"),
            "character": len(source.rsplit("\n", 1)[-1].encode("utf-16-le")) // 2,
        },
    }


class CompilerTests(unittest.TestCase):
    def assert_comments_preserve_output(self, source, *arguments):
        plain = "let asset = " + source + ";\nasset"
        stages = re.sub(r"\.(?=[A-Za-z_])", "// stage\n.", source)
        stages = stages.replace("(", "(// arguments\n").replace(",", ",// next argument\n")
        commented = "// asset\nlet // name\nasset = " + stages + "; // binding\nasset// result"
        expected = self.run_command(*arguments, source=plain)
        self.assertEqual(expected.returncode, 0, expected.stderr.decode())
        for newline in ("\n", "\r\n"):
            with self.subTest(newline=repr(newline)):
                actual = self.run_command(*arguments, source=commented.replace("\n", newline))
                self.assertEqual(actual.returncode, 0, actual.stderr.decode())
                self.assertEqual(actual.stderr, b"")
                self.assertEqual(actual.stdout, expected.stdout)

    def run_command(self, *arguments, source=""):
        return subprocess.run(
            [str(self.binary), self.command, *arguments], input=source.encode(),
            capture_output=True, timeout=10,
        )

    def lsp_session(self, *messages):
        messages = (
            request("initialize", 1), *messages,
            request("shutdown", 99), notification("exit"),
        )
        frames = []
        for message in messages:
            body = json.dumps(message).encode()
            frames.append(f"Content-Length: {len(body)}\r\n\r\n".encode() + body)
        result = subprocess.run(
            [str(self.binary), "lsp"], input=b"".join(frames),
            capture_output=True, timeout=10,
        )
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        self.assertEqual(result.stderr, b"")
        packets = []
        remaining = result.stdout
        while remaining:
            header, separator, body = remaining.partition(b"\r\n\r\n")
            self.assertEqual(separator, b"\r\n\r\n")
            self.assertRegex(header.decode(), r"^Content-Length: [0-9]+$")
            length = int(header.split(b":", 1)[1])
            self.assertGreaterEqual(len(body), length)
            packets.append(json.loads(body[:length]))
            remaining = body[length:]
        responses = {packet["id"]: packet for packet in packets if "id" in packet}
        self.assertEqual(responses[1]["result"]["serverInfo"]["name"], "fuse")
        self.assertIsNone(responses[99]["result"])
        return packets, responses

    def completions(self, source):
        _, responses = self.lsp_session(
            opened(source, self.language_id), request("textDocument/completion", 2, at(source)),
        )
        return {item["label"]: item for item in responses[2]["result"]["items"]}
