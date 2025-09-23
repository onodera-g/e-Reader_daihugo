// src/render.c
#include "render.h"
#include "obj_atlas.h"   // アトラス参照
#include "sprite_bare.h" // spr_dma_copy32
#include "erapi.h"

/* --- 内部状態 --- */
static int s_yagiri_tile_base = -1;  /* 48x16(=12タイル)の先頭タイル */
static int s_yagiri_visible   = 0;   /* 1=バナー表示中 */

/* ── 内部ユーティリティ ── */
static inline void force_obj_1d(void){ REG_DISPCNT |= (OBJ_ENABLE | OBJ_1D_MAP); }

/* 16x32（表）をVRAMへ8タイル分コピー＋OBJパレット再転送 */
static void upload_face_16x32_once_(const char* name, int tile_base, int pal_bank){
    int idx = objAtlasFindIndex(name); if (idx < 0) return;
    const ObjSpriteDesc* d = &objAtlasSprites[idx];
    if (!(d->w == 16 && d->h == 32)) return;
    const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
    u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
    spr_dma_copy32(dst, src, (8 * 32) / 4);                          // 8 tiles
    spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8); // 32B
}

/* 8x16（裏）をVRAMへ2タイル分コピー＋OBJパレット再転送 */
static void upload_back_8x16_once_(const char* name, int tile_base, int pal_bank){
    int idx = objAtlasFindIndex(name); if (idx < 0) return;
    const ObjSpriteDesc* d = &objAtlasSprites[idx];
    if (!(d->w == 8 && d->h == 16)) return;
    const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
    u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
    spr_dma_copy32(dst, src, (2 * 32) / 4);                          // 2 tiles
    spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8); // 32B
}

/* 48x16（yagiri）→ 8×8タイルで 6×2=12 タイルを転送 */
/* 48x16（yagiri）を 16x16×3ブロックに並べ替えてVRAMへ格納
   VRAM配置: [0..3]=左(2x2), [4..7]=中(2x2), [8..11]=右(2x2) */
static void upload_banner_48x16_once_(const char* name, int tile_base, int pal_bank){
    int idx = objAtlasFindIndex(name); if (idx < 0) return;
    const ObjSpriteDesc* d = &objAtlasSprites[idx];
    if (!(d->w == 48 && d->h == 16)) return;

    const u8* base_src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
    // アトラス上のタイル配置は 6x2（タイル幅=6、タイル高=2）
    const int tiles_w = 6;

    // 3ブロック (左0/中1/右2)
    for (int blk = 0; blk < 3; ++blk){
        int src_tx = blk * 2;   // このブロックの左上タイルの x(タイル単位)
        // 2x2タイルを行優先でコピー（上2枚→下2枚）
        for (int ty = 0; ty < 2; ++ty){
            for (int tx = 0; tx < 2; ++tx){
                int src_index = ty * tiles_w + (src_tx + tx);       // 6x2の中のインデックス
                const u8* src = base_src + src_index * 32;          // 4bpp 1タイル=32B

                int dst_index = tile_base + blk*4 + ty*2 + tx;      // 2x2を連続格納
                u8* dst = (u8*)OBJ_VRAM8 + dst_index * 32;

                spr_dma_copy32(dst, src, 32/4);
            }
        }
    }
    // パレット
    spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}


/* OAM: 16x32（表）1枚 */
static inline void oam_set_face_16x32_(int oam, int x, int y, int tile_base, int pal_bank){
    volatile u16* oo = OAM_ATTR(oam);
    oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
    oo[1] = ATTR1_X(x) | ATTR1_SIZE(2);       // 16x32
    oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* OAM: 8x16（裏）1枚 */
static inline void oam_set_back_8x16_(int oam, int x, int y, int tile_base, int pal_bank){
    volatile u16* oo = OAM_ATTR(oam);
    oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
    oo[1] = ATTR1_X(x) | ATTR1_SIZE(0);       // 8x16
    oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* OAM: 16x16（バナー分割で使用） */
static inline void oam_set_square_16x16_(int oam, int x, int y, int tile_base, int pal_bank){
    volatile u16* oo = OAM_ATTR(oam);
    oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_SQ;
    oo[1] = ATTR1_X(x) | ATTR1_SIZE(1);       // 16x16
    oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* 48x16 バナーを 16x16×3枚で合成表示（左/中/右） */
/* 48x16 バナーを 16x16×3枚で合成表示（左/中/右）
   ※ VRAMは [0..3]=左, [4..7]=中, [8..11]=右 になっている前提 */
static void render_banner_yagiri_(int* oam, int x, int y, int tile_base, int pal){
    oam_set_square_16x16_((*oam)++, x +  0, y, tile_base + 0, pal); // 左
    oam_set_square_16x16_((*oam)++, x + 16, y, tile_base + 4, pal); // 中
    oam_set_square_16x16_((*oam)++, x + 32, y, tile_base + 8, pal); // 右
}



/* ── 公開API ── */
void render_init_vram(const Hand* me,
                      int max_player_show,
                      int out_face_tile_base[12],
                      int* out_back_tile_base,
                      int* out_field_tile_base)
{
    enum { PAL_FACE = 0, PAL_BACK = 0 };
    int tb = 0;

    // 自分の表（最大 max_player_show 枚）
    int show = me->count; if (show > max_player_show) show = max_player_show;
    for (int i=0; i<show; ++i){
        upload_face_16x32_once_(me->cards[i], tb, PAL_FACE);
        out_face_tile_base[i] = tb;
        tb += 8;
    }

    // 裏面（CPU共通）
    *out_back_tile_base = tb;
    upload_back_8x16_once_(kBackName, *out_back_tile_base, PAL_BACK);
    tb += 2;

    // 場カード（16x32 = 8タイル）
    *out_field_tile_base = tb;
    tb += 8;

    // ★ 48x16 "yagiri" バナー（12タイル確保）
    s_yagiri_tile_base = tb;
    upload_banner_48x16_once_("yagiri", s_yagiri_tile_base, PAL_FACE);
    tb += 12; // ← 6ではなく12
}

void render_upload_field_card(const char* name, int field_tile_base){
    enum { PAL_FACE = 0 };
    if (field_tile_base >= 0 && name){
        upload_face_16x32_once_(name, field_tile_base, PAL_FACE);
    }
}

void render_frame(const int g_visible[PLAYERS],
                  const int player_face_tile_base[12],
                  int back_tile_base,
                  int field_tile_base,
                  int field_visible)
{
    // ERAPI_RenderFrame(1) 直後に必ず呼ばれる想定（OAM初期化後に再配置）
    spr_init_mode0_obj1d();
    force_obj_1d();

    // OBJパレットは毎フレーム書き戻す（ERAPIが触るため）
    spr_dma_copy32((void*)&OBJ_PAL16[0], obj_atlasPal, 8);

    int oam = 0;

    // CPU裏面：7枚ごと折り返し
    const int back_w=8, back_h=16, back_gap=1;
    const int row_max = 7;
    const int cpu_start_y = 24;
    const int row_spacing = back_h + 2;

    { // CPU1（左）
        int start_x=8, base_y=cpu_start_y;
        int show = g_visible[1];
        for(int i=0;i<show;i++){
            int row = i / row_max, col = i % row_max;
            int x = start_x + col * (back_w + back_gap);
            int y = base_y + row * row_spacing;
            oam_set_back_8x16_(oam++, x, y, back_tile_base, 0);
        }
    }
    { // CPU2（中央）
        int start_x=96, base_y=cpu_start_y;
        int show = g_visible[2];
        for(int i=0;i<show;i++){
            int row = i / row_max, col = i % row_max;
            int x = start_x + col * (back_w + back_gap);
            int y = base_y + row * row_spacing;
            oam_set_back_8x16_(oam++, x, y, back_tile_base, 0);
        }
    }
    { // CPU3（右）
        int start_x=184, base_y=cpu_start_y;
        int show = g_visible[3];
        for(int i=0;i<show;i++){
            int row = i / row_max, col = i % row_max;
            int x = start_x + col * (back_w + back_gap);
            int y = base_y + row * row_spacing;
            oam_set_back_8x16_(oam++, x, y, back_tile_base, 0);
        }
    }

    // 自分（表 16x32） 左下で横一列（最大12表示）
    {
        const int face_w=16, face_h=32, face_gap=1;
        const int start_x=8;
        const int y = 160 - face_h - 4;
        int show = g_visible[0]; if (show > 12) show = 12;
        for (int i=0;i<show;i++){
            int x = start_x + i * (face_w + face_gap);
            oam_set_face_16x32_(oam++, x, y, player_face_tile_base[i], 0);
        }
    }

    // 場（中央に1枚 表16x32）
    int fx = 112, fy = 75;
    if (field_visible && field_tile_base >= 0){
        oam_set_face_16x32_(oam++, fx, fy, field_tile_base, 0);
    }

    // ★ 8切りバナー（48x16を 16x16×3 で合成）…カードの“上”に非重なりで表示
    if (s_yagiri_visible && s_yagiri_tile_base >= 0 && field_visible){
        const int gap = 2;                 // カードとの余白
        const int bx  = fx - 16;           // カード中央に48pxを合わせる
        const int by  = fy - (16 + gap);   // カードの上

        render_banner_yagiri_(&oam, bx, by, s_yagiri_tile_base, 0);
    }

    // 余りOAMは画面外へ
    for (int i=oam; i<128; i++){
        volatile u16* oo = OAM_ATTR(i);
        oo[0] = ATTR0_Y(160); oo[1]=0; oo[2]=0;
    }
}

/* 自分の表カードを左から詰め直してVRAMへ再転送する */
void render_reload_hand_card(const Hand* me,
                             int player_face_tile_base[12],
                             int start_tile_base)
{
    enum { PAL_FACE = 0 };
    int tb = start_tile_base;
    int show = (me->count > 12) ? 12 : me->count;
    for (int i=0; i<show; ++i){
        upload_face_16x32_once_(me->cards[i], tb, PAL_FACE);
        player_face_tile_base[i] = tb;
        tb += 8;
    }
}

/* 8切りバナーの表示/非表示（毎フレーム呼んでOK） */
void render_set_yagiri_visible(int on){
    s_yagiri_visible = on ? 1 : 0;
}
