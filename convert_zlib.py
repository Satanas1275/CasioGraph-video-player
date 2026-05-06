#!/usr/bin/env python3
"""
convert_zlib.py - Convertit bad-apple.webm en .bin zlib pour fx-9860G
Usage: python3 convert_zlib.py bad-apple.webm bad-apple.bin
"""

import sys
import subprocess
import struct
import zlib
import numpy as np

TARGET_W        = 128
TARGET_H        = 64
BYTES_PER_FRAME = (TARGET_W * TARGET_H) // 8

def convert(input_path, output_path, fps=10, threshold=128):
    probe = subprocess.run([
        'ffprobe', '-v', 'error',
        '-show_entries', 'format=duration',
        '-of', 'default=noprint_wrappers=1:nokey=1',
        input_path
    ], capture_output=True, text=True)

    duration = float(probe.stdout.strip())
    expected = int(duration * fps)

    print(f"Durée        : {duration:.1f}s")
    print(f"Frames cible : {expected} @ {fps}fps")

    cmd = [
        'ffmpeg', '-i', input_path,
        '-vf', f'fps={fps},scale={TARGET_W}:{TARGET_H}',
        '-pix_fmt', 'gray',
        '-f', 'rawvideo',
        '-v', 'error',
        'pipe:1'
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    frames    = []
    total     = 0
    frame_idx = 0

    while True:
        raw = proc.stdout.read(TARGET_W * TARGET_H)
        if len(raw) < TARGET_W * TARGET_H:
            break

        arr    = np.frombuffer(raw, dtype=np.uint8).reshape(TARGET_H, TARGET_W)
        packed = np.packbits((arr < threshold).astype(np.uint8),
                             axis=1, bitorder='big').tobytes()

        compressed = zlib.compress(packed, level=9)
        frames.append(compressed)
        total += len(compressed)
        frame_idx += 1

        if frame_idx % 500 == 0:
            pct = frame_idx / expected * 100
            print(f"  {frame_idx}/{expected} ({pct:.0f}%) | total: {(total + frame_idx*2 + 16)/1024:.0f} Ko")

    proc.wait()

    total_size = total + len(frames) * 2 + 16
    print(f"\nFrames      : {len(frames)}")
    print(f"Taille      : {total_size / 1024 / 1024:.3f} Mo")

    with open(output_path, 'wb') as f:
        # Header: magic(4) + fps(4) + frame_count(4) + width(2) + height(2) = 16o
        f.write(b'FXZL')
        f.write(struct.pack('<I', fps))
        f.write(struct.pack('<I', len(frames)))
        f.write(struct.pack('<HH', TARGET_W, TARGET_H))

        for comp in frames:
            f.write(struct.pack('<H', len(comp)))  # taille compressée uint16
            f.write(comp)

    print(f"Ecrit: {output_path}")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 convert_zlib.py <input.webm> <output.bin> [fps] [seuil]")
        sys.exit(1)

    fps       = int(sys.argv[3]) if len(sys.argv) > 3 else 10
    threshold = int(sys.argv[4]) if len(sys.argv) > 4 else 128
    convert(sys.argv[1], sys.argv[2], fps, threshold)