/* gfx.c - procedural textures, mesh building, display lists, 2D drawing */
#include "gfx.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

GLuint g_tex[TEX_COUNT];
float g_tex_scale[TEX_COUNT];
float g_view_aspect = 4.0f / 3.0f;
float g_vw = 640;
int g_scr_w = 640, g_scr_h = 480;
unsigned int rng_state = 0x12345678u;

/* ======================================================================
 * Mesh
 * ====================================================================== */
void mesh_init(Mesh *m) { memset(m, 0, sizeof *m); }

void mesh_free(Mesh *m)
{
    int i;
    for (i = 0; i < TEX_COUNT; i++) free(m->g[i].v);
    memset(m, 0, sizeof *m);
}

int mesh_count(Mesh *m)
{
    int i, n = 0;
    for (i = 0; i < TEX_COUNT; i++) n += m->g[i].n;
    return n / 3;
}

void mesh_add(Mesh *m, int tex, const Vtx *a, const Vtx *b, const Vtx *c)
{
    VList *l = &m->g[tex];
    if (l->n + 3 > l->cap) {
        l->cap = l->cap ? l->cap * 2 : 768;
        l->v = (Vtx *)realloc(l->v, sizeof(Vtx) * l->cap);
    }
    l->v[l->n++] = *a;
    l->v[l->n++] = *b;
    l->v[l->n++] = *c;
}

GLuint mesh_compile(Mesh *m)
{
    GLuint dl = glGenLists(1);
    int t, i;
    glNewList(dl, GL_COMPILE);
    for (t = 0; t < TEX_COUNT; t++) {
        VList *l = &m->g[t];
        if (!l->n) continue;
        if (t == TEX_NONE) glDisable(GL_TEXTURE_2D);
        else { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_tex[t]); }
        glBegin(GL_TRIANGLES);
        for (i = 0; i < l->n; i++) {
            Vtx *v = &l->v[i];
            glColor4ubv(v->c);
            glNormal3fv(v->n);
            glTexCoord2fv(v->uv);
            glVertex3fv(v->p);
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
    glEndList();
    return dl;
}

/* ======================================================================
 * Model builder
 * ====================================================================== */
typedef struct { float m[12]; } M34;
static M34 mb_stack[32];
static int mb_sp;
static Mesh *mb_mesh;
static unsigned char mb_col[4] = { 255, 255, 255, 255 };
static int mb_curtex;
static float mb_uvs = 1.0f;

static void m34_ident(M34 *a)
{
    memset(a, 0, sizeof *a);
    a->m[0] = a->m[5] = a->m[10] = 1;
}
static void m34_mul(M34 *r, const M34 *a, const M34 *b)
{
    M34 o;
    int i, j;
    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++)
            o.m[i * 4 + j] = a->m[i * 4 + 0] * b->m[0 * 4 + j] + a->m[i * 4 + 1] * b->m[1 * 4 + j] + a->m[i * 4 + 2] * b->m[2 * 4 + j];
        o.m[i * 4 + 3] = a->m[i * 4 + 0] * b->m[3] + a->m[i * 4 + 1] * b->m[7] + a->m[i * 4 + 2] * b->m[11] + a->m[i * 4 + 3];
    }
    *r = o;
}
static vec3 m34_point(const M34 *a, vec3 p)
{
    return v3(a->m[0] * p.x + a->m[1] * p.y + a->m[2] * p.z + a->m[3],
              a->m[4] * p.x + a->m[5] * p.y + a->m[6] * p.z + a->m[7],
              a->m[8] * p.x + a->m[9] * p.y + a->m[10] * p.z + a->m[11]);
}
static vec3 m34_normal(const M34 *a, vec3 n)
{
    vec3 c0 = v3(a->m[0], a->m[4], a->m[8]);
    vec3 c1 = v3(a->m[1], a->m[5], a->m[9]);
    vec3 c2 = v3(a->m[2], a->m[6], a->m[10]);
    vec3 r = v3scale(v3cross(c1, c2), n.x);
    r = v3mad(r, v3cross(c2, c0), n.y);
    r = v3mad(r, v3cross(c0, c1), n.z);
    return v3norm(r);
}

void mb_begin(Mesh *m)
{
    mb_mesh = m;
    mb_sp = 0;
    m34_ident(&mb_stack[0]);
    mb_col[0] = mb_col[1] = mb_col[2] = mb_col[3] = 255;
    mb_curtex = TEX_NONE;
    mb_uvs = 1.0f;
}
void mb_end(void) { mb_mesh = NULL; }
void mb_push(void) { if (mb_sp < 31) { mb_stack[mb_sp + 1] = mb_stack[mb_sp]; mb_sp++; } }
void mb_pop(void) { if (mb_sp > 0) mb_sp--; }
void mb_identity(void) { m34_ident(&mb_stack[mb_sp]); }
void mb_translate(float x, float y, float z)
{
    M34 t; m34_ident(&t);
    t.m[3] = x; t.m[7] = y; t.m[11] = z;
    m34_mul(&mb_stack[mb_sp], &mb_stack[mb_sp], &t);
}
void mb_rotate(float deg, float x, float y, float z)
{
    M34 r;
    float a = RAD(deg), c = cosf(a), s = sinf(a), l = sqrtf(x * x + y * y + z * z);
    x /= l; y /= l; z /= l;
    m34_ident(&r);
    r.m[0] = x * x * (1 - c) + c;     r.m[1] = x * y * (1 - c) - z * s; r.m[2] = x * z * (1 - c) + y * s;
    r.m[4] = y * x * (1 - c) + z * s; r.m[5] = y * y * (1 - c) + c;     r.m[6] = y * z * (1 - c) - x * s;
    r.m[8] = x * z * (1 - c) - y * s; r.m[9] = y * z * (1 - c) + x * s; r.m[10] = z * z * (1 - c) + c;
    m34_mul(&mb_stack[mb_sp], &mb_stack[mb_sp], &r);
}
void mb_scale(float x, float y, float z)
{
    M34 t; m34_ident(&t);
    t.m[0] = x; t.m[5] = y; t.m[10] = z;
    m34_mul(&mb_stack[mb_sp], &mb_stack[mb_sp], &t);
}
void mb_color(float r, float g, float b) { mb_color4(r, g, b, 1); }
void mb_color4(float r, float g, float b, float a)
{
    mb_col[0] = (unsigned char)(clampf(r, 0, 1) * 255);
    mb_col[1] = (unsigned char)(clampf(g, 0, 1) * 255);
    mb_col[2] = (unsigned char)(clampf(b, 0, 1) * 255);
    mb_col[3] = (unsigned char)(clampf(a, 0, 1) * 255);
}
void mb_tex(int tex) { mb_curtex = tex; }
void mb_uvscale(float s) { mb_uvs = s; }

static void mk_vtx(Vtx *v, vec3 p, vec3 n)
{
    float ax = fabsf(n.x), ay = fabsf(n.y), az = fabsf(n.z);
    v->p[0] = p.x; v->p[1] = p.y; v->p[2] = p.z;
    v->n[0] = n.x; v->n[1] = n.y; v->n[2] = n.z;
    if (ay >= ax && ay >= az) { v->uv[0] = p.x * mb_uvs; v->uv[1] = p.z * mb_uvs; }
    else if (ax >= az) { v->uv[0] = p.z * mb_uvs; v->uv[1] = -p.y * mb_uvs; }
    else { v->uv[0] = p.x * mb_uvs; v->uv[1] = -p.y * mb_uvs; }
    memcpy(v->c, mb_col, 4);
}

void mb_tri_n(vec3 a, vec3 b, vec3 c, vec3 na, vec3 nb, vec3 nc)
{
    Vtx va, vb, vc;
    M34 *m = &mb_stack[mb_sp];
    mk_vtx(&va, m34_point(m, a), m34_normal(m, na));
    mk_vtx(&vb, m34_point(m, b), m34_normal(m, nb));
    mk_vtx(&vc, m34_point(m, c), m34_normal(m, nc));
    mesh_add(mb_mesh, mb_curtex, &va, &vb, &vc);
}

void mb_tri(vec3 a, vec3 b, vec3 c)
{
    vec3 n = v3norm(v3cross(v3sub(b, a), v3sub(c, a)));
    mb_tri_n(a, b, c, n, n, n);
}

void mb_quad(vec3 a, vec3 b, vec3 c, vec3 d)
{
    mb_tri(a, b, c);
    mb_tri(a, c, d);
}

static vec3 sph(float th, float ph) { return v3(cosf(ph) * sinf(th), sinf(ph), cosf(ph) * cosf(th)); }

void mb_sphere(int sl, int st)
{
    int i, j;
    for (j = 0; j < st; j++) {
        float p0 = -PI_F / 2 + PI_F * j / st, p1 = -PI_F / 2 + PI_F * (j + 1) / st;
        for (i = 0; i < sl; i++) {
            float t0 = TAU_F * i / sl, t1 = TAU_F * (i + 1) / sl;
            vec3 a = sph(t0, p0), b = sph(t1, p0), c = sph(t1, p1), d = sph(t0, p1);
            if (j > 0) mb_tri_n(a, b, c, a, b, c);
            if (j < st - 1) mb_tri_n(a, c, d, a, c, d);
        }
    }
}

void mb_hemisphere(int sl, int st)
{
    int i, j;
    for (j = 0; j < st; j++) {
        float p0 = PI_F / 2 * j / st, p1 = PI_F / 2 * (j + 1) / st;
        for (i = 0; i < sl; i++) {
            float t0 = TAU_F * i / sl, t1 = TAU_F * (i + 1) / sl;
            vec3 a = sph(t0, p0), b = sph(t1, p0), c = sph(t1, p1), d = sph(t0, p1);
            mb_tri_n(a, b, c, a, b, c);
            if (j < st - 1) mb_tri_n(a, c, d, a, c, d);
        }
    }
}

void mb_cylinder(int sl, int caps)
{
    int i;
    for (i = 0; i < sl; i++) {
        float t0 = TAU_F * i / sl, t1 = TAU_F * (i + 1) / sl;
        vec3 n0 = v3(sinf(t0), 0, cosf(t0)), n1 = v3(sinf(t1), 0, cosf(t1));
        vec3 a = n0, b = n1, c = v3(n1.x, 1, n1.z), d = v3(n0.x, 1, n0.z);
        mb_tri_n(a, b, c, n0, n1, n1);
        mb_tri_n(a, c, d, n0, n1, n0);
        if (caps) {
            mb_tri(v3(0, 1, 0), d, c);
            mb_tri(v3(0, 0, 0), b, a);
        }
    }
}

void mb_cone(int sl)
{
    int i;
    for (i = 0; i < sl; i++) {
        float t0 = TAU_F * i / sl, t1 = TAU_F * (i + 1) / sl;
        vec3 a = v3(sinf(t0), 0, cosf(t0)), b = v3(sinf(t1), 0, cosf(t1));
        vec3 na = v3norm(v3(a.x, 0.7f, a.z)), nb = v3norm(v3(b.x, 0.7f, b.z));
        vec3 apex = v3(0, 1, 0), nm = v3norm(v3add(na, nb));
        mb_tri_n(a, b, apex, na, nb, nm);
        mb_tri(v3(0, 0, 0), b, a);
    }
}

void mb_box(void)
{
    vec3 p[8];
    int i;
    for (i = 0; i < 8; i++) p[i] = v3((i & 1) ? 1 : -1, (i & 2) ? 1 : -1, (i & 4) ? 1 : -1);
    mb_quad(p[0], p[4], p[6], p[2]); /* -x */
    mb_quad(p[1], p[3], p[7], p[5]); /* +x */
    mb_quad(p[0], p[1], p[5], p[4]); /* -y */
    mb_quad(p[2], p[6], p[7], p[3]); /* +y */
    mb_quad(p[0], p[2], p[3], p[1]); /* -z */
    mb_quad(p[4], p[5], p[7], p[6]); /* +z */
}

void mb_torus(float R, float r, int seg, int seg2)
{
    int i, j;
    for (i = 0; i < seg; i++) {
        float a0 = TAU_F * i / seg, a1 = TAU_F * (i + 1) / seg;
        for (j = 0; j < seg2; j++) {
            float b0 = TAU_F * j / seg2, b1 = TAU_F * (j + 1) / seg2;
            vec3 n00 = v3(cosf(b0) * sinf(a0), sinf(b0), cosf(b0) * cosf(a0));
            vec3 n10 = v3(cosf(b0) * sinf(a1), sinf(b0), cosf(b0) * cosf(a1));
            vec3 n11 = v3(cosf(b1) * sinf(a1), sinf(b1), cosf(b1) * cosf(a1));
            vec3 n01 = v3(cosf(b1) * sinf(a0), sinf(b1), cosf(b1) * cosf(a0));
            vec3 p00 = v3add(v3(R * sinf(a0), 0, R * cosf(a0)), v3scale(n00, r));
            vec3 p10 = v3add(v3(R * sinf(a1), 0, R * cosf(a1)), v3scale(n10, r));
            vec3 p11 = v3add(v3(R * sinf(a1), 0, R * cosf(a1)), v3scale(n11, r));
            vec3 p01 = v3add(v3(R * sinf(a0), 0, R * cosf(a0)), v3scale(n01, r));
            mb_tri_n(p00, p10, p11, n00, n10, n11);
            mb_tri_n(p00, p11, p01, n00, n11, n01);
        }
    }
}

void mb_disc(int sl)
{
    int i;
    for (i = 0; i < sl; i++) {
        float t0 = TAU_F * i / sl, t1 = TAU_F * (i + 1) / sl;
        mb_tri(v3(0, 0, 0), v3(sinf(t0), 0, cosf(t0)), v3(sinf(t1), 0, cosf(t1)));
    }
}

void mb_star(float inner, float depth)
{
    vec3 pts[10];
    int i;
    for (i = 0; i < 10; i++) {
        float a = PI_F / 2 + TAU_F * i / 10.0f;
        float r = (i & 1) ? inner : 1.0f;
        pts[i] = v3(cosf(a) * r, sinf(a) * r, 0);
    }
    for (i = 0; i < 10; i++) {
        vec3 a = pts[i], b = pts[(i + 1) % 10];
        vec3 f = v3(0, 0, depth), bk = v3(0, 0, -depth);
        mb_tri(f, a, b);
        mb_tri(bk, b, a);
    }
}

void mb_ellipsoid(float x, float y, float z, float rx, float ry, float rz, int seg)
{
    mb_push();
    mb_translate(x, y, z);
    mb_scale(rx, ry, rz);
    mb_sphere(seg, seg / 2 + 1);
    mb_pop();
}

void mb_cuboid(float x0, float y0, float z0, float x1, float y1, float z1)
{
    mb_push();
    mb_translate((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f);
    mb_scale((x1 - x0) * 0.5f, (y1 - y0) * 0.5f, (z1 - z0) * 0.5f);
    mb_box();
    mb_pop();
}

/* ======================================================================
 * Procedural textures
 * ====================================================================== */
#define TS 64
static unsigned char texbuf[256 * 256 * 4];

static float hash2(int x, int y, int seed)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

static float vnoise(float x, float y, int period, int seed)
{
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float fx = x - xi, fy = y - yi;
    int x0 = ((xi % period) + period) % period, y0 = ((yi % period) + period) % period;
    int x1 = (x0 + 1) % period, y1 = (y0 + 1) % period;
    float a = hash2(x0, y0, seed), b = hash2(x1, y0, seed), c = hash2(x0, y1, seed), d = hash2(x1, y1, seed);
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}

/* u,v in 0..1, tileable */
static float fbm(float u, float v, int base, int oct, int seed)
{
    float sum = 0, amp = 0.5f, tot = 0;
    int i, p = base;
    for (i = 0; i < oct; i++) {
        sum += vnoise(u * p, v * p, p, seed + i * 17) * amp;
        tot += amp;
        amp *= 0.5f;
        p *= 2;
    }
    return sum / tot;
}

static void put(int w, int x, int y, float r, float g, float b, float a)
{
    unsigned char *p = &texbuf[(y * w + x) * 4];
    p[0] = (unsigned char)(clampf(r, 0, 1) * 255);
    p[1] = (unsigned char)(clampf(g, 0, 1) * 255);
    p[2] = (unsigned char)(clampf(b, 0, 1) * 255);
    p[3] = (unsigned char)(clampf(a, 0, 1) * 255);
}

static void upload(int id, int w, int h, int mip, int nearest)
{
    glBindTexture(GL_TEXTURE_2D, g_tex[id]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, nearest ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mip ? GL_LINEAR_MIPMAP_LINEAR : (nearest ? GL_NEAREST : GL_LINEAR));
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texbuf);
    if (mip) {
        int level = 0;
        static unsigned char tmp[256 * 256 * 4];
        while (w > 1 || h > 1) {
            int nw = w > 1 ? w / 2 : 1, nh = h > 1 ? h / 2 : 1, x, y, c;
            for (y = 0; y < nh; y++)
                for (x = 0; x < nw; x++)
                    for (c = 0; c < 4; c++) {
                        int x0 = x * 2, y0 = y * 2, x1 = (w > 1) ? x0 + 1 : x0, y1 = (h > 1) ? y0 + 1 : y0;
                        int s = texbuf[(y0 * w + x0) * 4 + c] + texbuf[(y0 * w + x1) * 4 + c] +
                                texbuf[(y1 * w + x0) * 4 + c] + texbuf[(y1 * w + x1) * 4 + c];
                        tmp[(y * nw + x) * 4 + c] = (unsigned char)(s / 4);
                    }
            memcpy(texbuf, tmp, (size_t)nw * nh * 4);
            w = nw; h = nh;
            level++;
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, texbuf);
        }
    }
}

static const unsigned char font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},{0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x01,0x01},{0x3E,0x41,0x41,0x51,0x32},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40},{0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x7F,0x20,0x18,0x20,0x7F},
    {0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x08,0x14,0x54,0x54,0x3C},
    {0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},{0x00,0x7F,0x10,0x28,0x44},
    {0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x08,0x08,0x2A,0x1C,0x08},{0x00,0x00,0x00,0x00,0x00}
};

static void gen_font(void)
{
    int c, x, y;
    memset(texbuf, 0, 128 * 64 * 4);
    for (c = 0; c < 96; c++) {
        int cx = (c % 16) * 8, cy = (c / 16) * 8;
        for (x = 0; x < 5; x++)
            for (y = 0; y < 7; y++)
                if (font5x7[c][x] & (1 << y)) put(128, cx + x + 1, cy + y, 1, 1, 1, 1);
    }
    upload(TEX_FONT, 128, 64, 0, 1);
}

/* little helpers for drawing pictures into texbuf */
static void pic_fill_tri(int w, float ax, float ay, float bx, float by, float cx, float cy, float r, float g, float b)
{
    int x, y;
    for (y = 0; y < w; y++)
        for (x = 0; x < w; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
            float d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
            float d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
            int neg = (d1 < 0) || (d2 < 0) || (d3 < 0), pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
            if (!(neg && pos)) put(w, x, y, r, g, b, 1);
        }
}
static void pic_circle(int w, float cx, float cy, float rad, float r, float g, float b)
{
    int x, y;
    for (y = 0; y < w; y++)
        for (x = 0; x < w; x++)
            if ((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy) < rad * rad) put(w, x, y, r, g, b, 1);
}
static void pic_rect(int w, int x0, int y0, int x1, int y1, float r, float g, float b)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            if (x >= 0 && y >= 0 && x < w && y < w) put(w, x, y, r, g, b, 1);
}
static void pic_frame(int w, float r, float g, float b)
{
    pic_rect(w, 0, 0, w, 3, r, g, b);
    pic_rect(w, 0, w - 3, w, w, r, g, b);
    pic_rect(w, 0, 0, 3, w, r, g, b);
    pic_rect(w, w - 3, 0, w, w, r, g, b);
}

static void gen_textures(void)
{
    int x, y, t;
    float u, v, n, n2;
    for (t = 1; t < TEX_COUNT; t++) {
        int w = TS, mip = 1;
        if (t == TEX_FONT) { gen_font(); continue; }
        for (y = 0; y < w; y++)
            for (x = 0; x < w; x++) {
                u = (float)x / w; v = (float)y / w;
                n = fbm(u, v, 4, 4, t * 31);
                n2 = hash2(x, y, t * 7);
                switch (t) {
                case TEX_GRASS: {   /* fresh powder snow (sparkly) */
                    float k = 0.86f + 0.14f * n + 0.04f * n2;
                    float sp = n2 > 0.985f ? 0.25f : 0;
                    put(w, x, y, 0.9f * k + sp, 0.94f * k + sp, 1.0f * k + sp, 1);
                    break;
                }
                case TEX_SNOW: {
                    float k = 0.9f + 0.1f * n + 0.05f * n2;
                    put(w, x, y, 0.93f * k, 0.96f * k, 1.0f * k, 1);
                    break;
                }
                case TEX_ROCK: {
                    float c = fbm(u, v, 8, 3, 99);
                    float k = 0.55f + 0.4f * n - (fabsf(c - 0.5f) < 0.03f ? 0.25f : 0) + 0.06f * n2;
                    put(w, x, y, 0.62f * k, 0.58f * k, 0.55f * k, 1);
                    break;
                }
                case TEX_BRICK: case TEX_STONE: {   /* frosted stone blocks, snow in the joints */
                    int bh = t == TEX_BRICK ? 8 : 16, bw = t == TEX_BRICK ? 16 : 32;
                    int row = y / bh, off = (row & 1) ? bw / 2 : 0;
                    int mx = (x + off) % bw, my = y % bh;
                    int mortar = mx < 1 || my < 1;
                    float k = 0.8f + 0.25f * n + 0.08f * hash2((x + off) / bw, row, 5);
                    float frost = my < 2 ? 0.12f : 0;
                    if (mortar) put(w, x, y, 0.92f, 0.95f, 1.0f, 1);
                    else if (t == TEX_BRICK) put(w, x, y, 0.72f * k + frost, 0.78f * k + frost, 0.88f * k + frost, 1);
                    else put(w, x, y, 0.68f * k + frost, 0.72f * k + frost, 0.8f * k + frost, 1);
                    break;
                }
                case TEX_WOOD: {
                    float s = sinf((v * 8 + n * 3) * TAU_F) * 0.5f + 0.5f;
                    float k = 0.7f + 0.2f * s + 0.1f * n2;
                    put(w, x, y, 0.62f * k, 0.42f * k, 0.24f * k, 1);
                    break;
                }
                case TEX_PLANK: {
                    int py = y % 16;
                    float s = sinf((u * 6 + n * 2) * TAU_F) * 0.5f + 0.5f;
                    float k = 0.75f + 0.15f * s + 0.1f * hash2(0, y / 16, 3);
                    if (py == 0) k *= 0.5f;
                    put(w, x, y, 0.7f * k, 0.5f * k, 0.3f * k, 1);
                    break;
                }
                case TEX_SAND: {   /* wind-packed snow with ripples (firn) */
                    float rip = sinf((v * 10 + n * 2.5f) * TAU_F) * 0.5f + 0.5f;
                    float k = 0.82f + 0.08f * n + 0.06f * rip + 0.03f * n2;
                    put(w, x, y, 0.86f * k, 0.9f * k, 0.98f * k, 1);
                    break;
                }
                case TEX_ICE: {
                    float s = fbm(u + v * 0.3f, v, 2, 4, 7);
                    float k = 0.8f + 0.25f * s;
                    put(w, x, y, 0.7f * k, 0.88f * k, 1.0f * k, 1);
                    break;
                }
                case TEX_SLIDE: {
                    float s = (sinf(u * TAU_F * 2) * 0.5f + 0.5f);
                    float k = 0.85f + 0.15f * n;
                    put(w, x, y, (0.6f + 0.3f * s) * k, (0.8f + 0.15f * s) * k, 1.0f * k, 1);
                    break;
                }
                case TEX_WATER: {
                    float c = fbm(u, v, 4, 3, 3);
                    float l = fabsf(c - 0.5f) < 0.04f ? 0.25f : 0;
                    put(w, x, y, 0.25f + l, 0.55f + l, 0.95f, 0.65f);
                    break;
                }
                case TEX_METAL: {
                    int px = x % 32, py = y % 32;
                    float k = 0.75f + 0.15f * n;
                    if (px == 0 || py == 0) k *= 0.6f;
                    if ((px - 4) * (px - 4) + (py - 4) * (py - 4) < 4) k *= 1.3f;
                    put(w, x, y, 0.6f * k, 0.63f * k, 0.68f * k, 1);
                    break;
                }
                case TEX_CIRCUIT: {
                    float k = 0.8f + 0.2f * n;
                    int trace = ((x % 16) == 7 && hash2(x / 16, y / 8, 1) > 0.4f) ||
                                ((y % 16) == 9 && hash2(x / 8, y / 16, 2) > 0.4f);
                    int pad = ((x % 16) >= 6 && (x % 16) <= 8 && (y % 16) >= 8 && (y % 16) <= 10);
                    if (pad) put(w, x, y, 0.95f, 0.8f, 0.3f, 1);
                    else if (trace) put(w, x, y, 0.75f * k, 0.6f * k, 0.25f * k, 1);
                    else put(w, x, y, 0.1f * k, 0.42f * k, 0.22f * k, 1);
                    break;
                }
                case TEX_LAVA: {   /* frozen abyss: dark ice with glowing cracks */
                    float c = fbm(u, v, 4, 4, 11);
                    float crack = fabsf(c - 0.5f) < 0.03f ? 1.0f : 0;
                    put(w, x, y, 0.05f + 0.35f * crack, 0.12f + 0.15f * c + 0.6f * crack, 0.3f + 0.2f * c + 0.7f * crack, 1);
                    break;
                }
                case TEX_CARPET: {
                    int bx = x % 32, by = y % 32;
                    int border = bx < 3 || by < 3;
                    float k = 0.85f + 0.15f * n2;
                    if (border) put(w, x, y, 0.95f, 0.78f, 0.25f, 1);
                    else put(w, x, y, 0.72f * k, 0.12f * k, 0.15f * k, 1);
                    break;
                }
                case TEX_TILE: {
                    int ch = ((x / 32) + (y / 32)) & 1;
                    float k = 0.85f + 0.15f * n;
                    if (ch) put(w, x, y, 0.94f * k, 0.97f * k, 1.0f * k, 1);
                    else put(w, x, y, 0.55f * k, 0.72f * k, 0.9f * k, 1);
                    break;
                }
                case TEX_DIRT: {   /* trodden snow path */
                    float k = 0.72f + 0.2f * n + 0.08f * n2;
                    put(w, x, y, 0.72f * k, 0.76f * k, 0.86f * k, 1);
                    break;
                }
                case TEX_ROOF: {
                    int row = y / 8, off = (row & 1) ? 4 : 0;
                    int mx = (x + off) % 8, my = y % 8;
                    float k = 0.75f + 0.25f * (my / 8.0f);
                    if (mx == 0) k *= 0.7f;
                    if (my < 3 && hash2(x / 3, row, 8) > 0.35f) put(w, x, y, 0.95f, 0.97f, 1.0f, 1);   /* snow on tiles */
                    else put(w, x, y, 0.8f * k, 0.2f * k, 0.15f * k, 1);
                    break;
                }
                case TEX_HEX: {
                    float k = 0.5f + 0.2f * n;
                    float gx = fmodf(x + (((y / 8) & 1) ? 4 : 0), 8.0f), gy = (float)(y % 8);
                    int line = gx < 1 || gy < 1;
                    if (line) put(w, x, y, 0.4f, 0.85f, 1.0f, 1);
                    else put(w, x, y, 0.12f * k, 0.18f * k, 0.3f * k, 1);
                    break;
                }
                case TEX_DARK: {
                    float k = 0.4f + 0.4f * n;
                    int glitch = hash2(0, y / 2, 9) > 0.92f;
                    if (glitch) put(w, x, y, 0.3f + 0.3f * n2, 0.7f, 1.0f, 1);
                    else put(w, x, y, 0.15f * k, 0.2f * k, 0.32f * k, 1);
                    break;
                }
                case TEX_CYGRID: {   /* dark floor with a glowing grid */
                    int gx = x % 16, gy = y % 16;
                    float line = (gx == 0 || gy == 0) ? 1.0f : ((gx == 8 || gy == 8) ? 0.35f : 0);
                    float k = 0.06f + 0.04f * n;
                    put(w, x, y, k * 0.5f + 0.15f * line, k * 0.8f + 0.85f * line, k * 1.6f + 1.0f * line, 1);
                    break;
                }
                case TEX_CYWALL: {   /* dark panels, thin bright seams, circuit traces and status leds */
                    int px = x % 32, py = y % 32;
                    int seam = px == 0 || py == 0;
                    int trace = (py == 12 && px > 4 && px < 28 && hash2(x / 32, y / 32, 4) > 0.4f) ||
                                (px == 20 && py > 12 && py < 28 && hash2(x / 32, y / 32, 5) > 0.5f);
                    int led = (px >= 4 && px <= 5 && py >= 4 && py <= 5);
                    float k = 0.07f + 0.05f * n;
                    if (seam) put(w, x, y, 0.1f, 0.55f, 0.85f, 1);
                    else if (led) put(w, x, y, hash2(x / 32, y / 32, 6) > 0.5f ? 1.0f : 0.2f, 1.0f, 0.4f, 1);
                    else if (trace) put(w, x, y, 0.15f, 0.7f, 1.0f, 1);
                    else put(w, x, y, k * 0.7f, k * 0.9f, k * 1.7f, 1);
                    break;
                }
                case TEX_CYDATA: {   /* columns of glowing bits */
                    int col = x / 4;
                    float on = hash2(col, y / 3, 12) > 0.55f ? 1.0f : 0.0f;
                    float fade = 0.4f + 0.6f * hash2(col, 0, 13);
                    if (x % 4 == 3) on = 0;
                    put(w, x, y, 0.05f + 0.1f * on * fade, 0.1f + 0.85f * on * fade, 0.25f + 0.75f * on * fade, 1);
                    break;
                }
                case TEX_SHADOW: case TEX_GLOW: {
                    float dx = u - 0.5f, dy = v - 0.5f, d = sqrtf(dx * dx + dy * dy) * 2;
                    float a = clampf(1 - d, 0, 1);
                    if (t == TEX_SHADOW) put(w, x, y, 0, 0, 0, clampf(a * 2.2f, 0, 1));
                    else put(w, x, y, 1, 1, 1, a * a);
                    break;
                }
                case TEX_CLOUD: {
                    float dx = u - 0.5f, dy = v - 0.5f, d = sqrtf(dx * dx + dy * dy) * 2;
                    float a = clampf((1 - d) * 1.6f * (0.6f + 0.6f * fbm(u, v, 4, 3, 77)), 0, 1);
                    put(w, x, y, 1, 1, 1, a);
                    break;
                }
                default:
                    put(w, x, y, 1, 1, 1, 1);
                }
            }
        /* course pictures */
        if (t >= TEX_PORTAL1 && t <= TEX_PORTAL12) {
            mip = 1;
            for (y = 0; y < w; y++)
                for (x = 0; x < w; x++) {
                    v = (float)y / w;
                    switch (t) {
                    case TEX_PORTAL1: put(w, x, y, 0.45f + 0.3f * v, 0.7f + 0.2f * v, 1.0f, 1); break;
                    case TEX_PORTAL2: put(w, x, y, 0.6f + 0.3f * v, 0.8f + 0.1f * v, 1.0f, 1); break;
                    case TEX_PORTAL3: put(w, x, y, 0.55f + 0.2f * v, 0.6f + 0.2f * v, 0.7f + 0.2f * v, 1); break;
                    case TEX_PORTAL4: put(w, x, y, 0.05f, 0.1f + 0.2f * v, 0.25f + 0.3f * v, 1); break;
                    case TEX_PORTAL6: put(w, x, y, 0.6f + 0.25f * v, 0.75f + 0.15f * v, 0.95f, 1); break;
                    case TEX_PORTAL7: put(w, x, y, 0.12f + 0.1f * v, 0.06f + 0.06f * v, 0.2f + 0.15f * v, 1); break;
                    case TEX_PORTAL8: put(w, x, y, 0.35f + 0.4f * v, 0.55f + 0.3f * v, 0.95f, 1); break;
                    case TEX_PORTAL9: put(w, x, y, 0.88f, 0.9f, 0.95f, 1); break;
                    case TEX_PORTAL11: put(w, x, y, 0.3f + 0.45f * v, 0.5f + 0.4f * v, 0.95f, 1); break;
                    case TEX_PORTAL12: put(w, x, y, 0.02f + 0.06f * v, 0.01f + 0.03f * v, 0.08f + 0.12f * v, 1); break;
                    case TEX_PORTAL10: put(w, x, y, 0.04f + 0.1f * v, 0.06f + 0.2f * v, 0.2f + 0.3f * v, 1); break;
                    default: put(w, x, y, 0.02f, 0.05f + 0.15f * v, 0.15f + 0.3f * v, 1); break;
                    }
                }
            if (t == TEX_PORTAL1) {
                pic_circle(w, 50, 12, 6, 1, 0.97f, 0.8f);
                pic_fill_tri(w, 8, 64, 34, 14, 60, 64, 0.62f, 0.7f, 0.82f);
                pic_fill_tri(w, 22, 37, 34, 14, 46, 37, 0.97f, 0.98f, 1);
                pic_rect(w, 0, 50, 64, 64, 0.92f, 0.95f, 1.0f);
                pic_circle(w, 14, 52, 4, 0.1f, 0.1f, 0.12f);
                pic_circle(w, 14, 54, 2.5f, 1, 1, 1);
                pic_circle(w, 14, 47, 3, 0.1f, 0.1f, 0.12f);
                pic_rect(w, 14, 47, 18, 48, 1, 0.6f, 0.1f);
            } else if (t == TEX_PORTAL2) {
                for (y = 36; y < 64; y++)
                    for (x = 0; x < 64; x++) {
                        float wv = sinf(x * 0.4f + y * 0.9f) * 0.08f;
                        put(w, x, y, 0.1f + wv, 0.4f + wv, 0.75f + wv, 1);
                    }
                pic_rect(w, 40, 14, 48, 40, 0.95f, 0.95f, 0.95f);
                pic_rect(w, 40, 20, 48, 24, 0.9f, 0.2f, 0.2f);
                pic_rect(w, 40, 30, 48, 34, 0.9f, 0.2f, 0.2f);
                pic_fill_tri(w, 38, 14, 44, 6, 50, 14, 0.9f, 0.2f, 0.2f);
                pic_circle(w, 44, 11, 2, 1, 1, 0.5f);
                pic_fill_tri(w, 30, 40, 44, 34, 58, 40, 0.9f, 0.94f, 1.0f);
                pic_fill_tri(w, 4, 40, 14, 22, 24, 40, 0.8f, 0.92f, 1.0f);   /* iceberg */
                pic_fill_tri(w, 10, 40, 14, 22, 24, 40, 0.65f, 0.82f, 0.95f);
            } else if (t == TEX_PORTAL3) {
                int i;
                for (i = 0; i < 40; i++) pic_circle(w, hash2(i, 1, 3) * 64, hash2(i, 2, 3) * 40, 0.8f, 1, 1, 1);
                pic_rect(w, 0, 52, 64, 64, 0.9f, 0.95f, 1);
                pic_rect(w, 14, 26, 50, 52, 0.45f, 0.5f, 0.6f);
                pic_rect(w, 26, 12, 38, 26, 0.45f, 0.5f, 0.6f);
                for (i = 0; i < 5; i++) pic_rect(w, 14 + i * 8, 22, 18 + i * 8, 26, 0.45f, 0.5f, 0.6f);
                pic_rect(w, 29, 40, 35, 52, 0.15f, 0.15f, 0.2f);
                pic_fill_tri(w, 24, 12, 32, 4, 40, 12, 0.7f, 0.85f, 1);
                for (i = 0; i < 70; i++) {   /* blizzard streaks */
                    float sx = hash2(i, 7, 5) * 64, sy = hash2(i, 8, 5) * 64;
                    int k;
                    for (k = 0; k < 6; k++) {
                        int px = (int)(sx + k), py = (int)(sy + k * 0.4f);
                        if (px >= 0 && px < 64 && py >= 0 && py < 64) put(w, px, py, 0.95f, 0.97f, 1, 1);
                    }
                }
            } else if (t == TEX_PORTAL4) {
                for (y = 0; y < 64; y++)
                    for (x = 0; x < 64; x++) {
                        float dx = x - 32.0f, dy = y - 32.0f, r = sqrtf(dx * dx + dy * dy), a = atan2f(dy, dx);
                        float s = sinf(a * 3 + r * 0.35f);
                        if (s > 0.3f) put(w, x, y, 0.6f + 0.3f * s, 0.85f, 1, 1);
                    }
            } else if (t == TEX_PORTAL5) {
                for (y = 0; y < 64; y++)
                    for (x = 0; x < 64; x++) {
                        if (hash2(x / 4, y, 4) > 0.93f) put(w, x, y, 0.4f, 0.9f, 1, 1);
                    }
                pic_circle(w, 32, 32, 14, 0.85f, 0.1f, 0.1f);
                pic_circle(w, 26, 28, 3, 1, 1, 0.3f);
                pic_circle(w, 38, 28, 3, 1, 1, 0.3f);
                pic_rect(w, 24, 37, 40, 40, 0.1f, 0, 0);
            }
            if (t == TEX_PORTAL6) {
                int i;
                pic_circle(w, 50, 12, 7, 1, 1, 0.9f);
                for (i = 0; i < 6; i++) pic_rect(w, 14 + i * 3, 44 - i * 5, 50 - i * 3, 49 - i * 5, 0.6f + i * 0.05f, 0.8f + i * 0.03f, 1.0f);
                for (y = 48; y < 64; y++)
                    for (x = 0; x < 64; x++) put(w, x, y, 0.9f, 0.94f + 0.04f * sinf(x * 0.3f), 1.0f, 1);
                pic_fill_tri(w, 3, 48, 7, 30, 11, 48, 0.55f, 0.85f, 1.0f);   /* ice crystals */
                pic_fill_tri(w, 54, 48, 57, 36, 60, 48, 0.55f, 0.85f, 1.0f);
            } else if (t == TEX_PORTAL7) {
                int i;
                pic_circle(w, 48, 13, 7, 0.95f, 0.95f, 0.8f);
                pic_circle(w, 51, 11, 6, 0.12f, 0.06f, 0.22f);
                pic_rect(w, 0, 52, 64, 64, 0.82f, 0.86f, 0.95f);
                pic_rect(w, 12, 26, 44, 52, 0.25f, 0.2f, 0.3f);
                pic_fill_tri(w, 10, 27, 28, 12, 46, 27, 0.85f, 0.9f, 1.0f);
                for (i = 0; i < 30; i++) pic_circle(w, hash2(i, 3, 9) * 64, hash2(i, 4, 9) * 50, 0.7f, 1, 1, 1);
                for (i = 0; i < 3; i++) pic_rect(w, 16 + i * 9, 32, 21 + i * 9, 38, 1, 0.85f, 0.3f);
                pic_rect(w, 25, 42, 31, 52, 0.05f, 0.05f, 0.05f);
                pic_circle(w, 52, 38, 4, 0.9f, 0.95f, 1);
                pic_circle(w, 51, 37, 1, 0, 0, 0);
                pic_circle(w, 54, 37, 1, 0, 0, 0);
            }
            if (t == TEX_PORTAL8) {   /* zig-zag tower of ice chimneys */
                int i;
                for (i = 0; i < 6; i++) {
                    int cx = (i & 1) ? 38 : 26, y0 = 58 - i * 9;
                    pic_rect(w, cx - 7, y0 - 9, cx - 5, y0, 0.75f, 0.9f, 1.0f);
                    pic_rect(w, cx + 5, y0 - 9, cx + 7, y0, 0.75f, 0.9f, 1.0f);
                    pic_rect(w, (i & 1) ? cx - 14 : cx + 5, y0 - 10, (i & 1) ? cx - 5 : cx + 14, y0 - 9, 1, 1, 1);
                }
                pic_rect(w, 0, 58, 64, 64, 0.95f, 0.97f, 1);
                pic_circle(w, 32, 41, 2.2f, 0.1f, 0.1f, 0.12f);   /* tiny Tux kicking */
                pic_circle(w, 32, 42, 1.3f, 1, 1, 1);
            } else if (t == TEX_PORTAL9) {   /* a maze seen from above, with red eyes */
                int i, j;
                for (i = 0; i <= 8; i++) {
                    pic_rect(w, 4 + i * 7, 4, 5 + i * 7, 60, 0.55f, 0.75f, 0.95f);
                    for (j = 0; j < 8; j++)
                        if (hash2(i, j, 21) > 0.45f) pic_rect(w, 4 + j * 7, 4 + i * 7, 12 + j * 7, 5 + i * 7, 0.55f, 0.75f, 0.95f);
                }
                for (i = 0; i < 8; i++) if (hash2(i, 9, 3) > 0.5f) pic_rect(w, 4 + i * 7, 4 + 3 * 7 + 3, 12 + i * 7, 4 + 3 * 7 + 4, 0.88f, 0.9f, 0.95f);
                pic_circle(w, 41, 30, 2, 1, 0.1f, 0.1f);
                pic_circle(w, 46, 30, 2, 1, 0.1f, 0.1f);
            }
            if (t == TEX_PORTAL10) {   /* aurora over an icy sea: a whale, a ramp and a speeding Tux */
                int i;
                for (y = 4; y < 30; y++)
                    for (x = 0; x < 64; x++) {
                        float band = sinf(x * 0.15f + y * 0.05f) * 6 + 14 - y;
                        float a = clampf(1.0f - fabsf(band) / 5.0f, 0, 1) * 0.8f;
                        if (a > 0.05f) put(w, x, y, 0.2f * a + 0.04f, 0.4f + 0.55f * a, 0.35f + 0.3f * a, 1);
                    }
                for (i = 0; i < 25; i++) pic_circle(w, hash2(i, 5, 7) * 64, hash2(i, 6, 7) * 30, 0.6f, 1, 1, 1);
                for (y = 38; y < 64; y++)
                    for (x = 0; x < 64; x++) {
                        float wv = sinf(x * 0.5f + y * 1.3f) * 0.06f;
                        put(w, x, y, 0.08f + wv, 0.25f + wv, 0.5f + wv, 1);
                    }
                pic_circle(w, 18, 40, 9, 0.2f, 0.25f, 0.38f);           /* whale back */
                pic_rect(w, 6, 40, 32, 46, 0.08f, 0.25f, 0.5f);
                pic_rect(w, 17, 26, 19, 32, 0.8f, 0.92f, 1.0f);          /* spout */
                pic_circle(w, 18, 26, 3, 0.85f, 0.95f, 1.0f);
                pic_fill_tri(w, 40, 46, 56, 46, 56, 38, 0.75f, 0.9f, 1.0f);   /* ice ramp */
                pic_circle(w, 46, 33, 2.4f, 0.1f, 0.1f, 0.12f);          /* Tux in the air */
                pic_circle(w, 46, 34, 1.4f, 1, 1, 1);
                pic_rect(w, 44, 35, 48, 36, 1, 0.5f, 0.1f);
                for (i = 0; i < 5; i++) pic_rect(w, 34 + i * 2, 48 + (i & 1), 36 + i * 2, 49 + (i & 1), 0.9f, 0.97f, 1);
            }
            if (t == TEX_PORTAL11) {   /* clouds, a looping trail and fish in the sky */
                int i;
                for (i = 0; i < 9; i++) pic_circle(w, 4 + i * 7, 56 + (i & 1) * 2, 7, 1, 1, 1);
                for (i = 0; i < 40; i++) {
                    float a = TAU_F * i / 40;
                    pic_circle(w, 32 + cosf(a) * 13, 28 + sinf(a) * 10, 1.0f, 0.6f, 0.95f, 1.0f);
                }
                for (i = 0; i < 6; i++) pic_circle(w, 26 + (i % 3) * 6, 25 + (i / 3) * 6, 1.6f, 1.0f, 0.82f, 0.15f);
                pic_circle(w, 46, 18, 2.4f, 0.1f, 0.1f, 0.12f);   /* flying Tux with his rocket */
                pic_circle(w, 46, 19, 1.4f, 1, 1, 1);
                pic_rect(w, 42, 20, 44, 22, 1.0f, 0.5f, 0.1f);
                pic_circle(w, 10, 10, 1.2f, 1.0f, 0.82f, 0.15f);
                pic_circle(w, 56, 38, 1.2f, 1.0f, 0.82f, 0.15f);
            }
            if (t == TEX_PORTAL12) {   /* stars, a ringed ice planet, a comet and Tux on his rocket */
                int i;
                for (i = 0; i < 40; i++) pic_circle(w, (i * 37 + 11) % 64, (i * 23 + 5) % 64, 0.6f, 0.9f, 0.9f, 1.0f);
                for (i = 0; i < 48; i++) {
                    float a = TAU_F * i / 48;
                    pic_circle(w, 24 + cosf(a) * 19, 38 + sinf(a) * 5, 1.0f, 0.75f, 0.85f, 1.0f);
                }
                pic_circle(w, 24, 38, 12, 0.55f, 0.8f, 1.0f);
                pic_circle(w, 24, 30, 6, 0.95f, 0.97f, 1.0f);
                for (i = 0; i < 24; i++) {
                    float a = PI_F + PI_F * i / 24;
                    pic_circle(w, 24 + cosf(a) * 19, 38 + sinf(a) * 5, 1.0f, 0.75f, 0.85f, 1.0f);
                }
                for (i = 0; i < 10; i++) pic_circle(w, 46 + i * 1.4f, 12 - i * 0.7f, 2.2f - i * 0.18f, 0.5f, 0.85f, 1.0f);
                pic_circle(w, 45, 12.5f, 2.2f, 1, 1, 1);
                pic_circle(w, 48, 46, 2.4f, 0.1f, 0.1f, 0.12f);   /* Tux and his rocket flame */
                pic_circle(w, 48, 47, 1.4f, 1, 1, 1);
                pic_rect(w, 43, 47, 46, 49, 1.0f, 0.5f, 0.1f);
                pic_circle(w, 41, 49, 1.5f, 1.0f, 0.85f, 0.3f);
            }
            pic_frame(w, 0.85f, 0.7f, 0.25f);
        }
        upload(t, w, w, mip, 0);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void gfx_bind(int tex)
{
    if (tex == TEX_NONE) glDisable(GL_TEXTURE_2D);
    else { glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, g_tex[tex]); }
}

void gfx_init(void)
{
    int i;
    glGenTextures(TEX_COUNT, g_tex);
    for (i = 0; i < TEX_COUNT; i++) g_tex_scale[i] = 0.25f;
    g_tex_scale[TEX_BRICK] = 0.2f;
    g_tex_scale[TEX_STONE] = 0.15f;
    g_tex_scale[TEX_CARPET] = 0.15f;
    g_tex_scale[TEX_TILE] = 0.125f;
    g_tex_scale[TEX_GRASS] = 0.15f;
    g_tex_scale[TEX_SNOW] = 0.15f;
    g_tex_scale[TEX_WATER] = 0.08f;
    g_tex_scale[TEX_LAVA] = 0.08f;
    g_tex_scale[TEX_PLANK] = 0.3f;
    gen_textures();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_FOG_HINT, GL_NICEST);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_COLOR_MATERIAL);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glAlphaFunc(GL_GREATER, 0.1f);
}

/* ======================================================================
 * camera / frame
 * ====================================================================== */
void gfx_begin_3d(float fov_deg, float zn, float zf)
{
    float f = 1.0f / tanf(RAD(fov_deg) * 0.5f);
    float m[16];
    glViewport(0, 0, g_scr_w, g_scr_h);
    g_view_aspect = g_scr_h > 0 ? (float)g_scr_w / g_scr_h : 1.0f;
    memset(m, 0, sizeof m);
    m[0] = f / g_view_aspect;
    m[5] = f;
    m[10] = (zf + zn) / (zn - zf);
    m[11] = -1;
    m[14] = 2 * zf * zn / (zn - zf);
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
}

void gfx_lookat(vec3 eye, vec3 at)
{
    vec3 f = v3norm(v3sub(at, eye));
    vec3 s = v3norm(v3cross(f, v3(0, 1, 0)));
    vec3 u = v3cross(s, f);
    float m[16];
    m[0] = s.x; m[4] = s.y; m[8] = s.z; m[12] = -v3dot(s, eye);
    m[1] = u.x; m[5] = u.y; m[9] = u.z; m[13] = -v3dot(u, eye);
    m[2] = -f.x; m[6] = -f.y; m[10] = -f.z; m[14] = v3dot(f, eye);
    m[3] = 0; m[7] = 0; m[11] = 0; m[15] = 1;
    glMatrixMode(GL_MODELVIEW);
    glLoadMatrixf(m);
}

void gfx_set_fog(float r, float g, float b, float start, float end)
{
    float c[4];
    c[0] = r; c[1] = g; c[2] = b; c[3] = 1;
    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogfv(GL_FOG_COLOR, c);
    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
    glEnable(GL_FOG);
}

void gfx_light(float ambient, float diffuse)
{
    float pos[4], amb[4], dif[4], zero[4] = { 0, 0, 0, 1 };
    pos[0] = 0.35f; pos[1] = 0.85f; pos[2] = 0.4f; pos[3] = 0;
    amb[0] = amb[1] = amb[2] = ambient; amb[3] = 1;
    dif[0] = dif[1] = dif[2] = diffuse; dif[3] = 1;
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, zero);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, dif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, zero);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHTING);
}

/* ======================================================================
 * 2D
 * ====================================================================== */
void gfx_begin_2d(void)
{
    glViewport(0, 0, g_scr_w, g_scr_h);
    g_view_aspect = g_scr_h > 0 ? (float)g_scr_w / g_scr_h : 1.0f;
    g_vw = 480.0f * g_view_aspect;
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, g_vw, 480, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
}

void gfx_rect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y); glVertex2f(x, y + h); glVertex2f(x + w, y + h); glVertex2f(x + w, y);
    glEnd();
}

/* ---- Japanese glyph atlas (generated bitmaps, see tools/gen_lang.py) ---- */
#include "font_ja.h"
static GLuint jf_tex;
static int jf_cols, jf_w, jf_h;

static void jf_init(void)
{
    int i, x, y, rows;
    unsigned char *img;
    jf_cols = 1024 / JF_CELL;
    rows = (JF_COUNT + jf_cols - 1) / jf_cols;
    jf_w = 1024;
    jf_h = 32;
    while (jf_h < rows * JF_CELL) jf_h *= 2;
    img = (unsigned char *)calloc((size_t)jf_w * jf_h, 4);
    for (i = 0; i < JF_COUNT; i++) {
        int cx = (i % jf_cols) * JF_CELL, cy = (i / jf_cols) * JF_CELL;
        for (y = 0; y < JF_CELL; y++)
            for (x = 0; x < JF_CELL; x++) {
                int k = y * JF_CELL + x;
                int v = (jf_bits[i][k >> 1] >> ((k & 1) * 4)) & 15;
                unsigned char *p = &img[((cy + y) * jf_w + cx + x) * 4];
                p[0] = p[1] = p[2] = 255;
                p[3] = (unsigned char)(v * 17);
            }
    }
    glGenTextures(1, &jf_tex);
    glBindTexture(GL_TEXTURE_2D, jf_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, jf_w, jf_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
    free(img);
}

static int jf_find(unsigned cp)
{
    int lo = 0, hi = JF_COUNT - 1;
    while (lo <= hi) {
        int m = (lo + hi) / 2;
        if (jf_codes[m] == cp) return m;
        if (jf_codes[m] < cp) lo = m + 1; else hi = m - 1;
    }
    return -1;
}

/* decode one UTF-8 code point, advancing *s */
unsigned utf8_next(const char **s)
{
    const unsigned char *p = (const unsigned char *)*s;
    unsigned c = *p;
    int n = 0;
    if (c < 0x80) { *s += 1; return c; }
    if ((c & 0xE0) == 0xC0) { c &= 0x1F; n = 1; }
    else if ((c & 0xF0) == 0xE0) { c &= 0x0F; n = 2; }
    else if ((c & 0xF8) == 0xF0) { c &= 0x07; n = 3; }
    else { *s += 1; return '?'; }
    p++;
    while (n-- > 0) {
        if ((*p & 0xC0) != 0x80) { *s = (const char *)p; return '?'; }
        c = (c << 6) | (*p++ & 0x3F);
    }
    *s = (const char *)p;
    return c;
}

/* advance of one code point in font units (scale 1) */
float glyph_adv(unsigned cp)
{
    if (cp < 128) return 6;
    if (cp == 0x3000) return 9;
    return 9;
}

static int has_wide(const char *s)
{
    for (; *s; s++) if ((unsigned char)*s >= 0x80) return 1;
    return 0;
}

float gfx_line_height(const char *s, float scale) { return (has_wide(s) ? 11.5f : 10.0f) * scale; }

void gfx_text(float x, float y, float scale, const char *s, float r, float g, float b, float a)
{
    float cw = 6 * scale, ch = 8 * scale, x0 = x, lh = gfx_line_height(s, scale);
    const char *p;
    int pass, wide = has_wide(s);
    if (!jf_tex && wide) jf_init();
    glEnable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    for (pass = 0; pass < (wide ? 2 : 1); pass++) {
        float cx = x0, cy = y;
        glBindTexture(GL_TEXTURE_2D, pass ? jf_tex : g_tex[TEX_FONT]);
        glBegin(GL_QUADS);
        for (p = s; *p;) {
            unsigned c = utf8_next(&p);
            if (c == '\n') { cx = x0; cy += lh; continue; }
            if (c < 128) {
                if (!pass) {
                    float u0, v0;
                    if (c < 32 || c > 127) c = '?';
                    c -= 32;
                    u0 = (c % 16) * 8 / 128.0f;
                    v0 = (c / 16) * 8 / 64.0f;
                    glTexCoord2f(u0, v0); glVertex2f(cx, cy);
                    glTexCoord2f(u0, v0 + 8 / 64.0f); glVertex2f(cx, cy + ch);
                    glTexCoord2f(u0 + 8 / 128.0f, v0 + 8 / 64.0f); glVertex2f(cx + 8 * scale, cy + ch);
                    glTexCoord2f(u0 + 8 / 128.0f, v0); glVertex2f(cx + 8 * scale, cy);
                }
                cx += cw;
            } else {
                if (pass) {
                    int gi = jf_find(c);
                    if (gi >= 0) {
                        float sz = 10 * scale, gx = cx - 0.5f * scale, gy = cy - 1.2f * scale;
                        float u0 = (float)((gi % jf_cols) * JF_CELL) / jf_w, v0 = (float)((gi / jf_cols) * JF_CELL) / jf_h;
                        float u1 = u0 + (float)JF_CELL / jf_w, v1 = v0 + (float)JF_CELL / jf_h;
                        glTexCoord2f(u0, v0); glVertex2f(gx, gy);
                        glTexCoord2f(u0, v1); glVertex2f(gx, gy + sz);
                        glTexCoord2f(u1, v1); glVertex2f(gx + sz, gy + sz);
                        glTexCoord2f(u1, v0); glVertex2f(gx + sz, gy);
                    }
                }
                cx += glyph_adv(c) * scale;
            }
        }
        glEnd();
    }
    glDisable(GL_TEXTURE_2D);
}

void gfx_text_shadow(float x, float y, float scale, const char *s, float r, float g, float b, float a)
{
    float o = scale * 0.6f;
    if (o < 1) o = 1;
    gfx_text(x + o, y + o, scale, s, 0, 0, 0, a * 0.8f);
    gfx_text(x, y, scale, s, r, g, b, a);
}

float gfx_text_width(const char *s, float scale)
{
    float w = 0, best = 0;
    int any = 0;
    while (*s) {
        unsigned c = utf8_next(&s);
        if (c == '\n') { if (w > best) best = w; w = 0; continue; }
        w += glyph_adv(c);
        any = 1;
    }
    if (w > best) best = w;
    return any ? best * scale - scale : 0;
}

void gfx_text_center(float cx, float y, float scale, const char *s, float r, float g, float b, float a)
{
    gfx_text_shadow(cx - gfx_text_width(s, scale) * 0.5f, y, scale, s, r, g, b, a);
}

void gfx_star2d(float cx, float cy, float rad, float r, float g, float b, float a)
{
    int i;
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLE_FAN);
    glColor4f(minf(1, r + 0.3f), minf(1, g + 0.3f), minf(1, b + 0.3f), a);
    glVertex2f(cx, cy);
    glColor4f(r, g, b, a);
    for (i = 0; i <= 10; i++) {
        float ang = -PI_F / 2 + TAU_F * i / 10.0f;
        float rr = (i & 1) ? rad * 0.45f : rad;
        glVertex2f(cx + cosf(ang) * rr, cy + sinf(ang) * rr);
    }
    glEnd();
}

void gfx_circle2d(float cx, float cy, float rad, float r, float g, float b, float a)
{
    int i;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (i = 0; i <= 24; i++) {
        float ang = TAU_F * i / 24.0f;
        glVertex2f(cx + cosf(ang) * rad, cy + sinf(ang) * rad);
    }
    glEnd();
}

void gfx_pie2d(float cx, float cy, float rad, int filled, int total, float r, float g, float b)
{
    int i, j;
    gfx_circle2d(cx, cy, rad + 3, 0.1f, 0.1f, 0.15f, 0.9f);
    for (i = 0; i < total; i++) {
        float a0 = -PI_F / 2 + TAU_F * i / total, a1 = -PI_F / 2 + TAU_F * (i + 1) / total;
        int on = i < filled;
        glBegin(GL_TRIANGLE_FAN);
        if (on) glColor4f(r, g, b, 1); else glColor4f(0.25f, 0.25f, 0.3f, 1);
        glVertex2f(cx, cy);
        for (j = 0; j <= 6; j++) {
            float a = a0 + (a1 - a0) * j / 6.0f;
            float rr = rad - 1.5f;
            glVertex2f(cx + cosf(a) * rr, cy + sinf(a) * rr);
        }
        glEnd();
    }
    glColor4f(0.1f, 0.1f, 0.15f, 1);
    glBegin(GL_LINES);
    for (i = 0; i < total; i++) {
        float a = -PI_F / 2 + TAU_F * i / total;
        glVertex2f(cx, cy);
        glVertex2f(cx + cosf(a) * rad, cy + sinf(a) * rad);
    }
    glEnd();
    gfx_circle2d(cx, cy, rad * 0.32f, 0.95f, 0.95f, 0.95f, 1);
}

void gfx_fish2d(float cx, float cy, float s, float r, float g, float b, float a)
{
    int i;
    glDisable(GL_TEXTURE_2D);
    glColor4f(r, g, b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx - s * 0.1f, cy);
    for (i = 0; i <= 20; i++) {
        float ang = TAU_F * i / 20.0f;
        glVertex2f(cx - s * 0.1f + cosf(ang) * s * 0.55f, cy + sinf(ang) * s * 0.32f);
    }
    glEnd();
    glBegin(GL_TRIANGLES);
    glVertex2f(cx + s * 0.35f, cy);
    glVertex2f(cx + s * 0.75f, cy - s * 0.35f);
    glVertex2f(cx + s * 0.75f, cy + s * 0.35f);
    glEnd();
    gfx_circle2d(cx - s * 0.38f, cy - s * 0.06f, s * 0.07f, 0, 0, 0, a);
}

void gfx_shadow(vec3 p, vec3 n, float radius, float alpha)
{
    vec3 t = fabsf(n.y) > 0.9f ? v3(1, 0, 0) : v3(0, 1, 0);
    vec3 a = v3norm(v3cross(n, t));
    vec3 b = v3cross(n, a);
    vec3 c = v3mad(p, n, 0.02f);
    a = v3scale(a, radius);
    b = v3scale(b, radius);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_SHADOW]);
    glColor4f(1, 1, 1, alpha);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0); glVertex3f(c.x - a.x - b.x, c.y - a.y - b.y, c.z - a.z - b.z);
    glTexCoord2f(1, 0); glVertex3f(c.x + a.x - b.x, c.y + a.y - b.y, c.z + a.z - b.z);
    glTexCoord2f(1, 1); glVertex3f(c.x + a.x + b.x, c.y + a.y + b.y, c.z + a.z + b.z);
    glTexCoord2f(0, 1); glVertex3f(c.x - a.x + b.x, c.y - a.y + b.y, c.z - a.z + b.z);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void gfx_billboard_quad(vec3 p, float size, float r, float g, float b, float a, vec3 cr, vec3 cu)
{
    vec3 rr = v3scale(cr, size), uu = v3scale(cu, size);
    glColor4f(r, g, b, a);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 1); glVertex3f(p.x - rr.x - uu.x, p.y - rr.y - uu.y, p.z - rr.z - uu.z);
    glTexCoord2f(1, 1); glVertex3f(p.x + rr.x - uu.x, p.y + rr.y - uu.y, p.z + rr.z - uu.z);
    glTexCoord2f(1, 0); glVertex3f(p.x + rr.x + uu.x, p.y + rr.y + uu.y, p.z + rr.z + uu.z);
    glTexCoord2f(0, 0); glVertex3f(p.x - rr.x + uu.x, p.y - rr.y + uu.y, p.z - rr.z + uu.z);
    glEnd();
}
