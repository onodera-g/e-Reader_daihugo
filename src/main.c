// src/main.c
#include "def.h"
#include "erapi.h"
#include "bg.h"
#include "obj_atlas.h"
#include "sprite_bare.h"

extern int __end[];

/* ────────── ユーティリティ ────────── */
static inline void force_obj_1d(void){ REG_DISPCNT |= (OBJ_ENABLE | OBJ_1D_MAP); }
static inline void wait_vblank_start(void){
  while (REG_VCOUNT >= 160);
  while (REG_VCOUNT < 160);
}

/* 配り演出パラメータ */
#define DEAL_DELAY_FRAMES  4      // 何フレームおきに1枚出すか
#define SFX_DEAL_ID        24     // ★CPU1の配布時に鳴らすSE ID

/* ────────── 背景 ────────── */
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

/* ────────── カード名（表16x32）・裏8x16 ────────── */
static const char* kHearts[]   = {"H_1","H_2","H_3","H_4","H_5","H_6","H_7","H_8","H_9","H_10","H_11","H_12","H_13"};
static const char* kDiamonds[] = {"D_1","D_2","D_3","D_4","D_5","D_6","D_7","D_8","D_9","D_10","D_11","D_12","D_13"};
static const char* kSpades[]   = {"S_1","S_2","S_3","S_4","S_5","S_6","S_7","S_8","S_9","S_10","S_11","S_12","S_13"};
static const char* kClubs[]    = {"C_1","C_2","C_3","C_4","C_5","C_6","C_7","C_8","C_9","C_10","C_11","C_12","C_13"};
static const char* kJokerName  = "J";     // Joker (name="J")
static const char* kBackName   = "card";  // 裏面 8x16

/* ────────── VRAM 転送（表16x32=8タイル / 裏8x16=2タイル） ────────── */
static void upload_face_16x32_once(const char* name, int tile_base, int pal_bank){
  int idx = objAtlasFindIndex(name); if (idx < 0) return;
  const ObjSpriteDesc* d = &objAtlasSprites[idx];
  if (!(d->w == 16 && d->h == 32)) return;
  const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
  u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
  spr_dma_copy32(dst, src, (8 * 32) / 4);
  spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}
static void upload_back_8x16_once(const char* name, int tile_base, int pal_bank){
  int idx = objAtlasFindIndex(name); if (idx < 0) return;
  const ObjSpriteDesc* d = &objAtlasSprites[idx];
  if (!(d->w == 8 && d->h == 16)) return;
  const u8* src = ((const u8*)obj_atlasTiles) + d->offset_words * 4;
  u8*       dst = (u8*)OBJ_VRAM8 + tile_base * 32;
  spr_dma_copy32(dst, src, (2 * 32) / 4);
  spr_dma_copy32((void*)&OBJ_PAL16[pal_bank*16], obj_atlasPal, 8);
}

/* ────────── OAM 直書き（表16x32 / 裏8x16） ────────── */
static inline void oam_set_face_16x32(int oam, int x, int y, int tile_base, int pal_bank){
  volatile u16* oo = OAM_ATTR(oam);
  oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
  oo[1] = ATTR1_X(x) | ATTR1_SIZE(2); // 16x32
  oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}
static inline void oam_set_back_8x16(int oam, int x, int y, int tile_base, int pal_bank){
  volatile u16* oo = OAM_ATTR(oam);
  oo[0] = ATTR0_Y(y) | ATTR0_MODE_REG | ATTR0_4BPP | ATTR0_SHAPE_TALL;
  oo[1] = ATTR1_X(x) | ATTR1_SIZE(0); // 8x16
  oo[2] = ATTR2_TILE(tile_base) | ATTR2_PBANK(pal_bank);
}

/* ────────── RNG / シャッフル ────────── */
static u32 g_rng = 2463534242u;
static inline void rng_seed(u32 s){ g_rng = (s ? s : 2463534242u); }
static inline u32 rng_next(void){ u32 x=g_rng; x^=x<<13; x^=x>>17; x^=x<<5; return g_rng=x; }
static void shuffle(const char** a, int n){
  for (int i=n-1;i>0;--i){ int j=(int)(rng_next()%(u32)(i+1)); const char*t=a[i]; a[i]=a[j]; a[j]=t; }
}

/* ────────── デッキ/ハンド ────────── */
#define PLAYERS 4
#define MAX_HAND 14
#define MAX_DECK 53
typedef struct { const char* cards[MAX_HAND]; int count; } Hand;

static int build_deck(const char** out){
  int n=0;
  for(int i=0;i<13;i++) out[n++]=kHearts[i];
  for(int i=0;i<13;i++) out[n++]=kDiamonds[i];
  for(int i=0;i<13;i++) out[n++]=kSpades[i];
  for(int i=0;i<13;i++) out[n++]=kClubs[i];
  if (objAtlasFindIndex(kJokerName) >= 0 && n < MAX_DECK) out[n++] = kJokerName;
  return n; // 52 or 53
}

// start_player=0（53枚時：自分14 / CPU1=13 / CPU2=13 / CPU3=13）
static void deal_round_robin_offset(const char** deck, int deck_n, int start_player, Hand hands[PLAYERS]){
  for(int p=0;p<PLAYERS;p++) hands[p].count=0;
  for(int i=0;i<deck_n;i++){
    int p = (start_player + i) & 3;
    if (hands[p].count < MAX_HAND) hands[p].cards[hands[p].count++] = deck[i];
  }
}

/* ────────── 表示用タイル割当 ────────── */
enum { PAL_FACE = 0, PAL_BACK = 0 };
static int g_player_face_tile_base[12]; // 自分の最大12枚
static int g_back_tile_base = 0;        // 裏面“card”共通

/* ────────── 配り演出カウンタ ────────── */
static int g_visible[PLAYERS] = {0,0,0,0}; // 現在可視枚数
static int g_target [PLAYERS] = {0,0,0,0}; // 目標可視枚数
static int g_deal_turn = 0;                // 次に配るプレイヤ（0..3）
static int g_deal_delay = DEAL_DELAY_FRAMES;

/* ────────── main ────────── */
int main(void){
  init_screen_and_ui();
  spr_init_mode0_obj1d();
  force_obj_1d();
  ERAPI_FadeIn(1);

  /* デッキ → シャッフル → 配る */
  const char* deck[MAX_DECK];
  int deck_n = build_deck(deck);
  rng_seed((u32)REG_VCOUNT ^ (u32)ERAPI_GetKeyStateRaw() ^ 0x9E3779B9u);
  shuffle(deck, deck_n);

  Hand hands[PLAYERS];
  deal_round_robin_offset(deck, deck_n, /*start=*/0, hands);
  const Hand* me = &hands[0];

  /* 起動時：自分の表 最大12枚 + 裏面1セット を VRAM へロード（VBlank中） */
  int tb = 0;
  wait_vblank_start();
  {
    int show = me->count; if (show > 12) show = 12;
    for(int i=0;i<show;i++){
      upload_face_16x32_once(me->cards[i], tb, PAL_FACE);
      g_player_face_tile_base[i] = tb;
      tb += 8; // 16x32 → 8タイル
    }
    g_back_tile_base = tb;
    upload_back_8x16_once(kBackName, g_back_tile_base, PAL_BACK);
    tb += 2; // 8x16 → 2タイル

    ERAPI_RenderFrame(1);
  }

  /* 目標可視枚数 */
  g_target[0] = (me->count > 12) ? 12 : me->count; // 自分は最大12
  g_target[1] = hands[1].count;                    // CPU1
  g_target[2] = hands[2].count;                    // CPU2
  g_target[3] = hands[3].count;                    // CPU3

  /* メイン：OAM直書き + OBJパレット再転送 + 配り演出
     ★ SE は “CPU1の枚数が増えたフレーム” にだけ鳴らす */
  u32 prev = 0;
  for(;;){
    /* 入力 */
    u32 key  = ERAPI_GetKeyStateRaw();
    u32 edge = key & ~prev; prev = key;
    if (edge & ERAPI_KEY_B) break;

    /* このフレームでCPU1が増えたかフラグ */
    int play_sfx_cpu1 = 0;

    /* 配り：一定フレームごとに“必ず1枚だけ”可視化 */
    if (g_deal_delay > 0){
      g_deal_delay--;
    } else {
      for (int t=0; t<PLAYERS; ++t){
        int p = (g_deal_turn + t) & 3;
        if (g_visible[p] < g_target[p]){
          g_visible[p]++;            // このフレームで見せる
          if (p == 1) play_sfx_cpu1 = 1;   // ★CPU1に配ったときだけSEトリガ
          g_deal_turn = (p + 1) & 3; // 次の人へ
          break;                     // 必ず1人でbreak
        }
      }
      g_deal_delay = DEAL_DELAY_FRAMES;
    }

    /* フレーム確定（ERAPIがOAM初期化・サウンド更新など） */
    ERAPI_RenderFrame(1);

    /* OBJ再設定 & パレット書き戻し */
    spr_init_mode0_obj1d();
    force_obj_1d();
    spr_dma_copy32((void*)&OBJ_PAL16[0], obj_atlasPal, 8);

    /* 描画（OAM直書き） */
    int oam = 0;

    // CPU 裏面：7枚ごと改行・開始座標を下げる
    const int back_w=8, back_h=16, back_gap=1;
    const int row_max = 7;
    const int cpu_start_y = 24;
    const int row_spacing = back_h + 2;

    { // CPU1（左列）
      int start_x=8, base_y=cpu_start_y;
      int show = g_visible[1];
      for(int i=0;i<show;i++){
        int row = i / row_max, col = i % row_max;
        int x = start_x + col * (back_w + back_gap);
        int y = base_y + row * row_spacing;
        oam_set_back_8x16(oam++, x, y, g_back_tile_base, PAL_BACK);
      }
    }
    { // CPU2（中央列）
      int start_x=96, base_y=cpu_start_y;
      int show = g_visible[2];
      for(int i=0;i<show;i++){
        int row = i / row_max, col = i % row_max;
        int x = start_x + col * (back_w + back_gap);
        int y = base_y + row * row_spacing;
        oam_set_back_8x16(oam++, x, y, g_back_tile_base, PAL_BACK);
      }
    }
    { // CPU3（右列）
      int start_x=184, base_y=cpu_start_y;
      int show = g_visible[3];
      for(int i=0;i<show;i++){
        int row = i / row_max, col = i % row_max;
        int x = start_x + col * (back_w + back_gap);
        int y = base_y + row * row_spacing;
        oam_set_back_8x16(oam++, x, y, g_back_tile_base, PAL_BACK);
      }
    }

    // 自分：左下 表 16x32（最大12）
    {
      const int face_w=16, face_h=32, face_gap=1;
      const int start_x=8;
      const int y = 160 - face_h - 4;
      int show = g_visible[0];
      for (int i=0;i<show;i++){
        int x = start_x + i * (face_w + face_gap);
        oam_set_face_16x32(oam++, x, y, g_player_face_tile_base[i], PAL_FACE);
      }
    }

    // 余りOAMは画面外へ
    for (int i=oam; i<128; i++){
      volatile u16* oo = OAM_ATTR(i);
      oo[0] = ATTR0_Y(160); oo[1]=0; oo[2]=0;
    }

    /* ★ SEは “CPU1に配ったフレーム” にだけ 1回 鳴らす */
    if (play_sfx_cpu1){
      ERAPI_PlaySoundSystem((u32)SFX_DEAL_ID);
    }
  }

  return ERAPI_EXIT_TO_MENU;
}
