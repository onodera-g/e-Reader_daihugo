#include "game.h"
#include "rng.h"
#include "cards.h"
#include "render.h"
#include "sound.h"
#include "def.h"
#include "ai.h"
#include "deck.h"

/* ==== サウンドID（数値直指定） ==== */
#define SE_NORMAL_PLAY   65  /* 通常 */
#define SE_ROLE_COMMON   28  /* 8切り/しばり/11バック/階段 */
#define SE_KAKUMEI       28  /* 革命 */

/* ==== 役待機（フレーム数） ==== */
#ifndef ROLE_WAIT_FRAMES
#define ROLE_WAIT_FRAMES 60   /* 約1秒（60fps想定） */
#endif
#ifndef FX_WAIT_CAP
#define FX_WAIT_CAP 240
#endif

/* --------- SE通知（main でのみ再生する） --------- */
static int s_sfx_pending = 0;
static int s_sfx_id      = -1;
/* main.c から取り出す用（ヘッダは触らず extern 宣言で使う） */
int game_consume_pending_sfx(int* out_id){
    if (!s_sfx_pending) return 0;
    if (out_id) *out_id = s_sfx_id;
    s_sfx_pending = 0;
    return 1;
}

/* ==== 役スプライト通知（main が拾う） ==== */
static int   s_banner_pending = 0;
static const char* s_banner_name = NULL;
/* 外部公開：main.c から取得して表示する */
int game_consume_pending_banner(const char** out_name){
    if (!s_banner_pending) return 0;
    if (out_name) *out_name = s_banner_name;
    s_banner_pending = 0;
    return 1;
}

/* 8切りの場クリア予約フラグ */
static int s_pending_yagiri_clear = 0;

/* ユーティリティ */
static void remove_card_at(Hand* h, int index){
    for (int i=index+1;i<h->count;++i) h->cards[i-1] = h->cards[i];
    h->count--;
}
static int remove_card_value_once(Hand* h, u8 card){
    for (int i=0;i<h->count;++i) if (h->cards[i] == card){ remove_card_at(h,i); return 1; }
    return 0;
}
static int deal_finished_all(const GameState* g){
    for (int p=0; p<PLAYERS; ++p) if (g->visible[p] < g->target[p]) return 0;
    return 1;
}
static inline u8 compute_suit_mask(const u8* cards, u8 n){
    u8 m=0; for (u8 i=0;i<n;++i) m |= (1u<<CARD_SUIT(cards[i])); return m;
}
static void reset_field(GameState* g){
    g->field_visible     = 0;
    g->field_count       = 0;
    g->field_eff_rank    = 0;
    for (int i=0;i<4;++i) g->field_names[i] = NULL;
    g->field_suit_mask   = 0;
    g->sibari_active     = 0;
    g->field_is_straight = 0;
}

/* 待機タイマー：最低 frames にセット（加算ではなく下駄をはかせる） */
static inline void fx_set_wait(GameState* g, int frames){
    if (frames <= 0) frames = ROLE_WAIT_FRAMES;
    if (frames > FX_WAIT_CAP) frames = FX_WAIT_CAP;
    if (g->fx_display_time < frames){
        g->fx_display_time = frames;
        g->fx_active = 1;
    }
}

/* 形・効果判定 */
typedef enum { PLAY_NONE=0, PLAY_SET, PLAY_STRAIGHT } PlayKind;
typedef struct {
    PlayKind kind;
    u8 new_mask;
    u8 eight_cut;
    u8 rev_toggle;
    u8 jback_toggle;
    u8 sibari_on;
} PlayInfo;

enum { RANK_8 = 8, RANK_J = 11, RANK_2 = 15, RANK_JOKER = 16 };

static void sort_u8(u8* a, u8 n){
    for (u8 i=0;i<n;i++) for (u8 j=i+1;j<n;j++) if (a[i]>a[j]){ u8 t=a[i]; a[i]=a[j]; a[j]=t; }
}

static int judge_straight(const u8* cards, u8 n){
    if (n < 3) return 0;
    u8 r[4];
    for (u8 i=0;i<n;++i){
        u8 ri = CARD_RANK(cards[i]);
        if (ri >= RANK_2) return 0; /* 2/Jokerは階段不可 */
        r[i] = ri;
    }
    u8 s0 = CARD_SUIT(cards[0]);
    for (u8 i=1;i<n;++i) if (CARD_SUIT(cards[i]) != s0) return 0;
    sort_u8(r, n);
    for (u8 i=1;i<n;++i) if (r[i] != (u8)(r[i-1]+1)) return 0;
    return 1;
}

static PlayKind detect_play_kind(const u8* cards, u8 n){
    if (n==1) return PLAY_NONE;
    if (judge_straight(cards, n)) return PLAY_STRAIGHT;
    u8 r0=CARD_RANK(cards[0]); for (u8 i=1;i<n;++i) if (CARD_RANK(cards[i])!=r0) return PLAY_NONE;
    return PLAY_SET;
}

static PlayInfo detect_play_effects(const u8* cards, u8 n, u8 prev_mask){
    PlayInfo info;
    info.kind         = detect_play_kind(cards, n);
    info.new_mask     = compute_suit_mask(cards, n);
    info.eight_cut    = 0;
    info.rev_toggle   = 0;
    info.jback_toggle = 0;
    info.sibari_on    = 0;

    u8 r0 = CARD_RANK(cards[0]);

    if (info.kind == PLAY_SET){
        if (r0 == RANK_8) info.eight_cut = 1;
        if (n >= 4)       info.rev_toggle = 1;
        if (prev_mask != 0 && info.new_mask == prev_mask) info.sibari_on = 1;
    }else if (info.kind == PLAY_NONE){
        if (r0 == RANK_J)      info.jback_toggle = 1;
        else if (r0 == RANK_8) info.eight_cut    = 1;
        if (prev_mask != 0 && info.new_mask == prev_mask) info.sibari_on = 1;
    }
    return info;
}

static int is_play_legal(const GameState* g, const u8* cards, u8 n){
    if (!g->field_visible) return 1;

    PlayKind kind = detect_play_kind(cards, n);
    if ((g->field_is_straight ? 1 : 0) != (kind == PLAY_STRAIGHT ? 1 : 0)) return 0;

    if (!g->field_is_straight){
        if (n != g->field_count) return 0;
        if (g->sibari_active){
            u8 mask = compute_suit_mask(cards, n);
            if (mask != g->field_suit_mask) return 0;
        }
    }

    u8 base_rank = CARD_RANK(cards[0]);
    if (kind == PLAY_STRAIGHT){
        u8 maxr = base_rank;
        for (u8 i=1;i<n;++i){ u8 r = CARD_RANK(cards[i]); if (r > maxr) maxr = r; }
        base_rank = maxr;
    }
    u8 eff = rank_effective_ext(base_rank, (u8)g->revolution_active, (u8)g->jback_active);
    return (eff > g->field_eff_rank);
}

/* ====== 出し適用（革命→8切り→Jバック→場更新→階段→しばり） ======
   ・役発生：SE“要求”を立てて 1 秒待機
   ・通常出し：SE=65“要求”、待機なし
*/
static void apply_play(GameState* g, const u8* cards, u8 n){
    PlayInfo info = detect_play_effects(cards, n, g->field_suit_mask);
    int did_role = 0;

    /* 1) 革命（毎回表示） */
    if (info.rev_toggle){
        g->revolution_active ^= 1;

        /* ★スプライト要求 */
        s_banner_name = "kakumei"; s_banner_pending = 1;

        /* SE と待機 */
        s_sfx_id = SE_KAKUMEI; s_sfx_pending = 1;
        fx_set_wait(g, ROLE_WAIT_FRAMES);
        did_role = 1;
    }

    /* 2) 8切り（毎回表示） */
    if (info.eight_cut && !info.rev_toggle){
        /* 場へ一旦表示（既存処理のまま） */
        g->field_visible     = 1;
        g->field_count       = n;
        g->field_is_straight = (detect_play_kind(cards, n) == PLAY_STRAIGHT);

        u8 base = CARD_RANK(cards[0]);
        if (g->field_is_straight){
            for (u8 i=1;i<n;++i){ u8 r = CARD_RANK(cards[i]); if (r > base) base = r; }
        }
        g->field_eff_rank = rank_effective_ext(base, (u8)g->revolution_active, (u8)g->jback_active);

        for (int i=0;i<4;++i) g->field_names[i] = NULL;
        for (u8 i=0;i<n;++i)  g->field_names[i] = card_to_string(cards[i]);
        g->field_suit_mask = compute_suit_mask(cards, n);

        render_set_field_cards((const char* const*)g->field_names, n);

        /* ★スプライト要求 */
        s_banner_name = "yagiri"; s_banner_pending = 1;

        /* SE と待機 */
        s_sfx_id = SE_ROLE_COMMON; s_sfx_pending = 1;
        fx_set_wait(g, ROLE_WAIT_FRAMES);

        s_pending_yagiri_clear = 1;
        return; /* 待機に入る */
    }

    /* 3) 11バック（毎回表示） */
    if (info.jback_toggle && !did_role){
        g->jback_active ^= 1;

        /* ★スプライト要求 */
        s_banner_name = "11back"; s_banner_pending = 1;

        s_sfx_id = SE_ROLE_COMMON; s_sfx_pending = 1;
        fx_set_wait(g, ROLE_WAIT_FRAMES);
        did_role = 1;
    }

    /* 4) 場更新 */
    g->field_visible     = 1;
    g->field_count       = n;
    g->field_is_straight = (info.kind == PLAY_STRAIGHT);

    {
        u8 base = CARD_RANK(cards[0]);
        if (g->field_is_straight){
            for (u8 i=1;i<n;++i){ u8 r = CARD_RANK(cards[i]); if (r > base) base = r; }
        }
        g->field_eff_rank = rank_effective_ext(base, (u8)g->revolution_active, (u8)g->jback_active);
    }
    for (int i=0;i<4;++i) g->field_names[i] = NULL;
    for (u8 i=0;i<n;++i)  g->field_names[i] = card_to_string(cards[i]);
    g->field_suit_mask = info.new_mask;

    render_set_field_cards((const char* const*)g->field_names, n);

    /* 5) 階段（★初成立時のみ表示） */
    if (g->field_is_straight && !did_role){
        s_banner_name = "kaidan"; s_banner_pending = 1;

        s_sfx_id = SE_ROLE_COMMON; s_sfx_pending = 1;
        fx_set_wait(g, ROLE_WAIT_FRAMES);
        did_role = 1;
    }

    /* 6) しばり新規成立（階段では付けない） */
    if (!g->field_is_straight && info.sibari_on && !did_role){
        g->sibari_active = 1;

        s_banner_name = "sibari"; s_banner_pending = 1;

        s_sfx_id = SE_ROLE_COMMON; s_sfx_pending = 1;
        fx_set_wait(g, ROLE_WAIT_FRAMES);
        did_role = 1;
    }

    /* 7) 役が無ければ通常SE（65） */
    if (!did_role){
        s_sfx_id = SE_NORMAL_PLAY; s_sfx_pending = 1;
    }
}

/* 初期化・配布 */
void game_init(GameState* g, const Hand hands[PLAYERS], int start_player_for_deal){
    for (int p=0;p<PLAYERS;++p){ g->visible[p]=0; g->target[p]=hands[p].count; }
    g->deal_turn  = start_player_for_deal & 3;
    g->deal_delay = 0;
    g->deal_done  = 0;

    g->turn_player = 0;
    g->turn_delay  = 0;

    g->field_visible     = 0;
    g->last_played       = -1;
    g->pass_count        = 0;
    g->field_count       = 0;
    g->field_eff_rank    = 0;
    g->field_suit_mask   = 0;
    g->field_is_straight = 0;
    g->sibari_active     = 0;
    g->revolution_active = 0;
    g->jback_active      = 0;
    for (int i=0;i<4;++i) g->field_names[i]=NULL;

    g->fx_active       = 0;
    g->fx_display_time = 0;

    s_pending_yagiri_clear = 0;
    s_sfx_pending = 0;
    s_sfx_id = -1;

    /* バナー通知も初期化 */
    s_banner_pending = 0;
    s_banner_name = NULL;
}

int game_step_deal(GameState* g){
    if (g->deal_done) return 0;
    if (g->deal_delay > 0){ --g->deal_delay; return 0; }

    int p = g->deal_turn;
    if (g->visible[p] < g->target[p]){
        g->visible[p]++;
        g->deal_delay = DEAL_DELAY_FRAMES;
    }else{
        g->deal_turn = (g->deal_turn + 1) & 3;
        if (deal_finished_all(g)) g->deal_done = 1;
    }
    return 1;
}

/* 1ターン進行（待機中は進めない。Jバックは場流しで解除） */
int game_step_turn(GameState* g, Hand hands[PLAYERS]){
    if (!g->deal_done) return 0;

    /* 役待機の消化：呼び出し1回につき1だけ減算（1フレーム=1回想定） */
    if (g->fx_display_time > 0){
        g->fx_display_time -= 1;
        if (g->fx_display_time <= 0){
            g->fx_display_time = 0;
            g->fx_active = 0;

            if (s_pending_yagiri_clear){
                reset_field(g);
                render_set_field_cards(NULL, 0);
                g->jback_active = 0;  /* Jバックは場流しで解除 */
                s_pending_yagiri_clear = 0;
            }
        }else{
            g->fx_active = 1;
        }
        return 0; /* 待機中は進行しない */
    }

    if (g->turn_delay > 0){ --g->turn_delay; return 0; }

    int p = g->turn_player;

    FieldState fs;
    fs.field_visible        = (u8)g->field_visible;
    fs.revolution           = (u8)g->revolution_active;
    fs.jback_active         = (u8)g->jback_active;
    fs.sibari_active        = g->sibari_active;
    fs.right_neighbor_count = 0;

    fs.field_count          = g->field_count;
    fs.field_eff_rank       = g->field_eff_rank;
    fs.field_suit_mask      = g->field_suit_mask;
    for (int i=0;i<4;++i) fs.field_names[i] = g->field_names[i];
    fs.field_is_straight    = g->field_is_straight;

    u8 chosen[4]={0,0,0,0}; u8 n=0;
    int played = ai_choose_move_group(&hands[p], &fs, chosen, &n);

    if (played && n > 0 && is_play_legal(g, chosen, n)){
        for (u8 i=0;i<n;++i) remove_card_value_once(&hands[p], chosen[i]);
        apply_play(g, chosen, n);
        g->visible[p]  = hands[p].count;
        g->last_played = p;
        g->pass_count  = 0;
        g->turn_player = (p + 1) & 3;
        g->turn_delay  = TURN_DELAY_FRAMES;
        return 1;
    }else{
        g->pass_count++;
        g->turn_player = (p + 1) & 3;
        g->turn_delay  = TURN_DELAY_FRAMES;
        if (g->pass_count >= 3){
            reset_field(g);
            render_set_field_cards(NULL, 0);
            g->jback_active = 0; /* Jバックは場流しで解除 */
            g->pass_count = 0;
        }
        return 0;
    }
}
