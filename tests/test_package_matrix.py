"""Require both architectures in every format and reject mislabelled executables."""
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from package_matrix import TARGETS, package_name, validate_elf


class PackageMatrixTest(unittest.TestCase):
    def test_all_formats_have_both_architectures_and_unique_safe_names(self):
        self.assertEqual(set(TARGETS), {f"{kind}-{arch}" for kind in ("deb", "rpm", "appimage")
                                       for arch in ("amd64", "arm64")})
        names = [package_name(target, "0.1.0-preview5") for target in TARGETS]
        self.assertEqual(len(set(names)), 6)
        for name in names:
            self.assertNotIn("~", name)
            self.assertEqual(Path(name).name, name)

    def test_rejects_wrong_elf_machine_and_non_elf_files(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "binary"
            for arch, machine, wrong in (("amd64", 62, "arm64"), ("arm64", 183, "amd64")):
                header = bytearray(20)
                header[:6] = b"\x7fELF\x02\x01"
                header[18:20] = machine.to_bytes(2, "little")
                path.write_bytes(header)
                validate_elf(path, arch)
                with self.assertRaises(ValueError):
                    validate_elf(path, wrong)
            path.write_text("not an executable")
            with self.assertRaises(ValueError):
                validate_elf(path, "arm64")


if __name__ == "__main__":
    unittest.main()
