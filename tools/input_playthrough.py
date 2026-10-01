"""Windows window-message playthrough of the real GLFW input path. No pip packages required."""
import ctypes
import json
import math
from pathlib import Path
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
user = ctypes.windll.user32
user.FindWindowW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p]
user.FindWindowW.restype = ctypes.c_void_p
user.PostMessageW.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_size_t, ctypes.c_ssize_t]
user.SetForegroundWindow.argtypes = [ctypes.c_void_p]
window = None
held = set()

def state():
    try:
        return json.loads((ROOT / 'captures/input-state.json').read_text())
    except (OSError, ValueError):
        return {}

def wait(predicate, label, timeout=10):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        data = state()
        if data and predicate(data):
            return data
        if process.poll() is not None:
            raise RuntimeError(f'Game exited before {label}')
        time.sleep(.025)
    raise RuntimeError(f'Timed out: {label}; last state={state()}')

def key(code, down):
    scan = user.MapVirtualKeyW(code, 0)
    param = 1 | (scan << 16)
    if not down:
        param |= (1 << 30) | (1 << 31)
    if not user.PostMessageW(window, 0x100 if down else 0x101, code, param):
        raise RuntimeError('PostMessageW failed')
    if down:
        held.add(code)
    else:
        held.discard(code)

def tap(code):
    key(code, True)
    time.sleep(.04)
    key(code, False)

def move_until(code, predicate, label):
    key(code, True)
    try:
        wait(predicate, label)
    finally:
        key(code, False)
    time.sleep(.10)

log = (ROOT / 'captures')
log.mkdir(exist_ok=True)
with (log / 'input-playthrough.log').open('w') as output:
    process = subprocess.Popen([str(ROOT / 'build/Release/loose.exe'), '--telemetry', '--validation'], cwd=ROOT, stdout=output, stderr=subprocess.STDOUT)
    try:
        deadline = time.monotonic() + 10
        while not window and time.monotonic() < deadline:
            window = user.FindWindowW(None, 'LOOSE / LEVEL ZERO')
            time.sleep(.05)
        if not window:
            raise RuntimeError('Native game window was not created')
        user.SetForegroundWindow(window)
        wait(lambda s: s['grounded'] and s['captured'] and s['state']=='PLAYING', 'spawn and mouse capture')
        tap(0x7B)  # F12 captures the rendered frame.
        move_until(ord('W'), lambda s: s['center'][2] < 2.45, 'WASD movement')
        key(ord('Q'), True)
        wait(lambda s: s['state']=='ROTATING', 'Q key-down transition')
        wait(lambda s: s['state']=='PLAYING' and s['grounded'] and s['up'][0]>.99, 'wall landing')
        time.sleep(.8)  # Still holding Q must not repeat.
        assert state()['up'][0]>.99, 'Holding Q repeated the rotation'
        key(ord('Q'), False)
        tap(0x7B)
        tap(ord('Q'))
        wait(lambda s: s['state']=='PLAYING' and s['grounded'] and s['up'][1]<-.99, 'ceiling landing')
        move_until(ord('A'), lambda s: s['center'][0] > -.08, 'movement with upside-down right vector')
        move_until(ord('W'), lambda s: s['center'][2] < -1.98 or s['state']=='COMPLETE', 'entering the ceiling aperture')
        wait(lambda s: s['state']=='COMPLETE' and not s['captured'], 'player-only completion and mouse release')
        tap(0x7B)
        tap(0x08)  # Backspace reset from completion.
        wait(lambda s: s['state']=='PLAYING' and s['up'][1]>.99 and s['center'][2]>3.7, 'reset completed')
        tap(0x1B)  # Escape pause.
        wait(lambda s: s['state']=='PAUSED' and not s['captured'], 'pause releases mouse')
        paused = state()['center']
        time.sleep(.3)
        assert sum((a-b)**2 for a,b in zip(paused,state()['center']))<.001, 'Paused character moved'
        tap(0x70)  # F1 help is reachable while paused.
        tap(0x7B)
        tap(0x1B)
        wait(lambda s: s['state']=='PLAYING' and s['captured'], 'resume captures mouse')
        move_until(ord('W'), lambda s: s['center'][2]<2.4, 'clear location before reset during turn')
        tap(ord('Q'))
        wait(lambda s: s['state']=='ROTATING', 'rotation before reset')
        tap(0x08)
        final = wait(lambda s: s['state']=='PLAYING' and s['up'][1]>.99 and s['center'][2]>3.7, 'Backspace reset during transition')
        assert final['keys']>=15, 'GLFW did not receive expected key-down callbacks'
        print('Native GLFW input playthrough passed:', final)
    finally:
        for code in list(held):
            key(code, False)
        if window:
            user.PostMessageW(window,0x10,0,0)  # WM_CLOSE: normal application cleanup.
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.terminate()
