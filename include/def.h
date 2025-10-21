#ifndef DEF_H
#define DEF_H

/* ---- Volatile 基本型（GBA系で使用） ---- */
typedef volatile unsigned char      vu8;
typedef volatile unsigned short     vu16;
typedef volatile unsigned int       vu32;
typedef volatile unsigned long long vu64;

/* ---- 基本型 ---- */
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;

typedef signed char      s8;
typedef signed short     s16;
typedef signed int       s32;
typedef signed long long s64;

/* --- NULL フォールバック --- */
#ifndef NULL
  #ifdef __cplusplus
    #define NULL 0
  #else
    #define NULL ((void*)0)
  #endif
#endif

/* ---- ゲーム定数 ---- */
#define PLAYERS   4
#define MAX_HAND  20
#define MAX_DECK  53

/* ---- 手札（u8） ---- */
typedef struct {
    u8  cards[MAX_HAND];
    int count;
} Hand;

#endif /* DEF_H */
