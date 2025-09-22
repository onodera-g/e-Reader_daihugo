#include "rng.h"

/*
 * rng.c
 * -----
 * Xorshift アルゴリズムを使った軽量乱数生成器。
 * GBA のような環境でも高速に動作する。
 *
 * 内部に g_rng という 32bit の状態を持ち、
 * rng_seed() で初期化し、rng_next() で次の乱数を返す。
 */

static u32 g_rng = 2463534242u;  // 初期状態（シード未指定時のデフォルト値）

/* 乱数の種を初期化する */
void rng_seed(u32 s){
    g_rng = (s ? s : 2463534242u);
}

/* 次の乱数値 (32bit) を返す */
u32 rng_next(void){
    u32 x = g_rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_rng = x;
    return g_rng;
}

/* lo 以上 hi 未満の整数乱数を返す */
int rng_range(int lo, int hi){
    if (hi <= lo) return lo;  // 範囲が不正なら lo を返す
    u32 span = (u32)(hi - lo);
    return lo + (int)(rng_next() % span);
}
