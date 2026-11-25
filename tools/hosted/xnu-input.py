#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Send keyboard and pointer input to the hosted macOS (XnuInput): Linux evdev records appended to xnu.ko's input_in.

    xnu-input.py [--file /var/tmp/xnu-input] [--screen 1280x800] COMMAND ...
      move X Y          pointer to pixel X,Y
      click [X Y]       left click (at X,Y)
      rclick [X Y]      right click
      drag X1 Y1 X2 Y2  left-button drag
      scroll N          wheel N notches (negative: down)
      type TEXT         type TEXT (US layout)
      key NAME...       press and release keys together (e.g. key cmd space, key return)
      sleep SECONDS
Several commands can follow one another: xnu-input.py click 640 400 type hello key return
"""
import struct, sys, time

EV_SYN, EV_KEY, EV_REL, EV_ABS = 0, 1, 2, 3
BTN = {'left': 0x110, 'right': 0x111, 'middle': 0x112}
KEYS = {  # Linux key codes (input-event-codes.h)
    'esc': 1, 'escape': 1, 'backspace': 14, 'delete': 14, 'tab': 15, 'return': 28, 'enter': 28, 'ctrl': 29, 'control': 29,
    'shift': 42, 'rshift': 54, 'alt': 56, 'option': 56, 'space': 57, 'capslock': 58, 'cmd': 125, 'command': 125,
    'rcmd': 126, 'up': 103, 'left': 105, 'right': 106, 'down': 108, 'home': 102, 'end': 107, 'pageup': 104,
    'pagedown': 109, 'forwarddelete': 111,
    **{f'f{i}': c for i, c in zip(range(1, 11), range(59, 69))}, 'f11': 87, 'f12': 88,
}
for i, ch in enumerate('1234567890'):
    KEYS[ch] = 2 + i
for row, start in (('qwertyuiop', 16), ('asdfghjkl', 30), ('zxcvbnm', 44)):
    for i, ch in enumerate(row):
        KEYS[ch] = start + i
PLAIN = {'-': 12, '=': 13, '[': 26, ']': 27, ';': 39, "'": 40, '`': 41, '\\': 43, ',': 51, '.': 52, '/': 53, ' ': 57,
         '\n': 28, '\t': 15}
SHIFTED = {'!': '1', '@': '2', '#': '3', '$': '4', '%': '5', '^': '6', '&': '7', '*': '8', '(': '9', ')': '0', '_': '-',
           '+': '=', '{': '[', '}': ']', ':': ';', '"': "'", '~': '`', '|': '\\', '<': ',', '>': '.', '?': '/'}


class Input:
    def __init__(self, path, width, height):
        self.out, self.w, self.h = open(path, 'ab', buffering=0), width, height

    def close(self):
        self.out.close()

    def ev(self, t, c, v):
        self.out.write(struct.pack('<HHi', t, c, v))

    def syn(self):
        self.ev(EV_SYN, 0, 0)
        time.sleep(0.02)

    def move(self, x, y):
        x = min(max(float(x), 0.0), self.w - 1)
        y = min(max(float(y), 0.0), self.h - 1)
        self.ev(EV_ABS, 0, int(x * 32767 / (self.w - 1)))
        self.ev(EV_ABS, 1, int(y * 32767 / (self.h - 1)))
        self.syn()

    def button(self, name, down):
        self.ev(EV_KEY, BTN[name], 1 if down else 0)
        self.syn()

    def keys(self, codes):
        for c in codes:
            self.ev(EV_KEY, c, 1)
        self.syn()
        for c in reversed(codes):
            self.ev(EV_KEY, c, 0)
        self.syn()

    def type(self, text):
        for ch in text:
            if ch in SHIFTED:
                self.keys([KEYS['shift'], KEYS.get(SHIFTED[ch]) or PLAIN[SHIFTED[ch]]])
            elif ch.isupper():
                self.keys([KEYS['shift'], KEYS[ch.lower()]])
            elif ch in KEYS:
                self.keys([KEYS[ch]])
            elif ch in PLAIN:
                self.keys([PLAIN[ch]])


def main(argv):
    path, screen = '/var/tmp/xnu-input', '1280x800'
    while argv and argv[0].startswith('--'):
        if len(argv) < 2:
            sys.exit(f'{argv[0]} requires a value')
        opt, value = argv[0], argv[1]
        argv = argv[2:]
        if opt == '--file':
            path = value
        elif opt == '--screen':
            screen = value
        else:
            sys.exit(f'unknown option {opt}')
    try:
        w, h = (int(v) for v in screen.split('x'))
    except (ValueError, TypeError):
        sys.exit(f'invalid screen size {screen!r}; use WIDTHxHEIGHT')
    if w < 2 or h < 2:
        sys.exit('screen width and height must both be at least 2')
    d = Input(path, w, h)
    def need(i, count, cmd):
        if i + count > len(argv):
            sys.exit(f'{cmd} needs {count} argument(s)')

    try:
        i = 0
        while i < len(argv):
            cmd = argv[i]; i += 1
            if cmd == 'move':
                need(i, 2, cmd); d.move(float(argv[i]), float(argv[i + 1])); i += 2
            elif cmd in ('click', 'rclick'):
                if i + 1 < len(argv):
                    try:
                        x, y = float(argv[i]), float(argv[i + 1])
                    except ValueError:
                        x = y = None
                    if x is not None and y is not None:
                        d.move(x, y); i += 2
                b = 'left' if cmd == 'click' else 'right'
                d.button(b, True); d.button(b, False)
            elif cmd == 'drag':
                need(i, 4, cmd)
                x1, y1, x2, y2 = (float(v) for v in argv[i:i + 4]); i += 4
                d.move(x1, y1); d.button('left', True)
                for s in range(1, 11):
                    d.move(x1 + (x2 - x1) * s / 10, y1 + (y2 - y1) * s / 10)
                d.button('left', False)
            elif cmd == 'scroll':
                need(i, 1, cmd); d.ev(EV_REL, 8, int(argv[i])); d.syn(); i += 1
            elif cmd == 'type':
                need(i, 1, cmd); d.type(argv[i]); i += 1
            elif cmd == 'key':
                names = []
                while i < len(argv) and argv[i] in KEYS:
                    names.append(KEYS[argv[i]]); i += 1
                if not names:
                    sys.exit('key needs at least one known key name')
                d.keys(names)
            elif cmd == 'sleep':
                need(i, 1, cmd); time.sleep(float(argv[i])); i += 1
            else:
                sys.exit(f'unknown command {cmd}')
    finally:
        d.close()


if __name__ == '__main__':
    main(sys.argv[1:])
