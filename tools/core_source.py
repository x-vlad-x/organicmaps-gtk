"""The pinned map core (tools/core.env) and its prepared checkout in core/."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "core"
# Build trees the preparation step leaves behind; they are not sources.
_SCRATCH = ("build_skin_generator_tool/", ".venv/")


def pinned():
    """Return the CORE_* settings, e.g. CORE_URL and CORE_COMMIT."""
    values = {}
    for line in (ROOT / "tools/core.env").read_text().splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            key, value = line.split("=", 1)
            values[key] = value
    return values


def commit():
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=CORE, text=True).strip()


def _list(repository, *options):
    output = subprocess.check_output(["git", "ls-files", "-z", *options], cwd=repository)
    return [name for name in output.decode().split("\0") if name]


def tracked_files():
    """Files of the core and all its submodules, relative to core/."""
    return _list(CORE, "--recurse-submodules")


def generated_files():
    """Files tools/prepare-core.sh added: world maps, generated styles, symbols and strings."""
    status = subprocess.check_output(["git", "submodule", "status", "--recursive"], cwd=CORE, text=True)
    repositories = [""] + [line[1:].split()[1] for line in status.splitlines()]
    names = []
    for repository in repositories:
        prefix = f"{repository}/" if repository else ""
        names += [prefix + name for name in _list(CORE / repository, "--others")
                  if not (prefix + name).startswith(_SCRATCH) and "__pycache__/" not in name]
    return sorted(names)
