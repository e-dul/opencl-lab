#!/usr/bin/env python3
"""
gen_pgm.py — Generate a synthetic occupancy grid PGM file.

Convention (ROS nav2 / map_server):
  255 = free space
    0 = occupied (obstacle)

Rectangles are specified as normalized coordinates [0.0, 1.0]:
  x0 y0 x1 y1   (top-left, bottom-right, inclusive)

Usage:
  python3 gen_pgm.py -o assets/warehouse.pgm --width 512 --height 512
  python3 gen_pgm.py -o my.pgm --width 256 --height 256 --no-border
"""

import argparse
import sys


# ---------------------------------------------------------------------------
# Obstacle layout (normalized coordinates)
# Edit this list to change the map layout.
# ---------------------------------------------------------------------------
OBSTACLES = [
    # x0,   y0,   x1,   y1    description
    (0.20, 0.20, 0.35, 0.40),  # top-left block
    (0.60, 0.15, 0.75, 0.35),  # top-right block
    (0.25, 0.60, 0.45, 0.75),  # bottom-left block
    (0.55, 0.55, 0.80, 0.70),  # bottom-right block
    (0.42, 0.38, 0.58, 0.42),  # thin horizontal wall in center
]

BORDER_THICKNESS = 0.02  # fraction of min(width, height)


def draw_rect(grid, width, height, x0n, y0n, x1n, y1n):
    """Fill rectangle (normalized coords) with obstacle pixels (0)."""
    x0 = max(0, int(x0n * width))
    y0 = max(0, int(y0n * height))
    x1 = min(width - 1, int(x1n * width))
    y1 = min(height - 1, int(y1n * height))
    for y in range(y0, y1 + 1):
        row_start = y * width
        for x in range(x0, x1 + 1):
            grid[row_start + x] = 0


def generate(width: int, height: int, border: bool) -> bytearray:
    grid = bytearray(b'\xff' * (width * height))  # all free

    if border:
        t = max(1, int(BORDER_THICKNESS * min(width, height)))
        tn = t / min(width, height)
        draw_rect(grid, width, height, 0.0,    0.0,    1.0,    tn)      # top
        draw_rect(grid, width, height, 0.0,    1.0-tn, 1.0,    1.0)     # bottom
        draw_rect(grid, width, height, 0.0,    0.0,    tn,     1.0)     # left
        draw_rect(grid, width, height, 1.0-tn, 0.0,    1.0,    1.0)     # right

    for (x0, y0, x1, y1) in OBSTACLES:
        draw_rect(grid, width, height, x0, y0, x1, y1)

    return grid


def write_pgm(path: str, width: int, height: int, grid: bytearray):
    with open(path, 'wb') as f:
        f.write(f'P5\n{width} {height}\n255\n'.encode('ascii'))
        f.write(grid)


def main():
    parser = argparse.ArgumentParser(
        description='Generate a synthetic occupancy grid PGM (255=free, 0=obstacle).')
    parser.add_argument('-o', '--output', default='warehouse.pgm',
                        help='Output PGM file path (default: warehouse.pgm)')
    parser.add_argument('--width',  type=int, default=512,
                        help='Map width  in pixels (default: 512)')
    parser.add_argument('--height', type=int, default=512,
                        help='Map height in pixels (default: 512)')
    parser.add_argument('--no-border', action='store_true',
                        help='Omit the outer border wall')
    args = parser.parse_args()

    if args.width <= 0 or args.height <= 0:
        print('Error: width and height must be positive.', file=sys.stderr)
        sys.exit(1)

    grid = generate(args.width, args.height, border=not args.no_border)
    write_pgm(args.output, args.width, args.height, grid)
    print(f'Wrote {args.output} ({args.width}x{args.height}, '
          f'{len(OBSTACLES)} obstacle rects'
          f'{", border" if not args.no_border else ""})')


if __name__ == '__main__':
    main()
