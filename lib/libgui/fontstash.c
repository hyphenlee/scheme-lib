//
// Copyright (c) 2011-2013 Andreas Krinke andreas.krinke@gmx.de
// Copyright (c) 2009 Mikko Mononen memon@inside.org
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
#include "logger.h"
#include "fontstash.h"
#include <math.h> /* @rlyeh: floorf() */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int idx = 1;

static unsigned int hashint(unsigned int a) {
  a += ~(a << 15);
  a ^= (a >> 10);
  a += (a << 3);
  a ^= (a >> 6);
  a += ~(a << 11);
  a ^= (a >> 16);
  return a;
}

static const unsigned char utf8d[] = {
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 00..1f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 20..3f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 40..5f
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,  // 60..7f
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   9,   9,   9,   9,   9,   9,
    9,   9,   9,   9,   9,   9,   9,   9,   9,   9,  // 80..9f
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,   7,
    7,   7,   7,   7,   7,   7,   7,   7,   7,   7,  // a0..bf
    8,   8,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2,   2,
    2,   2,   2,   2,   2,   2,   2,   2,   2,   2,  // c0..df
    0xa, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3,
    0x3, 0x3, 0x4, 0x3, 0x3,  // e0..ef
    0xb, 0x6, 0x6, 0x6, 0x5, 0x8, 0x8, 0x8, 0x8, 0x8, 0x8,
    0x8, 0x8, 0x8, 0x8, 0x8,  // f0..ff
    0x0, 0x1, 0x2, 0x3, 0x5, 0x8, 0x7, 0x1, 0x1, 0x1, 0x4,
    0x6, 0x1, 0x1, 0x1, 0x1,  // s0..s0
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   0,   1,   1,   1,   1,
    1,   0,   1,   0,   1,   1,   1,   1,   1,   1,  // s1..s2
    1,   2,   1,   1,   1,   1,   1,   2,   1,   2,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   2,   1,   1,   1,   1,   1,   1,   1,   1,  // s3..s4
    1,   2,   1,   1,   1,   1,   1,   1,   1,   2,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
    1,   3,   1,   3,   1,   1,   1,   1,   1,   1,  // s5..s6
    1,   3,   1,   1,   1,   1,   1,   3,   1,   3,   1,
    1,   1,   1,   1,   1,   1,   3,   1,   1,   1,   1,
    1,   1,   1,   1,   1,   1,   1,   1,   1,   1,  // s7..s8
};

unsigned int decutf8(unsigned int* state, unsigned int* codep,
                     unsigned int byte) {
  unsigned int type = utf8d[byte];
  *codep = (*state != UTF8_ACCEPT) ? (byte & 0x3fu) | (*codep << 6)
                                   : (0xff >> type) & (byte);
  *state = utf8d[256 + *state * 16 + type];
  return *state;
}

struct sth_stash* sth_create(int cachew, int cacheh) {
  struct sth_stash* stash = NULL;
  GLubyte* empty_data = NULL;
  struct sth_texture* texture = NULL;

  // Allocate memory for the font stash.
  stash = (struct sth_stash*)malloc(sizeof(struct sth_stash));
  if (stash == NULL){
    LOGE("malloc sth_stash error\n");
     goto error;
  }
  memset(stash, 0, sizeof(struct sth_stash));

  // Create data for clearing the textures
  empty_data = malloc(cachew * cacheh);
  if (empty_data == NULL){ 
    LOGE("malloc textures cache error\n");
    goto error;
  }
  memset(empty_data, 0, cachew * cacheh);

  // Allocate memory for the first texture
  texture = (struct sth_texture*)malloc(sizeof(struct sth_texture));
  if (texture == NULL){ 
    LOGE("malloc texture error\n");
    goto error;
  }
  memset(texture, 0, sizeof(struct sth_texture));

  // Create first texture for the cache.
  stash->tw = cachew;
  stash->th = cacheh;
  stash->itw = 1.0f / cachew;
  stash->ith = 1.0f / cacheh;
  stash->empty_data = empty_data;
  stash->tt_textures = texture;
  stash->flags = FONS_ZERO_TOPLEFT;
  stash->scale = 1.0;
  glGenTextures(1, &texture->id);
  if (!texture->id) {
    LOGE("glGenTextures id = %d erro \n",texture->id);
    goto error;
  }
  glBindTexture(GL_TEXTURE_2D, texture->id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, cachew, cacheh, 0, GL_ALPHA,
               GL_UNSIGNED_BYTE, empty_data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  return stash;

error:
  if (stash != NULL) free(stash);
  if (empty_data != NULL) free(empty_data);
  if (texture != NULL) free(texture);
  LOGE("font stash create error\n");
  return NULL;
}

int sth_add_font_from_memory(struct sth_stash* stash, unsigned char* buffer) {
  int ret, i, ascent, descent, fh, lineGap;
  struct sth_font* fnt = NULL;
  fnt = (struct sth_font*)malloc(sizeof(struct sth_font));
  if (fnt == NULL) {
    ret = STH_ENOMEM;
    goto error;
  }
  memset(fnt, 0, sizeof(struct sth_font));
  // Init hash lookup.
  for (i = 0; i < HASH_LUT_SIZE; ++i) fnt->lut[i] = -1;
  fnt->data = buffer;

  // Init stb_truetype
  if (!stbtt_InitFont(&fnt->font, fnt->data, 0)) {
    ret = STH_ETTFINIT;
    goto error;
  }
  // Store normalized line height. The real line height is got
  // by multiplying the lineh by font size.
  stbtt_GetFontVMetrics(&fnt->font, &ascent, &descent, &lineGap);
  fh = ascent - descent;
  fnt->ascender = (float)ascent / (float)fh;
  fnt->descender = (float)descent / (float)fh;
  fnt->lineh = (float)(fh + lineGap) / (float)fh;
  fnt->idx = idx;
  fnt->type = TTFONT_MEM;
  fnt->next = stash->fonts;
  fnt->nfallbacks=0;
  stash->fonts = fnt;
  return idx++;

error:
  if (fnt) {
    if (fnt->glyphs) free(fnt->glyphs);
    free(fnt);
  }
  return ret;
}

int sth_add_font(struct sth_stash* stash, const char* path) {
  FILE* fp = 0;
  int ret, datasize;
  unsigned char* data = NULL;
  int idx;

  // Read in the font data.
  fp = fopen(path, "rb");
  if (!fp) {
    ret = STH_EFILEIO;
    goto error;
  }
  fseek(fp, 0, SEEK_END);
  datasize = (int)ftell(fp);
  fseek(fp, 0, SEEK_SET);
  data = (unsigned char*)malloc(datasize);
  if (data == NULL) {
    ret = STH_ENOMEM;
    goto error;
  }
  fread(data, 1, datasize, fp);
  idx = sth_add_font_from_memory(stash, data);
  fclose(fp);
  fp = 0;
  // Modify type of the loaded font.
  if (idx)
    stash->fonts->type = TTFONT_FILE;
  else
    free(data);
  return idx;

error:
  if (data) free(data);
  if (fp) fclose(fp);
  LOGE("sth_add_font %s erro\n",path);
  return ret;
}

int sth_add_bitmap_font(struct sth_stash* stash, int ascent, int descent,
                        int line_gap) {
  int ret, i, fh;
  struct sth_font* fnt = NULL;

  fnt = (struct sth_font*)malloc(sizeof(struct sth_font));
  if (fnt == NULL) {
    ret = STH_ENOMEM;
    goto error;
  }
  memset(fnt, 0, sizeof(struct sth_font));

  // Init hash lookup.
  for (i = 0; i < HASH_LUT_SIZE; ++i) fnt->lut[i] = -1;

  // Store normalized line height. The real line height is got
  // by multiplying the lineh by font size.
  fh = ascent - descent;
  fnt->ascender = (float)ascent / (float)fh;
  fnt->descender = (float)descent / (float)fh;
  fnt->lineh = (float)(fh + line_gap) / (float)fh;

  fnt->idx = idx;
  fnt->type = BMFONT;
  fnt->next = stash->fonts;
  stash->fonts = fnt;

  return idx++;

error:
  if (fnt) free(fnt);
  return ret;
}

int sth_add_glyph_for_codepoint(struct sth_stash* stash, int idx, GLuint id,
                                unsigned int codepoint, short size, short base,
                                int x, int y, int w, int h, float xoffset,
                                float yoffset, float xadvance) {
  struct sth_texture* texture = NULL;
  struct sth_font* fnt = NULL;
  struct sth_glyph* glyph = NULL;

  if (stash == NULL) return STH_EINVAL;
  texture = stash->bm_textures;
  while (texture != NULL && texture->id != id) texture = texture->next;
  if (texture == NULL) {
    // Create new texture
    texture = (struct sth_texture*)malloc(sizeof(struct sth_texture));
    if (texture == NULL) return STH_ENOMEM;
    memset(texture, 0, sizeof(struct sth_texture));
    texture->id = id;
    texture->next = stash->bm_textures;
    stash->bm_textures = texture;
  }

  fnt = stash->fonts;
  while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
  if (fnt == NULL) return STH_EINVAL;
  if (fnt->type != BMFONT) return STH_EINVAL;

  // Alloc space for new glyph.
  fnt->nglyphs++;
  fnt->glyphs = (struct sth_glyph*)realloc(
      fnt->glyphs,
      fnt->nglyphs *
          sizeof(struct sth_glyph)); /* @rlyeh: explicit cast needed in C++ */
  if (!fnt->glyphs) return STH_ENOMEM;

  // Init glyph.
  glyph = &fnt->glyphs[fnt->nglyphs - 1];
  memset(glyph, 0, sizeof(struct sth_glyph));
  glyph->codepoint = codepoint;
  glyph->size = size;
  glyph->texture = texture;
  glyph->x0 = x;
  glyph->y0 = y;
  glyph->x1 = glyph->x0 + w;
  glyph->y1 = glyph->y0 + h;
  glyph->xoff = xoffset;
  glyph->yoff = yoffset - base;
  glyph->xadv = xadvance;

  // Find code point and size.
  h = hashint(codepoint) & (HASH_LUT_SIZE - 1);
  // Insert char to hash lookup.
  glyph->next = fnt->lut[h];
  fnt->lut[h] = fnt->nglyphs - 1;

  return STH_ESUCCESS;
}

inline int sth_add_glyph_for_char(struct sth_stash* stash, int idx, GLuint id,
                                  const char* s, short size, short base, int x,
                                  int y, int w, int h, float xoffset,
                                  float yoffset, float xadvance) {
  unsigned int codepoint;
  unsigned int state = 0;

  for (; *s; ++s) {
    if (!decutf8(&state, &codepoint, *(unsigned char*)s)) break;
  }
  if (state != UTF8_ACCEPT) return STH_EINVAL;

  return sth_add_glyph_for_codepoint(stash, idx, id, codepoint, size, base, x,
                                     y, w, h, xoffset, yoffset, xadvance);
}
//
int sth_add_fallback_font(struct sth_font* font,int fallback_id){
  if(font->nfallbacks<=FONS_MAX_FALLBACKS){
    font->fallbacks[font->nfallbacks]=fallback_id;
    font->nfallbacks++;
    return font->nfallbacks;
  }else{
    return -1;
  } 
}
struct sth_font* get_font_by_index(struct sth_stash* stash, int idx){
  struct sth_font* fnt = stash->fonts;
  while (fnt != NULL){
    if(fnt->idx == idx){
      return fnt;
    }
    fnt = fnt->next;
  }
  return NULL;
}
struct sth_glyph* get_glyph(struct sth_stash* stash, struct sth_font* fnt,
                            unsigned int codepoint, short isize) {
  int i, g, advance, lsb, x0, y0, x1, y1, gw, gh;
  float scale;
  struct sth_texture* texture = NULL;
  struct sth_glyph* glyph = NULL;
  unsigned char* bmp = NULL;
  unsigned int h;
  float size = isize / 10.0f;
  int rh;
  struct sth_row* br = NULL;

  // Find code point and size.
  h = hashint(codepoint) & (HASH_LUT_SIZE - 1);
  i = fnt->lut[h];
  while (i != -1) {
    if (fnt->glyphs[i].codepoint == codepoint &&
        (fnt->type == BMFONT || fnt->glyphs[i].size == isize))
      return &fnt->glyphs[i];
    i = fnt->glyphs[i].next;
  }
  // Could not find glyph.

  // For bitmap fonts: ignore this glyph.
  if (fnt->type == BMFONT) return 0;
  struct sth_font* temp_fnt=fnt;
  // For truetype fonts: create this glyph.
  g = stbtt_FindGlyphIndex(&fnt->font, codepoint);
  // Try to find the glyph in fallback fonts.
  if (g == 0) {
    // printf("====> fnt %p nfallbacks=%d idx=%d\n",fnt, fnt->nfallbacks,fnt->idx);
		for (i = 0; i < fnt->nfallbacks; ++i) {
			int index = fnt->fallbacks[i];
      struct sth_font *fallback_font=get_font_by_index(stash,index);
      if(fallback_font!=NULL){
        int fallbackIndex = stbtt_FindGlyphIndex(&fallback_font->font, codepoint);
        if (fallbackIndex != 0) {
          g = fallbackIndex;
          temp_fnt=fallback_font;
          break;
        }
      }
		}
    
		// It is possible that we did not find a fallback glyph.
		// In that case the glyph index 'g' is 0, and we'll proceed below and cache empty glyph.
	}
  if (!g) return 0; /* @rlyeh: glyph not found, ie, arab chars */
  scale = stbtt_ScaleForPixelHeight(&temp_fnt->font, size);
  stbtt_GetGlyphHMetrics(&temp_fnt->font, g, &advance, &lsb);
  stbtt_GetGlyphBitmapBox(&temp_fnt->font, g, scale, scale, &x0, &y0, &x1, &y1);
  gw = x1 - x0;
  gh = y1 - y0;

  // Check if glyph is larger than maximum texture size
  if (gw >= stash->tw || gh >= stash->th) return 0;

  // Find texture and row where the glyph can be fit.
  br = NULL;
  rh = (gh + 7) & ~7;
  texture = stash->tt_textures;
  while (br == NULL) {
    for (i = 0; i < texture->nrows; ++i) {
      if (texture->rows[i].h == rh && texture->rows[i].x + gw + 1 <= stash->tw)
        br = &texture->rows[i];
    }

    // If no row is found, there are 3 possibilities:
    //   - add new row
    //   - try next texture
    //   - create new texture
    if (br == NULL) {
      short py = 0;
      // Check that there is enough space.
      if (texture->nrows) {
        py = texture->rows[texture->nrows - 1].y +
             texture->rows[texture->nrows - 1].h + 1;
        if (py + rh > stash->th) {
          if (texture->next != NULL) {
            texture = texture->next;
          } else {
            // Create new texture
            texture->next =
                (struct sth_texture*)malloc(sizeof(struct sth_texture));
            texture = texture->next;
            if (texture == NULL) goto error;
            memset(texture, 0, sizeof(struct sth_texture));
            glGenTextures(1, &texture->id);
            if (!texture->id) goto error;
            glBindTexture(GL_TEXTURE_2D, texture->id);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, stash->tw, stash->th, 0,
                         GL_ALPHA, GL_UNSIGNED_BYTE, stash->empty_data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          }
          continue;
        }
      }
      // Init and add row
      br = &texture->rows[texture->nrows];
      br->x = 0;
      br->y = py;
      br->h = rh;
      texture->nrows++;
    }
  }

  // Alloc space for new glyph.
  fnt->nglyphs++;
  fnt->glyphs = (struct sth_glyph*)realloc(
      fnt->glyphs,
      fnt->nglyphs *
          sizeof(struct sth_glyph)); /* @rlyeh: explicit cast needed in C++ */
  if (!fnt->glyphs) return 0;

  // Init glyph.
  glyph = &fnt->glyphs[fnt->nglyphs - 1];
  memset(glyph, 0, sizeof(struct sth_glyph));
  glyph->codepoint = codepoint;
  glyph->size = isize;
  glyph->texture = texture;
  glyph->x0 = br->x;
  glyph->y0 = br->y;
  glyph->x1 = glyph->x0 + gw;
  glyph->y1 = glyph->y0 + gh;
  glyph->xadv = scale * advance;
  glyph->xoff = (float)x0;
  glyph->yoff = (float)y0;
  glyph->next = 0;

  // Advance row location.
  br->x += gw + 1;

  // Insert char to hash lookup.
  glyph->next = fnt->lut[h];
  fnt->lut[h] = fnt->nglyphs - 1;

  // Rasterize
  bmp = (unsigned char*)malloc(gw * gh);
  if (bmp) {
    stbtt_MakeGlyphBitmap(&temp_fnt->font, bmp, gw, gh, gw, scale, scale, g);
    // Update texture
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, glyph->x0, glyph->y0, gw, gh, GL_ALPHA,
                    GL_UNSIGNED_BYTE, bmp);
    free(bmp);
  }

  return glyph;

error:
  if (texture) free(texture);
  return 0;
}

float sth_get_advace(struct sth_stash* stash, struct sth_font* fnt,
                     struct sth_glyph* glyph, short isize) {
  int rx, ry;
  float scale = 1.0f;
  if (fnt->type == BMFONT) scale = isize / (glyph->size * 10.0f);
  return (int)scale * glyph->xadv;
  // return scale * (glyph->x1 - glyph->x0);
}

int get_quad(struct sth_stash* stash, struct sth_font* fnt,
             struct sth_glyph* glyph, short isize, float* x, float* y,
             struct sth_quad* q) {
  int rx, ry;
  float scale = 1.0f;

  if (fnt->type == BMFONT) scale = isize / (glyph->size * 10.0f);
  if (stash->flags & FONS_ZERO_TOPLEFT) {
    rx = floorf(*x + scale * glyph->xoff);
    ry = floorf(*y + scale * glyph->yoff);

    q->x0 = rx;
    q->y0 = ry;
    q->x1 = rx + scale * (glyph->x1 - glyph->x0);
    q->y1 = ry + scale * (glyph->y1 - glyph->y0);

  } else {
    rx = floorf(*x + scale * glyph->xoff);
    ry = floorf(*y - scale * glyph->yoff);

    q->x0 = rx;
    q->y0 = ry;
    q->x1 = rx + scale * (glyph->x1 - glyph->x0);
    q->y1 = ry - scale * (glyph->y1 - glyph->y0);
  }
  q->s0 = (glyph->x0) * stash->itw;
  q->t0 = (glyph->y0) * stash->ith;
  q->s1 = (glyph->x1) * stash->itw;
  q->t1 = (glyph->y1) * stash->ith;
  *x += scale * glyph->xadv;

  return 1;
}

float* setv(float* v, float x, float y, float s, float t) {
  v[0] = x;
  v[1] = y;
  v[2] = s;
  v[3] = t;
  return v + 4;
}

float* setc(float* v, float r, float g, float b, float a) {
  v[0] = r;
  v[1] = g;
  v[2] = b;
  v[3] = a;
  return v + 4;
}
void clear_draw(struct sth_stash* stash) {
  struct sth_texture* texture = stash->tt_textures;
  short tt = 1;
  while (texture) {
    if (texture->nverts > 0) {
      texture->nverts = 0;
    }
    texture = texture->next;
    if (!texture && tt) {
      texture = stash->bm_textures;
      tt = 0;
    }
  }
}

void flush_draw(struct sth_stash* stash) {
  struct sth_texture* texture = stash->tt_textures;
  short tt = 1;
  glActiveTexture(GL_TEXTURE0);
  glEnable(GL_TEXTURE_2D);
  while (texture) {
    if (texture->nverts > 0) {
      glBindTexture(GL_TEXTURE_2D, texture->id);

      glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, VERT_STRIDE,
                            texture->verts);
      glEnableVertexAttribArray(0);

      glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, VERT_STRIDE,
                            &texture->verts[2]);
      glEnableVertexAttribArray(1);

      glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, VERT_STRIDE,
                            texture->colors);
      glEnableVertexAttribArray(2);
      glDrawArrays(GL_TRIANGLES, 0, texture->nverts);
      glDisableVertexAttribArray(0);
      glDisableVertexAttribArray(1);
      glDisableVertexAttribArray(2);
      texture->nverts = 0;
    }
    texture = texture->next;
    if (!texture && tt) {
      texture = stash->bm_textures;
      tt = 0;
    }
  }
  glDisable(GL_TEXTURE_2D);
}

void sth_begin_draw(struct sth_stash* stash) {
  if (stash == NULL) return;
  if (stash->drawing) flush_draw(stash);
  stash->drawing = 1;
}

void sth_end_draw(struct sth_stash* stash) {
  if (stash == NULL) return;
  if (!stash->drawing) return;

  /*
          // Debug dump.
          if (stash->nverts+6 < VERT_COUNT)
          {
                  float x = 500, y = 100;
                  float* v = &stash->verts[stash->nverts*4];

                  v = setv(v, x, y, 0, 0);
                  v = setv(v, x+stash->tw, y, 1, 0);
                  v = setv(v, x+stash->tw, y+stash->th, 1, 1);

                  v = setv(v, x, y, 0, 0);
                  v = setv(v, x+stash->tw, y+stash->th, 1, 1);
                  v = setv(v, x, y+stash->th, 0, 1);

                  stash->nverts += 6;
          }
  */

  flush_draw(stash);
  stash->drawing = 0;
}

int sth_pos(struct sth_stash* stash, int idx, float size, float width, char* s,
            int count) {
  unsigned int codepoint;
  struct sth_glyph* glyph = NULL;
  struct sth_texture* texture = NULL;
  unsigned int state = 0;
  struct sth_quad q;
  short isize = (short)(size * 10.0f);
  float* v;
  float* c;
  float x = 0;
  float y = 0;
  float sx = x;
  float sy = y;
  struct sth_font* fnt = NULL;
  if (stash == NULL) return -1;
  if (stash->flags & FONS_ZERO_TOPLEFT) {
    y += stash->fonts->lineh * size * stash->scale;
  } else {
    y -= stash->fonts->lineh * size * stash->scale;
  }
  sy = y;
  fnt = stash->fonts;
  while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
  if (fnt == NULL) return -1;
  if (fnt->type != BMFONT && !fnt->data) return -1;
  int i=0;
  for (i = 0; *s && i < count; ++s) {
    if (decutf8(&state, &codepoint, *(unsigned char*)s)) {
      continue;
    }
    glyph = get_glyph(stash, fnt, codepoint, isize);
    if (!glyph) continue;
    if ((width + glyph->xoff - glyph->xadv ) < sx) {
      return i;
    }
    if (!get_quad(stash, fnt, glyph, isize, &x, &y, &q)) continue;
    sx = x;
    i++;
  }
  return -1;
}

void sth_measure(struct sth_stash* stash, int idx, float size, float width,
                 char* s, int count, float* dx, float* dy) {
  unsigned int codepoint;
  struct sth_glyph* glyph = NULL;
  struct sth_texture* texture = NULL;
  unsigned int state = 0;
  struct sth_quad q;
  short isize = (short)(size * 10.0f);
  float* v;
  float* c;
  float x = 0;
  float y = 0;
  float sx = x;
  float sy = y;
  struct sth_font* fnt = NULL;
  if (stash == NULL) return;
  y += sth_get_line(stash, size);
  sy = y;
  fnt = stash->fonts;
  while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
  if (fnt == NULL) return;
  if (fnt->type != BMFONT && !fnt->data) return;

  for (int i = 0; *s && i < count; ++s) {
    if (decutf8(&state, &codepoint, *(unsigned char*)s)) {
      continue;
    }
    i++;
    glyph = get_glyph(stash, fnt, codepoint, isize);
    if (!glyph) continue;

    texture = glyph->texture;
    if (width > 0 && x >= (width + sx - glyph->xadv)) {
      y += sth_get_line(stash, size);
      x = sx;
    }

    if (!get_quad(stash, fnt, glyph, isize, &x, &y, &q)) continue;
  }
  if (dx) *dx = x;
  if (dy) *dy = y;
}

float sth_get_line(struct sth_stash* stash, float size) {
  float lineh = 0.0;
  if (stash->flags & FONS_ZERO_TOPLEFT) {
    lineh += stash->fonts->lineh * size * stash->scale;
  } else {
    lineh -= stash->fonts->lineh * size * stash->scale;
  }
  return lineh;
}

float sth_get_start(struct sth_stash* stash, float size) {
  float y = 0.0;
  // if (stash->flags & FONS_ZERO_TOPLEFT) {
  //   float gap = (stash->fonts->lineh + stash->fonts->descender -
  //                stash->fonts->ascender);
  //   y += (stash->fonts->ascender - gap) * size;
  // } else {
  //   float gap = (stash->fonts->lineh + stash->fonts->descender -
  //                stash->fonts->ascender);
  //   y -= (stash->fonts->ascender - gap) * size;
  // }
  y += sth_get_line(stash, size) + stash->fonts->descender * size;
  return y;
}

void sth_draw_text(struct sth_stash* stash, int idx, float size, float x,
                   float y, float width, float height, const char* s, float r,
                   float g, float b, float a, float* dx, float* dy) {
  unsigned int codepoint;
  struct sth_glyph* glyph = NULL;
  struct sth_texture* texture = NULL;
  unsigned int state = 0;
  struct sth_quad q;
  short isize = (short)(size * 10.0f);
  float* v;
  float* c;
  float sx = x;
  float sy = y;
  struct sth_font* fnt = NULL;

  if (stash == NULL) return;
  if (s == NULL) return;

  y += sth_get_start(stash, size);

  sy = y;
  fnt = stash->fonts;
  while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
  if (fnt == NULL) return;
  if (fnt->type != BMFONT && !fnt->data) return;

  for (; *s; ++s) {
    // printf("here  %c %x %x\n",*s,*s,'\n');
    if (decutf8(&state, &codepoint, *(unsigned char*)s)) {
      continue;
    }
    glyph = get_glyph(stash, fnt, codepoint, isize);
    if (!glyph) continue;
    texture = glyph->texture;

    if (texture->nverts + 4 >= VERT_COUNT) {
      flush_draw(stash);
    }

    if (width > 0 && x >= (width + sx - glyph->xadv)) {
      y += sth_get_line(stash, size);
      x = sx;
    }

    if (!get_quad(stash, fnt, glyph, isize, &x, &y, &q)) continue;

    v = &texture->verts[texture->nverts * 4];

    // v = setv(v, q.x0, q.y0, q.s0, q.t0);
    // v = setv(v, q.x1, q.y0, q.s1, q.t0);
    // v = setv(v, q.x1, q.y1, q.s1, q.t1);
    // v = setv(v, q.x0, q.y1, q.s0, q.t1);


 		v = setv(v, q.x0, q.y0, q.s0, q.t0);
		v = setv(v, q.x1, q.y0, q.s1, q.t0);
		v = setv(v, q.x1, q.y1, q.s1, q.t1);

		v = setv(v, q.x0, q.y0, q.s0, q.t0);
		v = setv(v, q.x1, q.y1, q.s1, q.t1);
		v = setv(v, q.x0, q.y1, q.s0, q.t1);

    c = &texture->colors[texture->nverts * 4];
    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);
    
    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);
    

    texture->nverts += 6;
  }
  if (dx) *dx = x;
  if (dy) *dy = y;
}

void sth_draw_text_colors(struct sth_stash* stash, int idx, float size, float x,
                          float y, float width, float height, const char* s,
                          int* colors, float* dx, float* dy) {
  unsigned int codepoint;
  struct sth_glyph* glyph = NULL;
  struct sth_texture* texture = NULL;
  unsigned int state = 0;
  struct sth_quad q;
  short isize = (short)(size * 10.0f);
  float* v;
  float* c;
  float sx = x;
  float sy = y;
  size_t count = 0;
  struct sth_font* fnt = NULL;

  if (stash == NULL) return;
  if (s == NULL) return;
  y += sth_get_start(stash, size);
  sy = y;
  fnt = stash->fonts;
  while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
  if (fnt == NULL) return;
  if (fnt->type != BMFONT && !fnt->data) return;

  for (; *s; ++s) {
    if (decutf8(&state, &codepoint, *(unsigned char*)s)) {
      continue;
    }

    int color = colors[count];
    // printf("%c %x %x \n",*s,*s,color );
    count++;

    glyph = get_glyph(stash, fnt, codepoint, isize);
    if (!glyph) {
      // count++;
      continue;
    }

    texture = glyph->texture;

    if (texture->nverts + 4 >= VERT_COUNT) {
      flush_draw(stash);
    }

    if (width > 0 && x >= (width + sx - glyph->xadv)) {
      y += sth_get_line(stash, size);
      x = sx;
    }

    if (!get_quad(stash, fnt, glyph, isize, &x, &y, &q)) {
      continue;
    }

    v = &texture->verts[texture->nverts * 4];

    // v = setv(v, q.x0, q.y0, q.s0, q.t0);
    // v = setv(v, q.x1, q.y0, q.s1, q.t0);
    // v = setv(v, q.x1, q.y1, q.s1, q.t1);
    // v = setv(v, q.x0, q.y1, q.s0, q.t1);

    v = setv(v, q.x0, q.y0, q.s0, q.t0);
		v = setv(v, q.x1, q.y0, q.s1, q.t0);
		v = setv(v, q.x1, q.y1, q.s1, q.t1);

		v = setv(v, q.x0, q.y0, q.s0, q.t0);
		v = setv(v, q.x1, q.y1, q.s1, q.t1);
		v = setv(v, q.x0, q.y1, q.s0, q.t1);

    c = &texture->colors[texture->nverts * 4];

    // printf("%x ",color);

    float b = (color & 0xff) / 255.0;
    float g = (color >> 8 & 0xff) / 255.0;
    float r = (color >> 16 & 0xff) / 255.0;
    float a = (color >> 24 & 0xff) / 255.0;
    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);

    c = setc(c, r, g, b, a);
    c = setc(c, r, g, b, a);

    texture->nverts += 6;
  }
  if (dx) *dx = x;
  if (dy) *dy = y;
  // printf("\n\n");
}

void sth_dim_text(struct sth_stash* stash, int idx, float size, const char* s,
                  float* minx, float* miny, float* maxx, float* maxy) {
  unsigned int codepoint;
  struct sth_glyph* glyph = NULL;
  unsigned int state = 0;
  struct sth_quad q;
  short isize = (short)(size * 10.0f);
  struct sth_font* fnt = NULL;
  float x = 0, y = 0;

  *minx = *maxx = *miny = *maxy = 0; /* @rlyeh: reset vars before failing */

  if (stash == NULL) return;
  fnt = stash->fonts;
  while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
  if (fnt == NULL) return;
  if (fnt->type != BMFONT && !fnt->data) return;

  for (; *s; ++s) {
    if (decutf8(&state, &codepoint, *(unsigned char*)s)) continue;
    glyph = get_glyph(stash, fnt, codepoint, isize);
    if (!glyph) continue;
    if (!get_quad(stash, fnt, glyph, isize, &x, &y, &q)) continue;
    if (q.x0 < *minx) *minx = q.x0;
    if (q.x1 > *maxx) *maxx = q.x1;
    if (q.y1 < *miny) *miny = q.y1;
    if (q.y0 > *maxy) *maxy = q.y0;
  }
  if (floorf(x) > *maxx) *maxx = floorf(x);
}

void sth_vmetrics(struct sth_stash* stash, int idx, float size, float* ascender,
                  float* descender, float* lineh) {
  struct sth_font* fnt = NULL;

  if (stash == NULL) return;
 
  fnt = stash->fonts;
  while (fnt != NULL && fnt->idx != idx) fnt = fnt->next;
  if (fnt == NULL) return;
  if (fnt->type != BMFONT && !fnt->data) return;
  if (ascender) *ascender = fnt->ascender * size;
  if (descender) *descender = fnt->descender * size;
  if (lineh) *lineh = fnt->lineh * size * stash->scale;
}

void sth_delete(struct sth_stash* stash) {
  struct sth_texture* tex = NULL;
  struct sth_texture* curtex = NULL;
  struct sth_font* fnt = NULL;
  struct sth_font* curfnt = NULL;

  if (!stash) return;

  tex = stash->tt_textures;
  while (tex != NULL) {
    curtex = tex;
    tex = tex->next;
    if (curtex->id) glDeleteTextures(1, &curtex->id);
    free(curtex);
  }

  tex = stash->bm_textures;
  while (tex != NULL) {
    curtex = tex;
    tex = tex->next;
    if (curtex->id) glDeleteTextures(1, &curtex->id);
    free(curtex);
  }

  fnt = stash->fonts;
  while (fnt != NULL) {
    curfnt = fnt;
    fnt = fnt->next;
    if (curfnt->glyphs) free(curfnt->glyphs);
    if (curfnt->type == TTFONT_FILE && curfnt->data) free(curfnt->data);
    free(curfnt);
  }
  free(stash->empty_data);
  free(stash);
}
