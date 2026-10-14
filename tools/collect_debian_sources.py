#!/usr/bin/env python3
"""Download exact Debian source packages using SHA-256 from signed APT indexes.

Run against the bundled-packages.json generated inside the desktop build image:
  python3 tools/collect_debian_sources.py INVENTORY --container localhost/om-desktop
The temporary container enables deb-src; the host's APT configuration is untouched.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import re
import subprocess
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def parse_uris(output):
    files = []
    for line in output.splitlines():
        if not line.startswith("'"):
            continue
        match = re.fullmatch(r"'([^']+)' (\S+) (\d+) SHA256:([0-9a-f]{64})", line)
        if not match:
            raise ValueError(f"APT did not provide a SHA-256 source record: {line}")
        url, name, size, digest = match.groups()
        if Path(name).name != name or name in {".", ".."}:
            raise ValueError(f"Unsafe source filename: {name}")
        files.append(dict(url=url.replace("http://", "https://", 1), name=name,
                          size=int(size), sha256=digest))
    if not files or not any(f["name"].endswith(".dsc") for f in files):
        raise ValueError("APT did not return a complete source package")
    return files


def index_sources(inventory, destination):
    # Executed only inside the disposable Debian container as its root user.
    source_file = Path("/etc/apt/sources.list.d/debian.sources")
    text = source_file.read_text()
    source_file.write_text(re.sub(r"^Types: deb$", "Types: deb deb-src", text, flags=re.M))
    subprocess.run(["apt-get", "update", "-qq"], check=True)
    sources = sorted({(p["source_package"], p["source_version"]) for p in inventory})
    records, errors = [], []
    for package, version in sources:
        result = subprocess.run(["apt-get", "--print-uris", "--download-only", "--only-source",
                                 "-o", "Acquire::ForceHash=SHA256", "source", f"{package}={version}"],
                                text=True, capture_output=True)
        try:
            if result.returncode:
                raise ValueError(result.stderr.strip())
            records.append(dict(package=package, version=version, files=parse_uris(result.stdout)))
        except ValueError as error:
            errors.append(dict(package=package, version=version, error=str(error)))
        if (len(records) + len(errors)) % 10 == 0:
            print(f"Indexed {len(records) + len(errors)}/{len(sources)} sources", flush=True)
    destination.write_text(json.dumps(dict(sources=records, errors=errors), indent=2) + "\n")
    if errors:
        raise RuntimeError(f"Exact source versions unavailable: {errors}")


def fetch_file(record, directory):
    target = directory / record["name"]
    def valid(path):
        if not path.is_file() or path.stat().st_size != record["size"]:
            return False
        with path.open("rb") as stream:
            return hashlib.file_digest(stream, "sha256").hexdigest() == record["sha256"]
    if valid(target):
        return
    temporary = target.with_name(target.name + ".download")
    with urllib.request.urlopen(record["url"], timeout=90) as response, temporary.open("wb") as output:
        while data := response.read(1024 * 1024):
            output.write(data)
    if not valid(temporary):
        temporary.unlink(missing_ok=True)
        raise RuntimeError(f"Source checksum mismatch: {record['name']}")
    temporary.replace(target)


def verify_dsc(source, directory):
    records = {record["name"]: record for record in source["files"]}
    dsc = next(name for name in records if name.endswith(".dsc"))
    text = (directory / dsc).read_text()
    version = re.search(r"^Version: (.+)$", text, re.M)
    if not version or version.group(1) != source["version"]:
        raise ValueError(f"Wrong source version inside {dsc}")
    checksums = re.search(r"^Checksums-Sha256:\n((?: [^\n]+\n)+)", text, re.M)
    if not checksums:
        raise ValueError(f"Source package has no SHA-256 file list: {dsc}")
    names = set()
    for line in checksums.group(1).splitlines():
        digest, size, name = line.split()
        record = records.get(name)
        if not record or (digest, int(size)) != (record["sha256"], record["size"]):
            raise ValueError(f"Incomplete or conflicting source package: {dsc}: {name}")
        names.add(name)
    if names != set(records) - {dsc}:
        raise ValueError(f"Unexpected files in source package: {dsc}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path, nargs="+")
    parser.add_argument("--container", default="localhost/om-desktop")
    parser.add_argument("--output", type=Path, default=ROOT / "out/corresponding-sources/debian")
    parser.add_argument("--worker", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--reuse-index", action="store_true")
    args = parser.parse_args()
    directory = args.output.resolve()
    directory.mkdir(parents=True, exist_ok=True)
    index = directory / "sources.json"
    if args.worker:
        inventory = [record for path in args.inventory for record in json.loads(path.read_text())]
        index_sources(inventory, index)
        return
    if not args.reuse_index:
        relative = lambda p: str(Path("/src") / p.resolve().relative_to(ROOT))
        subprocess.run(["podman", "run", "--rm", "-v", f"{ROOT}:/src:z", "-w", "/src", args.container,
                        "python3", relative(Path(__file__)), *[relative(p) for p in args.inventory], "--worker",
                        "--output", relative(directory)], check=True)
    metadata = json.loads(index.read_text())
    if metadata["errors"]:
        raise RuntimeError("Source index is incomplete; see sources.json")
    files = {}
    for source in metadata["sources"]:
        for record in source["files"]:
            previous = files.setdefault(record["name"], record)
            if previous["sha256"] != record["sha256"]:
                raise RuntimeError(f"Conflicting source files: {record['name']}")
    with ThreadPoolExecutor(max_workers=4) as pool:
        list(pool.map(lambda f: fetch_file(f, directory), files.values()))
    for source in metadata["sources"]:
        verify_dsc(source, directory)
    (directory / "SHA256SUMS").write_text("".join(
        f"{record['sha256']}  {name}\n" for name, record in sorted(files.items())))
    print(f"Verified {len(files)} files for {len(metadata['sources'])} Debian source packages in {directory}")


if __name__ == "__main__":
    main()
