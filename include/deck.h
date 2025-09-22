#ifndef DECK_H
#define DECK_H

#include "game.h"

/*
 * デッキを構築・シャッフル・配布する関数群の宣言。
 * 実体は deck.c にある。
 * 
 */

// デッキを生成（52枚 or 53枚）
// out にカード名を格納し、総枚数を返す
int build_deck(const char** out);

// デッキをシャッフル（Fisher-Yates 法）
void shuffle_deck(const char** a, int n);

// デッキを start_player から順番に配る
void deal_round_robin_offset(const char** deck, int deck_n, int start, Hand hands[PLAYERS]);

#endif /* DECK_H */
