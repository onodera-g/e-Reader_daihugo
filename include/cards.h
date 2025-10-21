#ifndef CARDS_H
#define CARDS_H

#include "def.h"

/* ---- suit id ---- */
enum {
    SUIT_HEARTS   = 0,
    SUIT_DIAMONDS = 1,
    SUIT_SPADES   = 2,
    SUIT_CLUBS    = 3
};

/* ---- u8 encode (rank upper 4 bits, suit lower 2 bits) ----
 * rank: 3..14(A=14), 15=2, 16=Joker
 * suit: 0..3
 */
#define CARD_MAKE(rank, suit)  ( (u8)( ( ( ((rank)==16?14:((rank)-2)) & 0x0F ) << 2 ) | ((suit) & 0x03) ) )
#define CARD_RANK(c)           ( (u8)( ((((c)>>2) & 0x0F) == 14 ) ? 16 : (u8)((((c)>>2) & 0x0F) + 2) ) )
#define CARD_SUIT(c)           ( (u8)((c) & 0x03) )
#define CARD_IS_JOKER(c)       ( CARD_RANK(c) == 16 )
#define CARD_IS_TWO(c)         ( CARD_RANK(c) == 15 )

/* ---- UI assets (kept): sprite name tables ---- */
extern const char* kHearts[13];
extern const char* kDiamonds[13];
extern const char* kSpades[13];
extern const char* kClubs[13];
extern const char* kJokerName;
extern const char* kBackName;

/* ---- bridge for UI/log ---- */
const char* card_to_string(u8 c);

/* ---- rank → strength（比較用の一意スケール）----
   通常:   3(=3) ... A(=14) 2(=16) Joker(=17)
   反転時: 上記を完全反転（Joker最弱, 3最強） */
static inline u8 _rank_to_strength(u8 r){
    /* r: 3..16 (15=2, 16=Joker) */
    if (r == 16) return 17; /* Joker */
    if (r == 15) return 16; /* 2 */
    return r;               /* 3..14 */
}
static inline u8 _strength_effective(u8 s, u8 inv){
    /* s: 3..17 → inv=0: s, inv=1: 20 - s (3↔17, 4↔16, …) */
    return inv ? (u8)(20u - s) : s;
}

/* ---- 公開：有効ランク（比較用） ---- */
static inline u8 rank_effective_ext(u8 rank_3to16, u8 rev, u8 jb){
    u8 inv = (u8)((rev ^ jb) & 1u);
    u8 s   = _rank_to_strength(rank_3to16);
    return _strength_effective(s, inv);
}

/* ---- 互換/補助（必要なら呼ぶ） ---- */
static inline u8 card_rank_effective(u8 r, u8 rev, u8 jb){
    return rank_effective_ext(r, rev, jb);
}

#endif /* CARDS_H */
