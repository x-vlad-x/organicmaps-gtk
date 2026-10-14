"""Check that switching from the SSH install preserves user files and overrides."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
APP_ID = "io.github.x_vlad_x.OrganicMapsGtk"


class DesktopMigrationTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="omgtk desktop ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.home = self.root / "home"
        self.prefix = self.root / "prefix"
        self.data = self.home / "custom data"
        self.state = self.home / "custom state"
        self.entry = self.data / f"applications/{APP_ID}.desktop"
        self.entry.parent.mkdir(parents=True)
        (self.prefix / "bin").mkdir(parents=True)
        (self.prefix / "share/applications").mkdir(parents=True)
        self.system_entry = self.prefix / f"share/applications/{APP_ID}.desktop"
        self.system_entry.write_text("[Desktop Entry]\nType=Application\nName=Packaged app\nExec=organicmaps-gtk\n")
        launcher = self.prefix / "bin/organicmaps-gtk"
        launcher.write_text("#!/bin/sh\nexit 0\n")
        launcher.chmod(0o755)
        self.helper = self.prefix / "bin/organicmaps-gtk-migrate-launcher"
        shutil.copy2(ROOT / "packaging/organicmaps-gtk-migrate-launcher", self.helper)
        self.legacy = self.home / ".local/share/organicmaps-gtk"
        self.original = (
            "[Desktop Entry]\nType=Application\nName=Organic Maps\n"
            f"Exec={self.legacy}/organicmaps-gtk %U\n"
            f"Icon={self.legacy}/{APP_ID}.png\n"
        )
        self.env = dict(os.environ, HOME=str(self.home), XDG_DATA_HOME=str(self.data),
                        XDG_STATE_HOME=str(self.state))

    def migrate(self):
        return subprocess.run(["sh", str(self.helper)], env=self.env, capture_output=True, text=True)

    def backups(self):
        return list(self.state.glob(f"organicmaps-gtk/launcher-backups/*/{APP_ID}.desktop"))

    def test_migrates_only_launcher_and_keeps_backup(self):
        self.entry.write_text(self.original)
        self.legacy.mkdir(parents=True)
        old_binary = self.legacy / "organicmaps-gtk"
        old_binary.write_text("old install")
        bookmarks = self.data / "OrganicMaps/bookmarks.kml"
        bookmarks.parent.mkdir()
        bookmarks.write_text("bookmarks")
        self.assertEqual(self.migrate().returncode, 0)
        self.assertFalse(self.entry.exists())
        self.assertEqual(len(self.backups()), 1)
        self.assertEqual(self.backups()[0].read_text(), self.original)
        self.assertEqual(old_binary.read_text(), "old install")
        self.assertEqual(bookmarks.read_text(), "bookmarks")
        self.assertTrue(self.system_entry.is_file())
        self.assertEqual(self.migrate().returncode, 0)
        self.assertEqual(len(self.backups()), 1)

    def test_clean_install_needs_no_migration(self):
        self.assertEqual(self.migrate().returncode, 0)
        self.assertFalse(self.state.exists())

    def test_preserves_custom_or_hidden_entry(self):
        for text in (self.original.replace(" %U", " --custom %U"),
                     self.original + "Hidden=true\n", self.original + "NoDisplay=true\n"):
            with self.subTest(text=text):
                self.entry.write_text(text)
                self.assertNotEqual(self.migrate().returncode, 0)
                self.assertEqual(self.entry.read_text(), text)
                self.assertEqual(self.backups(), [])

    def test_preserves_symlink(self):
        target = self.home / "custom.desktop"
        target.write_text(self.original)
        self.entry.symlink_to(target)
        self.assertNotEqual(self.migrate().returncode, 0)
        self.assertTrue(self.entry.is_symlink())
        self.assertEqual(target.read_text(), self.original)

    def test_missing_package_does_not_hide_legacy_launcher(self):
        self.entry.write_text(self.original)
        self.system_entry.unlink()
        self.assertNotEqual(self.migrate().returncode, 0)
        self.assertEqual(self.entry.read_text(), self.original)


if __name__ == "__main__":
    unittest.main()
