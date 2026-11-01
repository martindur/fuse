#!/bin/sh
set -eu

usage() {
    cat <<'EOF'
Usage: sh install.sh [--version TAG] [--prefix PATH]

Build and install Fuse commands and man pages from a GitHub release.
Defaults: latest release, prefix $HOME/.local.
Requires: make, cc (C17), ar, libpng, GLFW 3, OpenGL headers, pkg-config, curl, and tar.
EOF
}

fail() {
    printf 'fuse installer: %s\n' "$*" >&2
    exit 1
}

dependency_help() {
    cat >&2 <<'EOF'
Install the build dependencies, then run the installer again:
  Debian/Ubuntu: sudo apt install build-essential pkg-config libpng-dev libglfw3-dev libgl1-mesa-dev curl
  Fedora:       sudo dnf install gcc make binutils pkgconf-pkg-config libpng-devel glfw-devel mesa-libGL-devel curl
  macOS:        xcode-select --install; brew install pkg-config libpng glfw
EOF
}

validate_version() {
    case "$1" in
        ''|*[!a-zA-Z0-9._-]*) fail "invalid release tag: $1" ;;
    esac
    case "$1" in
        v[0-9]*) ;;
        *) fail "release tags must start with v and a digit (for example v0.1.0)" ;;
    esac
}

main() {
    version=
    prefix=${HOME:?HOME must be set}/.local
    while [ "$#" -gt 0 ]; do
        case "$1" in
            --version)
                [ "$#" -ge 2 ] || fail "--version requires a release tag"
                version=$2
                validate_version "$version"
                shift 2
                ;;
            --prefix)
                [ "$#" -ge 2 ] || fail "--prefix requires an absolute path"
                prefix=$2
                shift 2
                ;;
            -h|--help) usage; return ;;
            *) fail "unknown option: $1 (use --help)" ;;
        esac
    done
    case "$prefix" in
        /*) ;;
        *) fail "--prefix must be an absolute path" ;;
    esac

    for tool in make cc ar pkg-config curl tar mktemp install; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            dependency_help
            fail "required command not found: $tool"
        fi
    done
    if ! pkg-config --exists libpng; then
        dependency_help
        fail "libpng development files not found by pkg-config"
    fi
    if ! pkg-config --exists glfw3; then
        dependency_help
        fail "GLFW development files not found by pkg-config"
    fi
    if [ "$(uname -s)" != Darwin ] && ! pkg-config --exists gl; then
        dependency_help
        fail "OpenGL development files not found by pkg-config"
    fi

    repository=https://github.com/martindur/fuse
    if [ -z "$version" ]; then
        release_url=$(curl -fsSL -o /dev/null -w '%{url_effective}' \
            "$repository/releases/latest") || fail "could not find the latest release; check that a release has been published"
        case "$release_url" in
            "$repository/releases/tag/"*) version=${release_url##*/} ;;
            *) fail "could not resolve the latest release tag" ;;
        esac
        validate_version "$version"
    fi

    work_dir=$(mktemp -d "${TMPDIR:-/tmp}/fuse-install.XXXXXXXX")
    trap 'rm -rf -- "$work_dir"' 0
    trap 'exit 1' HUP INT TERM

    printf 'Downloading Fuse %s...\n' "$version"
    curl -fsSL "$repository/archive/refs/tags/$version.tar.gz" \
        -o "$work_dir/source.tar.gz" || fail "could not download release $version"
    mkdir "$work_dir/source"
    tar -xzf "$work_dir/source.tar.gz" --strip-components=1 -C "$work_dir/source"

    printf 'Building Fuse %s...\n' "$version"
    unset CAPABILITIES MAKEFLAGS MFLAGS
    make -C "$work_dir/source" all
    make -C "$work_dir/source" PREFIX="$prefix" install

    printf '\nInstalled Fuse %s to %s/bin\n' "$version" "$prefix"
    case ":${PATH:-}:" in
        *":$prefix/bin:"*) ;;
        *) printf 'Add %s/bin to your PATH to use the installed commands.\n' "$prefix" ;;
    esac
}

main "$@"
