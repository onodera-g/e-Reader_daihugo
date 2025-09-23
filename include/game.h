#ifndef GAME_H
#define GAME_H

#include "def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- 基本定義 --- */
#define PLAYERS   4
#define MAX_HAND  14
#define MAX_DECK  53

/* --- 手札 --- */
typedef struct {
    const char* cards[MAX_HAND];
    int         count;
} Hand;

/* --- ディレイ --- */
#define DEAL_DELAY_FRAMES  4
#define TURN_DELAY_FRAMES  20

/* 8切りの待機時間（約1秒） */
#define YAGIRI_WAIT_FRAMES 60

/* --- 進行状態 --- */
typedef struct GameState {
    /* 配り演出 */
    int visible[PLAYERS];
    int target [PLAYERS];
    int deal_turn;
    int deal_delay;
    int deal_done;

    /* ターン進行 */
    int turn_player;        /* 0=自分, 1..3=CPU */
    int turn_delay;

    /* 場の状態（単カード版） */
    int         field_visible;  /* 1=表示中/0=場空 */
    const char* field_name;     /* いま場に出ている1枚 */
    int         last_played;    /* 直近で出したプレイヤID(-1=未出) */
    int         pass_count;     /* 直近の出札からの連続パス数 */

    /* 縛り（-1=なし, 0=♥,1=♦,2=♠,3=♣） */
    s8 bind_suit;

    /* 8切り（捨てステップ演出：1秒停止＆SE） */
    int yagiri_waiting;     /* 1=待機中（ターン停止） */
    int yagiri_timer;       /* 残りフレーム */
    int sfx_yagiri;         /* 1=このフレームでSEを鳴らす（mainで消費→0） */

} GameState;

/* 初期化・配り・ターン進行 */
void game_init(GameState* g, const Hand hands[PLAYERS], int start_player_for_deal);
void game_set_targets(GameState* g, const Hand hands[PLAYERS]);
int  game_step_deal(GameState* g);
int  game_step_turn(GameState* g,
                    Hand hands[PLAYERS],
                    int* out_played_player,
                    const char** out_field_name);

#ifdef __cplusplus
}
#endif
#endif /* GAME_H */
