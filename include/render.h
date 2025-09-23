#ifndef RENDER_H
#define RENDER_H

#include "def.h"
#include "game.h"
#include "cards.h"

/*
 * render.h
 * --------
 * GBAのOBJ描画（スプライト）モジュール。
 * - 初期VRAMロード（自分の表、裏面、場カード用）
 * - 場カードの差し替え（VRAM）
 * - 毎フレームのOAM並べ直し（ERAPI_RenderFrame(1)直後に呼ぶ）
 * - 自分の手札が減ったときのVRAM詰め直し（表、最大12枚まで）
 */

#ifdef __cplusplus
extern "C" {
#endif

/* 初期VRAMロード（VBlank中に呼ぶ） */
void render_init_vram(const Hand* me,
                      int max_player_show,
                      int out_face_tile_base[12],
                      int* out_back_tile_base,
                      int* out_field_tile_base);

/* 場カードの絵をVRAMに上書き（16x32=8タイル） */
void render_upload_field_card(const char* name, int field_tile_base);

/* 毎フレームのOAM更新（ERAPI_RenderFrame(1)直後に必ず呼ぶこと） */
void render_frame(const int g_visible[PLAYERS],
                  const int player_face_tile_base[12],
                  int back_tile_base,
                  int field_tile_base,
                  int field_visible);

/* 自分の手札が減った/並びが変わったときに表を再転送（最大12） */
void render_reload_hand_card(const Hand* me,
                             int player_face_tile_base[12],
                             int start_tile_base /*通常0*/);


/* 中央に 48x16 “yagiri” を出せるように初期VRAMロード */
void render_init_yagiri(int* out_tile_base);

/* フラグで表示/非表示を切り替える（毎フレーム呼んでOK） */
void render_set_yagiri_visible(int on);

#ifdef __cplusplus
}
#endif
#endif /* RENDER_H */
