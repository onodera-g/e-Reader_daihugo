#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 240x160 PNG と、領域JSON([{name,x,y,w,h},...])を GBA OBJ(4bpp)用の
# obj_atlas.c/.h にまとめる。
# python3 scripts/gba_obj_convert_atlas_regions.py assets/splite.png assets/splite_regions.json obj_atlas

import sys, json
from pathlib import Path
from PIL import Image

# ==== 固定設定（GBA 正式仕様）====
TILE_ORDER = "row"              # 行優先でタイルを並べる
LEFT_PIXEL_IS_LOW_NIBBLE = True # 左=下位/右=上位（必須）
# =================================

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

def size_code(px):
    # ERAPIサイズコード: 8->0x08, 16->0x06, 32->0x04, 64->0x02
    return 0x08 if px <= 8 else (0x06 if px <= 16 else (0x04 if px <= 32 else 0x02))

WORDS_PER_TILE = 8  # 8x8(4bpp)=32B=8words

def rect_to_words(pix, x, y, w_px, h_px):
    assert (w_px % 8) == 0 and (h_px % 8) == 0
    WT = w_px // 8  # 横タイル数
    HT = h_px // 8  # 縦タイル数
    out_words = []

    # 行優先（左→右、上→下）
    for ty in range(HT):
        for tx in range(WT):
            base_x = x + tx * 8
            base_y = y + ty * 8
            tile_bytes = bytearray(32)
            bi = 0
            for yy in range(8):
                row_y = base_y + yy
                p0 = pix[base_x + 0, row_y] & 0xF
                p1 = pix[base_x + 1, row_y] & 0xF
                p2 = pix[base_x + 2, row_y] & 0xF
                p3 = pix[base_x + 3, row_y] & 0xF
                p4 = pix[base_x + 4, row_y] & 0xF
                p5 = pix[base_x + 5, row_y] & 0xF
                p6 = pix[base_x + 6, row_y] & 0xF
                p7 = pix[base_x + 7, row_y] & 0xF

                # GBA 正式: 左=下位 nibble
                tile_bytes[bi+0] = (p1 << 4) | p0
                tile_bytes[bi+1] = (p3 << 4) | p2
                tile_bytes[bi+2] = (p5 << 4) | p4
                tile_bytes[bi+3] = (p7 << 4) | p6
                bi += 4

            # 32B -> 8 words
            for i in range(0, 32, 4):
                w = (tile_bytes[i+0]
                    | (tile_bytes[i+1] << 8)
                    | (tile_bytes[i+2] << 16)
                    | (tile_bytes[i+3] << 24))
                out_words.append(w)

    return out_words

def write_outputs(proj_root, base, tiles_words, pal_bgr, descs, names):
    inc = proj_root / "include" / f"{base}.h"
    src = proj_root / "src"     / f"{base}.c"
    inc.parent.mkdir(parents=True, exist_ok=True)
    src.parent.mkdir(parents=True, exist_ok=True)

    guard = f"GRIT_{base.upper()}_H"
    with open(inc, "w", encoding="utf-8") as f:
        f.write("//{{BLOCK(%s)\n\n" % base)
        f.write("//======================================================================\n//\n")
        f.write("//  OBJ atlas %s, 4bpp, shared 16-color palette\n" % base)
        f.write("//======================================================================\n\n")
        f.write("#ifndef %s\n#define %s\n\n" % (guard, guard))

        f.write("typedef struct {\n")
        f.write("  unsigned short w, h;\n")
        f.write("  unsigned short tiles_per_frame;\n")
        f.write("  unsigned short frames;\n")
        f.write("  unsigned int   offset_words;\n")
        f.write("  unsigned char  width_code;\n")
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
        f.write(f"const unsigned int {base}Tiles[{len(tiles_words)}] __attribute__((aligned(4))) = {{\n")
        for i, w in enumerate(tiles_words):
            f.write(("  " if i%8==0 else "") + f"0x{w:08X}," + ("\n" if i%8==7 else " "))
        if len(tiles_words)%8: f.write("\n")
        f.write("};\n\n")

        f.write(f"const unsigned short {base}Pal[16] __attribute__((aligned(4))) = {{\n  ")
        f.write(",".join(f"0x{p:04X}" for p in pal_bgr))
        f.write("\n};\n\n")

        f.write(f"const ObjSpriteDesc objAtlasSprites[{len(descs)}] = {{\n")
        for d in descs:
            f.write(f"  {{ {d['w']}, {d['h']}, {d['tiles_per_frame']}, 1, {d['offset_words']}, 0x{d['wcode']:02X}, 0x{d['hcode']:02X} }},\n")
        f.write("};\n\n")

        f.write(f"const char* objAtlasNames[{len(descs)}] = {{\n")
        for nm in names:
            safe = nm.replace('\\','\\\\').replace('"','\\"')
            f.write(f'  "{safe}",\n')
        f.write("};\n\n")

        f.write("static int _cmp_str(const char* a, const char* b){ while(*a && *a==*b){++a;++b;} return (unsigned char)*a-(unsigned char)*b; }\n")
        f.write("int objAtlasFindIndex(const char* name){ if(!name) return -1; for(int i=0;i<OBJ_ATLAS_SPRITE_COUNT;++i){ const char* s=objAtlasNames[i]; if(s && _cmp_str(s,name)==0) return i;} return -1; }\n")

def main():
    if len(sys.argv) < 4:
        print("usage: gba_obj_convert_atlas_regions.py <image> <manifest.json> <base_name>", file=sys.stderr)
        sys.exit(1)

    img_path   = Path(sys.argv[1])
    manifest   = Path(sys.argv[2])
    base       = sys.argv[3]
    proj_root  = Path.cwd()

    img = Image.open(img_path).convert("RGBA")
    imgP, pal_rgb = quantize_16(img)
    pal_bgr = [rgb_to_bgr555(rgb) for rgb in pal_rgb]
    pix = imgP.load()

    rects = json.loads(manifest.read_text(encoding="utf-8"))
    if not isinstance(rects, list):
        raise SystemExit("manifest must be a list of {name,x,y,w,h}")

    tiles_words = []
    descs, names = [], []

    for r in rects:
        name = str(r["name"]); x=int(r["x"]); y=int(r["y"]); w=int(r["w"]); h=int(r["h"])
        if (w|h) & 7: raise SystemExit(f"{name}: w,h must be multiples of 8")
        offset_words = len(tiles_words)
        tiles_words += rect_to_words(pix, x, y, w, h)

        descs.append({
            "name": name,
            "w": w, "h": h,
            "tiles_per_frame": (w//8)*(h//8),
            "offset_words": offset_words,
            "wcode": size_code(w), "hcode": size_code(h),
        })
        names.append(name)

    write_outputs(proj_root, base, tiles_words, pal_bgr, descs, names)
    print(f"[OK] include/{base}.h, src/{base}.c 生成")

if __name__ == "__main__":
    main()
