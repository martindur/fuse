"""Release assembly and publication contracts, without GitHub writes."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

from scripts import release


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="fuse-release-test-")
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "changes").mkdir()
        (self.root / "changes/README.md").write_text("Fragment instructions\n")
        (self.root / "VERSION").write_text("0.1.0\n")
        (self.root / "CHANGELOG.md").write_text("# Changelog\n\n")
        (self.root / "tree-sitter-fuse").mkdir()
        self.metadata = self.root / "tree-sitter-fuse/tree-sitter.json"
        self.metadata.write_text(json.dumps({"metadata": {"version": "0.1.0", "license": "MIT"}}))

    def fragment(self, name="comments.added.md", text="Support line comments."):
        path = self.root / "changes" / name
        path.write_text(text + "\n")
        return path

    def prepare(self, version="0.1.0"):
        self.fragment()
        return release.prepare(self.root, version, today="2026-10-31")

    def test_first_release_updates_metadata_and_consumes_only_fragments(self):
        self.fragment("preview.fixed.md", "Fix preview selection.")
        notes = self.prepare()
        self.assertIn("### Added\n\n- Support line comments.", notes)
        self.assertIn("### Fixed\n\n- Fix preview selection.", notes)
        self.assertTrue((self.root / "CHANGELOG.md").read_text().startswith(
            "# Changelog\n\n## [0.1.0] - 2026-10-31\n"))
        self.assertEqual(release.verify_release(self.root, "0.1.0"), notes)
        self.assertEqual(list((self.root / "changes").iterdir()), [self.root / "changes/README.md"])
        self.assertEqual(json.loads(self.metadata.read_text())["metadata"]["license"], "MIT")

    def test_next_release_preserves_history_and_bumps_both_versions(self):
        first = self.prepare()
        self.fragment("formatter.added.md", "Format Fuse programs.")
        second = release.prepare(self.root, "0.2.0", today="2026-11-01")
        self.assertEqual(release.check(self.root), "0.2.0")
        self.assertIn("Format Fuse programs.", second)
        self.assertNotIn("Support line comments.", second)
        self.assertEqual(release.release_notes(self.root, "0.1.0"), first)
        self.assertIn("## [0.2.0] - 2026-11-01", (self.root / "CHANGELOG.md").read_text())

    def test_rejects_duplicate_or_older_version_before_modifying_files(self):
        self.prepare()
        path = self.fragment("fix.fixed.md", "Fix a bug.")
        original = (self.root / "CHANGELOG.md").read_text()
        for version in ("0.1.0", "0.0.9"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                release.prepare(self.root, version)
            self.assertTrue(path.exists())
            self.assertEqual((self.root / "CHANGELOG.md").read_text(), original)

    def test_invalid_versions_are_rejected(self):
        for version in ("", "v0.1.0", "01.2.3", "1.2", "1.2.3-rc.1", "1.2.3\n", "$(id)"):
            with self.subTest(version=version), self.assertRaises(ValueError):
                release.parse_version(version)

    def test_invalid_or_empty_fragments_fail_before_mutation(self):
        for name, text in (("wrong.md", "A change"), ("thing.unknown.md", "A change"),
                           ("thing.fixed.md", ""), ("thing.fixed.md", "two\nlines")):
            path = self.fragment(name, text)
            with self.subTest(name=name, text=text), self.assertRaises(ValueError):
                release.prepare(self.root, "0.2.0")
            self.assertEqual((self.root / "VERSION").read_text(), "0.1.0\n")
            path.unlink()

    def test_empty_release_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "no pending"):
            release.prepare(self.root, "0.1.0")

    def test_version_metadata_must_agree(self):
        (self.root / "VERSION").write_text("0.2.0\n")
        with self.assertRaisesRegex(ValueError, "disagree"):
            release.check(self.root)

    def test_release_verification_rejects_unconsumed_fragments_and_wrong_version(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError, "does not match"):
            release.verify_release(self.root, "0.2.0")
        self.fragment("new-feature.added.md", "Add another feature.")
        with self.assertRaisesRegex(ValueError, "pending fragments"):
            release.verify_release(self.root, "0.1.0")

    def test_publication_tags_checked_commit_and_uses_curated_notes(self):
        notes = self.prepare()
        calls = []

        def command(*arguments):
            calls.append(arguments)
            if arguments[:2] == ("git", "rev-parse"):
                return "a" * 40
            if arguments[:3] == ("gh", "release", "list"):
                return "[]"
            if arguments[:3] == ("gh", "release", "create"):
                self.assertEqual(Path(arguments[arguments.index("--notes-file") + 1]).read_text(), notes)
                return "release-url"
            return ""

        with patch.object(release, "ROOT", self.root), patch.object(release, "run", side_effect=command):
            release.publish("0.1.0")
        self.assertIn(("git", "tag", "v0.1.0", "a" * 40), calls)
        self.assertIn(("git", "push", "origin", "refs/tags/v0.1.0"), calls)
        creation = next(call for call in calls if call[:3] == ("gh", "release", "create"))
        self.assertIn("--verify-tag", creation)

    def test_existing_tag_on_another_commit_is_never_overwritten(self):
        self.prepare()
        with patch.object(release, "ROOT", self.root), patch.object(release, "run") as command:
            command.side_effect = ["a" * 40, "b" * 40 + "\trefs/tags/v0.1.0"]
            with self.assertRaisesRegex(ValueError, "another commit"):
                release.publish("0.1.0")
            self.assertEqual(command.call_count, 2)

    def test_retry_after_tag_push_does_not_retag(self):
        self.prepare()
        calls = []

        def command(*arguments):
            calls.append(arguments)
            if arguments[:2] == ("git", "rev-parse"):
                return "a" * 40
            if arguments[:2] == ("git", "ls-remote"):
                return "a" * 40 + "\trefs/tags/v0.1.0"
            if arguments[:3] == ("gh", "release", "list"):
                return "[]"
            return ""

        with patch.object(release, "ROOT", self.root), patch.object(release, "run", side_effect=command):
            release.publish("0.1.0")
        self.assertFalse(any(call[:2] in (("git", "push"), ("git", "tag")) for call in calls))
        self.assertTrue(any(call[:3] == ("gh", "release", "create") for call in calls))

    def test_published_release_retry_does_not_publish_again(self):
        self.prepare()
        with patch.object(release, "ROOT", self.root), patch.object(release, "run") as command:
            command.side_effect = ["a" * 40, "a" * 40 + "\trefs/tags/v0.1.0",
                                   '[{"tagName":"v0.1.0","isDraft":false}]']
            release.publish("0.1.0")
            self.assertEqual(command.call_count, 3)

    def test_preparation_requires_main_and_a_clean_checkout(self):
        for responses, message in ((["feature/comments"], "from main"),
                                   (["main", " M VERSION"], "clean checkout")):
            with patch.object(release, "run", side_effect=responses) as command:
                with self.subTest(message=message), self.assertRaisesRegex(ValueError, message):
                    release.open_pr("0.2.0")
                self.assertEqual(command.call_count, len(responses))

    def test_preparation_rejects_main_that_differs_from_remote(self):
        with patch.object(release, "run") as command:
            command.side_effect = ["main", "", "", "local-commit", "remote-commit"]
            with self.assertRaisesRegex(ValueError, "must match origin/main"):
                release.open_pr("0.2.0")
            self.assertEqual(command.call_count, 5)

    def test_prepare_merge_and_publish_with_real_local_git(self):
        self.fragment()
        remote_directory = tempfile.TemporaryDirectory(prefix="fuse-release-remote-")
        self.addCleanup(remote_directory.cleanup)
        original_run = release.run
        github_calls = []
        published_notes = []

        def command(*arguments):
            if arguments[0] != "gh":
                return original_run(*arguments)
            github_calls.append(arguments)
            if arguments[1:3] in (("pr", "list"), ("release", "list")):
                return "[]"
            if arguments[1:3] == ("release", "create"):
                published_notes.append(Path(arguments[arguments.index("--notes-file") + 1]).read_text())
            return "https://github.com/martindur/fuse/example"

        with patch.object(release, "ROOT", self.root), patch.object(release, "run", side_effect=command):
            command("git", "init", "--initial-branch=main")
            command("git", "config", "user.name", "Release test")
            command("git", "config", "user.email", "release@example.invalid")
            command("git", "config", "commit.gpgsign", "false")
            command("git", "init", "--bare", remote_directory.name)
            command("git", "remote", "add", "origin", remote_directory.name)
            command("git", "add", ".")
            command("git", "commit", "-m", "Initial development")
            command("git", "push", "origin", "main")
            release.open_pr("0.2.0")
            self.assertEqual(command("git", "branch", "--show-current"), "release/v0.2.0")
            self.assertEqual(command("git", "status", "--porcelain"), "")
            self.assertEqual(command("git", "log", "-1", "--format=%an"), "Release test")
            self.assertFalse(any(call[1] == "workflow" for call in github_calls))
            self.assertTrue(any(call[1:3] == ("pr", "create") for call in github_calls))
            self.assertEqual(command("git", "tag", "--list"), "")
            self.assertEqual(published_notes, [])
            command("git", "switch", "main")
            command("git", "merge", "--no-ff", "release/v0.2.0", "-m", "Merge release PR")
            merged_commit = command("git", "rev-parse", "HEAD")
            command("git", "push", "origin", "main")
            release.publish("0.2.0")
            remote_tag = command("git", "ls-remote", "origin", "refs/tags/v0.2.0").split()[0]
            self.assertEqual(remote_tag, merged_commit)
            self.assertEqual(published_notes, [release.release_notes(self.root, "0.2.0")])

    def test_newer_published_version_prevents_tagging_an_older_release(self):
        self.prepare()
        with patch.object(release, "ROOT", self.root), patch.object(release, "run") as command:
            command.side_effect = ["a" * 40, "", '[{"tagName":"v0.2.0","isDraft":false}]']
            with self.assertRaisesRegex(ValueError, "newer release"):
                release.publish("0.1.0")
            self.assertEqual(command.call_count, 3)

    def test_make_requires_explicit_version_without_network_access(self):
        result = subprocess.run(
            ["make", "release"], cwd=release.ROOT, capture_output=True, text=True, timeout=10,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("version must be", result.stderr)


if __name__ == "__main__":
    unittest.main()
