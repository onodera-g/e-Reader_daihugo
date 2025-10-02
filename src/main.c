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
// 描写フレームの調整
static inline void wait_vblank_start(void){
  while (REG_VCOUNT >= 160);
  while (REG_VCOUNT < 160);
}

/* ── サウンド定義 ── */
// 鳴らすサウンド番号
#define SFX_DEAL_ID   24
#define SFX_YAGIRI_ID 25
#define SFX_SIBARI_ID 26

extern int __end[];

/* ── 背景初期化 ── */
//背景の初期化処理
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

// ゲームの進行状況を管理
static GameState g;

/* ── VRAMタイル参照（render_init_vramで埋める） ── */
// 自分の手札で画面に表示する最大枚数（通常12枚）
static int g_player_face_tile_base[12];
// 表カード（自分の手札）のVRAM上の開始位置を受け取る配列
static int g_back_tile_base = 0;
// 裏面カード（CPU手札）の開始位置を受け取る
static int g_field_tile_base = -1;

int main(void){
  // 背景の描写
  init_screen_and_ui();
  ERAPI_FadeIn(1);

  // デッキ生成 → シャッフル → 配る → ソート
  const char* deck[MAX_DECK];
  int deck_n = build_deck(deck);
  rng_seed((u32)REG_VCOUNT ^ (u32)ERAPI_GetKeyStateRaw() ^ 0x9E3779B9u);
  shuffle_deck(deck, deck_n);
  Hand hands[PLAYERS];//
  deal_round_robin_offset(deck, deck_n,1, hands); 
  for (int p = 0; p < PLAYERS; ++p) {
    sort_hand(&hands[p]);
  }

  // 進行状態の初期化
  game_init(&g, hands, /*start_player_for_deal=*/0);
  const Hand* myhand = &hands[0];

  // カードの描写
  wait_vblank_start();
  render_init_vram(myhand, /*max_player_show=*/12,g_player_face_tile_base,
                   &g_back_tile_base,&g_field_tile_base);
  ERAPI_RenderFrame(1); 

  u32 prev = 0; // 前回のキー入力af
  for(;;){
    // 入力検知
    u32 key  = ERAPI_GetKeyStateRaw(); // 現フレームのキー入力
    u32 edge = key & ~prev; prev = key; // 前フレームと比較
    if (edge & ERAPI_KEY_B) break;

    // 手札として配る処理(1プレイヤーに1枚)
    int play_sfx_cpu1 = game_step_deal(&g);
    if (play_sfx_cpu1){
      ERAPI_PlaySoundSystem((u32)SFX_DEAL_ID);
    }
 
    // ゲームを進行
    // gでゲーム進行状況を渡してる。手札が配り終わってない(フラグが立っていない)時は、何もしない。
    int who = g.turn_player; //who=誰が出したか
    int played = 0;// played=カード出したかフラグ
    played = game_step_turn(&g, hands); // 1ターンゲームを進行

    // カードの描写更新
    // played = 1　のとき、カードが出ているため、描写を更新
    if (played){
      // VRAM差し替え（単数/複数いずれも対応）
      if (g.field_visible && g.field_count > 0){
        render_upload_field_cards(g.field_names, g.field_count);
      }
      // 自分が出したときだけ、表VRAMを詰め直す（最大12）
      if (who == 0){
        render_reload_hand_card(&hands[0], g_player_face_tile_base, /*start=*/0);
      }
    }

    // 役成立時の処理
    // SE,BGM切替
    if (g.sfx_yagiri){
      ERAPI_PlaySoundSystem((u32)SFX_YAGIRI_ID);// ８切り成立
      g.sfx_yagiri = 0;
    }
    if (g.sfx_sibari){
      ERAPI_PlaySoundSystem((u32)SFX_SIBARI_ID);// しばり成立
      g.sfx_sibari = 0;
    }
    // スプライトの表示切替(ON,OFF)
    render_set_yagiri_visible(g.yagiri_waiting); // 役が不成立ならスプライトを消す
    /* === 毎フレームの描画（OAM を並べ直す） === */
    render_frame(g.visible,
                 g_player_face_tile_base,
                 g_back_tile_base,
                 g.field_visible,
                 g.field_count);


    // フレーム確定
    ERAPI_RenderFrame(1);
  }

  return ERAPI_EXIT_TO_MENU;
}
