"""Unified commands and per-document LSP selection."""
from pathlib import Path
import subprocess
import sys
import unittest

from tests.cli_support import CompilerTests, at, changed, notification, opened, request


class UnifiedTests(CompilerTests):
    binary = Path("build/fuse").resolve()
    capabilities = {"gl", "tex", "mesh", "view"}

    def run_command(self, *arguments, source=""):
        return subprocess.run(
            [str(self.binary), *arguments], input=source.encode(),
            capture_output=True, timeout=10,
        )

    def completion_items(self, packet):
        return {item["label"] for item in packet["result"]["items"]}

    def test_available_commands_and_help(self):
        for arguments in [(), ("--help",), ("-h",)]:
            result = self.run_command(*arguments)
            self.assertEqual(result.returncode, 0, result.stderr.decode())
            self.assertEqual(result.stderr, b"")
            for capability in ("gl", "tex", "mesh", "view"):
                self.assertEqual(f"  {capability} ".encode() in result.stdout, capability in self.capabilities)
        for capability in self.capabilities:
            result = self.run_command(capability, "--help")
            self.assertEqual(result.returncode, 0)
            self.assertIn(f"Usage: fuse {capability}".encode(), result.stdout)
        result = self.run_command("lsp", "--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn(b"Usage: fuse lsp", result.stdout)

    def test_invalid_and_unavailable_commands(self):
        arguments = [("unknown",), ("--builtins",), ("lsp", "--builtins"), ("--help", "extra")]
        arguments += [(name,) for name in {"gl", "tex", "mesh", "view"} - self.capabilities]
        arguments += [(name, "lsp") for name in self.capabilities & {"gl", "tex", "mesh"}]
        for args in arguments:
            with self.subTest(arguments=args):
                result = self.run_command(*args)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, b"")
                self.assertIn(b"Usage:", result.stderr)

    def test_unknown_document_does_not_get_an_arbitrary_catalog(self):
        uri = "untitled:unknown"
        packets, responses = self.lsp_session(
            opened("uv.", "plaintext", uri),
            request("textDocument/completion", 2, at("uv.", uri)),
            request("textDocument/hover", 3, at("u", uri)),
            changed("disk()", 2, uri),
        )
        self.assertEqual(self.completion_items(responses[2]), set())
        self.assertIsNone(responses[3]["result"])
        diagnostics = [p["params"]["diagnostics"] for p in packets
                       if p.get("method") == "textDocument/publishDiagnostics"]
        self.assertEqual(len(diagnostics), 2)
        for items in diagnostics:
            self.assertEqual(items[0]["source"], "fuse")
            self.assertIn("not supported", items[0]["message"])

    def test_unavailable_document_capability(self):
        unavailable = {"gl", "tex", "mesh"} - self.capabilities
        if not unavailable:
            self.skipTest("full build")
        for capability in unavailable:
            uri = "file:///tmp/missing." + {"gl": "pgl", "tex": "ptex", "mesh": "pmesh"}[capability]
            packets, responses = self.lsp_session(
                opened("uv.", "fuse" + capability, uri),
                request("textDocument/completion", 2, at("uv.", uri)),
            )
            self.assertEqual(self.completion_items(responses[2]), set())
            diagnostic = next(p["params"]["diagnostics"][0] for p in packets
                              if p.get("method") == "textDocument/publishDiagnostics")
            self.assertIn("not supported", diagnostic["message"])

    def test_mixed_documents_keep_separate_catalogs(self):
        if not {"gl", "tex", "mesh"} <= self.capabilities:
            self.skipTest("requires all capabilities")
        shader = "file:///tmp/shader.pgl"
        texture = "file:///tmp/texture.ptex"
        mesh = "file:///tmp/geometry.pmesh"
        packets, responses = self.lsp_session(
            opened("uv.sample(tex0)", "fusegl", shader),
            opened("uv.noise().colorize(rgb(1.0, 0.5, 0.0))", "fusetex", texture),
            opened("disk().radius(2.0)", "fusemesh", mesh),
            request("textDocument/completion", 2, at("uv.", shader)),
            request("textDocument/completion", 3, at("uv.", texture)),
            request("textDocument/completion", 4, at("disk().", mesh)),
            request("textDocument/hover", 5, at("uv.sam", shader)),
            request("textDocument/hover", 6, at("uv.no", texture)),
            request("textDocument/hover", 7, at("disk().ra", mesh)),
            changed("uv.noise()", 2, shader),
            changed("disk()", 2, texture),
            changed("1.0", 2, mesh),
            notification("textDocument/didClose", {"textDocument": {"uri": shader}}),
            request("textDocument/completion", 8, at("", texture)),
        )
        gl = self.completion_items(responses[2])
        tex = self.completion_items(responses[3])
        geometry = self.completion_items(responses[4])
        self.assertIn("sample", gl)
        self.assertNotIn("noise", gl)
        self.assertIn("noise", tex)
        self.assertNotIn("sample", tex)
        self.assertEqual(geometry, {"radius", "segments", "rings", "uv_radial"})
        for response, signature in [(5, "sample(vec2, texture)"), (6, "noise(vec2)"), (7, "radius(mesh, float)")]:
            self.assertIn(signature, responses[response]["result"]["contents"]["value"])
        diagnostics = [p["params"] for p in packets if p.get("method") == "textDocument/publishDiagnostics"]
        self.assertTrue(all(p["diagnostics"] == [] for p in diagnostics[:3]))
        for packet, identity in zip(diagnostics[3:6], ["fusegl", "fusetex", "fusemesh"]):
            self.assertEqual(packet["diagnostics"][0]["source"], identity)
        self.assertIn("unknown function", diagnostics[3]["diagnostics"][0]["message"])
        self.assertIn("unknown function", diagnostics[4]["diagnostics"][0]["message"])
        self.assertIn("must be mesh", diagnostics[5]["diagnostics"][0]["message"])
        self.assertEqual(diagnostics[6], {"uri": shader, "diagnostics": []})
        self.assertIn("noise", self.completion_items(responses[8]))
        self.assertNotIn("sample", self.completion_items(responses[8]))

    def test_language_id_precedes_extension_and_extension_is_a_fallback(self):
        if not {"gl", "tex"} <= self.capabilities:
            self.skipTest("requires gl and tex")
        uri = "file:///tmp/texture.ptex"
        for identity, included, excluded in [("fusegl", "sample", "noise"), ("plaintext", "noise", "sample"),
                                              (None, "noise", "sample")]:
            with self.subTest(identity=identity):
                _, responses = self.lsp_session(
                    opened("uv.", identity, uri),
                    request("textDocument/completion", 2, at("uv.", uri)),
                )
                items = self.completion_items(responses[2])
                self.assertIn(included, items)
                self.assertNotIn(excluded, items)

    def test_untitled_document_and_reopen_can_select_a_new_capability(self):
        if not {"gl", "tex"} <= self.capabilities:
            self.skipTest("requires gl and tex")
        uri = "untitled:asset"
        _, responses = self.lsp_session(
            opened("uv.", "fusetex", uri),
            request("textDocument/completion", 2, at("uv.", uri)),
            notification("textDocument/didClose", {"textDocument": {"uri": uri}}),
            opened("uv.", "fusegl", uri),
            request("textDocument/completion", 3, at("uv.", uri)),
        )
        self.assertIn("noise", self.completion_items(responses[2]))
        self.assertIn("sample", self.completion_items(responses[3]))
        self.assertNotIn("noise", self.completion_items(responses[3]))


if __name__ == "__main__":
    if len(sys.argv) > 1:
        UnifiedTests.binary = Path(sys.argv[1]).resolve()
        UnifiedTests.capabilities = set(sys.argv[2:])
        del sys.argv[1:]
    unittest.main()
