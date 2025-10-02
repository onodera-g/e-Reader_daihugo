#ifndef DECK_H
#define DECK_H

#include "def.h"
#include "game.h"  /* Hand */

/* 既存API */
int  build_deck(const char** out);
void shuffle_deck(const char** a, int n);
void deal_round_robin_offset(const char** deck, int deck_n, int start_player, Hand hands[PLAYERS]);

/* ──カードから数字情報を抽出 ──
   - 3..13=K, A=14, 2=15, Joker=16 */
u8   eval_rank_from_name(const char* card_name);

/* ── カードからスート情報を情報を抽出 ──
   - スートID（♥0, ♦1, ♠2, ♣3）。Joker/不定は -1 */
s8   suit_of_name(const char* name);

/* ── 手札を昇順にソート ─ */
void sort_hand(Hand* hand);

#endif /* DECK_H */
