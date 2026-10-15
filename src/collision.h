/* collision.h - triangle surface collision (floors / walls / ceilings), SM64 style */
#ifndef COLLISION_H
#define COLLISION_H
#include "mathx.h"

enum { SURF_FLOOR, SURF_WALL, SURF_CEIL };

/* surface flags */
#define SF_SLIPPERY   1    /* ice: low friction */
#define SF_SLIDE      2    /* always belly-slide (slide course) */
#define SF_BURN       4    /* lava / freezing water: hurts and bounces */
#define SF_DEATH      8    /* instant death plane */
#define SF_NOSLIP     16   /* never slide even if steep */
#define SF_NOCAM      32   /* camera ignores */
#define SF_SNOW       64   /* soft landing: no fall damage */
#define SF_POWDER     128  /* deep powder snow: you sink in it */

typedef struct {
    vec3 v[3];
    vec3 n;
    float d;           /* plane: dot(n,p) + d = 0 */
    float minx, maxx, miny, maxy, minz, maxz;
    unsigned char type, flags;
    short owner;       /* -1 static, else object index */
    short param;
} Surf;

void  col_reset(void);
void  col_add_tri(vec3 a, vec3 b, vec3 c, int flags, int param);
void  col_build_grid(void);
int   col_static_count(void);

/* dynamic (object) collision: cleared every frame, then objects add their tris */
void  col_dyn_clear(void);
void  col_dyn_add(vec3 a, vec3 b, vec3 c, int flags, int owner);

float col_floor(float x, float y, float z, Surf **out);    /* highest floor at or below y; -1e9 if none */
int   col_any_below(float x, float y, float z);
float col_ceil(float x, float y, float z, Surf **out);     /* lowest ceiling at or above y; 1e9 if none */
/* pushes pos out of walls (sphere at pos+yoff of radius). returns number of walls hit, last normal in *wn */
int   col_walls(vec3 *pos, float yoff, float radius, vec3 *wn, Surf **wall);
/* segment raycast; returns 1 on hit with t (0..1) */
int   col_ray(vec3 a, vec3 b, float *t, vec3 *n, int ignore_nocam);

#endif
