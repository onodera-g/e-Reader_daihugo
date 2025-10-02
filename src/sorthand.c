// src/sorthand.c
#include "deck.h"   // sort_hand, Hand, eval_rank_from_name, suit_of_name

/* 比較: ランク昇順 → スート昇順、Joker(=16)は常に末尾 */
static inline int card_cmp(const char* a, const char* b){
    int ra = (int)eval_rank_from_name(a);
    int rb = (int)eval_rank_from_name(b);

    /* Joker を常に末尾へ */
    if (ra == 16 && rb != 16) return  1;
    if (rb == 16 && ra != 16) return -1;

    if (ra != rb) return (ra - rb);

    /* 同ランク内はスート順 (♥0 < ♦1 < ♠2 < ♣3)。Joker(-1)はここに来ない想定 */
    int sa = (int)suit_of_name(a);
    int sb = (int)suit_of_name(b);
    return (sa - sb);
}

/* 小配列向けの挿入ソート（安定・O(n^2)、n≒13-14 なら超軽量） */
void sort_hand(Hand* hand){
    if (!hand || hand->count <= 1) return;

    for (int i = 1; i < hand->count; ++i){
        const char* key = hand->cards[i];
        int j = i - 1;
        while (j >= 0 && card_cmp(hand->cards[j], key) > 0){
            hand->cards[j + 1] = hand->cards[j];
            --j;
        }
        hand->cards[j + 1] = key;
    }
}
