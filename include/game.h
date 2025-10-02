#ifndef GAME_H
#define GAME_H

#include "def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- 基本定義 --- */
#define PLAYERS   4  // 人数
#define MAX_HAND  14 // 手札の最大枚数
#define MAX_DECK  53 // 山札の総数

/* --- 手札情報 --- */
typedef struct {
    const char* cards[MAX_HAND];
    int         count;
} Hand;

/* --- ディレイ --- */
#define DEAL_DELAY_FRAMES  4
#define TURN_DELAY_FRAMES  200

/* 8切りの待機時間（約1秒） */
#define YAGIRI_WAIT_FRAMES 60

/* --- 進行状態 --- */
typedef struct GameState {
    /* 配り演出 */
    int visible[PLAYERS]; // 今“画面上で見えている枚数
    int target [PLAYERS]; // 配る目標枚数
    int deal_turn;        // 配る順番のプレイヤーID
    int deal_delay;       // 待ち時間
    int deal_done;        // 配り演出がすべて完了したかどうか

    /* ターン進行 */
    int turn_player; // 次の番のプレイヤー：0=自分, 1..3=CPU 
    int turn_delay;  // 遅延フレーム数

    /* 場の状態（単カード版） */
    int field_visible;           // 場にカード出ているか：1=表示中/0=場空
    int last_played;             // 直近で出したプレイヤID：-1=なし,0,1,2,3 
    int pass_count;              // 直近の出札からの連続パス数 
    u8  field_count;             // 場に出てるカードの枚数：1..4
    u8  field_eff_rank;          // 場に出てるカードの強さ：　3..16を革命/JB反転後に正規化した値
    const char* field_names[4];  // 場に出ているカードの詳細
    u8 field_suit_mask;          // 出たスートのビット集合 (bit0=♠, bit1=♥, bit2=♦, bit3=♣)


    /* 8切り（捨てステップ演出：1秒停止＆SE） */
    int yagiri_waiting;     /* 1=待機中（ターン停止） */
    int yagiri_timer;       // ８切り中の待機時間
    int sfx_yagiri;         /* 1=このフレームでSEを鳴らす（mainで消費→0） */

    // 追加フィールド（yagiriに倣った待機フラグ＋タイマ）
    u8  sibari_waiting;   /* 1=縛り表示のため一時停止中 */
    u8  sibari_timer;     /* 縛り待機の残フレーム数（例:60） */
    /* 8切りSEトリガに合わせて、縛りSEのトリガも用意 */
    u8  sfx_sibari;      /* ★追加：縛りSE（新規成立時に1フレだけ1にする） */


} GameState;

/* 初期化・配り・ターン進行 */
void game_init(GameState* g, const Hand hands[PLAYERS], int start_player_for_deal);
void game_set_targets(GameState* g, const Hand hands[PLAYERS]);
int  game_step_deal(GameState* g);
int  game_step_turn(GameState* g,
                    Hand hands[PLAYERS]);

#ifdef __cplusplus
}
#endif
#endif /* GAME_H */
