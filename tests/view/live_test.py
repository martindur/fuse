"""Exercise the running preview; run under a display or xvfb-run."""
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest


class LiveViewTests(unittest.TestCase):
    binary = Path("build/fuse").resolve()

    def test_reload_survives_bad_edits_atomic_saves_and_file_deletion(self):
        with tempfile.TemporaryDirectory(prefix="fuse-live-") as directory:
            path = Path(directory) / "live.pgl"
            log = Path(directory) / "viewer.log"
            valid = "rgb(0.0, 1.0, 0.0).vec4(1.0)"
            replacement = "rgb(0.0, 0.0, 1.0).vec4(1.0)"
            path.write_text(valid)
            with log.open("wb") as errors:
                process = subprocess.Popen(
                    [str(self.binary), "view", "--width", "96", "--height", "64", str(path)],
                    stdout=subprocess.PIPE, stderr=errors,
                )

                def wait_for(predicate):
                    deadline = time.monotonic() + 10
                    while time.monotonic() < deadline:
                        text = log.read_text()
                        if predicate(text):
                            return text
                        self.assertIsNone(process.poll(), text)
                        time.sleep(0.02)
                    self.fail("viewer did not react to a source change:\n" + log.read_text())

                try:
                    wait_for(lambda text: text.count(": loaded") == 1)
                    path.write_text(valid)
                    time.sleep(0.45)
                    self.assertEqual(log.read_text().count(": loaded"), 1)
                    path.write_text("// broken edit\nuv.missing()")
                    wait_for(lambda text: ":2:4: semantic error: unknown function" in text)
                    self.assertIsNone(process.poll())
                    staged = path.with_suffix(".new")
                    staged.write_text(replacement)
                    staged.replace(path)
                    wait_for(lambda text: text.count(": loaded") == 2)
                    path.unlink()
                    wait_for(lambda text: "cannot read shader" in text)
                    time.sleep(0.45)
                    self.assertEqual(log.read_text().count("cannot read shader"), 1)
                    path.write_text(replacement)
                    wait_for(lambda text: text.count(": loaded") == 3)
                    self.assertIsNone(process.poll())
                finally:
                    process.terminate()
                    try:
                        output, _ = process.communicate(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        output, _ = process.communicate(timeout=5)
                self.assertEqual(output, b"")


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        LiveViewTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
