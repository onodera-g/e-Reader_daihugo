#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# gba_bg_convert_split.py
# 240x160 画像を GBA BG0 用（4bpp/16色, 32x20 マップ=256x160）に変換し、
# grit 風の .c / .h を出力する。
#
# 仕様:
# - 16色量子化（Pillow ADAPTIVE）
# - 8x8 タイル化 & 重複排除
# - タイル0番＝空タイル（全インデックス0）を必ず先頭に挿入
# - 240x160 を左上に配置し、右端の 16px（=2タイル列）を空タイルでパディング
# - パレット0番色＝左上ピクセル色（空タイルの地色に使う）
# - 出力配列:
#     const unsigned int   <name>Tiles[ ... ];   // 1タイル=32B=8ワード
#     const unsigned short <name>Map  [ 640 ];   // 32x20
#     const unsigned short <name>Pal  [ 16 ];
#   さらに .h には <name>TilesLen / MapLen / PalLen を 32/1280/32 などで定義
#
# 使い方:
#   python3 gba_bg_convert_split.py input.png bg_title
# 出力先:
#   include/<name>.h
#   src/<name>.c

import sys
from pathlib import Path
from PIL import Image

def clamp5(x):  # 0..255 -> 0..31
    return (x * 31 + 127) // 255

def rgb_to_bgr555(rgb):
    r, g, b = rgb
    return (clamp5(b) << 10) | (clamp5(g) << 5) | clamp5(r)

def quantize_16(img_rgba):
    q = img_rgba.convert("P", palette=Image.ADAPTIVE, colors=16)
    pal = q.getpalette()[:16*3]
    palette_rgb = []
    for i in range(16):
        r = pal[i*3+0] if i*3+0 < len(pal) else 0
        g = pal[i*3+1] if i*3+1 < len(pal) else 0
        b = pal[i*3+2] if i*3+2 < len(pal) else 0
        palette_rgb.append((r, g, b))
    return q, palette_rgb

def remap_indices_make_bg0(img_indexed, palette_rgb):
    # 左上ピクセルの色をパレット0番にする（空タイルの地色に採用）
    w, h = img_indexed.size
    src = img_indexed.load()
    bg_idx = src[0, 0] & 0xF
    if bg_idx == 0:
        return img_indexed, palette_rgb
    # 0 と bg_idx をスワップ
    map_tbl = list(range(16))
    map_tbl[0], map_tbl[bg_idx] = map_tbl[bg_idx], map_tbl[0]
    img2 = Image.new("P", (w, h))
    img2.putpalette(sum(([r, g, b] for (r, g, b) in palette_rgb), []))
    dst = img2.load()
    for y in range(h):
        for x in range(w):
            dst[x, y] = map_tbl[src[x, y] & 0xF]
    pal2 = palette_rgb[:]
    pal2[0], pal2[bg_idx] = pal2[bg_idx], pal2[0]
    return img2, pal2

def image_to_tiles_map(qimg):
    # 入力は 240x160 を想定（8の倍数）。マップは 32x20（=256x160）にする。
    w, h = qimg.size
    if (w % 8) or (h % 8):
        raise SystemExit(f"ERROR: image size must be multiples of 8 (got {w}x{h})")
    if w > 256 or h > 160:
        raise SystemExit(f"ERROR: image must fit within 256x160 (got {w}x{h})")

    pix = qimg.load()
    # まずは画像の 30x20 タイル分だけタイル化して辞書化（重複排除）
    tiles = []               # list[list of 64 indices]
    tile_dict = {}           # bytes(64) -> tile index
    map_w_src = w // 8       # 30
    map_h = h // 8           # 20
    tilemap = [[0]*map_w_src for _ in range(map_h)]

    for ty in range(map_h):
        for tx in range(map_w_src):
            block = []
            for y in range(8):
                for x in range(8):
                    block.append(pix[tx*8 + x, ty*8 + y] & 0xF)
            key = bytes(block)
            if key in tile_dict:
                idx = tile_dict[key]
            else:
                idx = len(tiles)
                tile_dict[key] = idx
                tiles.append(block)
            tilemap[ty][tx] = idx

    # 先頭に「空タイル（全0）」を挿入し、参照を +1
    blank = [0]*64
    tiles.insert(0, blank)
    for row in tilemap:
        for i in range(len(row)):
            row[i] += 1

    # 32x20 に右側を空タイルでパディング（2 列分）
    map_w = 32
    tilemap_32x20 = [[0]*map_w for _ in range(map_h)]
    for ty in range(map_h):
        # 左 30 列は元の参照、右の 2 列は 0（空タイル）
        for tx in range(map_w):
            tilemap_32x20[ty][tx] = tilemap[ty][tx] if tx < map_w_src else 0

    return tiles, tilemap_32x20  # tiles[0] は空タイル

def pack_tiles_4bpp_u32_words(tiles):
    # 4bpp パック -> 各タイル 64px -> 32B -> 8 x u32
    words = []
    for t in tiles:
        # バイト列に
        b = bytearray()
        for i in range(0, 64, 2):
            b.append((t[i] & 0xF) | ((t[i+1] & 0xF) << 4))
        # 4バイトごとに u32 (リトルエンディアン) 化
        for i in range(0, 32, 4):
            w = b[i] | (b[i+1] << 8) | (b[i+2] << 16) | (b[i+3] << 24)
            words.append(w)
    return words  # len = tiles * 8

def to_h_guard(name):
    return f"GRIT_{name.upper()}_H"

def write_header(h_path, name, tiles_len_bytes, map_len_bytes, pal_len_bytes):
    guard = to_h_guard(name)
    with open(h_path, "w", encoding="utf-8") as f:
        f.write("//{{BLOCK(%s)\n\n" % name)
        f.write("//======================================================================\n//\n")
        f.write("//\t%s, 256x160@4, \n" % name)
        f.write("//\t+ palette 16 entries, not compressed\n")
        f.write("//\t+ tiles (t|f|p reduced) not compressed\n")
        f.write("//\t+ regular map (flat), not compressed, 32x20 \n")
        total = pal_len_bytes + tiles_len_bytes + map_len_bytes
        f.write("//\tTotal size: %d + %d + %d = %d\n//\n" %
                (pal_len_bytes, tiles_len_bytes, map_len_bytes, total))
        f.write("//======================================================================\n\n")
        f.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
        f.write(f"#define {name}TilesLen {tiles_len_bytes}\n")
        f.write(f"extern const unsigned int {name}Tiles[{tiles_len_bytes//4}];\n\n")
        f.write(f"#define {name}MapLen {map_len_bytes}\n")
        f.write(f"extern const unsigned short {name}Map[{map_len_bytes//2}];\n\n")
        f.write(f"#define {name}PalLen {pal_len_bytes}\n")
        f.write(f"extern const unsigned short {name}Pal[16];\n\n")
        f.write("#endif // %s\n\n" % guard)
        f.write("//}}BLOCK(%s)\n" % name)

def chunked(iterable, n):
    it = iter(iterable)
    while True:
        buf = []
        try:
            for _ in range(n):
                buf.append(next(it))
        except StopIteration:
            if buf:
                yield buf
            break
        yield buf

def write_source(c_path, h_basename, name, tiles_words, map_entries, pal_bgr555):
    with open(c_path, "w", encoding="utf-8") as f:
        f.write("//{{BLOCK(%s)\n\n" % name)
        f.write("//======================================================================\n//\n")
        f.write("//\t%s, 256x160@4,\n" % name)
        f.write("//\t+ palette 16 entries, not compressed\n")
        f.write("//\t+ tiles (t|f|p reduced) not compressed\n")
        f.write("//\t+ regular map (flat), not compressed, 32x20\n")
        f.write("//\tTotal size: 32 + %d + 1280 = %d\n//\n" %
                (len(tiles_words)*4, 32 + len(tiles_words)*4 + 1280))
        f.write("//======================================================================\n\n")
        f.write(f'#include "{h_basename}"\n\n')

        # Tiles
        f.write(f"const unsigned int {name}Tiles[{len(tiles_words)}] __attribute__((aligned(4))) =\n{{\n")
        for row in chunked(tiles_words, 8):  # 8 words(=1タイル)単位で改行
            f.write("    " + ",".join(f"0x{w:08X}" for w in row) + ",\n")
        f.write("};\n\n")

        # Map (32x20 = 640 entries)
        f.write(f"const unsigned short {name}Map[640] __attribute__((aligned(4))) =\n{{\n")
        # 1行16エントリ表示（見やすさ用）
        for row in chunked(map_entries, 16):
            f.write("    " + ",".join(f"0x{v:04X}" for v in row) + ",\n")
        f.write("};\n\n")

        # Palette (16 entries)
        f.write(f"const unsigned short {name}Pal[16] __attribute__((aligned(4))) =\n{{\n")
        f.write("    " + ",".join(f"0x{p:04X}" for p in pal_bgr555) + "\n")
        f.write("};\n\n")
        f.write("//}}BLOCK(%s)\n" % name)

def main():
    if len(sys.argv) < 3:
        print("usage: gba_bg_convert_split.py <input_image> <name_base>", file=sys.stderr)
        print("example: gba_bg_convert_split.py assets/title.png bg_title", file=sys.stderr)
        sys.exit(1)

    in_img = Path(sys.argv[1])
    name   = sys.argv[2]  # 出力名ベース: bg_title -> include/bg_title.h, src/bg_title.c

    # 出力パス
    proj_root = Path.cwd()
    out_h = proj_root / "include" / f"{name}.h"
    out_c = proj_root / "src"     / f"{name}.c"
    out_h.parent.mkdir(parents=True, exist_ok=True)
    out_c.parent.mkdir(parents=True, exist_ok=True)

    # 画像読み込み
    img = Image.open(in_img).convert("RGBA")
    w, h = img.size
    if w != 240 or h != 160:
        print(f"NOTE: input is {w}x{h}. This tool assumes 240x160 and pads to 256x160 on the right.", file=sys.stderr)
    if (w % 8) or (h % 8):
        raise SystemExit("ERROR: input size must be multiples of 8")
    if w > 256 or h > 160:
        raise SystemExit("ERROR: input must fit within 256x160")

    # 量子化（16色）
    qimg, pal_rgb = quantize_16(img)

    # 左上色をパレット0番へ
    qimg, pal_rgb = remap_indices_make_bg0(qimg, pal_rgb)

    # タイル化 & 右側パディング（32x20 マップ）
    tiles, tilemap = image_to_tiles_map(qimg)

    # 4bpp パック（u32ワード列）
    tiles_words = pack_tiles_4bpp_u32_words(tiles)
    tiles_len_bytes = len(tiles_words) * 4  # 1ワード=4B

    # マップ（下位10bit=タイル番号、パレット=0、H/Vフリップ=0）
    map_entries = []
    for ty in range(20):
        for tx in range(32):
            idx = tilemap[ty][tx] & 0x03FF
            entry = idx
            map_entries.append(entry)
    map_len_bytes = len(map_entries) * 2  # u16

    # パレット（BGR555）
    pal_bgr555 = [rgb_to_bgr555(rgb) for rgb in pal_rgb]
    pal_len_bytes = 32  # 16*2

    # .h
    write_header(out_h, name, tiles_len_bytes, map_len_bytes, pal_len_bytes)
    # .c
    write_source(out_c, f"{name}.h", name, tiles_words, map_entries, pal_bgr555)

    print(f"[OK] Wrote:\n  {out_h}\n  {out_c}")
    print(f"     Tiles: {len(tiles_words)//8} tiles ({tiles_len_bytes} bytes)")
    print(f"     Map:   32x20 ({map_len_bytes} bytes)")
    print(f"     Pal:   16 entries ({pal_len_bytes} bytes)")

if __name__ == "__main__":
    main()
