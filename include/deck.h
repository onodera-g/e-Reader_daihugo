#ifndef DECK_H
#define DECK_H

#include "def.h"

/* --- デッキ生成 53枚（Joker含む）--- */
int  build_deck(u8* out);

/* --- デッキのシャッフル --- */
void shuffle_deck(u8* a, int n);

/* --- 手札を昇順にソート --- */
void sort_hand(Hand* h);

/* --- ラウンドロビン方式でカードを配布 --- */
void deal_round_robin(const u8* deck, int deck_n, int start_player, Hand hands[PLAYERS]);

#endif /* DECK_H */
