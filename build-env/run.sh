#!/usr/bin/env bash
# Runs a command inside the cross-build container with the project mounted at /src.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec podman run --rm --userns=keep-id -v "$HERE":/src:z -w /src localhost/om-cross "$@"
