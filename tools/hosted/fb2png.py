#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Convert the hosted framebuffer dump (raw 32-bit BGRA) to PNG.

    fb2png.py DUMP WIDTH HEIGHT OUT.png
    fb2png.py --watch DUMP WIDTH HEIGHT OUT.png

``--watch`` is useful with ``xnu.ko fb_dump=...``: the kernel writes the
file in place, so a reader can observe a short frame.  Incomplete frames are
retried and completed PNGs are installed atomically.
"""
import argparse
import os
import struct
import sys
import tempfile
import time
import zlib


class IncompleteFrame(Exception):
    pass


def read_frame(src, expected):
    try:
        with open(src, 'rb') as f:
            data = f.read(expected)
    except OSError as e:
        raise IncompleteFrame(str(e)) from e
    if len(data) != expected:
        raise IncompleteFrame(f'{src}: {len(data)} bytes, expected {expected}')
    return data


def png_bytes(data, width, height):
    rows = bytearray()
    stride = width * 4
    for y in range(height):
        row = data[y * stride:(y + 1) * stride]
        rgb = bytearray(width * 3)
        rgb[0::3] = row[2::4]   # R
        rgb[1::3] = row[1::4]   # G
        rgb[2::3] = row[0::4]   # B
        rows += b'\0' + rgb

    def chunk(kind, payload):
        return (struct.pack('>I', len(payload)) + kind + payload +
                struct.pack('>I', zlib.crc32(kind + payload) & 0xffffffff))

    return (b'\x89PNG\r\n\x1a\n' +
            chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(bytes(rows), 6)) +
            chunk(b'IEND', b''))


def write_atomic(path, payload):
    directory = os.path.dirname(os.path.abspath(path)) or '.'
    fd, temp = tempfile.mkstemp(prefix='.fb2png-', suffix='.tmp', dir=directory)
    try:
        with os.fdopen(fd, 'wb') as f:
            f.write(payload)
            f.flush()
            os.fsync(f.fileno())
        os.replace(temp, path)
    except BaseException:
        try:
            os.unlink(temp)
        except FileNotFoundError:
            pass
        raise


def convert_once(src, width, height, out, expected):
    write_atomic(out, png_bytes(read_frame(src, expected), width, height))


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--watch', action='store_true', help='keep converting new frames until interrupted')
    parser.add_argument('--interval', type=float, default=0.25, help='watch polling interval in seconds (default: 0.25)')
    parser.add_argument('dump')
    parser.add_argument('width', type=int)
    parser.add_argument('height', type=int)
    parser.add_argument('output')
    args = parser.parse_args(argv)
    if args.width <= 0 or args.height <= 0:
        parser.error('width and height must be positive')
    if args.interval <= 0:
        parser.error('interval must be positive')
    expected = args.width * args.height * 4
    if expected > 1 << 31:
        parser.error('frame is too large')

    if not args.watch:
        try:
            convert_once(args.dump, args.width, args.height, args.output, expected)
        except IncompleteFrame as e:
            parser.error(str(e))
        return 0

    last_stamp = None
    try:
        while True:
            try:
                stat = os.stat(args.dump)
                stamp = (stat.st_mtime_ns, stat.st_size)
                if stamp != last_stamp:
                    convert_once(args.dump, args.width, args.height, args.output, expected)
                    last_stamp = stamp
            except (FileNotFoundError, IncompleteFrame):
                pass
            time.sleep(args.interval)
    except KeyboardInterrupt:
        return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
