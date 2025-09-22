#include "deck.h"
#include "cards.h"
#include "rng.h"
#include "def.h"

/*
 * deck.c
 * ------
 * カード配列の実体と、デッキ操作関数の定義。
 */

// --- カード配列の実体 ---
const char* kHearts[13]   = {"H_1","H_2","H_3","H_4","H_5","H_6","H_7","H_8","H_9","H_10","H_11","H_12","H_13"};
const char* kDiamonds[13] = {"D_1","D_2","D_3","D_4","D_5","D_6","D_7","D_8","D_9","D_10","D_11","D_12","D_13"};
const char* kSpades[13]   = {"S_1","S_2","S_3","S_4","S_5","S_6","S_7","S_8","S_9","S_10","S_11","S_12","S_13"};
const char* kClubs[13]    = {"C_1","C_2","C_3","C_4","C_5","C_6","C_7","C_8","C_9","C_10","C_11","C_12","C_13"};
const char* kJokerName    = "J";
const char* kBackName     = "card";

/* デッキを生成（52 or 53枚を out に格納） */
int build_deck(const char** out){
    int n=0;
    for(int i=0;i<13;i++) out[n++]=kHearts[i];
    for(int i=0;i<13;i++) out[n++]=kDiamonds[i];
    for(int i=0;i<13;i++) out[n++]=kSpades[i];
    for(int i=0;i<13;i++) out[n++]=kClubs[i];
    if (n < MAX_DECK) out[n++] = kJokerName;
    return n;
}

/* 配列 a をシャッフルする（Fisher-Yates 法） */
void shuffle_deck(const char** a, int n){
    for (int i=n-1; i>0; --i){
        int j = rng_range(0, i+1);
        const char* t=a[i]; a[i]=a[j]; a[j]=t;
    }
}

/* デッキをラウンドロビンで配布する */
void deal_round_robin_offset(const char** deck, int deck_n, int start_player, Hand hands[PLAYERS]) {
    for(int p=0; p<PLAYERS; p++) hands[p].count = 0;
    for(int i=0; i<deck_n; i++){
        int p = (start_player + i) & 3;
        if (hands[p].count < MAX_HAND) {   
            hands[p].cards[hands[p].count++] = deck[i];
        }
    }
}

/* カードの強さを判定する(強さ：3..13(=K), A=14, 2=15, Joker=16 裏面 0)*/
static u8 map_num_to_strength(int num) {
    if (num == 1)  return 14;  // A
    if (num == 2)  return 15;  // 2
    return (u8)num;            // 3..13
}

u8 eval_rank_from_name(const char* card_name) {
    if (card_name == kJokerName) return 16; // Joker

    for (int i = 0; i < 13; ++i) {
        if (card_name == kHearts[i])   return map_num_to_strength(i + 1);
        if (card_name == kDiamonds[i]) return map_num_to_strength(i + 1);
        if (card_name == kSpades[i])   return map_num_to_strength(i + 1);
        if (card_name == kClubs[i])    return map_num_to_strength(i + 1);
    }
    return 0; // 裏面など想定外
}