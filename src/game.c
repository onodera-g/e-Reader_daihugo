// src/game.c
#include "game.h"
#include "rng.h"
#include "cards.h"
#include "render.h"
#include "def.h"
#include "ai.h"

/* =========================================
 * ヘルパ
 * ========================================= */

/* 手札配列から index の1枚を削除（左詰め） */
static int remove_card_by_index(const char** hand, int count, int index) {
    if (index < 0 || index >= count) return count;
    for (int i = index; i + 1 < count; ++i) hand[i] = hand[i + 1];
    return count - 1;
}

/* カード名 → スートID（♥0, ♦1, ♠2, ♣3）。Joker/不定は -1 */
static s8 suit_of_name(const char* n){
    for (int i=0; i<13; ++i){
        if (n == kHearts[i])   return 0;  // ♥
        if (n == kDiamonds[i]) return 1;  // ♦
        if (n == kSpades[i])   return 2;  // ♠
        if (n == kClubs[i])    return 3;  // ♣
    }
    return -1; // Joker 等
}

/* 内部: 全員の配り目標に達したか？ */
static int deal_finished_all(const GameState* g){
    for (int p=0; p<PLAYERS; ++p){
        if (g->visible[p] < g->target[p]) return 0;
    }
    return 1;
}

/* =========================================
 * 公開API
 * ========================================= */

void game_init(GameState* g, const Hand hands[PLAYERS], int start_player_for_deal){
    for (int p=0; p<PLAYERS; ++p){
        g->visible[p] = 0;
        g->target [p] = hands[p].count;    /* 配られた“実枚数”を着地点に */
    }
    g->deal_turn  = start_player_for_deal & 3;
    g->deal_delay = DEAL_DELAY_FRAMES;
    g->deal_done  = 0;

    g->turn_player   = 1;                  /* 例: CPU1から */
    g->turn_delay    = TURN_DELAY_FRAMES;
    g->field_visible = 0;
    g->field_name    = NULL;
    g->last_played   = -1;
    g->pass_count    = 0;
    g->bind_suit     = -1;                 /* 縛り無しスタート */

    /* 8切り（捨てステップ）初期化 */
    g->yagiri_waiting = 0;
    g->yagiri_timer   = 0;
    g->sfx_yagiri     = 0;                 /* SEトリガ（mainで消費） */
}

void game_set_targets(GameState* g, const Hand hands[PLAYERS]){
    for (int p=0; p<PLAYERS; ++p){
        g->target[p] = hands[p].count;
    }
}

int game_step_deal(GameState* g){
    if (g->deal_done) return 0;

    int cpu1_sfx = 0;

    if (g->deal_delay > 0){
        g->deal_delay--;
    } else {
        /* このフレームで必ず1人だけ可視化（“1フレーム1枚”を保証） */
        for (int t=0; t<PLAYERS; ++t){
            int p = (g->deal_turn + t) & 3;
            if (g->visible[p] < g->target[p]){
                g->visible[p]++;
                if (p == 1) cpu1_sfx = 1;         /* CPU1に配ったフレームは効果音 */
                g->deal_turn = (p + 1) & 3;       /* 次の人へ */
                break;
            }
        }
        g->deal_delay = DEAL_DELAY_FRAMES;
    }

    /* フレーム末尾で配り完了をラッチ */
    if (!g->deal_done && deal_finished_all(g)){
        g->deal_done   = 1;
        g->turn_player = 1;                      /* CPU1からターン開始 */
        g->turn_delay  = TURN_DELAY_FRAMES;
    }

    return cpu1_sfx;
}

int game_step_turn(GameState* g,
                   Hand hands[PLAYERS],
                   int* out_played_player,
                   const char** out_field_name)
{
    if (!g->deal_done) return 0;

    /* 出力は毎フレーム初期化（残り値防止） */
    if (out_played_player) *out_played_player = -1;
    if (out_field_name)    *out_field_name    = NULL;

    /* ★ 8切り待機中はカウントだけ進めてターン停止 */
    if (g->yagiri_waiting){
        if (g->yagiri_timer > 0){
            g->yagiri_timer--;
            return 0; /* このフレームは“見せるだけ” */
        } else {
            /* 1秒経過：場をクリア＆縛り解除、出した本人が続けてリード */
            g->field_visible = 0;
            g->field_name    = NULL;
            g->bind_suit     = -1;
            g->pass_count    = 0;

            g->yagiri_waiting = 0;  /* 待機終了 */
            /* 手番は last_played のまま（=8を出した本人） */
            g->turn_delay = TURN_DELAY_FRAMES;
            return 1;
        }
    }

    if (g->turn_delay > 0){
        g->turn_delay--;
        return 0; /* まだ待機 */
    }

    int p = g->turn_player;

    /* --- この手番で出す／出せなければパス --- */
    if (hands[p].count > 0){
        const char* chosen = NULL;
        FieldState fs;
        fs.field_name      = g->field_visible ? g->field_name : NULL;
        fs.field_visible   = g->field_visible ? 1 : 0;
        fs.revolution      = 0;            /* 未実装なら0でOK */
        fs.jback_active    = 0;            /* 未実装なら0でOK */
        fs.bind_suit       = g->bind_suit; /* 縛りをAIに伝える */
        int right = (p + 1) & 3;
        fs.right_neighbor_count = (u8)hands[right].count;

        int played = ai_choose_move_single(&hands[p], &fs, &chosen);
        if (played){
            const char* prev = g->field_visible ? g->field_name : NULL;

            /* 1) 出したカードを手札から削除（左詰め） */
            Hand* hp = &hands[p];
            int idx = -1;
            for (int i=0; i<hp->count; ++i){
                if (hp->cards[i] == chosen){ idx = i; break; }
            }
            if (idx >= 0){
                hp->count = remove_card_by_index(hp->cards, hp->count, idx);
            }

            /* 2) 縛り発動チェック（直前と同スートが2連続なら縛りON） */
            {
                s8 prev_s = suit_of_name(prev);
                s8 now_s  = suit_of_name(chosen);
                if (prev_s >= 0 && now_s >= 0 && prev_s == now_s){
                    g->bind_suit = now_s;
                }
            }

            /* 3) 場札を更新（このフレームは表示される） */
            g->field_name    = chosen;
            g->field_visible = 1;
            g->last_played   = p;
            g->pass_count    = 0; /* 出たのでパス連続をリセット */
            if (g->visible[p] > hands[p].count) g->visible[p] = hands[p].count;

            if (out_played_player) *out_played_player = p;
            if (out_field_name)    *out_field_name    = g->field_name;

            /* 4) ★8切り：今回出した札が「8」なら
             *    このフレームは 8 を描画して、その後1秒間停止 → 場流し */
            if (eval_rank_from_name(chosen) == 8){
                g->yagiri_waiting = 1;
                g->yagiri_timer   = 60;           /* 60フレーム ≒ 1秒 */
                g->turn_player    = p;            /* リード権は出した本人に保持 */
                g->turn_delay     = TURN_DELAY_FRAMES;
                g->sfx_yagiri     = 1;            /* このフレームでSEを鳴らす合図（mainで消費） */
                return 1;                         /* “出した”ので1を返す */
            }

        } else {
            /* 出せない = パス（場が空のときはカウントしない） */
            if (g->field_visible) g->pass_count++;
        }
    }

    /* --- 場流し判定：最後に誰かが出してから (PLAYERS-1) 人連続でパス --- */
    if (g->field_visible && g->pass_count >= (PLAYERS - 1)) {
        /* クリア：最後に出した人がそのままリードできる */
        g->field_visible = 0;
        g->field_name    = NULL;
        g->pass_count    = 0;
        g->bind_suit     = -1;          /* 縛り解除 */

        if (g->last_played >= 0) {
            g->turn_player = g->last_played;  /* リード権を戻す */
        }
        g->turn_delay  = TURN_DELAY_FRAMES;
        return 1; /* ここで終了（通常の+1はしない） */
    }

    /* --- 通常進行：次の人へ --- */
    g->turn_player = (g->turn_player + 1) & 3;
    g->turn_delay  = TURN_DELAY_FRAMES;
    return 1;
}
