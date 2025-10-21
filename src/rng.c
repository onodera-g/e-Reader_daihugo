#include "def.h"
#include "rng.h"
#include <stdint.h>

/* GBAハードウェアレジスタ */
#ifndef REG_BASE
#define REG_BASE 0x04000000
#endif
#define REG_VCOUNT   (*(volatile unsigned short*)(REG_BASE + 0x0006))
#define REG_TM0CNT_L (*(volatile unsigned short*)(REG_BASE + 0x0100))
#define REG_DIV      (*(volatile unsigned int  *)(REG_BASE + 0x0204))

/* xorshift32 本体 */
static u32 s_rng_state = 2463534242u; /* 非0で初期化（全0は停止状態） */

/* ランダムな待機時間の生成 */
static inline void wait_vblank_start_inline(void){
    /* VBlankの先頭を待つ：描画タイミング合わせ兼、時間経過の担保 */
    while (REG_VCOUNT >= 160);
    while (REG_VCOUNT < 160);
}

/* min..max の範囲で「擬似ランダム」な待機フレーム数を決める */
static int decide_wait_frames_(int min_frames, int max_frames){
    if (min_frames < 0) min_frames = 0;
    if (max_frames < min_frames) max_frames = min_frames;

    /* DIV は常時カウントアップ、TM0/VCOUNT と XOR → LCG で拡散 */
    u32 div  = REG_DIV;
    u32 t0   = (u32)REG_TM0CNT_L;
    u32 vcnt = (u32)REG_VCOUNT;

    u32 span = (u32)(max_frames - min_frames + 1);
    u32 x = div ^ (t0 << 16) ^ vcnt;
    x = x * 1664525u + 1013904223u;

    int add = (int)(x % span);
    return min_frames + add;
}

/* --- ランダム待機からシード生成 --- */
void rng_seed(int min_frames, int max_frames){
    /* 1) フレーム数だけ待つ（VBlank同期） */
    int frames = decide_wait_frames_(min_frames, max_frames);
    for (int i = 0; i < frames; ++i){
        wait_vblank_start_inline();
    }

    /* 2) 待機後にハード由来の揺らぎを混ぜて seed を生成 */
    u32 seed = 0x9E3779B9u;

    /* VCOUNT/TM0/DIV を何度かサンプリングして拡散 */
    for (int i = 0; i < 64; ++i){
        /* 極軽いスピンでサンプリング位相をずらす */
        u32 spin = (seed ^ 0x27D4EB2Du) & 0x3FFu;
        while (spin--) { __asm__ volatile ("":::"memory"); }

        u32 mix = REG_DIV ^ ((u32)REG_TM0CNT_L << 16) ^ (u32)REG_VCOUNT;
        seed ^= mix;
        seed = seed * 1664525u + 1013904223u; /* LCGで拡散 */
    }

    /* 最後にビット分散（Avalanche）＋全0回避 */
    seed ^= (seed >> 16);
    seed *= 0x45D9F3Bu;
    seed ^= (seed >> 16);
    if (seed == 0) seed = 2463534242u;
}

/* --- 32bit 乱数を返す（xorshift32）--- */
u32 rng_next(void){
    u32 x = s_rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_rng_state = x;
    return x;
}

/* --- (lo, hi) の半開区間で一様乱数を返す（hi <= lo の場合は lo を返す）--- */
int rng_range(int lo, int hi){
    if (hi <= lo) return lo;
    u32 span = (u32)(hi - lo);
    return lo + (int)(rng_next() % span);
}



