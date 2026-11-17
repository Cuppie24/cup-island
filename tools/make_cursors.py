"""Draws Cup Island's grab cursors (decision 144) and writes them as .cur files.

assets/grab.cur: an open hand, over what drags a card (its header, its handle).
assets/grabbing.cur: a closed hand, while a card is carried.
Each holds 32, 48 and 64 px images (Windows picks the one for the cursor size and
display scale), drawn on a 32-unit grid at 16x and scaled down, white with a black
outline like Windows' own cursors. Run: python3 tools/make_cursors.py [preview.png]
"""

import struct
import sys
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

GRID = 32          # the drawing's units
SCALE = 16         # drawn this much larger, then scaled down: smooth edges
SIZES = (32, 48, 64)
OUTLINE = 1.3      # the black edge, in units
HOTSPOT = (16, 18) # where it points: the middle of the palm, in units


def capsule(draw, a, b, width):
    """A stroke from a to b with round ends, `width` units wide."""
    s = SCALE
    r = width / 2 * s
    (x1, y1), (x2, y2) = (a[0] * s, a[1] * s), (b[0] * s, b[1] * s)
    draw.line([(x1, y1), (x2, y2)], fill=255, width=round(2 * r))
    for x, y in ((x1, y1), (x2, y2)):
        draw.ellipse([x - r, y - r, x + r, y + r], fill=255)


def rounded(draw, box, radius):
    s = SCALE
    draw.rounded_rectangle([v * s for v in box], radius=radius * s, fill=255)


FINGERS = (11.4, 15.0, 18.6, 22.0) # the fingers' middles, in units


def open_hand(draw):
    rounded(draw, (9.0, 13.5, 24.0, 27.0), 5.0)          # the palm
    for x, top in zip(FINGERS, (6.0, 4.2, 5.2, 8.4)):
        capsule(draw, (x, top), (x, 17.0), 3.3)           # four fingers
    capsule(draw, (5.6, 14.6), (10.6, 21.0), 3.4)         # the thumb, out to the left


def open_lines(draw):
    """The lines between the fingers, down to the palm: they read as four, not a block."""
    for left, right, top in ((FINGERS[0], FINGERS[1], 7.6), (FINGERS[1], FINGERS[2], 7.0), (FINGERS[2], FINGERS[3], 9.6)):
        x = (left + right) / 2
        capsule(draw, (x, top), (x, 15.0), 0.7)


def closed_hand(draw):
    rounded(draw, (9.0, 12.0, 24.0, 27.0), 5.0)           # the palm
    for x in FINGERS:
        capsule(draw, (x, 11.0), (x, 14.0), 3.3)          # knuckles: the fingers curled over
    capsule(draw, (7.8, 16.0), (11.0, 20.0), 3.4)         # the thumb, tucked in


def closed_lines(draw):
    for left, right in zip(FINGERS, FINGERS[1:]):
        x = (left + right) / 2
        capsule(draw, (x, 11.6), (x, 14.2), 0.7)


def render(shape, lines):
    """The hand at GRID * SCALE: white inside a black outline, black lines between its fingers."""
    big = GRID * SCALE
    mask = Image.new("L", (big, big), 0)
    shape(ImageDraw.Draw(mask))
    grow = round(OUTLINE * SCALE) * 2 + 1
    edge = mask.filter(ImageFilter.MaxFilter(grow))       # the shape grown by the outline
    inner = Image.new("L", (big, big), 0)
    lines(ImageDraw.Draw(inner))
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    image.paste((0, 0, 0, 255), (0, 0), edge)
    image.paste((255, 255, 255, 255), (0, 0), mask)
    image.paste((0, 0, 0, 255), (0, 0), ImageChops.multiply(inner, mask)) # inside the hand only
    return image


def dib(image):
    """One .cur image: BITMAPINFOHEADER, 32-bit BGRA rows bottom-up, then an all-zero AND mask."""
    w, h = image.size
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    rows = []
    pixels = image.load()
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for x in range(w):
            r, g, b, a = pixels[x, y]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    mask_row = ((w + 31) // 32) * 4
    return header + b"".join(rows) + b"\0" * (mask_row * h)


def write_cursor(path, big):
    images = [big.resize((s, s), Image.LANCZOS) for s in SIZES]
    blobs = [dib(i) for i in images]
    out = struct.pack("<HHH", 0, 2, len(blobs))
    offset = 6 + 16 * len(blobs)
    for size, blob in zip(SIZES, blobs):
        hx, hy = round(HOTSPOT[0] * size / GRID), round(HOTSPOT[1] * size / GRID)
        out += struct.pack("<BBBBHHII", size % 256, size % 256, 0, 0, hx, hy, len(blob), offset)
        offset += len(blob)
    Path(path).write_bytes(out + b"".join(blobs))


def main():
    root = Path(__file__).resolve().parent.parent / "assets"
    grab, grabbing = render(open_hand, open_lines), render(closed_hand, closed_lines)
    write_cursor(root / "grab.cur", grab)
    write_cursor(root / "grabbing.cur", grabbing)
    if len(sys.argv) > 1: # a sheet to look at: each size, on dark and light
        sheet = Image.new("RGBA", (2 * (sum(SIZES) + 40), 150), (32, 32, 32, 255))
        ImageDraw.Draw(sheet).rectangle([0, 75, sheet.width, 150], fill=(230, 230, 230, 255))
        x = 10
        for big in (grab, grabbing):
            for s in SIZES:
                small = big.resize((s, s), Image.LANCZOS)
                sheet.alpha_composite(small, (x, 5))
                sheet.alpha_composite(small, (x, 80))
                x += s + 6
            x += 20
        sheet.save(sys.argv[1])


if __name__ == "__main__":
    main()
