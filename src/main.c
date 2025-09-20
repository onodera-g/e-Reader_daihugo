// src/main.c
#include "def.h"
#include "erapi.h"
#include "bg.h"
#include "obj_atlas.h"
#include "sprite_bare.h"

extern int __end[];

// sprite_bare.h の REG_DISPCNT/OBJ_* を使う
static inline void force_obj_1d(void){ REG_DISPCNT |= (OBJ_ENABLE | OBJ_1D_MAP); }

/* ---------- 背景 ---------- */
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

/* ---------- アトラス 16x32 を1OBJで表示（OAM[index]） ---------- */
static const char* kSpriteNames[] = {
  "H_1", "H_2", "H_3", "H_4", "H_5", "H_6", "H_7",
  "H_8", "H_9", "H_10", "H_11", "H_12", "H_13",
};
static const int kSpriteCount = (int)(sizeof(kSpriteNames)/sizeof(kSpriteNames[0]));
static void show_card16x32_single(const char* name, int x, int y,
                                  int tile_base, int pal_bank, int oam_index){
  int idx = objAtlasFindIndex(name); if (idx < 0) return;
  const ObjSpriteDesc* d = &objAtlasSprites[idx];
  // 16x32以外はスキップ（必要なければ外してOK）
  if (!(d->w == 16 && d->h == 32)) return;

  // 画像データ（8タイル＝256B）を OBJ VRAM（1D）へ配置
  const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4; // words→bytes
  u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;                   // 1タイル=32B
  spr_dma_copy32(dst, src, (8 * 32) / 4);

  // パレット（共通16色、毎フレーム上書きでも問題なし）
  spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 16/2);

  // OAM登録（TALL, size=2 → 16x32）
  volatile u16* o = OAM_ATTR(oam_index);
  o[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
  o[1] = ATTR1_X(x) | ATTR1_SIZE(2);
  o[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* ---------- サウンド ---------- */
static inline void set_default_audio_params(void){
  ERAPI_SetSoundVolume(0,255); ERAPI_SetSoundVolume(1,255);
  ERAPI_SetSoundSpeed(0,128);  ERAPI_SetSoundSpeed(1,128);
}
static inline void stop_all_sound(void){ ERAPI_SoundPause(0); ERAPI_SoundPause(1); }
static inline void play_music_id(int id){
  set_default_audio_params(); ERAPI_SoundPause(0); ERAPI_SoundPause(1);
  ERAPI_PlaySoundSystem((u32)id);
}

/* ---------- main ---------- */
int main(void){
  init_screen_and_ui();
  set_default_audio_params();
  play_music_id(22);

  // 起動時初期化（OBJ: mode0 + 1D + 有効化）
  spr_init_mode0_obj1d();
  force_obj_1d();
  ERAPI_FadeIn(1);

  u32 prev = 0;
  for(;;){
    u32 key  = ERAPI_GetKeyStateRaw();
    u32 edge = key & ~prev; prev = key;
    if (edge & ERAPI_KEY_B) break;
    if (edge & ERAPI_KEY_A) play_music_id(22);

    // ERAPI がフレーム終端で OAM を触る → 直後に OBJ を毎フレーム再設定
    ERAPI_RenderFrame(1);
    spr_init_mode0_obj1d();
    force_obj_1d();

// レイアウト設定（16x32を整列表示する例：7列×複数行）
const int card_w = 16;   // カード幅
const int card_h = 32;   // カード高さ
const int gap_x  = 2;    // 横間隔なしで詰める
const int start_x = 5;   // 左余白
const int y = 40;        // 縦位置（全カード共通）

for (int i = 0; i < kSpriteCount; i++) {
  int x = start_x + i* (card_w + gap_x);  // 横方向に詰める

  int tile_base = 8 * i;  // 1枚=8タイル消費
  int oam_index = i;      // OAM index 連番

  show_card16x32_single(kSpriteNames[i], x, y, tile_base, /*pal=*/0, oam_index);
}
  }

  stop_all_sound();
  return ERAPI_EXIT_TO_MENU;
}
