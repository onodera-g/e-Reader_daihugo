#ifndef RNG_H
#define RNG_H

#include "def.h"

/* --- ランダム待機からシード生成 --- */
void rng_seed(int min_frames, int max_frames);

/* --- 32bit 乱数を返す（xorshift32）--- */
u32  rng_next(void);

/* --- (lo, hi) の半開区間で一様乱数を返す（hi <= lo の場合は lo を返す）--- */
int  rng_range(int lo, int hi);


#endif /* RNG_H */
