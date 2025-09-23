// src/ai.c
#include "ai.h"
#include "def.h"
#include "cards.h"

/* --- ヘルパ（比較/スート） --- */
static inline u8 rank_effective(u8 r, u8 rev, u8 jb){
  u8 inv = (rev ^ jb) & 1;        /* 1なら反転中 */
  return inv ? (u8)(19 - r) : r;  /* 3..16 -> 反転は(19-r)で大小逆転 */
}
static inline int beats_card(const char* a, const char* b, u8 rev, u8 jb){
  if (!b) return 1;
  u8 ra = eval_rank_from_name(a);
  u8 rb = eval_rank_from_name(b);
  return rank_effective(ra,rev,jb) > rank_effective(rb,rev,jb);
}
static inline s8 suit_of(const char* n){
  for (int i=0;i<13;i++){
    if (n==kHearts[i])   return 0;
    if (n==kDiamonds[i]) return 1;
    if (n==kSpades[i])   return 2;
    if (n==kClubs[i])    return 3;
  }
  return -1; /* Joker等 */
}

/* --- コア：単一出し決定 --- */
int ai_choose_move_single(const Hand* hand,
                          const FieldState* fs,
                          const char** out_card)
{
  /* 合法手の列挙（単一のみ）*/
  int best_i = -1;
  u8  best_rank_eff = 255;
  s8  field_suit = (fs->field_visible && fs->field_name) ? suit_of(fs->field_name) : -1;

  /* リード（場空）なら“最弱”を出すのが基本（後で点数ロジックを足せる） */
  if (!fs->field_visible || fs->field_name==NULL){
    u8 best_r = 255;
    for (int i=0;i<hand->count;i++){
      const char* c = hand->cards[i];
      /* bind中ならスート制約（未実装なら bind_suit=-1 でスキップ） */
      s8 s = suit_of(c);
      if (fs->bind_suit >= 0 && s != fs->bind_suit) { 
      /* Jokerは例外でOKにする */
      if (eval_rank_from_name(c) != 16) continue;
      }
      u8 r = rank_effective(eval_rank_from_name(c), fs->revolution, fs->jback_active);
      if (r < best_r){ best_r = r; best_i = i; }
    }
    if (best_i >= 0){ *out_card = hand->cards[best_i]; return 1; }
    return 0; /* 出せない（理論上ここは来ない想定） */
  }

  /* 追従（場あり）: 勝てる中で「最弱」を基本線に選ぶ */
  /*   さらに「最弱+2 以内 かつ 場と同スート」なら優先（縛り布石） */
  int weakest_i = -1;
  u8  weakest_eff = 255;
  s8  weakest_suit = -1;

  for (int i=0;i<hand->count;i++){
    const char* c = hand->cards[i];
        {
       s8 s = suit_of(c);
       if (fs->bind_suit >= 0 && s != fs->bind_suit) {
         /* ★縛り中でも Joker は候補に残す */
         if (eval_rank_from_name(c) != 16) continue;
       }
     }
    if (!beats_card(c, fs->field_name, fs->revolution, fs->jback_active)) continue;

    u8 eff = rank_effective(eval_rank_from_name(c), fs->revolution, fs->jback_active);
    if (eff < weakest_eff){
      weakest_eff = eff; weakest_i = i; weakest_suit = suit_of(c);
    }
  }

  if (weakest_i < 0) return 0; /* 出せない → パス */

  /* 「最弱+2以内且つ場と同スート」があればそれを選ぶ（なければ最弱） */
  int pick_i = weakest_i;
  if (field_suit >= 0){
    u8 cap = (u8)(weakest_eff + 2);
    for (int i=0;i<hand->count;i++){
      const char* c = hand->cards[i];
      if (suit_of(c) != field_suit) continue;
      if (!beats_card(c, fs->field_name, fs->revolution, fs->jback_active)) continue;
      u8 eff = rank_effective(eval_rank_from_name(c), fs->revolution, fs->jback_active);
      if (eff >= weakest_eff && eff <= cap){
        pick_i = i; break; /* 簡易優先：見つけたら即採用 */
      }
    }
  }

  *out_card = hand->cards[pick_i];
  return 1;
}
