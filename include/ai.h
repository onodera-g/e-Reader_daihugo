#ifndef AI_H
#define AI_H

#include "def.h"

/* 場の状態（AI評価用スナップショット） */
typedef struct {
    u8  field_visible;
    u8  revolution;
    u8  jback_active;
    u8  sibari_active;
    u8  right_neighbor_count;

    u8  field_count;       /* セット:枚数 / 階段:長さ */
    u8  field_eff_rank;    /* セット:必要有効ランク / 階段:トップの有効ランク */
    u8  field_suit_mask;   /* セット:場スート集合 / 階段:単一スートbit */
    const char* field_names[4];
    u8  field_is_straight; /* 1=階段, 0=セット */
} FieldState;

/* AI の打ち手選択（out_cards に u8 のカードID、out_n に枚数） */
int ai_choose_move_group(const Hand* hand,
                         const FieldState* fs,
                         u8 out_cards[4],
                         u8* out_n);

/* rank反転込みの有効ランク（革命/JBのXORで反転） */
u8 rank_effective_ext(u8 r, u8 rev, u8 jb);

#endif /* AI_H */
