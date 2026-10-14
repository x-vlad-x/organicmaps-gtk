"""Exercise launcher relocation and argument handling without touching user data."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class LauncherTest(unittest.TestCase):
    def check_layout(self, installed):
        with tempfile.TemporaryDirectory(prefix="omgtk launcher ") as directory:
            directory = Path(directory)
            launcher_dir = directory / "usr/bin" if installed else directory / "SSH bundle"
            private = directory / "usr/libexec/organicmaps-gtk" if installed else launcher_dir
            launcher_dir.mkdir(parents=True)
            (private / "bin").mkdir(parents=True, exist_ok=True)
            binary = private / "bin/organicmaps-gtk"
            binary.write_text('#!/bin/sh\nprintf "%s\\n" "$MWM_RESOURCES_DIR" "$MWM_WRITABLE_DIR" "$@"\n')
            binary.chmod(0o755)
            launcher = launcher_dir / "organicmaps-gtk"
            shutil.copy2(ROOT / "packaging/organicmaps-gtk", launcher)
            launcher.chmod(0o755)
            env = dict(os.environ, XDG_DATA_HOME=str(directory / "user data"), XDG_CONFIG_HOME=str(directory / "user config"))
            result = subprocess.run([str(launcher), "a bookmark.gpx", "geo:52.5,13.4"], env=env,
                                    check=True, capture_output=True, text=True)
            self.assertEqual(result.stdout.splitlines(), [str(private / "data") + "/",
                str(directory / "user data/OrganicMaps") + "/", "a bookmark.gpx", "geo:52.5,13.4"])
            self.assertTrue((directory / "user config/OrganicMaps").is_dir())

    def test_relocated_system_package(self):
        self.check_layout(True)

    def test_legacy_ssh_bundle(self):
        self.check_layout(False)


if __name__ == "__main__":
    unittest.main()
