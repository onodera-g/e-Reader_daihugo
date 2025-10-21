#ifndef GAME_H
#define GAME_H

#include "def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 演出テンポ ---- */
#define DEAL_DELAY_FRAMES   4    /* 配りの待機フレーム */
#define TURN_DELAY_FRAMES   50   /* ターン間ディレイ */

/* ---- 効果種別（render.c が effect に応じてスプライトを選ぶ） ----
 *   FXE_YAGIRI   -> "yagiri"
 *   FXE_SIBARI   -> "sibari"
 *   FXE_BACK11   -> "11back"
 *   FXE_KAIDAN   -> "kaidan"
 *   FXE_KAKUMEI  -> "kakumei"
 */
typedef enum {
    FXE_YAGIRI = 0,
    FXE_SIBARI,
    FXE_BACK11,
    FXE_KAIDAN,
    FXE_KAKUMEI,
    FXE_COUNT
} FxEffect;

/* 1役あたりの表示時間（フレーム）／上限（安全弁） */
#define FX_WAIT_ADD_PER_EFFECT 20
#define FX_WAIT_CAP            240

/* ---- ゲーム進行状態 ---- */
typedef struct GameState {
    /* 配り演出（既存） */
    int visible[PLAYERS];
    int target [PLAYERS];
    int deal_turn;
    int deal_delay;
    int deal_done;

    /* ターン進行 */
    int turn_player;   /* 0=自分, 1..3=CPU */
    int turn_delay;

    /* 場 */
    int  field_visible;          /* 1=場にカードがある */
    int  last_played;            /* 直近の出し手（-1=なし） */
    int  pass_count;             /* 連続パス数（3で場流し） */
    u8   field_count;            /* セット枚数/階段長 */
    u8   field_eff_rank;         /* 有効ランク（革命⊕Jバック反転後） */
    const char* field_names[4];  /* 表示用カード名（最大4枚） */
    u8   field_suit_mask;        /* 場のスート集合 bit0..3 */
    u8   field_is_straight;      /* 階段フラグ */
    u8   sibari_active;          /* しばり成立中 */
    u8   revolution_active;      /* 革命中 */
    u8   jback_active;           /* Jバック中 */

    /* ---- 汎用エフェクト（表示中はゲーム停止） ----
     * 役が立つ度に「SE → renderへenqueue → 表示時間を加算」。
     * fx_display_time > 0 の間は進行を止める。
     * （SEは共通1種類。役ごとに変数は持たない）
     */
    int fx_active;               /* 1=表示中（進行停止） */
    int fx_display_time;         /* 残り表示フレーム */
} GameState;

/* 初期化/配布/進行 */
void game_init(GameState* g, const Hand hands[PLAYERS], int start_player_for_deal);
int  game_step_deal(GameState* g);
int  game_step_turn(GameState* g, Hand hands[PLAYERS]);

#ifdef __cplusplus
}
#endif
#endif /* GAME_H */
