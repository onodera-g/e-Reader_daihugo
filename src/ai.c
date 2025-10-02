// src/ai.c

#include "ai.h"      
#include "cards.h"   
#include <stddef.h>  // NULL

/* ------------------------------------------------------------
 * 有効ランク（革命/JB反転込み）
 *  - 強さは 3..16 (Joker=16)
 *  - 反転が有効（rev ^ jb == 1）のとき 3..16 を 16..3 に対応させる (=19-r)
 * ------------------------------------------------------------ */
static u8 rank_effective(u8 r, u8 rev, u8 jb){
    u8 inv = (rev ^ jb) & 1u;
    return inv ? (u8)(19u - r) : r;
}

/* 外部公開：ai.h の要求どおりラップを提供 */
u8 rank_effective_ext(u8 r, u8 rev, u8 jb){
    return rank_effective(r, rev, jb);
}

/* 手札中の各ランク(3..16)の枚数を数える（ヒストグラム） */
static void build_histogram(const Hand* hand, u8 have[17]){
    for (int i=0;i<17;++i) have[i]=0;
    for (int i=0;i<hand->count;++i){
        u8 r = eval_rank_from_name(hand->cards[i]); // 3..16
        if (r>=3 && r<=16) have[r]++;
    }
}

/* 指定ランク r のカードを n枚 out_cards[] に詰める（左から）。Joker複数は不許可。*/
static int take_cards_of_rank(const Hand* hand, u8 r, u8 n, const char** out_cards){
    if (r==16 && n>=2) return 0; // Joker は単騎のみ許可
    int wrote=0;
    for (int i=0;i<hand->count && wrote<n;++i){
        if (eval_rank_from_name(hand->cards[i]) == r){
            out_cards[wrote++] = hand->cards[i];
        }
    }
    return (wrote==n) ? wrote : 0;
}

/* 追従：場の枚数 n に限定し、勝てる中で“最も弱い”ランクを選ぶ */
static int pick_follow_minwin(const Hand* hand, const FieldState* fs,
                              const u8 have[17], u8 n,
                              const char** out_cards, u8* out_n)
{
    const u8 rev = fs->revolution;
    const u8 jb  = fs->jback_active;

    // 場の“必要有効ランク”。ai.h で field_eff_rank が与えられている。
    if (!fs->field_visible || fs->field_count==0) return 0; // リードならここは来ない想定
    u8 need_eff = fs->field_eff_rank;

    u8 best_r = 0;
    u8 best_e = 255;

    for (u8 r=3; r<=16; ++r){
        if (have[r] < n) continue;             // n枚作れない
        if (r==16 && n>=2) continue;           // Joker複数は不可
        u8 e = rank_effective(r, rev, jb);
        if (e <= need_eff) continue;           // 勝てない
        if (!best_r || e < best_e){ best_r = r; best_e = e; }
    }
    if (!best_r) return 0;

    int wrote = take_cards_of_rank(hand, best_r, n, out_cards);
    if (wrote != (int)n) return 0;
    *out_n = n;
    return 1;
}

/* リード：4→3→2→1 の順で探し、同じ n の中で“最も弱い”ランクを選ぶ */
static int pick_lead_pref_multi(const Hand* hand, const FieldState* fs,
                                const u8 have[17],
                                const char** out_cards, u8* out_n)
{
    const u8 rev = fs->revolution;
    const u8 jb  = fs->jback_active;

    for (u8 n=4; n>=1; --n){
        u8 best_r = 0;
        u8 best_e = 255;
        for (u8 r=3; r<=16; ++r){
            if (have[r] < n) continue;
            if (r==16 && n>=2) continue;       // Joker複数は不可
            u8 e = rank_effective(r, rev, jb); // 反転考慮で“弱い方”を温存
            if (!best_r || e < best_e){ best_r = r; best_e = e; }
        }
        if (best_r){
            int wrote = take_cards_of_rank(hand, best_r, n, out_cards);
            if (wrote == (int)n){ *out_n = n; return 1; }
        }
        if (n==1) break; // u8 の underflow 防止
    }
    return 0;
}

/* ------------------------------------------------------------
 * 公開API
 * ------------------------------------------------------------ */

/* 単騎（n=1 の特化版）—グループ選択ロジックを流用 */
int ai_choose_move_single(const Hand* hand,
                          const FieldState* fs,
                          const char** out_card)
{
    const char* tmp[4] = {0};
    u8 n = 0;
    u8 have[17]; build_histogram(hand, have);
    int ok = 0;

    if (!fs->field_visible || fs->field_count==0){
        ok = pick_lead_pref_multi(hand, fs, have, tmp, &n);
    }else{
        ok = pick_follow_minwin(hand, fs, have, /*n=*/fs->field_count, tmp, &n);
    }
    if (ok && n==1){ *out_card = tmp[0]; return 1; }
    return 0;
}

/* ── 場に出すカードの選択 ──
   - ゲームの状況をFsとして受け取る
   - 場に出すカードの枚数をnに格納
   - 場に出すカードの種類をchosen[0]~chosen[3]に格納 */
int ai_choose_move_group(const Hand* hand,
                         const FieldState* fs,
                         const char** out_cards,
                         u8* out_n)
    {
        u8 have[17];
        build_histogram(hand, have);

        if (!fs->field_visible || fs->field_count==0){
            // リード：複数優先 4→3→2→1、同n内で“最弱ランク”を選ぶ
            return pick_lead_pref_multi(hand, fs, have, out_cards, out_n);
        }else{
            // 追従：同枚数のみ検討し、勝てる中で“最弱”を選ぶ
            u8 n = fs->field_count;
            if (n<1 || n>4) return 0;
            return pick_follow_minwin(hand, fs, have, n, out_cards, out_n);
        }
    }
