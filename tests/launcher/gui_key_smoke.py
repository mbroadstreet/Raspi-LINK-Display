#!/usr/bin/env python3
"""Opt-in X11 test-event sender, confined to the owned app window; not a control API."""
import argparse
import ctypes as c
import ctypes.util
import json
import os
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--helper", required=True, help="Explicit path to the verified launcher")
p.add_argument("--window", required=True)
p.add_argument("--pid", required=True, type=int)
p.add_argument("--key", required=True, choices=("F1", "f", "p", "r", "q", "Escape"))
a = p.parse_args()
if not re.fullmatch(r"0x[0-9a-fA-F]+", a.window):
    p.error("Invalid window identifier")
verified = subprocess.run(["python3", a.helper, "status"], check=True, stdout=subprocess.PIPE, text=True)
owned = json.loads(verified.stdout)
if owned.get("state") != "active" or (owned.get("identity") or {}).get("pid") != a.pid or a.window not in owned.get("mapped_windows", []):
    raise RuntimeError("Launcher provenance/window ownership does not match the requested test target")
desktop_keys = {"DISPLAY", "XAUTHORITY", "XDG_RUNTIME_DIR", "XDG_SESSION_TYPE", "DBUS_SESSION_BUS_ADDRESS"}
desktop = owned.get("desktop")
if (not isinstance(desktop, dict) or set(desktop) - desktop_keys
        or any(not isinstance(value, str) for value in desktop.values())
        or not desktop.get("DISPLAY") or not desktop.get("XAUTHORITY")
        or desktop.get("XDG_SESSION_TYPE") != "x11"):
    raise RuntimeError("Verified service desktop environment is absent or invalid")
# Do not inherit another SSH/caller display or authority. Values come from the
# helper's provenance-verified saved desktop, never from authority cookie bytes.
run_env = {key: value for key, value in os.environ.items() if key not in desktop_keys}
run_env.update(desktop)
for key in desktop_keys:
    os.environ.pop(key, None)
os.environ.update(desktop)
managed = subprocess.run(["systemctl", "--user", "show", "raspi-link-display.service", "--property=MainPID", "--property=Description", "--property=Transient"], check=True, stdout=subprocess.PIPE, text=True).stdout
if "MainPID={}\n".format(a.pid) not in managed or "Description=Raspi Link Display (link-displayctl)\n" not in managed or "Transient=yes\n" not in managed:
    raise RuntimeError("Expected owned application is not running")
prop = subprocess.run(["xprop", "-id", a.window, "_NET_WM_PID"], check=True, stdout=subprocess.PIPE, text=True, env=run_env).stdout
if not re.search(r"=\s*{}\s*$".format(a.pid), prop):
    raise RuntimeError("Window no longer belongs to expected application")


class XKeyEvent(c.Structure):
    _fields_ = [("type", c.c_int), ("serial", c.c_ulong), ("send_event", c.c_int), ("display", c.c_void_p), ("window", c.c_ulong), ("root", c.c_ulong), ("subwindow", c.c_ulong), ("time", c.c_ulong), ("x", c.c_int), ("y", c.c_int), ("x_root", c.c_int), ("y_root", c.c_int), ("state", c.c_uint), ("keycode", c.c_uint), ("same_screen", c.c_int)]


class XEvent(c.Union):
    _fields_ = [("xkey", XKeyEvent), ("pad", c.c_long * 24)]


x = c.CDLL(ctypes.util.find_library("X11"))
x.XOpenDisplay.argtypes = [c.c_char_p]
x.XOpenDisplay.restype = c.c_void_p
x.XDefaultRootWindow.argtypes = [c.c_void_p]
x.XDefaultRootWindow.restype = c.c_ulong
x.XStringToKeysym.argtypes = [c.c_char_p]
x.XStringToKeysym.restype = c.c_ulong
x.XKeysymToKeycode.argtypes = [c.c_void_p, c.c_ulong]
x.XKeysymToKeycode.restype = c.c_uint
x.XSendEvent.argtypes = [c.c_void_p, c.c_ulong, c.c_int, c.c_long, c.POINTER(XEvent)]
x.XSendEvent.restype = c.c_int
x.XFlush.argtypes = [c.c_void_p]
x.XSync.argtypes = [c.c_void_p, c.c_int]
x.XCloseDisplay.argtypes = [c.c_void_p]
display = x.XOpenDisplay(desktop["DISPLAY"].encode())
if not display:
    raise RuntimeError("Cannot open intended X11 desktop")
try:
    keycode = x.XKeysymToKeycode(display, x.XStringToKeysym(a.key.encode()))
    if not keycode:
        raise RuntimeError("Key has no mapping")
    for event_type, mask in ((2, 1), (3, 2)):
        event = XEvent()
        event.xkey.type = event_type
        event.xkey.send_event = 1
        event.xkey.display = display
        event.xkey.window = int(a.window, 16)
        event.xkey.root = x.XDefaultRootWindow(display)
        event.xkey.same_screen = 1
        event.xkey.keycode = keycode
        if not x.XSendEvent(display, event.xkey.window, 0, mask, c.byref(event)):
            raise RuntimeError("XSendEvent failed")
    x.XSync(display, 0)
finally:
    x.XCloseDisplay(display)
print("Sent {} test events only to owned window {}; assert the resulting behavior separately.".format(a.key, a.window))
