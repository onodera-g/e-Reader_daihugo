// src/render.c
#include "render.h"
#include "obj_atlas.h"
#include "sprite_bare.h"
#include "erapi.h"

/* ================= 内部状態 ================= */

/* ★ バナー共用スロット（“yagiri”/“sibari”をその都度ロード）
   仕様：見た目は常にどちらか一方のみ。優先順位は「sibari ＞ yagiri」。 */
typedef enum { BANNER_NONE=0, BANNER_YAGIRI, BANNER_SIBARI } BannerKind;
static int  s_banner_tile_base = -1;   /* 48x16(=12タイル) */
static u8   s_banner_wants_yagiri = 0; /* 1=8切り表示要求（待機中） */
static int  s_banner_sibari_left  = 0; /* >0: しばり一時表示（毎フレーム--） */
static BannerKind s_banner_loaded = BANNER_NONE; /* VRAMに今入ってる種類 */

/* 場スロット（表16x32=8タイル×4） */
static int s_field_slot0_base = -1;
static int s_field_tile_bases[4] = {-1,-1,-1,-1};
static int s_field_count = 0;

/* ================= ユーティリティ ================= */

static inline void force_obj_1d(void){ REG_DISPCNT |= (OBJ_ENABLE | OBJ_1D_MAP); }

static void upload_face_16x32_once_(const char* name, int tile_base, int pal_bank){
    int idx = objAtlasFindIndex(name); if (idx < 0) return;
    const ObjSpriteDesc* d = &objAtlasSprites[idx];
    if (!(d->w == 16 && d->h == 32)) return;
    const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
    u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
    spr_dma_copy32(dst, src, (8 * 32) / 4);
    spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}

static void upload_back_8x16_once_(const char* name, int tile_base, int pal_bank){
    int idx = objAtlasFindIndex(name); if (idx < 0) return;
    const ObjSpriteDesc* d = &objAtlasSprites[idx];
    if (!(d->w == 8 && d->h == 16)) return;
    const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
    u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
    spr_dma_copy32(dst, src, (2 * 32) / 4);
    spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}

static void upload_banner_48x16_once_(const char* name, int tile_base, int pal_bank){
    int idx = objAtlasFindIndex(name); if (idx < 0) return;
    const ObjSpriteDesc* d = &objAtlasSprites[idx];
    if (!(d->w == 48 && d->h == 16)) return;

    const u8* base_src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
    const int tiles_w = 6;

    for (int blk = 0; blk < 3; ++blk){
        int src_tx = blk * 2;
        for (int ty = 0; ty < 2; ++ty){
            for (int tx = 0; tx < 2; ++tx){
                int src_index = ty * tiles_w + (src_tx + tx);
                const u8* src = base_src + src_index * 32;

                int dst_index = tile_base + blk*4 + ty*2 + tx;
                u8* dst = (u8*)OBJ_VRAM8 + dst_index * 32;

                spr_dma_copy32(dst, src, 32/4);
            }
        }
    }
    spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}

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

static inline void oam_set_square_16x16_(int oam, int x, int y, int tile_base, int pal_bank){
    volatile u16* oo = OAM_ATTR(oam);
    oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_SQ;
    oo[1] = ATTR1_X(x) | ATTR1_SIZE(1);
    oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

static void render_banner_48x16_(int* oam, int x, int y, int tile_base, int pal){
    oam_set_square_16x16_((*oam)++, x +  0, y, tile_base + 0, pal);
    oam_set_square_16x16_((*oam)++, x + 16, y, tile_base + 4, pal);
    oam_set_square_16x16_((*oam)++, x + 32, y, tile_base + 8, pal);
}

/* 共用バナーVRAMへ必要な方をロード（必要時のみ上書き） */
static void banner_ensure_loaded_(BannerKind need){
    if (s_banner_loaded == need) return;
    if (need == BANNER_YAGIRI) {
        upload_banner_48x16_once_("yagiri", s_banner_tile_base, 0);
    } else if (need == BANNER_SIBARI) {
        upload_banner_48x16_once_("sibari", s_banner_tile_base, 0);
    }
    s_banner_loaded = need;
}

/* ================ 公開API ================ */

void render_init_vram(const Hand* me,
                      int max_player_show,
                      int out_face_tile_base[12],
                      int* out_back_tile_base,
                      int* out_field_tile_base)
{
    enum { PAL_FACE = 0, PAL_BACK = 0 };
    int tb = 0;

    /* 自分の表 */
    int show = me->count; if (show > max_player_show) show = max_player_show;
    for (int i=0; i<show; ++i){
        upload_face_16x32_once_(me->cards[i], tb, PAL_FACE);
        out_face_tile_base[i] = tb;
        tb += 8;
    }

    /* 裏（CPU共通） */
    *out_back_tile_base = tb;
    upload_back_8x16_once_(kBackName, *out_back_tile_base, PAL_BACK);
    tb += 2;

    /* 場スロット 4枚ぶん（各8タイル） */
    s_field_slot0_base = tb;
    for (int i=0;i<4;++i) s_field_tile_bases[i] = -1;
    s_field_count = 0;
    tb += 8 * 4;

    /* ★ 共用バナー 12タイル（初期はロードなし） */
    s_banner_tile_base = tb;
    s_banner_loaded = BANNER_NONE;
    tb += 12;

    if (out_field_tile_base) *out_field_tile_base = s_field_slot0_base;
}

void render_upload_field_card(const char* name, int field_tile_base){
    enum { PAL_FACE = 0 };
    if (field_tile_base >= 0 && name){
        upload_face_16x32_once_(name, field_tile_base, PAL_FACE);
    }
}

void render_set_field_cards(const char* const names[4], int count){
    if (count < 0) count = 0;
    if (count > 4) count = 4;
    s_field_count = count;
    for (int i=0; i<4; ++i) s_field_tile_bases[i] = -1;
    for (int i=0; i<count; ++i){
        if (names && names[i]) {
            int base = s_field_slot0_base + 8 * i;
            upload_face_16x32_once_(names[i], base, 0);
            s_field_tile_bases[i] = base;
        }
    }
}

/* 8切りの可視要求（待機中に1） */
void render_set_yagiri_visible(int on){
    s_banner_wants_yagiri = on ? 1 : 0;
}

/* しばりの一時表示要求（nフレーム） */
void render_trigger_sibari(int frames){
    if (frames < 0) frames = 0;
    s_banner_sibari_left = frames; /* “要求”だけ覚えておく（描画時に優先） */
}

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
    const int cpu_start_y = 24;
    const int row_spacing = back_h + 2;

    { int start_x=8, base_y=cpu_start_y, show=g_visible[1];
      for(int i=0;i<show;i++){ int row=i/row_max, col=i%row_max;
        oam_set_back_8x16_(oam++, start_x+col*(back_w+back_gap), base_y+row*row_spacing, back_tile_base, 0); } }
    { int start_x=96, base_y=cpu_start_y, show=g_visible[2];
      for(int i=0;i<show;i++){ int row=i/row_max, col=i%row_max;
        oam_set_back_8x16_(oam++, start_x+col*(back_w+back_gap), base_y+row*row_spacing, back_tile_base, 0); } }
    { int start_x=184, base_y=cpu_start_y, show=g_visible[3];
      for(int i=0;i<show;i++){ int row=i/row_max, col=i%row_max;
        oam_set_back_8x16_(oam++, start_x+col*(back_w+back_gap), base_y+row*row_spacing, back_tile_base, 0); } }

    /* 自分（表） */
    { const int face_w=16, face_h=32, face_gap=1;
      const int start_x=8, y=160-face_h-4;
      int show=g_visible[0]; if (show>12) show=12;
      for(int i=0;i<show;i++){
          oam_set_face_16x32_(oam++, start_x+i*(face_w+face_gap), y, player_face_tile_base[i], 0);
      } }

    /* 場（中央揃え：1〜4枚） */
    const int fx_center=112, fy=75, dx=18;
    if (field_visible && field_count>0){
        if (field_count > s_field_count) field_count = s_field_count;
        const int total=(field_count-1)*dx, start=fx_center-(total/2);
        for(int i=0;i<field_count;i++){
            int base = s_field_tile_bases[i];
            if (base<0) continue;
            oam_set_face_16x32_(oam++, start + i*dx, fy, base, 0);
        }
    }

    /* --- バナー優先判定：sibari が残っていれば sibari、なければ yagiri --- */
    BannerKind want = BANNER_NONE;
    if (field_visible){
        if (s_banner_sibari_left > 0) {
            want = BANNER_SIBARI;
            s_banner_sibari_left--; /* 毎フレーム1デクリメント */
        } else if (s_banner_wants_yagiri) {
            want = BANNER_YAGIRI;
        }
    }

    if (want != BANNER_NONE){
        banner_ensure_loaded_(want);
        const int gap=2, bx=fx_center-16, by=fy-(16+gap);
        render_banner_48x16_(&oam, bx, by, s_banner_tile_base, 0);
    }

    /* 余りOAMを画面外へ */
    for (int i=oam; i<128; i++){
        volatile u16* oo = OAM_ATTR(i);
        oo[0] = ATTR0_Y(160); oo[1]=0; oo[2]=0;
    }
}

/* 互換API */
void render_upload_field_cards(const char** names, int count){
    render_set_field_cards((const char* const*)names, count);
}

/* 自分の表カードを左から詰め直してVRAMへ再転送する */
void render_reload_hand_card(const Hand* me,
                             int player_face_tile_base[12],
                             int start_tile_base)
{
    enum { PAL_FACE = 0 };
    int tb = start_tile_base;
    int show = (me->count > 12) ? 12 : me->count;
    for (int i = 0; i < show; ++i) {
        upload_face_16x32_once_(me->cards[i], tb, PAL_FACE);
        player_face_tile_base[i] = tb;
        tb += 8;
    }
}
