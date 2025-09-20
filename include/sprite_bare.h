#ifndef SPRITE_BARE_H
#define SPRITE_BARE_H

#include <stdint.h>
#define REG_DISPCNT   (*(volatile uint16_t*)0x04000000)
// --- OBJ 関連ビット ---
#define OBJ_ENABLE    0x1000  // OBJ表示を有効化
#define OBJ_1D_MAP    0x0040  // OBJ VRAMを1Dマッピング

// ===== GBA regs (tonc風の素朴版) =====
#define REG_DISPCNT   (*(volatile uint16_t*)0x04000000)
#define REG_VCOUNT    (*(volatile uint16_t*)0x04000006)

// Display control bits
#define DCNT_MODE0    0x0000
#define DCNT_BG0      0x0100
#define DCNT_OBJ      0x1000
#define DCNT_OBJ_1D   0x0040

// DMA3 (32bit) 簡易
#define REG_DMA3SAD   (*(volatile const void**)0x040000D4)
#define REG_DMA3DAD   (*(volatile void**)0x040000D8)
#define REG_DMA3CNT   (*(volatile uint32_t*)0x040000DC)
#define DMA_ENABLE    (1u<<31)
#define DMA_32        (1u<<26)

// OAM / OBJ VRAM / OBJ PAL
#define OAM16         ((volatile uint16_t*)0x07000000)   // attr0/1/2/...
#define OAM_ATTR(n)   (&OAM16[(n)*4])
#define OBJ_VRAM8     ((volatile uint8_t*) 0x06010000)   // 0x6010000
#define OBJ_PAL16     ((volatile uint16_t*)0x05000200)   // 16*16 entries

// attr ヘルパ
#define ATTR0_Y(y)        ((y) & 0x00FF)
#define ATTR0_MODE_REG    0x0000
#define ATTR0_4BPP        0x0000
#define ATTR0_SHAPE_SQ    0x0000
#define ATTR0_SHAPE_TALL  0x8000
#define ATTR0_SHAPE_WIDE  0x4000

#define ATTR1_X(x)        ((x) & 0x01FF)
#define ATTR1_SIZE_8x8    0x0000 // shape=square
#define ATTR1_SIZE_16x16  0x4000 // shape=square
#define ATTR1_SIZE_32x32  0x8000
#define ATTR1_SIZE_64x64  0xC000
// 縦長(TALL)のとき size=0/1/2/3 → 8x16/8x32/16x32/32x64
#define ATTR1_SIZE_TALL_16x32 0x8000

#define ATTR2_TILE(i)     ((i) & 0x03FF)
#define ATTR2_PBANK(p)    (((p) & 0x0F) << 12)
#ifndef ATTR1_SIZE
#  define ATTR1_SIZE(n)   ((n) << 14)   // n=0..3 (0=8px,1=16px,2=32px,3=64px)
#endif

// 公開API
void spr_init_mode0_obj1d(void);
void spr_wait_vblank(void);
void spr_dma_copy32(void* dst, const void* src, uint32_t words);

#endif
