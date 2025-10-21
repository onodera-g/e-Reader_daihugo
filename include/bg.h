//{{BLOCK(bg) 

//======================================================================
//
//	bg, 256x160@4, 
//	+ palette 16 entries, not compressed
//	+ tiles (t|f|p reduced) not compressed
//	+ regular map (flat), not compressed, 32x20 
//	Total size: 32 + 832 + 1280 = 2144
//
//======================================================================

#ifndef GRIT_BG_H
#define GRIT_BG_H

#define bgTilesLen 832
extern const unsigned int bgTiles[208];

#define bgMapLen 1280
extern const unsigned short bgMap[640];

#define bgPalLen 32
extern const unsigned short bgPal[16];

#endif // GRIT_BG_H

//}}BLOCK(bg)
    