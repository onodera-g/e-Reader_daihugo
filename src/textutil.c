// src/textutil.c
#include "textutil.h"

//--------------------------------------------------------------
// 数値→ASCII（ゼロ埋め）
// out: 出力バッファ（末尾NUL）
// value: 表示したい整数（負数は0扱い）
// digits: 桁数（不足分は'0'で埋める）
//--------------------------------------------------------------
static void to_padded_decimal_ascii(char* out, unsigned int outsz, int value, int digits){
  if (outsz == 0) return;
  if (digits < 1) digits = 1;
  if ((unsigned int)digits + 1 > outsz) digits = (int)(outsz - 1);
  if (value < 0) value = 0;

  int v = value;
  for (int i = digits - 1; i >= 0; --i) {
    out[i] = (char)('0' + (v % 10));
    v /= 10;
  }
  out[digits] = '\0';
}

//--------------------------------------------------------------
// ASCIIの'0'〜'9'をSJISの全角数字（"０"〜"９"）へ詰め替え
// dst: 出力（2バイト/1文字+終端）。不足時は入り切る分まで。
// digits: 生成する桁数（表示桁）
//--------------------------------------------------------------
static void ascii_digits_to_sjis_fullwidth(char* dst, unsigned int dstsz, const char* src, int digits){
  const unsigned char hi = 0x82;
  const unsigned char lo_base = 0x4F; // '０'
  unsigned int need = (unsigned int)digits * 2 + 1;
  if (dstsz < need) digits = (int)((dstsz > 0) ? ((dstsz - 1) / 2) : 0);

  unsigned int p = 0;
  for (int i = 0; i < digits; ++i) {
    char c = src[i];
    unsigned char lo = (c>='0' && c<='9') ? (unsigned char)(lo_base + (c - '0')) : lo_base;
    if (p + 2 < dstsz) { dst[p++] = (char)hi; dst[p++] = (char)lo; }
  }
  if (p < dstsz) dst[p] = '\0';
}

//--------------------------------------------------------------
// 公開API：整数を「全角数字（SJIS）」へ変換
//--------------------------------------------------------------
void TU_ToFullwidthDigits(char *dst, unsigned int dstsz, int value, int digits){
  char tmp_ascii[32];
  if ((unsigned int)digits + 1 > sizeof(tmp_ascii)) digits = (int)sizeof(tmp_ascii) - 1;
  to_padded_decimal_ascii(tmp_ascii, sizeof(tmp_ascii), value, digits);
  ascii_digits_to_sjis_fullwidth(dst, dstsz, tmp_ascii, digits);
}

//--------------------------------------------------------------
// 公開API：全角数字を描画
//--------------------------------------------------------------
void TU_DrawNumberFW(u32 region, int px, int py, int value, int digits){
  char numfw[32];
  TU_ToFullwidthDigits(numfw, sizeof(numfw), value, digits);
  ERAPI_DrawText(region, px, py, numfw);
}

//--------------------------------------------------------------
// 公開API：「ラベル＋数字（全角）」を描画
//--------------------------------------------------------------
void TU_DrawLabelAndNumberFW(u32 region, int px, int py,
                             const unsigned char* label_sjis, int value, int digits){
  ERAPI_DrawText(region, px,   py, (const char*)label_sjis);
  TU_DrawNumberFW(region, px+64, py, value, digits); // 8px*8=64px右に数字
}
