#include "sprite_bare.h"

void spr_init_mode0_obj1d(void){
  // BGは既存(ERAPI)でも使えるよう MODE0据え置き＋OBJ(1D)を有効化
  REG_DISPCNT = (REG_DISPCNT & ~0x0007) | DCNT_MODE0;
  REG_DISPCNT |= (DCNT_OBJ | DCNT_OBJ_1D);
}

void spr_wait_vblank(void){
  // VDraw(0..159)を抜けてVBlankに入るのを待つ
  while(REG_VCOUNT >= 160);
  while(REG_VCOUNT < 160);
}

void spr_dma_copy32(void* dst, const void* src, uint32_t words){
  REG_DMA3CNT = 0;                 // 一旦停止
  REG_DMA3SAD = src;
  REG_DMA3DAD = dst;
  REG_DMA3CNT = DMA_ENABLE | DMA_32 | words;
}
