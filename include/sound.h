#ifndef SOUND_H
#define SOUND_H

#include "def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Project sound IDs (edit as needed) --- */
enum {
    SND_SE_DEAL   = 24,
    SND_SE_YAGIRI = 25,
    SND_SE_SIBARI = 26,

    /* BGM IDs: adjust to your asset table */
    SND_BGM_TITLE = 1,
    SND_BGM_GAME  = 22
};

/* Initialize sound subsystem (no-op if not needed) */
void sound_init(void);

/* Play/stop background music. If loop != 0, it will loop. */
void sound_play_bgm(int bgm_id, int loop);
void sound_stop_bgm(void);

/* Play a one-shot sound effect. */
void sound_play_se(int se_id);

/* Per-frame update (optional; safe to call every frame). */
void sound_update(void);

/* BGM音量設定（0〜127） */
void sound_set_bgm_volume(int vol_0_127);
/* SE音量設定（0〜127） */
void sound_set_se_volume(int vol_0_127);

#ifdef __cplusplus
}
#endif
#endif /* SOUND_H */
