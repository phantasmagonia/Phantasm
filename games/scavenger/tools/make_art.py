"""Generates Scavenger's pixel-art tiles into ../assets (original art).

Usage: python3 make_art.py
Art is drawn at 1x and displayed at 2x by the game.
"""
import pathlib
import struct
import zlib

OUT = pathlib.Path(__file__).resolve().parent.parent / "assets"
CLEAR = (0, 0, 0, 0)


def write_png(path, pixels):
    height, width = len(pixels), len(pixels[0])
    raw = b"".join(b"\x00" + bytes(c for px in row for c in px) for row in pixels)

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    path.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(raw, 9))
        + chunk(b"IEND", b"")
    )


def in_diamond(px, py, cx, cy, half_w, half_h):
    return abs(px + 0.5 - cx) / half_w + abs(py + 0.5 - cy) / half_h <= 1.0


def outline(pixels, color):
    height, width = len(pixels), len(pixels[0])
    result = [row[:] for row in pixels]
    for y in range(height):
        for x in range(width):
            if pixels[y][x] == CLEAR:
                continue
            for nx, ny in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                if not (0 <= nx < width and 0 <= ny < height) or pixels[ny][nx] == CLEAR:
                    result[y][x] = color
                    break
    return result


def floor_tile(base, line, border, stripes):
    """32x16 diamond. stripes: draw plank lines every 4 px along one axis."""
    pixels = [[CLEAR] * 32 for _ in range(16)]
    for y in range(16):
        for x in range(32):
            if in_diamond(x, y, 16, 8, 16, 8):
                plank = stripes and (x // 2 + y) % 6 == 0
                pixels[y][x] = line if plank else base
    return outline(pixels, border)


def wall_block():
    """32x64: a full-story block. Top diamond rows 0-15, base diamond centered at y=56."""
    top, left, right, border = (196, 176, 150, 255), (150, 128, 104, 255), (112, 94, 76, 255), (52, 42, 36, 255)
    pixels = [[CLEAR] * 32 for _ in range(64)]
    for y in range(64):
        for x in range(32):
            dx = abs(x + 0.5 - 16) / 16
            reach = 8 * (1 - dx)
            if in_diamond(x, y, 16, 8, 16, 8):
                pixels[y][x] = top
            elif 8 - reach <= y + 0.5 <= 56 + reach:
                pixels[y][x] = left if x < 16 else right
    for y in range(16, 56):  # vertical edge between the two side faces
        pixels[y][15] = border if pixels[y][15] != CLEAR else CLEAR
    return outline(pixels, border)


def from_art(art, palette):
    return [[palette[ch] for ch in row] for row in art]


PLAYER = [
    "................",
    "......KKKK......",
    ".....KSSSSK.....",
    ".....KSESEK.....",
    ".....KSSSSK.....",
    "......KSSK......",
    "....KKJJJJKK....",
    "...KJJJJJJJJK...",
    "...KJJBBJJJJK...",
    "...KJJBBJJJJK...",
    "...KSJJJJJJSK...",
    "....KJJJJJJK....",
    "....KPPPPPPK....",
    "....KPPKKPPK....",
    "....KPPKKPPK....",
    "....KPPKKPPK....",
    "....KPPKKPPK....",
    "....KHHKKHHK....",
    "....KKKK.KKKK...",
    "................",
]
PLAYER_PALETTE = {
    ".": CLEAR, "K": (30, 28, 34, 255), "S": (226, 186, 150, 255), "E": (30, 28, 34, 255),
    "J": (176, 92, 52, 255), "B": (120, 60, 34, 255), "P": (62, 74, 110, 255), "H": (70, 52, 40, 255),
}

SUPPLY = [
    "..............",
    ".KKKKKKKKKKKK.",
    ".KTTTTTTTTTTK.",
    ".KKKKKKKKKKKK.",
    ".KCCCCCCCCCCK.",
    ".KCCLLLLLLCCK.",
    ".KCCLRRRRLCCK.",
    ".KCCLRWWRLCCK.",
    ".KCCLRRRRLCCK.",
    ".KCCLLLLLLCCK.",
    ".KCCCCCCCCCCK.",
    ".KKKKKKKKKKKK.",
]
SUPPLY_PALETTE = {
    ".": CLEAR, "K": (40, 30, 24, 255), "T": (214, 170, 96, 255), "C": (176, 128, 64, 255),
    "L": (240, 230, 210, 255), "R": (200, 60, 50, 255), "W": (255, 255, 255, 255),
}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    write_png(OUT / "floor_wood.png", floor_tile((158, 112, 72, 255), (132, 92, 58, 255), (96, 66, 42, 255), True))
    write_png(OUT / "floor_grass.png", floor_tile((92, 140, 72, 255), (84, 128, 66, 255), (70, 108, 56, 255), False))
    write_png(OUT / "wall.png", wall_block())
    write_png(OUT / "player.png", from_art(PLAYER, PLAYER_PALETTE))
    write_png(OUT / "supply.png", from_art(SUPPLY, SUPPLY_PALETTE))


if __name__ == "__main__":
    main()
