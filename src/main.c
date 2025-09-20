// src/main.c
#include "def.h"
#include "erapi.h"
#include "bg.h"
#include "obj_atlas.h"
#include "sprite_bare.h"

extern int __end[];

/* ---------------- 基本ユーティリティ ---------------- */
static inline void force_obj_1d(void){ REG_DISPCNT |= (OBJ_ENABLE | OBJ_1D_MAP); }
static inline void wait_vblank_start(void){
  while (REG_VCOUNT >= 160); // 画面期間が終わるのを待つ
  while (REG_VCOUNT < 160);  // VBlank突入を待つ
}

/* ---------------- 背景 ---------------- */
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

/* ---------------- カード名（16x32前提） ---------------- */
static const char* kHearts[]   = {"H_1","H_2","H_3","H_4","H_5","H_6","H_7","H_8","H_9","H_10","H_11","H_12","H_13"};
static const char* kDiamonds[] = {"D_1","D_2","D_3","D_4","D_5","D_6","D_7","D_8","D_9","D_10","D_11","D_12","D_13"};
static const char* kSpades[]   = {"S_1","S_2","S_3","S_4","S_5","S_6","S_7","S_8","S_9","S_10","S_11","S_12","S_13"};
static const char* kClubs[]    = {"C_1","C_2","C_3","C_4","C_5","C_6","C_7","C_8","C_9","C_10","C_11","C_12","C_13"};
static const int kPerSuit = 13;

/* ---------------- 1回だけのVRAMロード ---------------- */
static void upload_card16x32_once(const char* name, int tile_base, int pal_bank){
  int idx = objAtlasFindIndex(name); if (idx < 0) return;
  const ObjSpriteDesc* d = &objAtlasSprites[idx];
  if (!(d->w == 16 && d->h == 32)) return;

  // 画像（8タイル=256B）→ OBJ VRAM（1D）
  const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
  u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32; // 1タイル=32B
  spr_dma_copy32(dst, src, (8 * 32) / 4);

  // 共通16色パレット
  spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 16/2);
}

/* ---------------- OAM 1体ぶんの直書き ---------------- */
static inline void oam_set_16x32(int oam_index, int x, int y, int tile_base, int pal_bank){
  volatile u16* o = OAM_ATTR(oam_index);
  o[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
  o[1] = ATTR1_X(x) | ATTR1_SIZE(2);
  o[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* ---------------- 1行のVRAMロード（VBlank内だけ） ---------------- */
static void preload_row_16x32(const char** names, int count, int* tile_base, int pal_bank){
  for (int i = 0; i < count; i++){
    int idx = objAtlasFindIndex(names[i]);
    if (idx < 0) continue;
    upload_card16x32_once(names[i], *tile_base, pal_bank);
    *tile_base += 8; // 16x32/4bpp → 8タイル
  }
}

/* ---------------- サウンド ---------------- */
static inline void set_default_audio_params(void){
  ERAPI_SetSoundVolume(0,255); ERAPI_SetSoundVolume(1,255);
  ERAPI_SetSoundSpeed(0,128);  ERAPI_SetSoundSpeed(1,128);
}
static inline void stop_all_sound(void){ ERAPI_SoundPause(0); ERAPI_SoundPause(1); }
static inline void play_music_id(int id){
  set_default_audio_params(); ERAPI_SoundPause(0); ERAPI_SoundPause(1);
  ERAPI_PlaySoundSystem((u32)id);
}

/* ---------------- main ---------------- */
int main(void){
  init_screen_and_ui();
  set_default_audio_params();
  play_music_id(22);

  // 起動時（OBJ: mode0 + 1D + 有効化）
  spr_init_mode0_obj1d();
  force_obj_1d();
  ERAPI_FadeIn(1);

  /* ===== 起動時にだけ VRAM を段階ロード（4フレーム）===== */
  int tb = 0;
  // ♥
  wait_vblank_start(); preload_row_16x32(kHearts,   kPerSuit, &tb, 0); ERAPI_RenderFrame(1);
  // ♦
  wait_vblank_start(); preload_row_16x32(kDiamonds, kPerSuit, &tb, 0); ERAPI_RenderFrame(1);
  // ♠
  wait_vblank_start(); preload_row_16x32(kSpades,   kPerSuit, &tb, 0); ERAPI_RenderFrame(1);
  // ♧
  wait_vblank_start(); preload_row_16x32(kClubs,    kPerSuit, &tb, 0); ERAPI_RenderFrame(1);

  /* ===== メインループ：OAM 直書きのみ（毎フレーム）===== */
  u32 prev = 0;
  for(;;){
    u32 key  = ERAPI_GetKeyStateRaw();
    u32 edge = key & ~prev; prev = key;
    if (edge & ERAPI_KEY_B) break;
    if (edge & ERAPI_KEY_A) play_music_id(22);

    // ERAPI がフレーム終端で OAM を触る → 直後に OBJ 再設定して OAM を直書き
    ERAPI_RenderFrame(1);
    spr_init_mode0_obj1d();
    force_obj_1d();

    // レイアウト（横1ドット間隔・各スート1行）
    const int start_x = 8, gap_x = 1;
    const int card_w = 16, card_h = 32;

    int oam = 0;
    int tile_base = 0;

    // ♥ 行
    for (int i = 0; i < kPerSuit; i++){
      int idx = objAtlasFindIndex(kHearts[i]); if (idx < 0) continue;
      int x = start_x + i * (card_w + gap_x);
      int y = 8;
      oam_set_16x32(oam++, x, y, tile_base, 0);
      tile_base += 8;
    }
    // ♦ 行
    for (int i = 0; i < kPerSuit; i++){
      int idx = objAtlasFindIndex(kDiamonds[i]); if (idx < 0) continue;
      int x = start_x + i * (card_w + gap_x);
      int y = 48;
      oam_set_16x32(oam++, x, y, tile_base, 0);
      tile_base += 8;
    }
    // ♠ 行
    for (int i = 0; i < kPerSuit; i++){
      int idx = objAtlasFindIndex(kSpades[i]); if (idx < 0) continue;
      int x = start_x + i * (card_w + gap_x);
      int y = 88;
      oam_set_16x32(oam++, x, y, tile_base, 0);
      tile_base += 8;
    }
    // ♧ 行
    for (int i = 0; i < kPerSuit; i++){
      int idx = objAtlasFindIndex(kClubs[i]); if (idx < 0) continue;
      int x = start_x + i * (card_w + gap_x);
      int y = 128;
      oam_set_16x32(oam++, x, y, tile_base, 0);
      tile_base += 8;
    }
    // 余りOAMは画面外に退避
    for (int i = oam; i < 128; i++){
      volatile u16* o = OAM_ATTR(i);
      o[0] = ATTR0_Y(160);
      o[1] = 0;
      o[2] = 0;
    }
  }

  stop_all_sound();
  return ERAPI_EXIT_TO_MENU;
}
