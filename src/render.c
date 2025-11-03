#include "render.h"
#include "obj_atlas.h"
#include "sprite_bare.h"
#include "erapi.h"
#include "cards.h"
#include "bg.h"

/* ================= 初期化 ================= */

void render_init_ui(void) {
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

/* ================= 内部状態 ================= */

/* エフェクト寿命（将来復帰用。描画はしない） */
static int s_fx_time[FXE_COUNT] = {0};

/* 場スロット（表 16x32 = 8タイル×4） */
static int s_field_slot0_base = -1;
static int s_field_tile_bases[4] = {-1,-1,-1,-1};
static int s_field_count = 0;

/* 役バナー：VRAMタイル先頭 / 表示フラグ / いまVRAMに載っている名前 */
static int  s_banner_tile_base = -1;       /* 12タイル確保（48x16 = 6x2 タイル） */
static int  s_banner_visible   = 0;
static char s_banner_loaded[16] = {0};     /* キャッシュ名 */
/* ★追加：-1=中央/0..3=各プレイヤの位置に表示 */
static int  s_banner_anchor_player = -1;

/* ================= ユーティリティ ================= */

static inline void force_obj_1d(void){
  REG_DISPCNT = (REG_DISPCNT & ~DCNT_OBJ) | DCNT_OBJ | DCNT_OBJ_1D;
}

static void upload_face_16x32_once_(const char* name, int tile_base, int pal_bank){
  int idx = objAtlasFindIndex(name);
  if (idx < 0) return;
  const ObjSpriteDesc* d = &objAtlasSprites[idx];
  if (!(d->w == 16 && d->h == 32)) return;

  const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
  u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
  /* 16x32 は 8タイル(8*32bytes) */
  spr_dma_copy32(dst, src, (8 * 32) / 4);
  spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}

static void upload_back_8x16_once_(const char* name, int tile_base, int pal_bank){
  int idx = objAtlasFindIndex(name);
  if (idx < 0) return;
  const ObjSpriteDesc* d = &objAtlasSprites[idx];
  if (!(d->w == 8 && d->h == 16)) return;

  const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
  u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
  /* 8x16 は 2タイル(2*32bytes) */
  spr_dma_copy32(dst, src, (2 * 32) / 4);
  spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}

/* 48x16 バナーを 12タイル(6x2)として VRAM に転送 */
static void upload_banner_48x16_once_(const char* name, int tile_base, int pal_bank){
  int idx = objAtlasFindIndex(name);
  if (idx < 0) return;
  const ObjSpriteDesc* d = &objAtlasSprites[idx];
  if (!(d->w == 48 && d->h == 16)) return;

  const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
  u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;

  /* タイルは 6(横) x 2(縦)。1Dで16x16を正しく組むため、
     [0,1,6,7] を dst[0..3] に、[2,3,8,9] を dst[4..7] に、
     [4,5,10,11] を dst[8..11] に並べ替えてコピーする */
  const int tiles_w = 6;
  const int tiles_h = 2; /* 高さ16なので2行 */
  const int TILE_BYTES = 32;

  /* ブロック i=0..2（左/中央/右 の 16x16） */
  for (int i = 0; i < 3; ++i){
    int c = i * 2;              /* このブロックの先頭列（8x8単位） */
    int src_idx[4] = {
      /* 上段2枚 */ c + 0, c + 1,
      /* 下段2枚 */ c + tiles_w + 0, c + tiles_w + 1
    };
    int dst_off = i * 4;        /* このブロックの出力先（4タイル分） */
    for (int j = 0; j < 4; ++j){
      const u8* s = src + src_idx[j] * TILE_BYTES;
      u8*       d2 = dst + (dst_off + j) * TILE_BYTES;
      spr_dma_copy32(d2, s, TILE_BYTES / 4);
    }
  }
    spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}

/* OAM 設定ヘルパー */
static inline void oam_set_face_16x32_(int oam, int x, int y, int tile_base, int pal_bank){
  volatile u16* oo = OAM_ATTR(oam);
  oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
  oo[1] = ATTR1_X(x) | ATTR1_SIZE(2);
  oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

static inline void oam_set_back_8x16_(int oam, int x, int y, int tile_base, int pal_bank){
  volatile u16* oo = OAM_ATTR(oam);
  oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
  oo[1] = ATTR1_X(x) | ATTR1_SIZE(0);
  oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* 16x16（正方形）を1枚出す。バナーは 16x16×3 で合成表示する */
static inline void oam_set_square_16x16_(int oam, int x, int y, int tile_base, int pal_bank){
  volatile u16* oo = OAM_ATTR(oam);
  oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_SQ;
  oo[1] = ATTR1_X(x) | ATTR1_SIZE(1); /* 16x16 */
  oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* =============== 公開 API =============== */

void render_init_vram(const Hand* me,
                      int max_player_show,
                      int out_face_tile_base[12],
                      int* out_back_tile_base,
                      int* out_field_tile_base)
{
  enum { PAL_FACE = 0, PAL_BACK = 0, PAL_BANNER = 0 };
  int tb = 0;

  force_obj_1d();
  spr_init_mode0_obj1d();

  /* 自分の表（u8 → 名前変換してロード） */
  int show = me->count; if (show > max_player_show) show = max_player_show;
  for (int i=0; i<show; ++i){
    const char* name = card_to_string(me->cards[i]);
    upload_face_16x32_once_(name, tb, PAL_FACE);
    out_face_tile_base[i] = tb;
    tb += 8; /* 16x32 は 8タイル */
  }

  /* 自分の手札領域は常に max 分確保して次の領域へ進める */
  tb = 8 * max_player_show;

  /* CPU用 裏面（共通） */
  const char* back = kBackName;
  upload_back_8x16_once_(back, tb, PAL_BACK);
  if (out_back_tile_base) *out_back_tile_base = tb;
  tb += 2; /* 8x16 は 2タイル */

  /* 場スロット（最大4枚）の先頭ベースと個別ベース */
  s_field_slot0_base = tb;
  for (int i=0;i<4;++i){ s_field_tile_bases[i] = s_field_slot0_base + 8 * i; }
  s_field_count = 0;
  tb += 8 * 4;

  /* 役バナー用のVRAM（12タイル確保：48x16） */
  s_banner_tile_base = tb;
  tb += 12;

  /* 初期状態：非表示 */
  s_banner_visible = 0;
  s_banner_loaded[0] = '\0';
  s_banner_anchor_player = -1;

  if (out_field_tile_base) *out_field_tile_base = s_field_slot0_base;
}

/* 場カード「1枚だけ」転送（旧来の base 指定パス） */
void render_upload_field_card(const char* name, int field_tile_base){
  enum { PAL_FACE = 0 };
  if (!(field_tile_base >= 0 && name)) return;
  upload_face_16x32_once_(name, field_tile_base, PAL_FACE);
}

/* 場のカード一括設定（名前配列→VRAM転送） */
void render_set_field_cards(const char* const names[4], int count){
  enum { PAL_FACE = 0 };
  s_field_count = 0;
  if (!names || count <= 0) return;
  if (count > 4) count = 4;

  for (int i=0;i<count;i++){
    const char* nm = names[i];
    if (!nm) continue;
    upload_face_16x32_once_(nm, s_field_tile_bases[i], PAL_FACE);
    s_field_count++;
  }
}

/* --- 互換API（旧演出は無効） --- */
void render_set_yagiri_visible(int on){ (void)on; /* no-op */ }
void render_trigger_sibari(int frames){ (void)frames; /* no-op */ }

/* --- 汎用API（役演出キューは保持のみ） --- */
void render_effect_enqueue(int effect /*FxEffect*/, int frames){
  if (!(effect >=0 && effect < FXE_COUNT)) return;
  s_fx_time[effect] = frames;
}

/* ===== 新規：役スプライト API ===== */
void render_show_role_sprite(const char* name){
  if (!name || s_banner_tile_base < 0) return;

  /* すでに同じ名前が載っていれば再転送しない */
  int same = 0;
  if (s_banner_loaded[0] != '\0'){
    const char* a = s_banner_loaded; const char* b = name;
    same = 1;
    while (*a || *b){ if (*a != *b){ same = 0; break; } ++a; ++b; }
  }

  if (!same){
    /* 48x16 の画像を 12タイル分 VRAM に転送 */
    upload_banner_48x16_once_(name, s_banner_tile_base, /*PAL_BANNER=*/0);
    /* 名前をキャッシュ */
    int i=0; for (; i<15 && name[i]; ++i) s_banner_loaded[i] = name[i];
    s_banner_loaded[i] = '\0';
  }
  s_banner_visible = 1;
}

void render_hide_role_sprite(void){
  s_banner_visible = 0;
}

/* ★追加：PASS を出したプレイヤの位置にバナーを出すための API */
void render_set_banner_player(int player){
  if (player < -1 || player > 3) player = -1;
  s_banner_anchor_player = player;
}

/* 1フレーム描画（カード＋役バナー） */
void render_frame(const int g_visible[PLAYERS],
                  const int player_face_tile_base[12],
                  int back_tile_base,
                  int field_visible,
                  int field_count)
{
  spr_init_mode0_obj1d();
  force_obj_1d();
  spr_dma_copy32((void*)&OBJ_PAL16[0], obj_atlasPal, 8);

  int oam = 0;

  /* CPU裏（共通） */
  const int back_w=8, back_h=16, back_gap=1;
  const int row_max = 7;
  const int cpu_start_y = 15;
  const int row_spacing = back_h + 2;

  { int start_x=8, base_y=cpu_start_y, show=g_visible[1];
    for(int i=0;i<show;i++){ int row=i/row_max, col=i%row_max;
      oam_set_back_8x16_(oam++, start_x+col*(back_w+back_gap), base_y+row*row_spacing, back_tile_base, 0); } }
  { int start_x=90, base_y=cpu_start_y, show=g_visible[2];
    for(int i=0;i<show;i++){ int row=i/row_max, col=i%row_max;
      oam_set_back_8x16_(oam++, start_x+col*(back_w+back_gap), base_y+row*row_spacing, back_tile_base, 0); } }
  { int start_x=170, base_y=cpu_start_y, show=g_visible[3];
    for(int i=0;i<show;i++){ int row=i/row_max, col=i%row_max;
      oam_set_back_8x16_(oam++, start_x+col*(back_w+back_gap), base_y+row*row_spacing, back_tile_base, 0); } }

  /* 自分（表：最大12枚） */
  { const int face_w=16, face_h=32, face_gap=1;
    const int start_x=8, y=160-face_h-4;
    int show=g_visible[0]; if (show>12) show=12;
    for(int i=0;i<show;i++){
      oam_set_face_16x32_(oam++, start_x+i*(face_w+face_gap), y, player_face_tile_base[i], 0);
    } }

  /* 場（表：最大4枚） */
  if (field_visible){
    int count = field_count; if (count>4) count=4; if (count<0) count=0;
    const int face_w=16, face_h=32, gap=2;
    const int fy = (160/2) - (face_h/2)+20;
    const int total_w = count*face_w + (count? (count-1)*gap : 0);
    const int start = (240/2) - (total_w/2);
    for (int i=0;i<count;i++){
      int base = s_field_tile_bases[i];
      if (base<0) continue;
      int dx = face_w + gap;
      oam_set_face_16x32_(oam++, start + i*dx, fy, base, 0);
    }
  }

  /* 役バナー：表示要求があれば 16x16×3 を指定位置に合成表示 */
  if (s_banner_visible && s_banner_tile_base >= 0){
    const int bw = 16;
    const int total_w = 3 * bw;

    /* 既定：中央上（従来） */
    int x = (240 - total_w) / 2;
    int y = 60;

    /* ★プレイヤ別に位置を切替 */
    switch (s_banner_anchor_player){
      case 0: /* 自分（下） */
        x = 110;
        y = 110;  /* 自分の手札の少し上 */
        break;
      case 1: /* 左上CPU */
        x = 30;
        y = 50;
        break;
      case 2: /* 上中央CPU */
        x = 110;
        y = 50;
        break;
      case 3: /* 右上CPU */
        x = 190;
        y = 50;
        break;
      default: /* -1: 従来の中央上 */
        break;
    }

    for (int i=0;i<3;i++){
      int tb = s_banner_tile_base + i * 4; /* 16x16 は 4タイル */
      oam_set_square_16x16_(oam++, x + i*bw, y, tb, 0);
    }
  }

  /* 役バナーの寿命カウンタ（将来復帰用） */
  for (int i=0;i<FXE_COUNT;i++){
    if (s_fx_time[i] > 0) s_fx_time[i]--;
  }

  /* 余りOAMを画面外へ */
  for (int i=oam; i<128; i++){
    volatile u16* oo = OAM_ATTR(i);
    oo[0] = ATTR0_Y(160); oo[1]=0; oo[2]=0;
  }
}

/* まとめて場のカードロード（互換） */
void render_upload_field_cards(const char** names, int count){
  render_set_field_cards((const char* const*)names, count);
}

/* 自分の表カード再転送（交換/取得時） */
void render_reload_hand_card(const Hand* me,
                             int player_face_tile_base[12],
                             int start_tile_base)
{
  enum { PAL_FACE = 0 };
  int tb = start_tile_base;
  int show = (me->count > 12) ? 12 : me->count;
  for (int i = 0; i < show; ++i) {
    const char* name = card_to_string(me->cards[i]);
    upload_face_16x32_once_(name, tb, PAL_FACE);
    player_face_tile_base[i] = tb;
    tb += 8;
  }
}
