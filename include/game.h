#ifndef GAME_H
#define GAME_H

#include "def.h"

/*
 * game.h
 * ------
 * ゲーム進行（配り演出・ターン管理・場に出すロジック）のインターフェース。
 * - VRAM/OAM は扱わない（描画は render.* の責務）
 * - 効果音を鳴らすかどうかなどのイベントはフラグで main に返す
 *
 * 既存：PLAYERS/MAX_HAND/MAX_DECK/Hand はここに集約
 */

#ifdef __cplusplus
extern "C" {
#endif

/* --- 既存の基本定義（必要ならここをプロジェクト既定に合わせて調整） --- */
#define PLAYERS   4
#define MAX_HAND  14        /* 1人あたりの最大枚数（自分が14になるケースに備える） */
#define MAX_DECK  53        /* 52 + Joker */

/* 手札 */
typedef struct {
    const char* cards[MAX_HAND];
    int count;
} Hand;

/* --- 進行パラメータ（必要に応じて変更可） --- */
#define DEAL_DELAY_FRAMES  4     /* 何フレームおきに1枚可視化（配り演出） */
#define TURN_DELAY_FRAMES  20    /* 次の手番までの待機フレーム */

/* --- ゲーム進行の状態（描画で使う可視枚数や場の情報も含む） --- */
typedef struct GameState {
    /* 配り可視化の状態 */
    int visible[PLAYERS];   /* 現在可視化している枚数（描画に使う） */
    int target [PLAYERS];   /* 最終的に可視化したい枚数（配った結果） */
    int deal_turn;          /* 次に可視化するプレイヤ（0..3） */
    int deal_delay;         /* ディレイ（カウントダウン） */
    int deal_done;          /* 1: 配り演出が完了 */

    /* ターン/場 */
    int         turn_player;   /* 現在の手番（CPU1→CPU2→CPU3→自分の順で進行する例） */
    int         turn_delay;    /* 次手番までの待機ディレイ */
    int         field_visible; /* 1: 場にカード表示中 */
    const char* field_name;    /* 場に出ているカード名（アトラス名） */
    int  last_played;   /* 直近で場に出したプレイヤ（-1=まだ誰も出してない） */
    int  pass_count;    /* 最後に出されてからの連続パス回数 */
} GameState;

/* 初期化：配りの着地点（target）を hands に合わせて設定。deal開始プレイヤを指定。 */
void game_init(GameState* g, const Hand hands[PLAYERS], int start_player_for_deal);

/* hands の内容に合わせて target をセットし直す（配りロジックを変更したとき等に使用） */
void game_set_targets(GameState* g, const Hand hands[PLAYERS]);

/* 配り演出を1フレーム進める。
   戻り値: このフレームで「CPU1に可視枚数を増やした」なら 1（＝SEを鳴らしたい合図）、それ以外 0。 */
int game_step_deal(GameState* g);

/* 配り完了後のターン進行を1フレーム進める。
   - 誰かが場にカードを出したら 1 を返す（out_played_player / out_field_name に結果をセット）。
   - hands はこの関数内で減る（出した1枚を手札から除去）。
   - 自分(0)が出した場合のみ、呼び出し側で表VRAMの詰め直し（render_reload_hand_card）を行うこと。 */
int game_step_turn(GameState* g,
                   Hand hands[PLAYERS],
                   int* out_played_player,
                   const char** out_field_name);

#ifdef __cplusplus
}
#endif
#endif /* GAME_H */
