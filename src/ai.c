#include "ai.h"
#include "cards.h"
#include <stddef.h>  // NULL

/* 調整パラメータ */
#define STRAIGHT_MIN  3

/* rank 有効値（革命/JB 反転） */
static u8 rank_effective(u8 r, u8 rev, u8 jb){
    u8 inv = (rev ^ jb) & 1u;
    return inv ? (u8)(19u - r) : r; /* 3..16 を反転マップ */
}

//u8 rank_effective_ext(u8 r, u8 rev, u8 jb){ return rank_effective(r, rev, jb); }

/* ---- ユーティリティ ---- */
typedef struct {
    u8 card;     /* 実カードID */
    u8 rank;     /* 素のランク（3..16） */
    u8 suit;     /* 0..3 */
    u8 rank_eff; /* 有効ランク（革命/JB反転後） */
} CardView;

static inline u8 suit_of(u8 c){ return CARD_SUIT(c); }
static inline u8 rank_of(u8 c){ return CARD_RANK(c); }
static inline int is_joker(u8 c){ return rank_of(c)==16; }

/* --- 構造体コピーを一切使わないヘルパ --- */
static inline void cv_set(CardView* d, u8 card, u8 rank, u8 suit, u8 rank_eff){
    d->card = card; d->rank = rank; d->suit = suit; d->rank_eff = rank_eff;
}
static inline void cv_copy(CardView* d, const CardView* s){
    d->card = s->card; d->rank = s->rank; d->suit = s->suit; d->rank_eff = s->rank_eff;
}
static inline void cv_swap(CardView* a, CardView* b){
    u8 t_card = a->card, t_rank = a->rank, t_suit = a->suit, t_eff = a->rank_eff;
    a->card = b->card; a->rank = b->rank; a->suit = b->suit; a->rank_eff = b->rank_eff;
    b->card = t_card;  b->rank = t_rank;  b->suit = t_suit;  b->rank_eff = t_eff;
}

/* 手札をビューに展開（有効ランクを前計算） */
static void build_card_view(const Hand* h, u8 rev, u8 jb, CardView out[20], int* out_n){
    int n=0;
    for (int i=0;i<h->count;++i){
        u8 c = h->cards[i];
        u8 r = rank_of(c);
        u8 s = suit_of(c);
        u8 e = rank_effective(r, rev, jb);
        cv_set(&out[n++], c, r, s, e);
    }
    *out_n = n;
}

/* ランク昇順（同値は suit で安定） */
static void sort_by_eff_rank(CardView a[], int n){
    for (int i=0;i<n;i++){
        for (int j=i+1;j<n;j++){
            if (a[i].rank_eff > a[j].rank_eff ||
               (a[i].rank_eff==a[j].rank_eff && a[i].suit > a[j].suit)){
                cv_swap(&a[i], &a[j]);
            }
        }
    }
}

/* スート毎の枚数を数える（Jokerは除外） */
static void build_histogram_from_view(const CardView* cv, int n, u8 have[17]){
    for (int r=0;r<=16;++r) have[r]=0;
    for (int i=0;i<n;++i){
        if (!is_joker(cv[i].card)) have[cv[i].rank]++;
    }
}

/* ---- 出し判定ヘルパ ---- */

/* 同ランク n枚の最小（条件を満たす最小）を pick */
static int pick_min_set_from_view(const CardView* cv, int ncv, u8 need_rank_eff, int need_n,
                                  const FieldState* fs,
                                  u8 out_cards[4], u8* out_n){
    u8 suit_mask = fs->field_suit_mask;
    for (int i=0;i<ncv;i++){
        u8 r = cv[i].rank;
        u8 re= cv[i].rank_eff;
        if (re <= need_rank_eff) continue;
        u8 tmp_cards[4]; int tn=0;
        u8 suit_mask_set = 0;
        for (int j=i;j<ncv && tn<need_n;j++){
            if (cv[j].rank == r && cv[j].rank_eff==re){
                suit_mask_set |= (1u<<cv[j].suit);
                tmp_cards[tn++] = cv[j].card;
            }
        }
        if (tn==need_n){
            if (fs->sibari_active){
                if (suit_mask_set != suit_mask) continue; /* セット時もしばり集合一致 */
            }
            for (int k=0;k<tn;k++) out_cards[k]=tmp_cards[k];
            *out_n = (u8)tn;
            return 1;
        }
    }
    return 0;
}

/* 階段：同一スートで連番 STRAIGHT_MIN.. 2/Jokerは不可、トップの rank_eff で比較 */
static int pick_min_straight_from_view(const CardView* cv, int ncv, u8 need_top_eff, int need_len,
                                       const FieldState* fs,
                                       u8 out_cards[4], u8* out_n){
    (void)fs; /* しばりは階段に適用しない仕様 */
    for (u8 s=0; s<4; ++s){
        CardView arr[20]; int an=0;
        for (int i=0;i<ncv;i++){
            if (cv[i].suit==s){
                if (cv[i].rank>=15) continue; /* 2(15)/Joker(16) は階段不可 */
                cv_copy(&arr[an++], &cv[i]);
            }
        }
        if (an < need_len) continue;
        sort_by_eff_rank(arr, an);

        for (int i=0;i<an;i++){
            int run=1;
            out_cards[0]=arr[i].card;
            for (int j=i+1;j<an && run<need_len;j++){
                if (arr[j].rank_eff == arr[j-1].rank_eff) continue;
                if (arr[j].rank_eff == (u8)(arr[j-1].rank_eff+1)){
                    out_cards[run++] = arr[j].card;
                }else{
                    break;
                }
            }
            if (run==need_len){
                u8 top_eff = rank_effective(rank_of(out_cards[need_len-1]), fs->revolution, fs->jback_active);
                if (top_eff > need_top_eff){
                    *out_n = (u8)need_len;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* 先出し：複数枚（4→3→2）を優先、なければ単体、最後に階段 */
static int pick_lead_pref_multi_from_view(const CardView* cv, int ncv, const FieldState* fs,
                                          u8 out_cards[4], u8* out_n){
    CardView tmp[20];
    for (int i=0;i<ncv;i++) cv_copy(&tmp[i], &cv[i]);
    sort_by_eff_rank(tmp, ncv);

    /* クアッド→トリプル→ペア→単体 の順で弱いものから */
    for (int need_n=4; need_n>=2; --need_n){
        u8 dummy_prev_eff = 0; /* 先出しなので下限なし扱い（0） */
        if (pick_min_set_from_view(tmp,ncv,dummy_prev_eff,need_n,fs,out_cards,out_n)) return 1;
    }

    /* 単体：しばり中はスート一致 */
    for (int i=0;i<ncv;i++){
        if (is_joker(tmp[i].card)) continue;
        if (fs->sibari_active){
            u8 mask = (1u<<tmp[i].suit);
            if (mask != fs->field_suit_mask) continue;
        }
        out_cards[0]=tmp[i].card; *out_n=1; return 1;
    }

    /* 最後に階段（最低 STRAIGHT_MIN） */
    for (int need_len=4; need_len>=STRAIGHT_MIN; --need_len){
        if (pick_min_straight_from_view(cv,ncv,0,need_len,fs,out_cards,out_n)) return 1;
    }
    return 0;
}

/* 後追い：場の形に合わせて最小勝ち（単体/セット/階段） */
static int pick_follow_minwin_from_view(const CardView* cv, int ncv,
                                        const FieldState* fs, const u8 have[17],
                                        u8 out_cards[4], u8* out_n){
    if (fs->field_is_straight){
        int need_len = fs->field_count;
        u8 need_top_eff = fs->field_eff_rank;
        if (pick_min_straight_from_view(cv,ncv,need_top_eff,need_len,fs,out_cards,out_n)) return 1;
        return 0;
    }

    /* セット（または単体） */
    int need_n = fs->field_count;
    u8 need_eff = fs->field_eff_rank;

    /* セット後追い（速度のためざっくり枚数確認） */
    for (int i=0;i<ncv;i++){
        u8 r = cv[i].rank;
        if (have[r] < need_n) continue;
        if (pick_min_set_from_view(cv,ncv,need_eff,need_n,fs,out_cards,out_n)) return 1;
        break;
    }

    /* 単体後追い：最小の re>need_eff を選び、しばり中はスート一致 */
    for (int i=0;i<ncv;i++){
        if (is_joker(cv[i].card)) continue;
        if (cv[i].rank_eff <= need_eff) continue;
        if (fs->sibari_active){
            u8 mask = (1u<<cv[i].suit);
            if (mask != fs->field_suit_mask) continue;
        }
        out_cards[0]=cv[i].card; *out_n=1; return 1;
    }
    return 0;
}

/* ---- 公開API ---- */
int ai_choose_move_group(const Hand* hand, const FieldState* fs, u8 out_cards[4], u8* out_n){
    CardView cv[20];
    int ncv=0;
    build_card_view(hand, fs->revolution, fs->jback_active, cv, &ncv);

    u8 have[17]; build_histogram_from_view(cv,ncv,have);

    if (!fs->field_visible || fs->field_count==0){
        return pick_lead_pref_multi_from_view(cv,ncv,fs,out_cards,out_n);
    }else{
        return pick_follow_minwin_from_view(cv,ncv,fs,have,out_cards,out_n);
    }
}
