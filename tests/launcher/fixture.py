#!/usr/bin/env python3
"""Isolated service fixture; never opens a display or an Ableton Link session."""
import argparse
import os
from pathlib import Path
import signal
import sys
import time

p = argparse.ArgumentParser()
p.add_argument("--config", required=True)
p.add_argument("--print-config", action="store_true")
p.add_argument("--windowed", action="store_true")
a = p.parse_args()
mode = Path(a.config).read_text().strip()
if mode not in ("normal", "fail", "ignore"):
    print("fixture: invalid configuration", file=sys.stderr, flush=True)
    sys.exit(2)
if a.print_config:
    print("mode=" + mode)
    sys.exit(0)
if mode == "fail":
    print("fixture: deliberate startup failure", file=sys.stderr, flush=True)
    sys.exit(7)
quit_requested = False


def terminate(signum, frame):
    global quit_requested
    print("fixture: SIGTERM received", flush=True)
    quit_requested = True


signal.signal(signal.SIGTERM, signal.SIG_IGN if mode == "ignore" else terminate)
print("fixture: ready pid={} mode={} cwd={}".format(os.getpid(), mode, os.getcwd()), flush=True)
while not quit_requested:
    time.sleep(0.1)
print("fixture: normal shutdown", flush=True)
