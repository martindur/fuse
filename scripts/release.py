"""Release preparation and publication for Fuse."""
import argparse
from datetime import date
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = "martindur/fuse"
CATEGORIES = ("Added", "Changed", "Deprecated", "Removed", "Fixed", "Security")
VERSION_PATTERN = r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)"


def parse_version(value):
    match = re.fullmatch(VERSION_PATTERN, value)
    if not match:
        raise ValueError("version must be MAJOR.MINOR.PATCH, for example 0.1.0")
    return tuple(map(int, match.groups()))


def run(*arguments):
    return subprocess.run(arguments, cwd=ROOT, check=True, text=True, capture_output=True).stdout.strip()


def fragments(root):
    result = []
    for path in sorted((root / "changes").glob("*.md")):
        if path.name == "README.md":
            continue
        match = re.fullmatch(r"[a-z0-9][a-z0-9-]*\.([a-z]+)\.md", path.name)
        if not match or match[1].capitalize() not in CATEGORIES:
            raise ValueError(f"invalid changelog fragment name: {path.name}")
        text = path.read_text().strip()
        if not text or "\n" in text:
            raise ValueError(f"{path.name}: use one nonempty paragraph on a single line")
        result.append((path, match[1].capitalize(), text))
    return result


def check(root):
    version = (root / "VERSION").read_text().strip()
    parse_version(version)
    metadata = json.loads((root / "tree-sitter-fuse/tree-sitter.json").read_text())
    if metadata["metadata"]["version"] != version:
        raise ValueError("VERSION and Tree-sitter metadata disagree")
    history = re.findall(r"^## \[([^\]]+)\]", (root / "CHANGELOG.md").read_text(), re.MULTILINE)
    versions = [parse_version(entry) for entry in history]
    if versions and (versions[0] > parse_version(version)
                     or any(newer <= older for newer, older in zip(versions, versions[1:]))):
        raise ValueError("changelog versions must descend without exceeding VERSION")
    fragments(root)
    return version


def release_notes(root, version):
    parse_version(version)
    changelog = (root / "CHANGELOG.md").read_text()
    match = re.search(
        rf"^## \[{re.escape(version)}\] - \d{{4}}-\d{{2}}-\d{{2}}\n(.*?)(?=^## |\Z)",
        changelog, re.MULTILINE | re.DOTALL,
    )
    if not match or not match[1].strip():
        raise ValueError(f"CHANGELOG.md has no release notes for {version}")
    return match[1].strip() + "\n"


def prepare(root, version, today=None):
    requested = parse_version(version)
    current = parse_version(check(root))
    changelog_path = root / "CHANGELOG.md"
    changelog = changelog_path.read_text()
    if requested < current:
        raise ValueError("release version must not go backwards")
    if re.search(rf"^## \[{re.escape(version)}\]", changelog, re.MULTILINE):
        raise ValueError(f"changelog already contains {version}")
    pending = fragments(root)
    if not pending:
        raise ValueError("no pending changelog fragments")
    sections = [f"## [{version}] - {today or date.today().isoformat()}"]
    for category in CATEGORIES:
        entries = [f"- {text}" for _, kind, text in pending if kind == category]
        if entries:
            sections.append(f"### {category}\n\n" + "\n".join(entries))
    entry = "\n\n".join(sections) + "\n\n"
    heading, separator, history = changelog.partition("\n\n")
    if heading != "# Changelog" or not separator:
        raise ValueError("CHANGELOG.md must start with '# Changelog' and a blank line")
    metadata_path = root / "tree-sitter-fuse/tree-sitter.json"
    metadata = json.loads(metadata_path.read_text())
    metadata["metadata"]["version"] = version
    (root / "VERSION").write_text(version + "\n")
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n")
    changelog_path.write_text(heading + "\n\n" + entry + history)
    for path, _, _ in pending:
        path.unlink()
    return release_notes(root, version)


def verify_release(root, version):
    if check(root) != version:
        raise ValueError("release branch version does not match VERSION")
    if fragments(root):
        raise ValueError("release has pending fragments; include them in the release PR before merging")
    changelog = (root / "CHANGELOG.md").read_text()
    if not changelog.startswith(f"# Changelog\n\n## [{version}] - "):
        raise ValueError("release must be the newest CHANGELOG.md entry")
    return release_notes(root, version)


def open_pr(version):
    parse_version(version)
    if run("git", "branch", "--show-current") != "main":
        raise ValueError("prepare releases from main")
    if run("git", "status", "--porcelain"):
        raise ValueError("release preparation requires a clean checkout")
    run("git", "fetch", "origin", "main", "--tags")
    if run("git", "rev-parse", "HEAD") != run("git", "rev-parse", "origin/main"):
        raise ValueError("main must match origin/main; pull or push your changes first")
    tag = f"v{version}"
    branch = f"release/{tag}"
    if run("git", "tag", "--list", tag):
        raise ValueError(f"tag {tag} already exists")
    if run("git", "branch", "--list", branch) or run("git", "ls-remote", "--heads", "origin", branch):
        raise ValueError(f"branch {branch} already exists")
    prs = json.loads(run("gh", "pr", "list", "--repo", REPOSITORY, "--base", "main",
                         "--state", "open", "--json", "headRefName", "--limit", "1000"))
    if any(pr["headRefName"].startswith("release/v") for pr in prs):
        raise ValueError("a release PR is already open; merge or close it first")
    run("git", "var", "GIT_AUTHOR_IDENT")
    run("git", "var", "GIT_COMMITTER_IDENT")
    notes = prepare(ROOT, version)
    run("git", "switch", "-c", branch)
    run("git", "add", "--", "VERSION", "CHANGELOG.md", "tree-sitter-fuse/tree-sitter.json", "changes")
    run("git", "commit", "-m", f"Prepare release {tag}")
    run("git", "push", "origin", branch)
    with tempfile.TemporaryDirectory(prefix="fuse-release-") as directory:
        body = Path(directory) / "body.md"
        body.write_text(f"Release Fuse {tag}. Merging this PR runs checks, then tags and publishes the merged commit.\n\n" + notes)
        url = run("gh", "pr", "create", "--repo", REPOSITORY, "--base", "main", "--head", branch,
                  "--title", f"Release {tag}", "--body-file", str(body))
    print(url)


def publish(version):
    notes = verify_release(ROOT, version)
    tag = f"v{version}"
    commit = run("git", "rev-parse", "HEAD")
    refs = run("git", "ls-remote", "--tags", "origin", f"refs/tags/{tag}", f"refs/tags/{tag}^{{}}")
    targets = dict(line.split()[::-1] for line in refs.splitlines())
    existing = targets.get(f"refs/tags/{tag}^{{}}", targets.get(f"refs/tags/{tag}"))
    if existing and existing != commit:
        raise ValueError(f"tag {tag} already points to another commit")
    releases = json.loads(run("gh", "release", "list", "--repo", REPOSITORY,
                              "--limit", "1000", "--json", "tagName,isDraft"))
    for release in releases:
        if release["tagName"] == tag:
            if release["isDraft"]:
                raise ValueError(f"{tag} has an existing draft release; resolve it before retrying")
            print(f"Release {tag} already published")
            return
    for release in releases:
        published = release["tagName"]
        if not release["isDraft"] and re.fullmatch("v" + VERSION_PATTERN, published):
            if parse_version(published[1:]) > parse_version(version):
                raise ValueError(f"newer release {published} already exists")
    if not existing:
        run("git", "tag", tag, commit)
        run("git", "push", "origin", f"refs/tags/{tag}")
    with tempfile.TemporaryDirectory(prefix="fuse-release-") as directory:
        path = Path(directory) / "notes.md"
        path.write_text(notes)
        print(run("gh", "release", "create", tag, "--repo", REPOSITORY, "--verify-tag",
                  "--title", f"Fuse {tag}", "--notes-file", str(path), "--latest"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("check")
    for command in ("open-pr", "prepare", "verify", "publish"):
        subparser = commands.add_parser(command)
        subparser.add_argument("version", nargs="?", default=os.environ.get("FUSE_RELEASE_VERSION", ""))
    arguments = parser.parse_args()
    try:
        if arguments.command == "check":
            check(ROOT)
        elif arguments.command == "open-pr":
            open_pr(arguments.version)
        elif arguments.command == "prepare":
            print(prepare(ROOT, arguments.version))
        elif arguments.command == "verify":
            print(verify_release(ROOT, arguments.version))
        elif arguments.command == "publish":
            publish(arguments.version)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        detail = error.stderr if isinstance(error, subprocess.CalledProcessError) else str(error)
        parser.exit(1, f"release: {detail}\n")


if __name__ == "__main__":
    main()
