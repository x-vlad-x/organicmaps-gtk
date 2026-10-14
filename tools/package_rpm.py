#!/usr/bin/env python3
"""Build a Fedora RPM and a rebuildable SRPM from the pinned, complete source tree."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import shutil
import subprocess
import tarfile

from package import ROOT, container_path, project_state
from package_matrix import package_name, srpm_name, stage_path


def full_sources(destination):
    version = (ROOT / "VERSION").read_text().strip()
    commit, dirty = project_state()
    names = subprocess.check_output(["git", "ls-files", "-z"], cwd=ROOT).decode().split("\0")
    names = sorted(filter(None, names))
    if any(".local." in name or name.startswith(("out/", "organicmaps/")) for name in names):
        raise ValueError("Private planning or generated files must not enter the source RPM")
    core = ROOT / "organicmaps"
    core_commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=core, text=True).strip()
    upstream = subprocess.check_output(["git", "ls-files", "--recurse-submodules", "-z"], cwd=core).decode().split("\0")
    metadata = dict(commit=commit, dirty=dirty, core_commit=core_commit,
                    core_qt_version=subprocess.check_output(["bash", "tools/unix/version.sh", "qt_version"], cwd=core, text=True).strip(),
                    core_int_version=subprocess.check_output(["bash", "tools/unix/version.sh", "qt_int_version"], cwd=core, text=True).strip(),
                    files={name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in names})
    prefix = f"Lichen-{version}"
    with tarfile.open(destination, "w:gz", compresslevel=2) as archive:
        for name in names + ["organicmaps/" + n for n in filter(None, upstream)]:
            path = ROOT / name
            if not path.exists() and not path.is_symlink():
                raise ValueError(f"Missing source file: {name}")
            archive.add(path, arcname=f"{prefix}/{name}", recursive=False)
        content = (json.dumps(metadata, indent=2) + "\n").encode()
        member = tarfile.TarInfo(f"{prefix}/SOURCE-REVISION.json")
        member.size, member.mode = len(content), 0o644
        archive.addfile(member, io.BytesIO(content))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--container", required=True)
    parser.add_argument("--arch", choices=("amd64", "arm64"), required=True)
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    version = (ROOT / "VERSION").read_text().strip()
    workspace = ROOT / "out/rpm" / args.arch
    for name in ("RPMS", "SRPMS"):
        if (workspace / name).exists():
            shutil.rmtree(workspace / name)
    sources = workspace / "SOURCES"
    sources.mkdir(parents=True, exist_ok=True)
    full_sources(sources / f"Lichen-{version}-full-sources.tar.gz")
    specs = workspace / "SPECS"
    specs.mkdir(exist_ok=True)
    spec = (ROOT / "packaging/lichen.spec").read_text().replace(
        "@APP_VERSION@", version).replace("@RPM_VERSION@", version.replace("-preview", "~preview"))
    (specs / "lichen.spec").write_text(spec)
    def execute(command):
        subprocess.run(["podman", "run", "--rm", "--network=none", "--userns=keep-id",
                        "-v", f"{ROOT}:/src:z", "-w", "/src",
                        args.container, *map(str, command)], check=True)
    execute(["rpmbuild", "-ba" if args.arch == "amd64" else "-bb",
             "--define", f"_topdir {container_path(workspace)}", "--define", "_buildhost localhost",
             "--define", f"_smp_build_ncpus {args.jobs}", container_path(specs / "lichen.spec")])
    binaries = list((workspace / "RPMS").glob("*/*.rpm"))
    if len(binaries) != 1:
        raise ValueError(f"Expected exactly one binary RPM, got {binaries}")
    output = ROOT / "out/packages" / package_name(f"rpm-{args.arch}", version)
    output.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(binaries[0], output)
    stage = ROOT / stage_path(f"rpm-{args.arch}")
    if stage.exists():
        shutil.rmtree(stage)
    stage.mkdir(parents=True)
    # Extract our own completed package, so CI validates the exact shipped contents.
    execute(["python3", "-c", "import subprocess,sys; p=subprocess.Popen(['rpm2cpio',sys.argv[1]],stdout=subprocess.PIPE); "
             "subprocess.run(['cpio','-idm','--quiet','--no-preserve-owner'],cwd=sys.argv[2],stdin=p.stdout,check=True); "
             "p.stdout.close(); sys.exit(p.wait())", container_path(output), container_path(stage)])
    if args.arch == "amd64":
        source_rpms = list((workspace / "SRPMS").glob("*.src.rpm"))
        if len(source_rpms) != 1:
            raise ValueError("Expected exactly one source RPM")
        shutil.copy2(source_rpms[0], output.parent / srpm_name(version))
    with output.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    output.with_name(output.name + ".sha256").write_text(f"{digest}  {output.name}\n")
    print(f"Created RPM: {output}")


if __name__ == "__main__":
    main()
