/* gfx.h - OpenGL 1.1 fixed-function rendering helpers */
#ifndef GFX_H
#define GFX_H
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>
#include "mathx.h"

enum {
    TEX_NONE, TEX_GRASS, TEX_SNOW, TEX_ROCK, TEX_BRICK, TEX_WOOD, TEX_SAND, TEX_ICE,
    TEX_WATER, TEX_METAL, TEX_CIRCUIT, TEX_LAVA, TEX_CARPET, TEX_TILE, TEX_DIRT, TEX_STONE,
    TEX_ROOF, TEX_PORTAL1, TEX_PORTAL2, TEX_PORTAL3, TEX_PORTAL4, TEX_PORTAL5, TEX_PORTAL6, TEX_PORTAL7, TEX_PORTAL8, TEX_PORTAL9, TEX_PORTAL10, TEX_PORTAL11, TEX_PORTAL12,
    TEX_SHADOW, TEX_FONT, TEX_GLOW, TEX_CLOUD, TEX_DARK, TEX_PLANK, TEX_HEX, TEX_SLIDE,
    TEX_CYGRID, TEX_CYWALL, TEX_CYDATA,   /* cyberspace (castle interior) */
    TEX_COUNT
};
extern GLuint g_tex[TEX_COUNT];
extern float g_tex_scale[TEX_COUNT];

typedef struct { float p[3], n[3], uv[2]; unsigned char c[4]; } Vtx;
typedef struct { Vtx *v; int n, cap; } VList;
typedef struct { VList g[TEX_COUNT]; } Mesh;

void   mesh_init(Mesh *m);
void   mesh_free(Mesh *m);
void   mesh_add(Mesh *m, int tex, const Vtx *a, const Vtx *b, const Vtx *c);
GLuint mesh_compile(Mesh *m);
int    mesh_count(Mesh *m);

/* ---- CPU-side model builder (transform stack applied to generated primitives) ---- */
void mb_begin(Mesh *m);
void mb_end(void);
void mb_push(void);
void mb_pop(void);
void mb_identity(void);
void mb_translate(float x, float y, float z);
void mb_rotate(float deg, float x, float y, float z);
void mb_scale(float x, float y, float z);
void mb_color(float r, float g, float b);
void mb_color4(float r, float g, float b, float a);
void mb_tex(int tex);
void mb_uvscale(float s);
void mb_tri(vec3 a, vec3 b, vec3 c);
void mb_tri_n(vec3 a, vec3 b, vec3 c, vec3 na, vec3 nb, vec3 nc);
void mb_quad(vec3 a, vec3 b, vec3 c, vec3 d);
void mb_sphere(int slices, int stacks);          /* unit sphere */
void mb_hemisphere(int slices, int stacks);      /* upper half */
void mb_cylinder(int slices, int caps);          /* r=1, y 0..1 */
void mb_cone(int slices);                        /* r=1 at y=0, apex y=1 */
void mb_box(void);                               /* -1..1 cube */
void mb_torus(float R, float r, int seg, int seg2);
void mb_disc(int slices);                        /* r=1 facing +y */
void mb_star(float inner, float depth);          /* 5-point star, r=1, facing +z */
/* convenience */
void mb_ellipsoid(float x, float y, float z, float rx, float ry, float rz, int seg);
void mb_cuboid(float x0, float y0, float z0, float x1, float y1, float z1);

/* ---- textures ---- */
void gfx_init(void);
void gfx_bind(int tex);

/* ---- frame / camera ---- */
void gfx_begin_3d(float fov_deg, float znear, float zfar);
void gfx_lookat(vec3 eye, vec3 at);
void gfx_set_fog(float r, float g, float b, float start, float end);
void gfx_light(float ambient, float diffuse);
extern float g_view_aspect;
extern int g_scr_w, g_scr_h;

/* ---- 2D (virtual coords, height 480, width 480*aspect) ---- */
extern float g_vw;   /* virtual width */
void gfx_begin_2d(void);
void gfx_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void gfx_text(float x, float y, float scale, const char *s, float r, float g, float b, float a);
void gfx_text_shadow(float x, float y, float scale, const char *s, float r, float g, float b, float a);
float gfx_text_width(const char *s, float scale);
float gfx_line_height(const char *s, float scale);
unsigned utf8_next(const char **s);
float glyph_adv(unsigned cp);
void gfx_text_center(float cx, float y, float scale, const char *s, float r, float g, float b, float a);
void gfx_star2d(float cx, float cy, float rad, float r, float g, float b, float a);
void gfx_circle2d(float cx, float cy, float rad, float r, float g, float b, float a);
void gfx_pie2d(float cx, float cy, float rad, int segments_filled, int total, float r, float g, float b);
void gfx_fish2d(float cx, float cy, float s, float r, float g, float b, float a);

/* ---- world helpers ---- */
void gfx_shadow(vec3 p, vec3 n, float radius, float alpha);
void gfx_billboard_quad(vec3 p, float size, float r, float g, float b, float a, vec3 cam_right, vec3 cam_up);

#endif
