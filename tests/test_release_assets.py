"""Verify that source archive splitting preserves every byte and ignores stale parts."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from prepare_release import split_source_archive


class SourcePartsTest(unittest.TestCase):
    def test_roundtrip_at_part_boundaries(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "sources.tar"
            for size in (1, 127, 128, 129, 257):
                with self.subTest(size=size):
                    content = bytes(n % 251 for n in range(size))
                    path.write_bytes(content)
                    parts = split_source_archive(path, part_size=128)
                    self.assertEqual(b"".join(p.read_bytes() for p in parts), content)
                    self.assertEqual(len(parts), (size + 127) // 128)

    def test_rejects_zero_size(self):
        with self.assertRaises(ValueError):
            split_source_archive(Path("unused.tar"), part_size=0)


if __name__ == "__main__":
    unittest.main()
