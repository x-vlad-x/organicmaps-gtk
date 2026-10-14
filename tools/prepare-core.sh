#!/usr/bin/env bash
# Fetch the pinned map core (tools/core.env) into core/ and prepare it for building:
# submodules, the Lichen build patches, the world maps, and the files the core generates with
# its configure.sh (drawing rules, symbols, strings, search categories, Boost headers).
# Generation runs once per core revision and patch set, in a container without network access;
# its output does not depend on the target architecture.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck source=core.env
source "$ROOT/tools/core.env"
CORE="$ROOT/core"
IMAGE=localhost/lichen-core

for tool in podman git python3; do
  command -v "$tool" >/dev/null || { echo "Required tool is missing: $tool" >&2; exit 1; }
done
if [[ ! -d "$CORE/.git" ]]; then
  git init -q "$CORE"
  git -C "$CORE" remote add origin "$CORE_URL"
  git -C "$CORE" fetch --depth 1 origin "$CORE_COMMIT"
  git -C "$CORE" checkout -q FETCH_HEAD
fi
if [[ "$(git -C "$CORE" rev-parse HEAD)" != "$CORE_COMMIT" ]]; then
  echo "Expected $CORE_NAME $CORE_COMMIT in core/. Existing sources were left untouched." >&2
  exit 1
fi
git -C "$CORE" submodule update --init --recursive --depth 1
for patch in "$ROOT"/patches/*.patch; do
  if git -C "$CORE" apply --reverse --check "$patch" 2>/dev/null; then
    continue
  fi
  git -C "$CORE" apply --check "$patch"
  git -C "$CORE" apply "$patch"
done
python3 "$ROOT/tools/fetch_world_maps.py" "$CORE/data"

stamp="$CORE/.lichen-prepared"
state="$CORE_COMMIT $(cat "$ROOT"/patches/*.patch "$ROOT/build-env/Containerfile.core" | sha256sum | cut -c1-16)"
if [[ "$(cat "$stamp" 2>/dev/null)" != "$state" ]]; then
  podman build -t "$IMAGE" -f "$ROOT/build-env/Containerfile.core" "$ROOT/build-env"
  # The world maps are already in place; configure.sh only checks the submodules here.
  podman run --rm --network=none --userns=keep-id -v "$CORE:/core:z" -w /core \
    -e SKIP_PYTHON_VENV=1 -e SKIP_MAP_DOWNLOAD=1 -e GIT_CONFIG_COUNT=1 \
    -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0='*' "$IMAGE" ./configure.sh
  # The symbol generator's build tree is only needed during generation.
  rm -rf "$CORE/build_skin_generator_tool"
  echo "$state" > "$stamp"
fi
