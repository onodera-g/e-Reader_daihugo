#pragma once
#include "def.h"
#include "erapi.h"
#include "messages_autogen.h"

//--------------------------------------------------------------
//  数字ユーティリティ（SJIS全角）
//  ・TU_ToFullwidthDigits : 整数→SJIS全角数字へ変換
//  ・TU_DrawNumberFW      : 全角数字だけを描画
//  ・TU_DrawLabelAndNumberFW : ラベルと全角数字を横並びで描画
//--------------------------------------------------------------

void TU_ToFullwidthDigits(char *dst, unsigned int dstsz, int value, int digits);
void TU_DrawNumberFW(u32 region, int px, int py, int value, int digits);

void TU_DrawLabelAndNumberFW(u32 region, int px, int py,
                             const unsigned char* label_sjis, int value, int digits);

// セル単位の1文字描画（SJIS）
// cell_x/cell_y は 8px 単位でのマス座標
static inline void TU_PutCell(u32 region, int cell_x, int cell_y, const unsigned char* sjis){
  ERAPI_DrawText(region, cell_x*8, cell_y*8, (const char*)sjis);
}
