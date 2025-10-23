#include "def.h"
#include "erapi.h"
#include "bg.h"
#include "obj_atlas.h"
#include "sprite_bare.h"
#include "rng.h"
#include "cards.h"
#include "deck.h"
#include "game.h"
#include "render.h"
#include "sound.h"

/* game.c 側のエクスポート（ヘッダは触らず extern 宣言で使う） */
extern int game_consume_pending_sfx(int* out_id);
extern int game_consume_pending_banner(const char** out_name);

/* 画面のVBlank開始まで待つ（描画同期） */
static inline void wait_vblank_start(void){
  while (REG_VCOUNT >= 160);
  while (REG_VCOUNT < 160);
}
extern int __end[];

/* 背景/VRAMの初期化 */
static void init_system(void){
  int kb = ((ERAPI_RAM_END - (u32)__end) >> 10) - 32; if (kb < 0) kb = 0;
  ERAPI_InitMemory(kb);
  ERAPI_SetBackgroundMode(0);
}

/* ゲーム用の大域変数類 */
static GameState g;
static int g_player_face_tile_base[12];
static int g_back_tile_base = 0;
static int g_field_tile_base = -1;
static int banner_shown = 0;

int main(void){
  /* 画面＆UI */
  init_system();  
  render_init_ui();

  /* サウンド初期化（BGMは必要に応じて） */
  sound_init();
  sound_set_bgm_volume(100);
  sound_set_se_volume(127);
  int bgm_started = 0;

  /* デッキ構築・乱数初期化・配布 */
  u8 deck[MAX_DECK];
  int deck_n = build_deck(deck);
  rng_seed(15, 120);
  shuffle_deck(deck, deck_n);
  Hand hands[PLAYERS];
  deal_round_robin(deck, deck_n, 1, hands);
  for (int p=0; p<PLAYERS; ++p) sort_hand(&hands[p]);

  /* ゲーム状態を初期化 */
  game_init(&g, hands, /*start_player_for_deal=*/0);
  const Hand* myhand = &hands[0];

  /* VRAM 初期セットアップ */
  wait_vblank_start();
  render_init_vram(myhand, /*max_player_show=*/12,
                   g_player_face_tile_base,
                   &g_back_tile_base,
                   &g_field_tile_base);
  ERAPI_RenderFrame(1);
  ERAPI_FadeIn(1);

  u32 prev = 0; // 前フレームの入力状況
  for(;;){
    const char* fxname = NULL; /* 役スプライト名（毎フレーム初期化） */

    /* 入力（Bで終了） */
    u32 key  = ERAPI_GetKeyStateRaw();
    u32 edge = key & ~prev; prev = key;
    if (edge & ERAPI_KEY_B) break;

    /* 1) 配りアニメ */
    int dealt_now = game_step_deal(&g);
    if (dealt_now){
      sound_play_se(SND_SE_DEAL);
    }

    /* 2) 配り完了の瞬間だけ BGM を開始 */
    if (!bgm_started && g.deal_done){
      sound_play_bgm(SND_BGM_GAME, /*loop=*/1);
      bgm_started = 1;
    }

    /* 3) 通常ターン進行（役SEは game が要求→main が鳴らす） */
    int who = g.turn_player;
    int played = game_step_turn(&g, hands);
    if (played){
      if (g.field_visible && g.field_count > 0){
        render_upload_field_cards(g.field_names, g.field_count);
      }
      if (who == 0){
        render_reload_hand_card(&hands[0], g_player_face_tile_base, /*start=*/0);
      }
    }

    /* 4) ★ SE 再生：game の要求を1フレームに一度だけ消費して鳴らす */
    int se_id = -1;
    if (game_consume_pending_sfx(&se_id)){
      sound_play_se(se_id);
    }

    /* 5) ★ 役スプライト表示要求：game → main で消費して出す */
    if (game_consume_pending_banner(&fxname)) {
      render_show_role_sprite(fxname);   // "yagiri","sibari","11back","kaidan","kakumei"
      banner_shown = 1;
    }

    /* 6) ★ 待機が終わったら消す（待機は game 側 g.fx_display_time で管理） */
    if (banner_shown && g.fx_display_time == 0) {
      render_hide_role_sprite();
      banner_shown = 0;
    }

    /* サウンド更新（必要に応じて） */
    sound_update();

    /* 描画更新 */
    render_frame(g.visible, g_player_face_tile_base, g_back_tile_base,
                 g.field_visible, g.field_count);
    ERAPI_RenderFrame(1);
  }

  /* 終了時 */
  sound_stop_bgm();
  return ERAPI_EXIT_TO_MENU;
}
