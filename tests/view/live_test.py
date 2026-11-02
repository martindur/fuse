"""Exercise the running preview; run under a display or xvfb-run."""
from contextlib import contextmanager
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest


class LiveViewTests(unittest.TestCase):
    binary = Path("build/fuse").resolve()

    @contextmanager
    def viewer(self, log, *arguments):
        with log.open("wb") as errors:
            process = subprocess.Popen(
                [str(self.binary), "view", "--width", "96", "--height", "64", *map(str, arguments)],
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
                yield process, wait_for
            finally:
                process.terminate()
                try:
                    output, _ = process.communicate(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    output, _ = process.communicate(timeout=5)
                self.assertEqual(output, b"")

    def test_texture_reload_is_independent_and_recovers_from_bad_edits(self):
        with tempfile.TemporaryDirectory(prefix="fuse-textures-") as directory:
            directory = Path(directory)
            shader = directory / "shader.pgl"
            tex0 = directory / "first.ptex"
            tex1 = directory / "second.ptex"
            log = directory / "viewer.log"
            shader.write_text("uv.sample(tex0).multiply(uv.sample(tex1))")
            tex0.write_text("vec4(uv, 0.0, 1.0)")
            tex1.write_text("vec4(1.0, 1.0, 1.0, 1.0)")

            def counts():
                text = log.read_text()
                return tuple(text.count(f"{path}: loaded") for path in (shader, tex0, tex1))

            with self.viewer(log, shader, "--tex0", tex0, "--tex1", tex1,
                             "--texture-width", "8", "--texture-height", "4", "--seed", "0") as (process, wait):
                wait(lambda text: counts() == (1, 1, 1))
                tex0.write_text(tex0.read_text())
                time.sleep(0.45)
                self.assertEqual(counts(), (1, 1, 1))
                tex0.write_text("uv.missing()")
                wait(lambda text: f"tex0: {tex0}:1:4: semantic error:" in text)
                self.assertEqual(counts(), (1, 1, 1))
                shader.write_text("// shader still reloads with the last good texture\nuv.sample(tex0)")
                wait(lambda text: counts() == (2, 1, 1))
                replacement = tex0.with_suffix(".new")
                replacement.write_text("vec4(uv, 1.0, 1.0)")
                replacement.replace(tex0)
                wait(lambda text: counts() == (2, 2, 1))
                tex1.unlink()
                wait(lambda text: f"tex1: {tex1}: cannot read source" in text)
                time.sleep(0.45)
                self.assertEqual(log.read_text().count("cannot read source"), 1)
                tex1.write_text("vec4(1.0, 1.0, 1.0, 1.0)")
                wait(lambda text: counts() == (2, 2, 2))
                self.assertIsNone(process.poll())

    def test_initial_texture_error_recovers_without_editing_shader(self):
        with tempfile.TemporaryDirectory(prefix="fuse-textures-") as directory:
            directory = Path(directory)
            shader = directory / "shader.pgl"
            texture = directory / "texture.ptex"
            log = directory / "viewer.log"
            shader.write_text("uv.sample(tex3)")
            texture.write_text("uv.missing()")
            with self.viewer(log, shader, "--tex3", texture,
                             "--texture-width", "8", "--texture-height", "4") as (process, wait):
                wait(lambda text: "shader requires a loaded tex3" in text)
                self.assertNotIn(": loaded", log.read_text())
                texture.write_text("vec4(uv, 0.0, 1.0)")
                wait(lambda text: f"{shader}: loaded" in text)
                self.assertEqual(log.read_text().count(": loaded"), 2)
                self.assertIsNone(process.poll())

    def test_standalone_texture_show_and_recovery(self):
        with tempfile.TemporaryDirectory(prefix="fuse-texture-view-") as directory:
            directory = Path(directory)
            texture = directory / "texture.ptex"
            log = directory / "viewer.log"
            texture.write_text("uv.noise().show().missing()")
            with self.viewer(log, texture, "--texture-width", "8", "--texture-height", "4") as (process, wait):
                wait(lambda text: f"texture: {texture}: loaded" in text)
                self.assertNotIn("error", log.read_text())
                texture.write_text("uv.missing().show()")
                wait(lambda text: "semantic error: unknown function" in text)
                self.assertEqual(log.read_text().count(": loaded"), 1)
                texture.unlink()
                wait(lambda text: "cannot read source" in text)
                texture.write_text("uv.noise().show().missing()")
                wait(lambda text: text.count(": loaded") == 2)
                self.assertIsNone(process.poll())

    def test_standalone_texture_explicit_line_overrides_show(self):
        with tempfile.TemporaryDirectory(prefix="fuse-texture-view-") as directory:
            directory = Path(directory)
            texture = directory / "texture.ptex"
            log = directory / "viewer.log"
            texture.write_text("uv\n.missing()\n.show()")
            with self.viewer(log, texture, "--preview-line", "1",
                             "--texture-width", "8", "--texture-height", "4") as (process, wait):
                wait(lambda text: f"texture: {texture}: loaded" in text)
                self.assertNotIn("error", log.read_text())
                self.assertIsNone(process.poll())

    def test_reload_survives_bad_edits_atomic_saves_and_file_deletion(self):
        with tempfile.TemporaryDirectory(prefix="fuse-live-") as directory:
            path = Path(directory) / "live.pgl"
            log = Path(directory) / "viewer.log"
            valid = "rgb(0.0, 1.0, 0.0).vec4(1.0)"
            replacement = "rgb(0.0, 0.0, 1.0).vec4(1.0)"
            path.write_text(valid)
            with self.viewer(log, path) as (process, wait):
                wait(lambda text: text.count(": loaded") == 1)
                path.write_text(valid)
                time.sleep(0.45)
                self.assertEqual(log.read_text().count(": loaded"), 1)
                path.write_text("// broken edit\nuv.missing()")
                wait(lambda text: ":2:4: semantic error: unknown function" in text)
                self.assertIsNone(process.poll())
                staged = path.with_suffix(".new")
                staged.write_text(replacement)
                staged.replace(path)
                wait(lambda text: text.count(": loaded") == 2)
                path.unlink()
                wait(lambda text: "cannot read source" in text)
                time.sleep(0.45)
                self.assertEqual(log.read_text().count("cannot read source"), 1)
                path.write_text(replacement)
                wait(lambda text: text.count(": loaded") == 3)
                self.assertIsNone(process.poll())


if __name__ == "__main__":
    if len(sys.argv) > 1 and not sys.argv[1].startswith("-"):
        LiveViewTests.binary = Path(sys.argv.pop(1)).resolve()
    unittest.main()
