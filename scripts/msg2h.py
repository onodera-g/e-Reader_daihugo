#!/usr/bin/env python3
#UTF-8のテキストをShift_JISのCバイト列に変換
import sys, pathlib, re

def norm_key(k: str) -> str:
    k = k.strip()
    k = re.sub(r'[^A-Za-z0-9_]', '_', k)
    return k.upper()

def parse_txt(path: pathlib.Path):
    kv = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        if "=" in s:
            k, v = s.split("=", 1)
            kv[norm_key(k)] = v
    return kv

def to_sjis_bytes_literal(s: str) -> str:
    b = s.encode("shift_jis")  # ここでUTF-8→SJIS
    # C文字列のバイト列（\xHH\xHH...）として出力
    return "".join(f"\\x{byte:02X}" for byte in b)

def emit_header(out_path: pathlib.Path, kv: dict):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("// Auto-generated from text/messages_jpn.txt (UTF-8 -> Shift_JIS). DO NOT EDIT.\n")
        for k, v in kv.items():
            lit = to_sjis_bytes_literal(v)
            f.write(f"static const unsigned char MSG_{k}[] = \"{lit}\";\n")

if __name__ == "__main__":
    # usage: msg2h.py text/messages_jpn.txt include/messages_autogen.h
    in_txt = pathlib.Path(sys.argv[1])
    out_h  = pathlib.Path(sys.argv[2])
    kv = parse_txt(in_txt)
    emit_header(out_h, kv)
