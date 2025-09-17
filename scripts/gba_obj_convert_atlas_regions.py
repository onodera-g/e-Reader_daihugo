#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 画像(240x160想定) + 矩形マニフェスト(JSON: [{name,x,y,w,h},...]) を
# GBA OBJ(4bpp/16色)の「単一 .c/.h（grit風）」にまとめる。
#
# 出力:
#   include/<base>.h
#   src/<base>.c
# 内容:
#   - const unsigned int   <base>Tiles[];   // 全スプライトを連結（4bpp→u32 words）
#   - const unsigned short <base>Pal[16];   // 共有16色パレット（画像全体から抽出）
#   - typedef struct ObjSpriteDesc {...};
#   - const ObjSpriteDesc  objAtlasSprites[];     // 各スプライトのメタ（offset/size等）
#   - const char*          objAtlasNames[];       // 名前一覧（マニフェストの順）
#   - int objAtlasFindIndex(const char* name);    // 名前→インデックス（線形検索）
#
# 使い方:
#   python3 scripts/gba_obj_convert_atlas_regions.py assets/splite.png assets/splite_regions.json obj_atlas
#
# 注意:
#   - 各領域の w,h は 8 の倍数にしてください（OBJは8x8タイル単位）
#   - 画像の色数は 16 色に量子化されます（OBJは背景とは別パレット）

import sys, json
from pathlib import Path
from PIL import Image

def clamp5(x): return (x * 31 + 127) // 255
def rgb_to_bgr555(rgb):
    r,g,b = rgb
    return (clamp5(b) << 10) | (clamp5(g) << 5) | clamp5(r)

def quantize_16(img_rgba):
    q = img_rgba.convert("P", palette=Image.ADAPTIVE, colors=16)
    pal = q.getpalette()[:16*3]
    pal_rgb = []
    for i in range(16):
        r = pal[i*3+0] if i*3+0 < len(pal) else 0
        g = pal[i*3+1] if i*3+1 < len(pal) else 0
        b = pal[i*3+2] if i*3+2 < len(pal) else 0
        pal_rgb.append((r,g,b))
    return q, pal_rgb

def pack_tile_to_words(tile64):
    # 64px (0..15) -> 32B -> 8 * u32(LE)
    b = bytearray()
    for i in range(0, 64, 2):
        b.append((tile64[i] & 0xF) | ((tile64[i+1] & 0xF) << 4))
    words = []
    for i in range(0, 32, 4):
        words.append(b[i] | (b[i+1] << 8) | (b[i+2] << 16) | (b[i+3] << 24))
    return words

def rect_to_words(imgP, x, y, w, h):
    if w % 8 or h % 8:
        raise SystemExit(f"ERROR: region size must be multiples of 8 (got {w}x{h})")
    pix = imgP.load()
    words = []
    for ty in range(h//8):
        for tx in range(w//8):
            tile = []
            for yy in range(8):
                for xx in range(8):
                    tile.append(pix[x+tx*8+xx, y+ty*8+yy] & 0xF)
            words.extend(pack_tile_to_words(tile))
    return words  # 8 words per 8x8 tile

def size_code(px):
    # ERAPIサイズコード: 8->0x08, 16->0x06, 32->0x04, 64->0x02（範囲超は丸め）
    return 0x08 if px <= 8 else (0x06 if px <= 16 else (0x04 if px <= 32 else 0x02))

def write_outputs(proj_root, base, tiles_words, pal_bgr, descs, names):
    inc = proj_root / "include" / f"{base}.h"
    src = proj_root / "src"     / f"{base}.c"
    inc.parent.mkdir(parents=True, exist_ok=True)
    src.parent.mkdir(parents=True, exist_ok=True)

    guard = f"GRIT_{base.upper()}_H"
    with open(inc, "w", encoding="utf-8") as f:
        f.write("//{{BLOCK(%s)\n\n" % base)
        f.write("//======================================================================\n//\n")
        f.write("//\tOBJ atlas %s, 4bpp, shared 16-color palette\n" % base)
        f.write("//======================================================================\n\n")
        f.write("#ifndef %s\n#define %s\n\n" % (guard, guard))

        f.write("typedef struct {\n")
        f.write("  unsigned short w, h;\n")
        f.write("  unsigned short tiles_per_frame;\n")
        f.write("  unsigned short frames;\n")
        f.write("  unsigned int   offset_words; // Tiles[] 先頭からの u32 words オフセット\n")
        f.write("  unsigned char  width_code;   // 8/16/32/64 → 0x08/0x06/0x04/0x02\n")
        f.write("  unsigned char  height_code;\n")
        f.write("} ObjSpriteDesc;\n\n")

        f.write(f"#define {base}TilesLen {len(tiles_words)*4}\n")
        f.write(f"extern const unsigned int {base}Tiles[{len(tiles_words)}];\n")
        f.write(f"#define {base}PalLen 32\n")
        f.write(f"extern const unsigned short {base}Pal[16];\n\n")

        f.write(f"extern const ObjSpriteDesc objAtlasSprites[{len(descs)}];\n")
        f.write(f"extern const char* objAtlasNames[{len(descs)}];\n")
        f.write(f"#define OBJ_ATLAS_SPRITE_COUNT {len(descs)}\n\n")

        f.write("int objAtlasFindIndex(const char* name);\n\n")
        f.write("#endif\n\n")
        f.write("//}}BLOCK(%s)\n" % base)

    with open(src, "w", encoding="utf-8") as f:
        f.write(f'#include "{base}.h"\n\n')
        # Tiles
        f.write(f"const unsigned int {base}Tiles[{len(tiles_words)}] __attribute__((aligned(4))) =\n{{\n")
        for i, w32 in enumerate(tiles_words):
            if i % 8 == 0: f.write("    ")
            f.write(f"0x{w32:08X},")
            if i % 8 == 7: f.write("\n")
            else: f.write(" ")
        if len(tiles_words) % 8 != 0: f.write("\n")
        f.write("};\n\n")
        # Palette
        f.write(f"const unsigned short {base}Pal[16] __attribute__((aligned(4))) =\n{{\n")
        f.write("    " + ",".join(f"0x{p:04X}" for p in pal_bgr) + "\n};\n\n")
        # Descriptors
        f.write(f"const ObjSpriteDesc objAtlasSprites[{len(descs)}] = {{\n")
        for d in descs:
            f.write(f"  {{ {d['w']}, {d['h']}, {d['tiles_per_frame']}, {d['frames']}, {d['offset_words']}, 0x{d['wcode']:02X}, 0x{d['hcode']:02X} }},\n")
        f.write("};\n\n")
        # Names
        f.write(f"const char* objAtlasNames[{len(descs)}] = {{\n")
        for nm in names:
            # C 文字列でエスケープが必要な場合は簡易対応
            safe = nm.replace('\\', '\\\\').replace('"', '\\"')
            f.write(f'  "{safe}",\n')
        f.write("};\n\n")
        # Find-by-name (線形検索)
        f.write("#include <string.h>\n")
        f.write("int objAtlasFindIndex(const char* name){\n")
        f.write("  if(!name) return -1;\n")
        f.write("  for(int i=0;i<OBJ_ATLAS_SPRITE_COUNT;++i){\n")
        f.write("    const char* s = objAtlasNames[i];\n")
        f.write("    if(s && strcmp(s, name)==0) return i;\n")
        f.write("  }\n")
        f.write("  return -1;\n")
        f.write("}\n")

def main():
    if len(sys.argv) < 4:
        print("usage: gba_obj_convert_atlas_regions.py <image> <manifest.json> <base_name>", file=sys.stderr)
        sys.exit(1)

    img_path   = Path(sys.argv[1])
    manifest   = Path(sys.argv[2])
    base       = sys.argv[3]
    proj_root  = Path.cwd()

    img = Image.open(img_path).convert("RGBA")
    if img.width != 240 or img.height != 160:
        print(f"NOTE: source image is {img.width}x{img.height} (expected 240x160). Continuing.", file=sys.stderr)

    # 画像全体から共通16色パレットを抽出（OBJは背景とは別パレット）
    imgP, pal_rgb = quantize_16(img)
    pal_bgr = [rgb_to_bgr555(rgb) for rgb in pal_rgb]

    # マニフェスト読み込み
    rects = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(rects, list):
        raise SystemExit("manifest must be a list of {name,x,y,w,h}")

    tiles_words = []
    descs = []
    names = []

    for r in rects:
        try:
            name = str(r["name"])
            x = int(r["x"]); y = int(r["y"]); w = int(r["w"]); h = int(r["h"])
        except Exception:
            raise SystemExit("each rect must have name,x,y,w,h (ints)")

        if x < 0 or y < 0 or x+w > img.width or y+h > img.height:
            raise SystemExit(f"region {name} out of bounds")
        if w % 8 or h % 8:
            raise SystemExit(f"region {name} size must be multiples of 8 (got {w}x{h})")

        offset_words = len(tiles_words)
        words = rect_to_words(imgP, x, y, w, h)
        tiles_words.extend(words)

        tiles_per_frame = (w//8) * (h//8)
        descs.append({
            "name": name,
            "w": w, "h": h,
            "tiles_per_frame": tiles_per_frame,
            "frames": 1,
            "offset_words": offset_words,
            "wcode": size_code(w),
            "hcode": size_code(h),
        })
        names.append(name)

    write_outputs(proj_root, base, tiles_words, pal_bgr, descs, names)
    print("[OK] generated:", proj_root / "include" / f"{base}.h")
    print("[OK] generated:", proj_root / "src"     / f"{base}.c")
    print(f"sprites: {len(descs)}, tiles words: {len(tiles_words)}")

if __name__ == "__main__":
    main()
