#ifndef CARDS_H
#define CARDS_H

#include "def.h"

/*
 * cards.h
 * -------
 * カード画像（スプライト名）の文字列を外部公開する。
 * 
 * - kHearts[13]   : ハート A〜K の名前
 * - kDiamonds[13] : ダイヤ A〜K の名前
 * - kSpades[13]   : スペード A〜K の名前
 * - kClubs[13]    : クラブ A〜K の名前
 * - kJokerName    : ジョーカー
 * - kBackName     : 裏面カード

 */

extern const char* kHearts[13];
extern const char* kDiamonds[13];
extern const char* kSpades[13];
extern const char* kClubs[13];
extern const char* kJokerName;
extern const char* kBackName;

u8 eval_rank_from_name(const char* card_name);

#endif /* CARDS_H */
