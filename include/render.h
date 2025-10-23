#ifndef RENDER_H
#define RENDER_H

#include "def.h"
#include "game.h"
#include "cards.h"

#ifdef __cplusplus
extern "C" {
#endif

/*ゲーム画面（背景やUI）を初期化 */
void render_init_ui(void); 

/* 初期VRAMロード（VBlank中に呼ぶ） */
void render_init_vram(const Hand* me,
                      int max_player_show,
                      int out_face_tile_base[12],
                      int* out_back_tile_base,
                      int* out_field_tile_base /* 互換のため残すが未使用可 */);

/* （互換）場カード1枚の絵をVRAMに上書き（16x32=8タイル）
   ※ 複数枚対応後は render_set_field_cards() を推奨 */
void render_upload_field_card(const char* name, int field_tile_base);

/* 場のカード群（表）をVRAMにロード（最大4枚） */
void render_set_field_cards(const char* const names[4], int count);

/* 毎フレームのOAM更新（ERAPI_RenderFrame(1)直後に必ず呼ぶこと）
   新API：場は field_visible / field_count のみ渡す（内部にロード済み配列あり） */
void render_frame(const int g_visible[PLAYERS],
                  const int player_face_tile_base[12],
                  int back_tile_base,
                  int field_visible,
                  int field_count);

/* 自分の手札が減った/並びが変わったときに表を再転送（最大12） */
void render_reload_hand_card(const Hand* me,
                             int player_face_tile_base[12],
                             int start_tile_base /*通常0*/);

/* 8切りの表示要求（待機中だけON）— 見た目の優先度は「しばり ＞ 8切り」 */
void render_set_yagiri_visible(int on);

/* しばりを N フレームだけ表示要求（例: 60で約1秒）。
   8切りの表示要求より“見た目上”優先。 */
void render_trigger_sibari(int frames);

/* 互換API: 旧名を新実装へフォワード（旧 main.c 対応） */
void render_upload_field_cards(const char** names, int count);

void render_effect_enqueue(int effect, int frames);

int  render_is_effect_active(void);
void render_show_role_sprite(const char* name);  /* "yagiri","sibari","11back","kaidan","kakumei" */
void render_hide_role_sprite(void);

#ifdef __cplusplus
}
#endif
#endif /* RENDER_H */
