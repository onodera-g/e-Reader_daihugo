#include "render.h"
#include "obj_atlas.h"   // アトラス参照
#include "sprite_bare.h" // DMA/OAMユーティリティ
#include "erapi.h"       // 毎フレームOAM初期化のため

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

/* OAM: 16x32（表）1枚 */
static inline void oam_set_face_16x32_(int oam, int x, int y, int tile_base, int pal_bank){
    volatile u16* oo = OAM_ATTR(oam);
    oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
    oo[1] = ATTR1_X(x) | ATTR1_SIZE(2);
    oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* OAM: 8x16（裏）1枚 */
static inline void oam_set_back_8x16_(int oam, int x, int y, int tile_base, int pal_bank){
    volatile u16* oo = OAM_ATTR(oam);
    oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
    oo[1] = ATTR1_X(x) | ATTR1_SIZE(0);
    oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
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

    // 自分の表を最大 max_player_show 枚展開（16x32=8タイル/枚）
    int show = me->count; if (show > max_player_show) show = max_player_show;
    for (int i=0; i<show; ++i){
        upload_face_16x32_once_(me->cards[i], tb, PAL_FACE);
        out_face_tile_base[i] = tb;
        tb += 8;
    }

    // 裏面を1セット展開（CPU共通 8x16=2タイル）
    *out_back_tile_base = tb;
    upload_back_8x16_once_(kBackName, *out_back_tile_base, PAL_BACK);
    tb += 2;

    // 場カード用のスロット確保（16x32=8タイル、絵はまだ入れない）
    *out_field_tile_base = tb;
    tb += 8;
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
    // ERAPI_RenderFrame(1) 直後に必ず呼ばれる想定。
    // ERAPIがOAMを初期化するため、毎フレームOAMを並べ直す。
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
    if (field_visible && field_tile_base >= 0){
        const int x = 112;
        const int y = 75;
        oam_set_face_16x32_(oam++, x, y, field_tile_base, 0);
    }

    // 余りOAMは画面外へ
    for (int i=oam; i<128; i++){
        volatile u16* oo = OAM_ATTR(i);
        oo[0] = ATTR0_Y(160); oo[1]=0; oo[2]=0;
    }
}

void render_reload_hand_card(const Hand* me,
                             int player_face_tile_base[12],
                             int start_tile_base)
{
    // 自分の表カードを左から詰め直してVRAMへ再転送する
    // （CPUは裏共通なので不要。自分の手札が減ったときだけ呼べばOK）
    enum { PAL_FACE = 0 };
    int tb = start_tile_base;
    int show = (me->count > 12) ? 12 : me->count;
    for (int i=0; i<show; ++i){
        upload_face_16x32_once_(me->cards[i], tb, PAL_FACE);
        player_face_tile_base[i] = tb;
        tb += 8;
    }
}
