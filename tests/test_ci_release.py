"""Check release boundaries: tags, transferred paths and uploaded checksums."""
import io
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import ci_release


class CiReleaseTest(unittest.TestCase):
    def test_tag_must_match_supported_version(self):
        for version in ("0.1.0", "0.1.0-preview4"):
            ci_release.check_version(version, f"refs/tags/v{version}")
            ci_release.check_version(version, "refs/heads/main")
        for version, ref in (("0.1.0", "refs/tags/v0.2.0"), ("../../x", ""),
                             ("0.1.0-preview0", ""), ("0.1.0\n", "")):
            with self.subTest(version=version, ref=ref), self.assertRaises(ValueError):
                ci_release.check_version(version, ref)

    def test_release_manifest_rejects_tampering_duplicates_and_traversal(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            asset = directory / "app.deb"
            asset.write_bytes(b"verified installer")
            line = f"{ci_release.digest(asset)}  app.deb\n"
            sums = directory / "SHA256SUMS"
            sums.write_text(line)
            self.assertEqual(ci_release.release_assets(directory), [asset, sums])
            for content in (line + line, line.replace("app.deb", "../app.deb"), "", "0" * 64 + "  app.deb\n"):
                sums.write_text(content)
                with self.subTest(content=content), self.assertRaises(ValueError):
                    ci_release.release_assets(directory)

    def test_handoff_rejects_outside_paths_and_links(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            for name, kind in (("../VERSION", tarfile.REGTYPE), ("VERSION", tarfile.REGTYPE),
                               ("/out/file", tarfile.REGTYPE), ("out/file", tarfile.SYMTYPE)):
                path = directory / "handoff.tar"
                with tarfile.open(path, "w") as archive:
                    member = tarfile.TarInfo(name)
                    member.type = kind
                    member.linkname = "/tmp/target" if kind == tarfile.SYMTYPE else ""
                    archive.addfile(member)
                with patch.object(ci_release, "ROOT", directory), self.assertRaises(ValueError):
                    ci_release.extract(path)

    def test_handoff_preserves_executable_mode(self):
        with tempfile.TemporaryDirectory() as tmp:
            directory = Path(tmp)
            path = directory / "handoff.tar"
            with tarfile.open(path, "w") as archive:
                member = tarfile.TarInfo("out/packages/app.AppImage")
                member.mode = 0o755
                member.size = 4
                archive.addfile(member, io.BytesIO(b"test"))
            with patch.object(ci_release, "ROOT", directory):
                ci_release.extract(path)
            extracted = directory / "out/packages/app.AppImage"
            self.assertEqual(extracted.read_bytes(), b"test")
            self.assertTrue(extracted.stat().st_mode & 0o111)


if __name__ == "__main__":
    unittest.main()
