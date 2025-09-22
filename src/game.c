#include "game.h"
#include "rng.h"
#include "cards.h"
#include "render.h"
#include "def.h"

// 手札配列から index の1枚を削除（左詰め）
static int remove_card_by_index(const char** hand, int count, int index) {
    if (index < 0 || index >= count) return count;
    for (int i = index; i + 1 < count; ++i) hand[i] = hand[i + 1];
    return count - 1;
}

static int cpu_play_min_single(Hand hands[PLAYERS], int cpu_id,
                               const char* field_name, const char** out_name);

/* 内部: 全員の配り目標に達したか？ */
static int deal_finished_all(const GameState* g){
    for (int p=0; p<PLAYERS; ++p){
        if (g->visible[p] < g->target[p]) return 0;
    }
    return 1;
}


/* ===== 公開API ===== */

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

    // 出力は毎フレーム初期化（残り値防止）
    if (out_played_player) *out_played_player = -1;
    if (out_field_name)    *out_field_name    = NULL;

    if (g->turn_delay > 0){
        g->turn_delay--;
        return 0;
    }

    int p = g->turn_player;

    // --- この手番で出す／出せなければパス ---
    if (hands[p].count > 0){
        const char* chosen = NULL;
        const char* field  = g->field_visible ? g->field_name : NULL;
        int played = cpu_play_min_single(hands, p, field, &chosen);
        if (played){
            g->field_name    = chosen;
            g->field_visible = 1;
            g->last_played   = p;
            g->pass_count    = 0; // 出たのでパス連続をリセット
            if (g->visible[p] > hands[p].count) g->visible[p] = hands[p].count;

            if (out_played_player) *out_played_player = p;
            if (out_field_name)    *out_field_name    = g->field_name;
        } else {
            // パスは「場に札がある時だけ」カウント（場が空なら自由出しなのでカウントしない）
            if (g->field_visible) g->pass_count++;
        }
    }

    // --- 場流し判定：最後に誰かが出してから (PLAYERS-1) 人連続でパス ---
    if (g->field_visible && g->pass_count >= (PLAYERS - 1)) {
        // 場をリセット
        g->field_visible = 0;
        g->field_name    = NULL;
        g->pass_count    = 0;

        // ★ リード権を「最後に出した本人」に戻す
        if (g->last_played >= 0) {
            g->turn_player = g->last_played;
        }
        g->turn_delay  = TURN_DELAY_FRAMES;
        return 1; // ここで終了（通常の+1はしない）
    }

    // --- 通常進行：次の人へ ---
    g->turn_player = (g->turn_player + 1) & 3;
    g->turn_delay  = TURN_DELAY_FRAMES;
    return 1;
}






/* CPU: 「場より大きい中で最小の1枚」を出す。なければパス。*/
static int cpu_play_min_single(Hand hands[PLAYERS], int cpu_id,
                               const char* field_name, const char** out_name)
{
    Hand* h = &hands[cpu_id];
    int best_i = -1;
    u8 field_rank = field_name ? eval_rank_from_name(field_name) : 0;

    for (int i = 0; i < h->count; ++i) {
        u8 r = eval_rank_from_name(h->cards[i]);
        if (field_name && r <= field_rank) continue;
        if (best_i < 0 || r < eval_rank_from_name(h->cards[best_i])) best_i = i;
    }

    if (best_i < 0) { if (out_name) *out_name = NULL; return 0; }

    const char* chosen = h->cards[best_i];
    h->count = remove_card_by_index(h->cards, h->count, best_i);

    if (out_name) *out_name = chosen;
    return 1;
}