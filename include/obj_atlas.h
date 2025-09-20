//{{BLOCK(obj_atlas)

//======================================================================
//
//  OBJ atlas obj_atlas, 4bpp, shared 16-color palette
//======================================================================

#ifndef GRIT_OBJ_ATLAS_H
#define GRIT_OBJ_ATLAS_H

typedef struct {
  unsigned short w, h;
  unsigned short tiles_per_frame;
  unsigned short frames;
  unsigned int   offset_words;
  unsigned char  width_code;
  unsigned char  height_code;
} ObjSpriteDesc;

#define obj_atlasTilesLen 15840
extern const unsigned int obj_atlasTiles[3960];
#define obj_atlasPalLen 32
extern const unsigned short obj_atlasPal[16];

extern const ObjSpriteDesc objAtlasSprites[61];
extern const char* objAtlasNames[61];
#define OBJ_ATLAS_SPRITE_COUNT 61

int objAtlasFindIndex(const char* name);

#endif

//}}BLOCK(obj_atlas)
