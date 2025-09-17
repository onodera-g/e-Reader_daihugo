
//{{BLOCK(bg_blue)

//======================================================================
//
//	bg_blue, 256x160@4, 
//	+ palette 16 entries, not compressed
//	+ 1 tiles (t|f|p reduced) not compressed
//	+ regular map (flat), not compressed, 32x20 
//	Total size: 32 + 32 + 1280 = 1344
//
//	Time-stamp: 2025-09-12, 14:14:13
//	Exported by Cearn's GBA Image Transmogrifier, v0.9.2
//	( http://www.coranac.com/projects/#grit )
//
//======================================================================

#ifndef GRIT_BG_BLUE_H
#define GRIT_BG_BLUE_H

#define bg_blueTilesLen 32
extern const unsigned int bg_blueTiles[8];

#define bg_blueMapLen 1280
extern const unsigned short bg_blueMap[640];

#define bg_bluePalLen 32
extern const unsigned short bg_bluePal[16];

#endif // GRIT_BG_BLUE_H

//}}BLOCK(bg_blue)
