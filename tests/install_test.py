"""Exercise the source installer without network access or a published release."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class InstallTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.archive_directory = tempfile.TemporaryDirectory(prefix="fuse-source-")
        cls.archive = Path(cls.archive_directory.name) / "source.tar.gz"
        with tarfile.open(cls.archive, "w:gz") as archive:
            for name in ("Makefile", "src", "include", "docs"):
                archive.add(ROOT / name, arcname=f"fuse-0.1.0/{name}")

    @classmethod
    def tearDownClass(cls):
        cls.archive_directory.cleanup()

    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="fuse-installer-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.home = self.root / "home"
        self.home.mkdir()
        self.temporary = self.root / "tmp"
        self.temporary.mkdir()
        self.log = self.root / "requests.jsonl"
        self.env = {
            **os.environ,
            "HOME": str(self.home),
            "TMPDIR": str(self.temporary),
            "PATH": f"{self.bin}{os.pathsep}{os.environ['PATH']}",
            "FUSE_TEST_ARCHIVE": str(self.archive),
            "FUSE_TEST_LOG": str(self.log),
        }
        curl = self.bin / "curl"
        curl.write_text(f"#!{sys.executable}\n" + '''
import json
import os
from pathlib import Path
import shutil
import sys

arguments = sys.argv[1:]
url = next(argument for argument in arguments if argument.startswith("https://"))
with open(os.environ["FUSE_TEST_LOG"], "a") as log:
    log.write(json.dumps(url) + "\\n")
if os.environ.get("FUSE_TEST_DOWNLOAD_FAILURE"):
    sys.exit(22)
if url.endswith("/releases/latest"):
    print("https://github.com/martindur/fuse/releases/tag/v0.1.0", end="")
else:
    destination = arguments[arguments.index("-o") + 1]
    shutil.copyfile(os.environ["FUSE_TEST_ARCHIVE"], destination)
''')
        curl.chmod(0o755)

    def run_installer(self, *arguments):
        return subprocess.run(
            ["sh", "-s", "--", *arguments], input=(ROOT / "install.sh").read_text(),
            env=self.env, capture_output=True, text=True, timeout=120,
        )

    def assert_installation(self, prefix, result):
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        binary = prefix / "bin/fuse"
        self.assertTrue(os.access(binary, os.X_OK))
        self.assertTrue((prefix / "share/man/man1/fuse.1").is_file())
        for command in ("gl", "tex", "mesh", "view", "lsp"):
            subprocess.run([str(binary), command, "--help"], check=True, capture_output=True)
        self.assertEqual({p.name for p in (prefix / "bin").iterdir()}, {"fuse"})
        self.assertFalse((prefix / "include").exists())
        self.assertFalse((prefix / "lib").exists())
        self.assertEqual(list(self.temporary.iterdir()), [])
        self.assertIn("Installed Fuse v0.1.0", result.stdout)
        self.assertIn(f"Add {prefix}/bin to your PATH", result.stdout)
        self.assertEqual(list(self.home.glob(".*rc")), [])

    def requests(self):
        return [json.loads(line) for line in self.log.read_text().splitlines()]

    def test_latest_release_and_default_prefix(self):
        result = self.run_installer()
        self.assert_installation(self.home / ".local", result)
        self.assertEqual(self.requests(), [
            "https://github.com/martindur/fuse/releases/latest",
            "https://github.com/martindur/fuse/archive/refs/tags/v0.1.0.tar.gz",
        ])

    def test_pinned_release_and_prefix_with_spaces(self):
        self.env["CAPABILITIES"] = "gl"
        prefix = self.root / "custom prefix"
        result = self.run_installer("--version", "v0.1.0", "--prefix", str(prefix))
        self.assert_installation(prefix, result)
        self.assertEqual(self.requests(), [
            "https://github.com/martindur/fuse/archive/refs/tags/v0.1.0.tar.gz",
        ])

    def test_help_and_invalid_arguments_do_not_download(self):
        result = self.run_installer("--help")
        self.assertEqual(result.returncode, 0)
        self.assertIn("Usage:", result.stdout)
        for arguments in (
            ("--unknown",), ("--version",), ("--version", "main"),
            ("--version", "v1/other"), ("--prefix",), ("--prefix", "relative"),
        ):
            with self.subTest(arguments=arguments):
                result = self.run_installer(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("fuse installer:", result.stderr)
        self.assertFalse(self.log.exists())

    def test_download_failure_leaves_no_installation(self):
        self.env["FUSE_TEST_DOWNLOAD_FAILURE"] = "1"
        for arguments in ((), ("--version", "v0.1.0")):
            with self.subTest(arguments=arguments):
                result = self.run_installer(*arguments)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn("could not", result.stderr)
                self.assertFalse((self.home / ".local").exists())
                self.assertEqual(list(self.temporary.iterdir()), [])

    def test_missing_libpng_explains_dependencies_before_download(self):
        pkg_config = self.bin / "pkg-config"
        pkg_config.write_text("#!/bin/sh\nexit 1\n")
        pkg_config.chmod(0o755)
        result = self.run_installer()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("libpng development files not found", result.stderr)
        self.assertIn("apt install", result.stderr)
        self.assertFalse(self.log.exists())

    def test_missing_windowing_dependency_is_reported_before_download(self):
        pkg_config = self.bin / "pkg-config"
        pkg_config.write_text('#!/bin/sh\n[ "$2" != glfw3 ]\n')
        pkg_config.chmod(0o755)
        result = self.run_installer()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("GLFW development files not found", result.stderr)
        self.assertFalse(self.log.exists())


if __name__ == "__main__":
    unittest.main()
