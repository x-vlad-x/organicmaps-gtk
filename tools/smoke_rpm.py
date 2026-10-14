#!/usr/bin/env python3
"""Exercise the installed RPM as an unprivileged user on an isolated software display."""
import json
import os
from pathlib import Path
import pwd
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import time


def main():
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=True)
    user = pwd.getpwnam("app")
    processes = []
    with tempfile.TemporaryDirectory(prefix="organicmaps-rpm-smoke-") as tmp:
        root = Path(tmp)
        for directory in (root, root / "home", root / "runtime", root / "framebuffer"):
            directory.mkdir(exist_ok=True)
            directory.chmod(0o700)
            os.chown(directory, user.pw_uid, user.pw_gid)
        env = dict(os.environ, HOME=str(root / "home"), XDG_RUNTIME_DIR=str(root / "runtime"),
                   XDG_DATA_HOME=str(root / "home/data"), XDG_CONFIG_HOME=str(root / "home/config"),
                   DISPLAY=":99", GDK_BACKEND="x11", LIBGL_ALWAYS_SOFTWARE="1", GSK_RENDERER="cairo",
                   GTK_A11Y="none", LANG="en_US.UTF-8")
        try:
            with (output / "display.log").open("w") as display_log, (output / "application.log").open("w") as app_log:
                display = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1024x768x24", "-nolisten", "tcp",
                                            "-fbdir", str(root / "framebuffer")], env=env,
                                           user=user.pw_uid, group=user.pw_gid, stdout=display_log, stderr=subprocess.STDOUT)
                processes.append(display)
                deadline = time.monotonic() + 15
                while not Path("/tmp/.X11-unix/X99").is_socket():
                    if display.poll() is not None or time.monotonic() > deadline:
                        raise RuntimeError("The isolated test display did not start")
                    time.sleep(0.1)
                icon_check = '''import gi
gi.require_version("Gtk", "4.0")
from gi.repository import Gdk, Gio, Gtk
Gtk.init()
app_id = "io.github.x_vlad_x.OrganicMapsGtk"
assert Gio.DesktopAppInfo.new(app_id + ".desktop") is not None
theme = Gtk.IconTheme.get_for_display(Gdk.Display.get_default())
for size in (48, 64, 128, 256, 512):
    icon = theme.lookup_icon(app_id, None, size, 1, Gtk.TextDirection.NONE, Gtk.IconLookupFlags.FORCE_REGULAR)
    assert icon.get_file() and app_id in icon.get_file().get_path()
print("Desktop entry and all five icon sizes resolve")
'''
                subprocess.run(["python3", "-c", icon_check], env=env, user=user.pw_uid, group=user.pw_gid, check=True)
                application = subprocess.Popen(["dbus-run-session", "--", "/usr/bin/organicmaps-gtk"], env=env,
                                               user=user.pw_uid, group=user.pw_gid, stdout=app_log,
                                               stderr=subprocess.STDOUT, start_new_session=True)
                processes.append(application)
                deadline = time.monotonic() + 45
                while True:
                    text = (output / "application.log").read_text()
                    if application.poll() is not None:
                        raise RuntimeError(f"The installed app exited during startup: {application.returncode}")
                    if re.search(r"(?:^|\n)E\(\d+\)|Gtk-ERROR|GLib-ERROR|GtkGLArea error|Can't create OpenGL", text):
                        raise RuntimeError("The installed app reported a graphics or startup error")
                    if "Api = OpenGLES3" in text and "VTF enabled" in text:
                        break
                    if time.monotonic() > deadline:
                        raise RuntimeError("The map renderer did not initialize")
                    time.sleep(0.2)
                time.sleep(2)
                if application.poll() is not None:
                    raise RuntimeError("The installed app exited after renderer initialization")
                shutil.copy2(root / "framebuffer/Xvfb_screen0", output / "screen.xwd")
                (output / "result.json").write_text(json.dumps(dict(
                    version=subprocess.check_output(["organicmaps-gtk", "--version"], env=env,
                                                    user=user.pw_uid, group=user.pw_gid, text=True).strip(),
                    architecture=os.uname().machine, unprivileged_gui=True, renderer="OpenGLES3",
                    desktop_entry=True, icon_sizes=[48, 64, 128, 256, 512]), indent=2) + "\n")
                print("Installed RPM: unprivileged startup, desktop icons and OpenGL ES 3 map renderer passed")
        finally:
            for process in reversed(processes):
                if process.poll() is None:
                    if process is processes[-1] and len(processes) > 1:
                        os.killpg(process.pid, signal.SIGTERM)
                    else:
                        process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            print((output / "application.log").read_text() if (output / "application.log").exists() else "")


if __name__ == "__main__":
    main()
