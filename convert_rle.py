#!/usr/bin/env python3
"""
convert_rle.py - Convertit bad-apple.webm en .bin RLE pour fx-9860G
Usage: python3 convert_rle.py bad-apple.webm bad-apple.bin
"""

import sys
import subprocess
import struct
import numpy as np

TARGET_W     = 128
TARGET_H     = 64
BYTES_PER_FRAME = (TARGET_W * TARGET_H) // 8

def rle_encode(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        val = data[i]
        count = 1
        while i + count < len(data) and data[i + count] == val and count < 255:
            count += 1
        out.append(count)
        out.append(val)
        i += count
    return bytes(out)

def convert(input_path, output_path, fps=10, threshold=128):
    probe = subprocess.run([
        'ffprobe', '-v', 'error',
        '-show_entries', 'format=duration',
        '-of', 'default=noprint_wrappers=1:nokey=1',
        input_path
    ], capture_output=True, text=True)

    duration    = float(probe.stdout.strip())
    expected    = int(duration * fps)

    print(f"Durée        : {duration:.1f}s")
    print(f"Frames cible : {expected} @ {fps}fps")
    print(f"Taille brute : {expected * BYTES_PER_FRAME / 1024 / 1024:.2f} Mo (sans compression)")

    cmd = [
        'ffmpeg', '-i', input_path,
        '-vf', f'fps={fps},scale={TARGET_W}:{TARGET_H}',
        '-pix_fmt', 'gray',
        '-f', 'rawvideo',
        '-v', 'error',
        'pipe:1'
    ]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE)

    frames        = []
    total_rle     = 0
    frame_idx     = 0

    while True:
        raw = proc.stdout.read(TARGET_W * TARGET_H)
        if len(raw) < TARGET_W * TARGET_H:
            break

        frame  = np.frombuffer(raw, dtype=np.uint8).reshape(TARGET_H, TARGET_W)
        binary = (frame < threshold).astype(np.uint8)
        packed = np.packbits(binary, axis=1, bitorder='big').tobytes()

        rle   = rle_encode(packed)
        frames.append(rle)
        total_rle += len(rle)
        frame_idx += 1

        if frame_idx % 200 == 0:
            pct = frame_idx / expected * 100
            avg = total_rle / frame_idx
            print(f"  {frame_idx}/{expected} ({pct:.0f}%) | moy RLE: {avg:.0f}o/frame | total: {total_rle/1024:.0f} Ko")

    proc.wait()

    total_size = total_rle + 2 * len(frames) + 16  # +2 par frame pour le uint16 size, +16 header
    print(f"\nFrames      : {len(frames)}")
    print(f"Taille RLE  : {total_size / 1024 / 1024:.3f} Mo")
    print(f"Ratio       : {(total_size / (len(frames) * BYTES_PER_FRAME)) * 100:.1f}%")

    if total_size > 1024 * 1024:
        print(f"\nATTENTION: {total_size/1024/1024:.2f} Mo > 1 Mo !")
        print("Tu peux augmenter le seuil (ex: 180) ou baisser les fps")
    else:
        print(f"\nOK: rentre dans 1 Mo !")

    with open(output_path, 'wb') as f:
        # Header: magic(4) + fps(4) + frame_count(4) + width(2) + height(2) = 16o
        f.write(b'FXRL')
        f.write(struct.pack('<I', fps))
        f.write(struct.pack('<I', len(frames)))
        f.write(struct.pack('<HH', TARGET_W, TARGET_H))

        for rle in frames:
            f.write(struct.pack('<H', len(rle)))  # taille frame compressée
            f.write(rle)

    print(f"Ecrit: {output_path}")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 convert_rle.py <input.webm> <output.bin> [fps] [seuil]")
        sys.exit(1)

    fps       = int(sys.argv[3]) if len(sys.argv) > 3 else 10
    threshold = int(sys.argv[4]) if len(sys.argv) > 4 else 128
    convert(sys.argv[1], sys.argv[2], fps, threshold)