// include/ai.h
#ifndef AI_H
#define AI_H

#include "def.h"
#include "game.h"   // Hand
#include "cards.h"  // eval_rank_from_name

/* 場の状態をAIに渡す */
typedef struct {
  /* 場のカード情報 */
  const char* field_names[4];// 場に出ているカード情報
  u8  field_visible;         // 場にカードが出ているか：0/1
  u8  revolution;            // 革命の成立有無：0/1
  u8  jback_active;          // １１バックのの成立有無：0/1
  u8  right_neighbor_count;  // 右隣プレイヤの残り枚数（未使用なら0)

  /* 複数枚出し用の追加情報 */
  u8  field_count;           // 場の枚数：0=場なし, 1..4
  u8  field_eff_rank;        // 場の“有効ランク”（反転後で比較に使う値）
  s8  field_suit_mask;       // 出たスートのビット集合：bit0=♠, bit1=♥, bit2=♦, bit3=♣
} FieldState;

/* 単一出しの思考：出せるなら1を返し、out_cardに1枚入れる。出せなければ0（パス） */
int ai_choose_move_single(const Hand* hand,
                          const FieldState* fs,
                          const char** out_card);
int ai_choose_move_group(const Hand* hand,
                         const FieldState* fs,
                         const char** out_cards,
                         u8* out_n);

/* ★共有ユーティリティ：有効ランク計算（革命/JB反転込み）。
   実装は ai.c 側で rank_effective をラップして提供してください。*/
u8 rank_effective_ext(u8 r, u8 rev, u8 jb);

#endif /* AI_H */
