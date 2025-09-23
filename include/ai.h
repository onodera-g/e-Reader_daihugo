// include/ai.h
#ifndef AI_H
#define AI_H

#include "def.h"
#include "game.h"   // Hand
#include "cards.h"  // eval_rank_from_name

/* 場の状態をAIに渡す（必要最小限） */
typedef struct {
  const char* field_name;  /* NULLなら場空 */
  u8  field_visible;       /* 0/1 */
  u8  revolution;          /* 0/1（未実装なら常に0でOK） */
  u8  jback_active;        /* 0/1（未実装なら0） */
  s8  bind_suit;           /* -1=no bind, 0..3 suit（未実装なら-1） */

  u8  right_neighbor_count;/* 右隣プレイヤの残り枚数（阻止用・未使用なら0） */
} FieldState;

/* 単一出しの思考：出せるなら1を返し、out_cardに1枚入れる。出せなければ0（パス） */
int ai_choose_move_single(const Hand* hand,
                          const FieldState* fs,
                          const char** out_card);

#endif /* AI_H */
