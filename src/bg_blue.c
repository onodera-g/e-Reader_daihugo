//{{BLOCK(bg_blue)

//======================================================================
//
//  bg_blue, 256x160@4,
//  + palette 16 entries, not compressed
//  + 1 tiles (t|f|p reduced) not compressed
//  + regular map (flat), not compressed, 32x20
//  Total size: 32 + 32 + 1280 = 1344
//
//======================================================================

#include "bg_blue.h"

// 8x8 4bpp タイル：全ピクセル “色3(=青)” にする
const unsigned int bg_blueTiles[8] __attribute__((aligned(4))) =
{
    0x33333333,0x33333333,0x33333333,0x33333333,
    0x33333333,0x33333333,0x33333333,0x33333333,
};

// 32x20 = 640 マップ。全部タイル0を敷く
const unsigned short bg_blueMap[640] __attribute__((aligned(4))) =
{
#define Z0 0x0000
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,

    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,

    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,

    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,

    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0,
    Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0, Z0,Z0,Z0,Z0,Z0,Z0,Z0,Z0
#undef Z0
};

// パレット: 0=黒, 3=青（他は未使用）
const unsigned short bg_bluePal[16] __attribute__((aligned(4))) =
{
0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00, 0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00
};

//}}BLOCK(bg_blue)
