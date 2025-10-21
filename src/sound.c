#include "sound.h"
#include "erapi.h"

/* ---------------------------------------------------------
 * サウンド管理用の内部状態
 * ---------------------------------------------------------*/
static int s_bgm_id     = -1;    /* 現在再生中のBGM ID（未再生: -1） */
static int s_bgm_loop   = 0;     /* 1=ループ再生 */
static int s_bgm_volume = 127;   /* BGM音量（0〜127） */
static int s_se_volume  = 127;   /* SE音量（0〜127） */

static int _clamp127(int v){
    if (v < 0)   return 0;
    if (v > 127) return 127;
    return v;
}

/* 内部：BGMの実再生（音量も即反映） */
static inline void _play_bgm_with_volume(int id){
    ERAPI_PlaySoundSystem((u32)id);
    ERAPI_SetSoundVolume((u32)id, (u32)s_bgm_volume);
}

/* 内部：BGM停止（必要なら実装） */
static inline void _stop_bgm(void){
    /* no-op */
}

/* ---------------------------------------------------------
 * 公開API
 * ---------------------------------------------------------*/
void sound_init(void){
    s_bgm_id = -1;
    s_bgm_loop = 0;
    s_bgm_volume = 127;
    s_se_volume  = 127;
}

void sound_play_bgm(int bgm_id, int loop){
    s_bgm_id   = bgm_id;
    s_bgm_loop = loop ? 1 : 0;
    _play_bgm_with_volume(bgm_id);
}

void sound_stop_bgm(void){
    _stop_bgm();
    s_bgm_id = -1;
    s_bgm_loop = 0;
}

void sound_play_se(int se_id){
    /* ★ 渡された se_id をそのまま再生（固定IDを使わない） */
    ERAPI_PlaySoundSystem((u32)se_id);
    ERAPI_SetSoundVolume((u32)se_id, (u32)s_se_volume);
}

void sound_update(void){
    /* ループ制御が必要ならここに追加 */
    (void)0;
}

/* 音量設定 */
void sound_set_bgm_volume(int vol_0_127){
    s_bgm_volume = _clamp127(vol_0_127);
    if (s_bgm_id >= 0){
        ERAPI_SetSoundVolume((u32)s_bgm_id, (u32)s_bgm_volume);
    }
}
void sound_set_se_volume(int vol_0_127){
    s_se_volume = _clamp127(vol_0_127);
}
