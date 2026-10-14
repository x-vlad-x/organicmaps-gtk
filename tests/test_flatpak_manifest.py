"""Keep the Flatpak manifest, the core pin and the AppStream release in step."""
import json
from pathlib import Path
import shutil
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import flatpak_manifest

ROOT = Path(__file__).resolve().parents[1]
FILES = ("VERSION", "tools/build-packages.sh", "packaging/flatpak/io.github.x_vlad_x.OrganicMapsGtk.json",
         "packaging/io.github.x_vlad_x.OrganicMapsGtk.metainfo.xml")


class FlatpakManifestTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        for name in FILES:
            (self.root / name).parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, self.root / name)

    def tearDown(self):
        self.tmp.cleanup()

    def edit(self, name, old, new):
        path = self.root / name
        text = path.read_text()
        self.assertIn(old, text)
        path.write_text(text.replace(old, new, 1))

    def test_repository_files_are_consistent(self):
        flatpak_manifest.check(self.root)

    def test_core_versions_follow_the_pinned_tag(self):
        self.assertEqual(flatpak_manifest.core_versions("2026.09.29-32-android", "b20d1417eef1e3c6"),
                         ("2026.09.29-32-b20d1417-Linux", "260929"))
        with self.assertRaises(ValueError):
            flatpak_manifest.core_versions("v2026.09.29", "b20d1417")

    def test_rejects_a_core_pin_that_differs_from_other_packages(self):
        tag, commit = flatpak_manifest.core_pin(self.root)
        self.edit("tools/build-packages.sh", commit, "0" * 40)
        with self.assertRaises(ValueError):
            flatpak_manifest.check(self.root)

    def test_rejects_stale_core_version_options(self):
        self.edit(FILES[2], "-DOMGTK_CORE_INT_VERSION=", "-DOMGTK_CORE_INT_VERSION=1")
        with self.assertRaises(ValueError):
            flatpak_manifest.check(self.root)

    def test_rejects_a_version_without_appstream_release(self):
        (self.root / "VERSION").write_text("0.1.0-preview99\n")
        with self.assertRaises(ValueError):
            flatpak_manifest.check(self.root)

    def test_flathub_variant_builds_the_pinned_release_tag(self):
        manifest = flatpak_manifest.load(self.root)
        result = flatpak_manifest.flathub(manifest, "a" * 40, "0.1.0")
        sources = result["modules"][-1]["sources"]
        self.assertEqual(sources[0], {"type": "git", "url": flatpak_manifest.REPOSITORY,
                                      "tag": "v0.1.0", "commit": "a" * 40})
        self.assertNotIn("dir", {source["type"] for source in sources})
        self.assertEqual(manifest["modules"][-1]["sources"][0]["type"], "dir")
        self.assertEqual(json.loads(json.dumps(result))["modules"][:-1], manifest["modules"][:-1])
        with self.assertRaises(ValueError):
            flatpak_manifest.flathub(manifest, "v0.1.0", "0.1.0")


if __name__ == "__main__":
    unittest.main()
