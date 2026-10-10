"""Generate an original charcoal skin material using Python's standard library.

Deterministic grayscale PNG; no external assets. GPL-3.0, like the project.
Run from any directory to rebuild assets/Themes/Textures/skyrim-charcoal.png.
"""
from pathlib import Path
import math
import random
import struct
import zlib


def chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))


def main():
    size = 512
    rng = random.Random(0x534B5952)
    grids = []
    for frequency, weight in [(4, .52), (8, .27), (16, .13), (64, .08)]:
        grids.append((frequency, weight, [[rng.random() for _ in range(frequency)] for _ in range(frequency)]))

    def noise(x, y):
        value = 0.
        for frequency, weight, grid in grids:
            gx, gy = x * frequency / size, y * frequency / size
            ix, iy = int(gx), int(gy)
            fx, fy = gx - ix, gy - iy
            fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
            a = grid[iy % frequency][ix % frequency] * (1 - fx) + grid[iy % frequency][(ix + 1) % frequency] * fx
            b = grid[(iy + 1) % frequency][ix % frequency] * (1 - fx) + grid[(iy + 1) % frequency][(ix + 1) % frequency] * fx
            value += (a * (1 - fy) + b * fy) * weight
        return value

    rows = bytearray()
    for y in range(size):
        rows.append(0)
        for x in range(size):
            cloud = noise(x, y)
            grain = rng.uniform(-16, 16)
            fibre = math.sin(x * .72 + y * .17 + cloud * 8) * 5
            tone = max(90, min(255, round(90 + cloud * 174 + grain + fibre)))
            rows.extend((tone, tone, tone, 255))
    header = struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', header) + chunk(b'IDAT', zlib.compress(rows, 9)) + chunk(b'IEND', b'')
    target = Path(__file__).resolve().parents[1] / 'assets/Themes/Textures/skyrim-charcoal.png'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(png)
    print(f'{target}: {size}x{size}, {len(png)} bytes')


if __name__ == '__main__':
    main()
