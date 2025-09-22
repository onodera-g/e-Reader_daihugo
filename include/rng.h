#ifndef RNG_H
#define RNG_H

#include "def.h"

/* 
 * 乱数生成モジュールのインタフェース宣言。
 * 
 * - rng_seed(s): 乱数の種を初期化する
 * - rng_next() : 次の乱数値 (u32) を返す
 * - rng_range(lo, hi): lo 以上 hi 未満の範囲で乱数を返す
 *
 */

void rng_seed(u32 s);
u32  rng_next(void);
int  rng_range(int lo, int hi);

#endif /* RNG_H */
