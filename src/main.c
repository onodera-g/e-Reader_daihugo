// src/main.c
//==============================
// 大富豪
//==============================
#include "def.h"
#include "erapi.h"
#include "messages_autogen.h"
#include "textutil.h"
#include "bg.h"

extern int __end[]; // リンカが与えるROM終端アドレス。空きRAM計算に利用

//==============================
//  背景の描写
//==============================

// BG0=BG3 に 背景画像をロード
static void init_screen_and_ui(void){
  // メモリ初期化（残り容量を ERAPI に通知）
  int kb = ((ERAPI_RAM_END - (u32)__end) >> 10) - 32;
  if (kb < 0) kb = 0;
  //メモリの確保z
  ERAPI_InitMemory(kb);
  // 背景モード設定
  ERAPI_SetBackgroundMode(0);
  // 背景データの読み出し
  ERAPI_BACKGROUND bgdef; //構造体の名称
  bgdef.data_gfx = (u8*)bgTiles;// タイルのデータ(画像)
  bgdef.data_pal = (u8*)bgPal;  // パレット
  bgdef.data_map = (u8*)bgMap;  // マップ
  bgdef.tiles    = (u16)(bgTilesLen / 32);  // タイルの総数
  bgdef.palettes = (u16)(bgPalLen   / 32);  // 読み込むパレット
  // 背景の表示
  ERAPI_LoadBackgroundCustom(0, &bgdef); // BG0にセット
  ERAPI_LayerShow(0); // BG0で書き出し
}

//==============================
//  BGM
//==============================

// 音量/速度の既定値をセット
static inline void set_default_audio_params(void){
  ERAPI_SetSoundVolume(0, 255);
  ERAPI_SetSoundVolume(1, 255);
  ERAPI_SetSoundSpeed(0, 128);
  ERAPI_SetSoundSpeed(1, 128);
}
// すべての音を止める
static inline void stop_all_sound(void){
  ERAPI_SoundPause(0);
  ERAPI_SoundPause(1);
}

// 指定IDのサウンドを再生
static inline void play_music_id(int id){
  set_default_audio_params();
  ERAPI_SoundPause(0);
  ERAPI_SoundPause(1);
  ERAPI_PlaySoundSystem((u32)id);
}


//==============================
//  main
//==============================

// 初期化 → 毎フレームUI更新 → 入力処理 → ループ
int main(void){
 // 初期フレーム
 init_screen_and_ui(); //画面、UIの初期化
 set_default_audio_params(); // 音量/速度を既定化
 play_music_id(22);// BGM再生
 ERAPI_FadeIn(1); // フェードイン演出

  u32 prev = 0; // 前回のキー入力(1フレーム目は前回がないので未入力で初期化)
  // 2フレーム以降
  for(;;){
    // キー入力の取得
    u32 key  = ERAPI_GetKeyStateRaw();  // 現在のキー入力を取得(ビットマスク)
    u32 not_prev = ~prev; // 前回の状態を反転する(前回押されていないキーを取得)
    u32 edge = key & not_prev; // 「key：今回押されている」かつ「not prev：前回押されていない」新たに押されたキーを抽出
    prev = key; // 前回のキー入力の更新
    // キー入力から分岐
    if (edge & ERAPI_KEY_B) break; // Bでゲーム終了
    if (edge & ERAPI_KEY_A){play_music_id(22);} // AでBGMを再スタート
    ERAPI_RenderFrame(1); // フレーム確定
  }
  //終了処理
  stop_all_sound(); // 終了時にBGM停止
  return ERAPI_EXIT_TO_MENU; // メニューに戻る
}
