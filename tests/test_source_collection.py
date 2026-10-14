"""Reject unauthenticated source listings and incomplete Debian source packages."""
from pathlib import Path
import importlib.util
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("source_collection", Path(__file__).resolve().parents[1] / "tools/collect_debian_sources.py")
collector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(collector)


class SourceCollectionTest(unittest.TestCase):
    def test_rejects_weak_hash_and_traversal(self):
        for name, digest in (("x.dsc", "MD5Sum:abcd"), ("../x.dsc", "SHA256:" + "a" * 64)):
            with self.subTest(name=name):
                with self.assertRaises(ValueError):
                    collector.parse_uris(f"'https://example.org/x' {name} 2 {digest}")

    def test_requires_dsc(self):
        with self.assertRaises(ValueError):
            collector.parse_uris("'https://example.org/x' x.tar.xz 2 SHA256:" + "a" * 64)

    def test_cross_checks_all_dsc_files_and_version(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            dsc = directory / "x.dsc"
            dsc.write_text("Version: 1\nChecksums-Sha256:\n " + "a" * 64 + " 2 x.tar.xz\n")
            source = dict(version="1", files=[dict(name="x.dsc"), dict(name="x.tar.xz", size=2, sha256="a" * 64)])
            collector.verify_dsc(source, directory)
            source["files"][1]["sha256"] = "b" * 64
            with self.assertRaises(ValueError):
                collector.verify_dsc(source, directory)
            source["files"].pop()
            with self.assertRaises(ValueError):
                collector.verify_dsc(source, directory)
            source["version"] = "2"
            with self.assertRaises(ValueError):
                collector.verify_dsc(source, directory)


if __name__ == "__main__":
    unittest.main()
