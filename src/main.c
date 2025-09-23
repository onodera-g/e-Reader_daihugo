// src/main.c
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

/* ── ユーティリティ ── */
static inline void wait_vblank_start(void){
  while (REG_VCOUNT >= 160);
  while (REG_VCOUNT < 160);
}

/* ── サウンド定義 ── */
#define SFX_DEAL_ID  24
#define SFX_YAGIRI_ID 25

extern int __end[];

/* ── 背景初期化 ── */
static void init_screen_and_ui(void){
  int kb = ((ERAPI_RAM_END - (u32)__end) >> 10) - 32; if (kb < 0) kb = 0;
  ERAPI_InitMemory(kb);
  ERAPI_SetBackgroundMode(0);
  ERAPI_BACKGROUND bg = {
    .data_gfx = (u8*)bgTiles,
    .data_pal = (u8*)bgPal,
    .data_map = (u8*)bgMap,
    .tiles    = (u16)(bgTilesLen / 32),
    .palettes = (u16)(bgPalLen   / 32),
  };
  ERAPI_LoadBackgroundCustom(0, &bg);
  ERAPI_LayerShow(0);
}

/* ── 進行状態（描画に渡すのは render_* の引数で） ── */
static GameState g;

/* ── VRAMタイル参照（render_init_vramで埋める） ── */
static int g_player_face_tile_base[12];
static int g_back_tile_base = 0;
static int g_field_tile_base = -1;

int main(void){
  init_screen_and_ui();
  ERAPI_FadeIn(1);

  /* デッキ → シャッフル → 配る（CPU1を開始にして14枚になる配り順） */
  const char* deck[MAX_DECK];
  int deck_n = build_deck(deck);
  rng_seed((u32)REG_VCOUNT ^ (u32)ERAPI_GetKeyStateRaw() ^ 0x9E3779B9u);
  shuffle_deck(deck, deck_n);

  Hand hands[PLAYERS];
  deal_round_robin_offset(deck, deck_n, /*start=*/1, hands); /* CPU1 から配り始め */

  /* 進行状態の初期化：配り着地点を hands に合わせて設定 */
  game_init(&g, hands, /*start_player_for_deal=*/0);

  const Hand* me = &hands[0];

  /* 初期VRAMロード（VBlank 中） */
  wait_vblank_start();
  render_init_vram(me, /*max_player_show=*/12,
                   g_player_face_tile_base,
                   &g_back_tile_base,
                   &g_field_tile_base);
  ERAPI_RenderFrame(1); /* 初期反映 */

  u32 prev = 0;
  for(;;){
    /* 入力 */
    u32 key  = ERAPI_GetKeyStateRaw();
    u32 edge = key & ~prev; prev = key;
    if (edge & ERAPI_KEY_B) break;

    /* === 配り演出：1フレーム進める（CPU1に配ったら1が返る） === */
    int play_sfx_cpu1 = game_step_deal(&g);

    /* === ERAPIフレーム確定（OAM初期化/サウンド更新など） === */
    ERAPI_RenderFrame(1);

    /* === 配り完了後：ターン進行（誰かが出したら1を返す） === */
    int played = 0, who = -1;
    const char* field_name = NULL;
    played = game_step_turn(&g, hands, &who, &field_name);
    if (played && field_name){
      /* 場カードのVRAM差し替え（描画は次の render_frame で反映） */
      render_upload_field_card(field_name, g_field_tile_base);

      /* 自分が出したときだけ、表VRAMを詰め直す（最大12） */
      if (who == 0){
        render_reload_hand_card(&hands[0], g_player_face_tile_base, /*start=*/0);
      }
    }

    /* ★ 8切りSEをこのフレームだけ鳴らす */
    if (g.sfx_yagiri){
      ERAPI_PlaySoundSystem((u32)SFX_YAGIRI_ID);
      g.sfx_yagiri = 0;  /* 消費 */
    }

    /* ★ バナー表示ON/OFF：待機中だけ表示 */
    render_set_yagiri_visible(g.yagiri_waiting);

    /* === 毎フレームの描画（OAM を並べ直す） === */
    render_frame(g.visible,
                 g_player_face_tile_base,
                 g_back_tile_base,
                 g_field_tile_base,
                 g.field_visible);

    /* CPU1に配ったフレームだけSE */
    if (play_sfx_cpu1){
      ERAPI_PlaySoundSystem((u32)SFX_DEAL_ID);
    }
  }

  return ERAPI_EXIT_TO_MENU;
}
