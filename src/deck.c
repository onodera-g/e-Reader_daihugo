#include "deck.h"
#include "cards.h"
#include "rng.h"
#include "def.h"

/* スプライト名テーブル（UI表示用） */
const char* kHearts[13]   = {"H_1","H_2","H_3","H_4","H_5","H_6","H_7","H_8","H_9","H_10","H_11","H_12","H_13"};
const char* kDiamonds[13] = {"D_1","D_2","D_3","D_4","D_5","D_6","D_7","D_8","D_9","D_10","D_11","D_12","D_13"};
const char* kSpades[13]   = {"S_1","S_2","S_3","S_4","S_5","S_6","S_7","S_8","S_9","S_10","S_11","S_12","S_13"};
const char* kClubs[13]    = {"C_1","C_2","C_3","C_4","C_5","C_6","C_7","C_8","C_9","C_10","C_11","C_12","C_13"};
const char* kJokerName    = "J";
const char* kBackName     = "card";

/* u8→名前（UI用） */
const char* card_to_string(u8 c){
    if (CARD_IS_JOKER(c)) return kJokerName;
    u8 r = CARD_RANK(c);           // 3..16
    u8 s = CARD_SUIT(c);           // 0..3
    int idx = 0;                   // 1..13 -> [0..12] 

    if      (r == 16) return kJokerName;
    else if (r == 14) idx = 1;     // A 表示は "_1"
    else if (r == 15) idx = 2;     // 2 表示は "_2"
    else              idx = r;     /* 3..13 */

    // "H_1" は A を指すため、テーブルの 0 が A（=1）
    int t = 0;
    if      (idx == 1)  t = 0;      // A 
    else if (idx == 2)  t = 1;      // 2 
    else                t = idx-1;  // 3..13 -> 2..12

    if (t < 0 || t >= 13) return kBackName;

    switch (s){
        case SUIT_HEARTS:   return kHearts[t];
        case SUIT_DIAMONDS: return kDiamonds[t];
        case SUIT_SPADES:   return kSpades[t];
        case SUIT_CLUBS:    return kClubs[t];
        default:            return kBackName;
    }
}

/* --- デッキ生成（53枚; Joker あり） --- */
int build_deck(u8* out){
    int n=0;
    for (int s=0; s<4; ++s){
        // A(14),2(15),3..13 の順で揃えても良いが、等価 
        for (int r=3; r<=13; ++r) out[n++] = CARD_MAKE((u8)r, (u8)s);
        out[n++] = CARD_MAKE(14, (u8)s);   // A 
        out[n++] = CARD_MAKE(15, (u8)s);   // 2  
    }
    out[n++] = CARD_MAKE(16, 0); // Joker 
    return n;
}

/* --- デッキのシャッフル --- */
void shuffle_deck(u8* a, int n){
    for (int i=n-1; i>0; --i){
        int j = rng_range(0, i+1);
        u8 t=a[i]; a[i]=a[j]; a[j]=t;
    }
}

/* カードの比較 */
static int cmp_card(u8 a, u8 b){
    u8 ea = CARD_RANK(a);
    u8 eb = CARD_RANK(b);
    if (ea != eb) return (int)ea - (int)eb;
    return (int)CARD_SUIT(a) - (int)CARD_SUIT(b);
}

/* --- 手札を昇順にソート --- */
void sort_hand(Hand* h){
    /* 単純挿入で十分（枚数少） */
    for (int i=1; i<h->count; ++i){
        u8 key = h->cards[i];
        int j = i-1;
        while (j>=0 && cmp_card(h->cards[j], key) > 0){
            h->cards[j+1] = h->cards[j];
            --j;
        }
        h->cards[j+1] = key;
    }
}

/* --- ラウンドロビン方式でカードを配布 --- */
void deal_round_robin(const u8* deck, int deck_n, int start_player, Hand hands[PLAYERS]) {
    for (int p=0; p<PLAYERS; ++p) hands[p].count = 0;
    for (int i=0; i<deck_n; ++i){
        int p = (start_player + i) & 3;
        if (hands[p].count < MAX_HAND) {
            hands[p].cards[hands[p].count++] = deck[i];
        }
    }
    for (int p=0; p<PLAYERS; ++p) sort_hand(&hands[p]);
}
