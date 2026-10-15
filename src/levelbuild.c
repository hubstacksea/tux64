/* levelbuild.c - helpers that emit render geometry + collision for level construction */
#include "game.h"
#include <stdlib.h>
#include <string.h>

static Mesh lb_mesh, lb_mesh_a;
int lb_alpha_mode;

void lb_begin(void)
{
    mesh_init(&lb_mesh);
    mesh_init(&lb_mesh_a);
    lb_alpha_mode = 0;
}

void lb_end(GLuint *dl)
{
    *dl = mesh_compile(&lb_mesh);
    mesh_free(&lb_mesh);
}

void lb_end_alpha(GLuint *dl)
{
    *dl = mesh_compile(&lb_mesh_a);
    mesh_free(&lb_mesh_a);
}

static void lb_target(int tex, float r, float g, float b, float a)
{
    mb_begin(lb_alpha_mode ? &lb_mesh_a : &lb_mesh);
    mb_tex(tex);
    mb_uvscale(g_tex_scale[tex]);
    mb_color4(r, g, b, a);
}

void lb_tri(int tex, vec3 a, vec3 b, vec3 c, float r, float g, float bb, int flags, int collide)
{
    lb_target(tex, r, g, bb, lb_alpha_mode ? 0.7f : 1.0f);
    mb_tri(a, b, c);
    if (collide) col_add_tri(a, b, c, flags, 0);
}

void lb_quad(int tex, vec3 a, vec3 b, vec3 c, vec3 d, float r, float g, float bb, int flags, int collide)
{
    lb_tri(tex, a, b, c, r, g, bb, flags, collide);
    lb_tri(tex, a, c, d, r, g, bb, flags, collide);
}

/* generic 4-sided prism with flat bottom and arbitrary top corner heights.
 * corners: (x0,z0)=h00 (x1,z0)=h10 (x1,z1)=h11 (x0,z1)=h01 */
static void prism(float x0, float z0, float x1, float z1, float h00, float h10, float h11, float h01, float yb,
                  int tt, int ts, float r, float g, float b, int flags)
{
    vec3 t00 = v3(x0, h00, z0), t10 = v3(x1, h10, z0), t11 = v3(x1, h11, z1), t01 = v3(x0, h01, z1);
    vec3 b00 = v3(x0, yb, z0), b10 = v3(x1, yb, z0), b11 = v3(x1, yb, z1), b01 = v3(x0, yb, z1);
    float sr = r * 0.85f, sg = g * 0.85f, sb = b * 0.85f;
    lb_quad(tt, t00, t01, t11, t10, r, g, b, flags, 1);                  /* top */
    lb_quad(ts, b00, b10, b11, b01, sr, sg, sb, flags & ~SF_SLIDE, 1);   /* bottom */
    lb_quad(ts, b10, t10, t11, b11, sr, sg, sb, flags & ~SF_SLIDE, 1);   /* +x */
    lb_quad(ts, b00, b01, t01, t00, sr, sg, sb, flags & ~SF_SLIDE, 1);   /* -x */
    lb_quad(ts, b01, b11, t11, t01, sr, sg, sb, flags & ~SF_SLIDE, 1);   /* +z */
    lb_quad(ts, b00, t00, t10, b10, sr, sg, sb, flags & ~SF_SLIDE, 1);   /* -z */
}

void lb_box(float x0, float y0, float z0, float x1, float y1, float z1, int tt, int ts,
            float r, float g, float b, int flags)
{
    float t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    if (z0 > z1) { t = z0; z0 = z1; z1 = t; }
    prism(x0, z0, x1, z1, y1, y1, y1, y1, y0, tt, ts, r, g, b, flags);
}

void lb_ramp(float x0, float z0, float x1, float z1, float y0, float y1, int axis, float thick, int tt,
             int ts, float r, float g, float b, int flags)
{
    /* axis 0: height goes y0 at x0 -> y1 at x1 ; axis 1: along z */
    float yb = minf(y0, y1) - thick, t;
    if (axis == 0 && x0 > x1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    if (axis == 1 && z0 > z1) { t = z0; z0 = z1; z1 = t; t = y0; y0 = y1; y1 = t; }
    if (axis == 0 && z0 > z1) { t = z0; z0 = z1; z1 = t; }
    if (axis == 1 && x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (axis == 0) prism(x0, z0, x1, z1, y0, y1, y1, y0, yb, tt, ts, r, g, b, flags);
    else prism(x0, z0, x1, z1, y0, y0, y1, y1, yb, tt, ts, r, g, b, flags);
}

void lb_stairs(float x0, float z0, float x1, float z1, float y0, float y1, int axis, int steps, int tex,
               float r, float g, float b)
{
    int i;
    for (i = 0; i < steps; i++) {
        float t0 = (float)i / steps, t1 = (float)(i + 1) / steps;
        float h = y0 + (y1 - y0) * t1;
        float sh = (i & 1) ? 0.93f : 1.0f;
        if (axis == 0) lb_box(lerpf(x0, x1, t0), y0, z0, lerpf(x0, x1, t1), h, z1, tex, tex, r * sh, g * sh, b * sh, 0);
        else lb_box(x0, y0, lerpf(z0, z1, t0), x1, h, lerpf(z0, z1, t1), tex, tex, r * sh, g * sh, b * sh, 0);
    }
}

void lb_obox(vec3 c, vec3 h, float yaw, float pitch, int tt, int ts, float r, float g, float b, int flags)
{
    vec3 p[8];
    int i;
    float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
    for (i = 0; i < 8; i++) {
        vec3 l = v3((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
        /* pitch about x: positive pitch raises +z end */
        vec3 q = v3(l.x, l.y * cp + l.z * sp, -l.y * sp + l.z * cp);
        vec3 w = v3(q.x * cy + q.z * sy, q.y, -q.x * sy + q.z * cy);
        p[i] = v3add(c, w);
    }
    lb_quad(tt, p[2], p[6], p[7], p[3], r, g, b, flags, 1);                         /* +y */
    lb_quad(ts, p[0], p[1], p[5], p[4], r * .85f, g * .85f, b * .85f, flags & ~SF_SLIDE, 1);  /* -y */
    lb_quad(ts, p[1], p[3], p[7], p[5], r * .85f, g * .85f, b * .85f, flags & ~SF_SLIDE, 1);  /* +x */
    lb_quad(ts, p[0], p[4], p[6], p[2], r * .85f, g * .85f, b * .85f, flags & ~SF_SLIDE, 1);  /* -x */
    lb_quad(ts, p[4], p[5], p[7], p[6], r * .85f, g * .85f, b * .85f, flags & ~SF_SLIDE, 1);  /* +z */
    lb_quad(ts, p[0], p[2], p[3], p[1], r * .85f, g * .85f, b * .85f, flags & ~SF_SLIDE, 1);  /* -z */
}

void lb_cyl(float x, float y, float z, float rad, float h, int seg, int tt, int ts,
            float r, float g, float b, int flags)
{
    int i;
    for (i = 0; i < seg; i++) {
        float a0 = TAU_F * i / seg, a1 = TAU_F * (i + 1) / seg;
        vec3 p0 = v3(x + sinf(a0) * rad, y, z + cosf(a0) * rad);
        vec3 p1 = v3(x + sinf(a1) * rad, y, z + cosf(a1) * rad);
        vec3 q0 = v3(p0.x, y + h, p0.z), q1 = v3(p1.x, y + h, p1.z);
        lb_quad(ts, p0, p1, q1, q0, r * .85f, g * .85f, b * .85f, flags & ~SF_SLIDE, 1);
        lb_tri(tt, v3(x, y + h, z), q0, q1, r, g, b, flags, 1);
        lb_tri(ts, v3(x, y, z), p1, p0, r * .7f, g * .7f, b * .7f, 0, 1);
    }
}

void lb_cone(float x, float y, float z, float rad, float h, int seg, int tex, float r, float g, float b, int collide)
{
    int i;
    for (i = 0; i < seg; i++) {
        float a0 = TAU_F * i / seg, a1 = TAU_F * (i + 1) / seg;
        vec3 p0 = v3(x + sinf(a0) * rad, y, z + cosf(a0) * rad);
        vec3 p1 = v3(x + sinf(a1) * rad, y, z + cosf(a1) * rad);
        lb_tri(tex, p0, p1, v3(x, y + h, z), r, g, b, 0, collide);
    }
}

typedef struct { float h; vec3 n; } TV;

void lb_terrain(float x0, float z0, float x1, float z1, int nx, int nz, float (*hf)(float, float),
                void (*style)(float x, float y, float z, vec3 n, int *tex, float *col, int *flags))
{
    int i, j;
    float dx = (x1 - x0) / nx, dz = (z1 - z0) / nz;
    TV *g = (TV *)malloc(sizeof(TV) * (size_t)(nx + 1) * (nz + 1));
    for (j = 0; j <= nz; j++)
        for (i = 0; i <= nx; i++) {
            float x = x0 + i * dx, z = z0 + j * dz;
            float e = 0.25f;
            TV *t = &g[j * (nx + 1) + i];
            t->h = hf(x, z);
            t->n = v3norm(v3(hf(x - e, z) - hf(x + e, z), 2 * e, hf(x, z - e) - hf(x, z + e)));
        }
    for (j = 0; j < nz; j++)
        for (i = 0; i < nx; i++) {
            int k;
            TV *a = &g[j * (nx + 1) + i], *b = &g[j * (nx + 1) + i + 1];
            TV *c = &g[(j + 1) * (nx + 1) + i + 1], *d = &g[(j + 1) * (nx + 1) + i];
            vec3 pa = v3(x0 + i * dx, a->h, z0 + j * dz), pb = v3(x0 + (i + 1) * dx, b->h, z0 + j * dz);
            vec3 pc = v3(x0 + (i + 1) * dx, c->h, z0 + (j + 1) * dz), pd = v3(x0 + i * dx, d->h, z0 + (j + 1) * dz);
            for (k = 0; k < 2; k++) {
                vec3 A = pa, B = k ? pc : pd, C = k ? pb : pc;
                vec3 nA = a->n, nB = k ? c->n : d->n, nC = k ? b->n : c->n;
                vec3 fn = v3norm(v3cross(v3sub(B, A), v3sub(C, A)));
                vec3 cen = v3scale(v3add(A, v3add(B, C)), 1.0f / 3);
                int tex = TEX_GRASS, fl = 0;
                float col[3] = { 1, 1, 1 };
                style(cen.x, cen.y, cen.z, fn, &tex, col, &fl);
                lb_target(tex, col[0], col[1], col[2], 1);
                if (fn.y < 0.6f) mb_tri(A, B, C);   /* hard edge on cliffs */
                else mb_tri_n(A, B, C, nA, nB, nC);
                col_add_tri(A, B, C, fl, 0);
            }
        }
    free(g);
}

void lb_water(float x0, float z0, float x1, float z1, float y, float ybot)
{
    int i, j, n = 8;
    if (lv.nwater < 8) {
        WaterBox *w = &lv.water[lv.nwater++];
        w->x0 = x0; w->z0 = z0; w->x1 = x1; w->z1 = z1; w->y = y; w->ybot = ybot; w->warm = 0;
    }
    lb_alpha_mode = 1;
    for (j = 0; j < n; j++)
        for (i = 0; i < n; i++) {
            float ax = lerpf(x0, x1, (float)i / n), bx = lerpf(x0, x1, (float)(i + 1) / n);
            float az = lerpf(z0, z1, (float)j / n), bz = lerpf(z0, z1, (float)(j + 1) / n);
            vec3 a = v3(ax, y, az), b = v3(bx, y, az), c = v3(bx, y, bz), d = v3(ax, y, bz);
            lb_quad(TEX_WATER, a, d, c, b, 0.8f, 0.9f, 1, 0, 0);
            lb_quad(TEX_WATER, a, b, c, d, 0.6f, 0.7f, 0.9f, 0, 0);
        }
    lb_alpha_mode = 0;
}

void lb_painting(vec3 c, float yaw, float w, float h, int tex)
{
    vec3 f = yaw_dir(yaw), rt = v3(f.z, 0, -f.x);
    vec3 p = v3mad(c, f, 0.06f);
    vec3 a = v3add(v3mad(p, rt, -w * 0.5f), v3(0, -h * 0.5f, 0));
    vec3 b = v3add(v3mad(p, rt, w * 0.5f), v3(0, -h * 0.5f, 0));
    vec3 cc = v3add(b, v3(0, h, 0)), d = v3add(a, v3(0, h, 0));
    Mesh *m = &lb_mesh;
    Vtx va, vb, vc, vd;
    float fr = 0.3f;
    /* frame */
    lb_obox(v3mad(v3add(c, v3(0, h * 0.5f + fr * 0.5f, 0)), f, 0.05f), v3(w * 0.5f + fr, fr * 0.5f, 0.1f), yaw, 0, TEX_WOOD, TEX_WOOD, 0.95f, 0.75f, 0.3f, 0);
    lb_obox(v3mad(v3add(c, v3(0, -h * 0.5f - fr * 0.5f, 0)), f, 0.05f), v3(w * 0.5f + fr, fr * 0.5f, 0.1f), yaw, 0, TEX_WOOD, TEX_WOOD, 0.95f, 0.75f, 0.3f, 0);
    lb_obox(v3mad(v3mad(c, rt, w * 0.5f + fr * 0.5f), f, 0.05f), v3(fr * 0.5f, h * 0.5f, 0.1f), yaw, 0, TEX_WOOD, TEX_WOOD, 0.95f, 0.75f, 0.3f, 0);
    lb_obox(v3mad(v3mad(c, rt, -w * 0.5f - fr * 0.5f), f, 0.05f), v3(fr * 0.5f, h * 0.5f, 0.1f), yaw, 0, TEX_WOOD, TEX_WOOD, 0.95f, 0.75f, 0.3f, 0);
    /* picture with explicit uvs */
    memset(&va, 0, sizeof va);
    va.n[0] = f.x; va.n[1] = 0; va.n[2] = f.z;
    va.c[0] = va.c[1] = va.c[2] = va.c[3] = 255;
    vb = vc = vd = va;
    va.p[0] = a.x; va.p[1] = a.y; va.p[2] = a.z; va.uv[0] = 0; va.uv[1] = 1;
    vb.p[0] = b.x; vb.p[1] = b.y; vb.p[2] = b.z; vb.uv[0] = 1; vb.uv[1] = 1;
    vc.p[0] = cc.x; vc.p[1] = cc.y; vc.p[2] = cc.z; vc.uv[0] = 1; vc.uv[1] = 0;
    vd.p[0] = d.x; vd.p[1] = d.y; vd.p[2] = d.z; vd.uv[0] = 0; vd.uv[1] = 0;
    /* winding: front faces the +f direction */
    {
        vec3 nn = v3cross(v3sub(b, a), v3sub(cc, a));
        if (v3dot(nn, f) > 0) { mesh_add(m, tex, &va, &vb, &vc); mesh_add(m, tex, &va, &vc, &vd); }
        else { mesh_add(m, tex, &va, &vc, &vb); mesh_add(m, tex, &va, &vd, &vc); }
    }
}

/* an "application window" floating in front of a wall: the portal into a course.
 * c = centre of the content area, the title bar sits on top; (r,g,b) = title bar colour */
void lb_appwindow(vec3 c, float yaw, float w, float h, int tex, float r, float g, float b)
{
    vec3 f = yaw_dir(yaw), rt = v3(f.z, 0, -f.x);
    float tb = 0.7f, bw = 0.12f;
    vec3 p = v3mad(c, f, 0.12f);
    vec3 a = v3add(v3mad(p, rt, -w * 0.5f), v3(0, -h * 0.5f, 0));
    vec3 bb = v3add(v3mad(p, rt, w * 0.5f), v3(0, -h * 0.5f, 0));
    vec3 cc = v3add(bb, v3(0, h, 0)), d = v3add(a, v3(0, h, 0));
    Mesh *m = &lb_mesh;
    Vtx va, vb, vc, vd;
    int i;
    /* window body and title bar */
    lb_obox(v3mad(v3add(c, v3(0, tb * 0.5f, 0)), f, 0.02f), v3(w * 0.5f + bw, (h + tb) * 0.5f + bw, 0.08f), yaw, 0,
            TEX_NONE, TEX_NONE, 0.06f, 0.08f, 0.14f, 0);
    lb_obox(v3mad(v3add(c, v3(0, h * 0.5f + tb * 0.5f, 0)), f, 0.1f), v3(w * 0.5f, tb * 0.45f, 0.02f), yaw, 0,
            TEX_NONE, TEX_NONE, r, g, b, 0);
    /* minimize / maximize / close buttons */
    for (i = 0; i < 3; i++) {
        static const float bc[3][3] = { { 0.3f, 0.9f, 0.4f }, { 1.0f, 0.8f, 0.2f }, { 1.0f, 0.3f, 0.3f } };
        vec3 bp = v3add(v3mad(v3mad(c, rt, w * 0.5f - 0.35f - (2 - i) * 0.55f), f, 0.13f), v3(0, h * 0.5f + tb * 0.5f, 0));
        lb_obox(bp, v3(0.18f, 0.18f, 0.02f), yaw, 0, TEX_NONE, TEX_NONE, bc[i][0], bc[i][1], bc[i][2], 0);
    }
    /* neon outline */
    lb_obox(v3mad(v3add(c, v3(0, h * 0.5f + tb + bw * 0.5f, 0)), f, 0.1f), v3(w * 0.5f + bw, bw * 0.5f, 0.03f), yaw, 0, TEX_NONE, TEX_NONE, 0.3f, 0.95f, 1, 0);
    lb_obox(v3mad(v3add(c, v3(0, -h * 0.5f - bw * 0.5f, 0)), f, 0.1f), v3(w * 0.5f + bw, bw * 0.5f, 0.03f), yaw, 0, TEX_NONE, TEX_NONE, 0.3f, 0.95f, 1, 0);
    lb_obox(v3mad(v3add(v3mad(c, rt, w * 0.5f + bw * 0.5f), v3(0, tb * 0.5f, 0)), f, 0.1f), v3(bw * 0.5f, (h + tb) * 0.5f + bw, 0.03f), yaw, 0, TEX_NONE, TEX_NONE, 0.3f, 0.95f, 1, 0);
    lb_obox(v3mad(v3add(v3mad(c, rt, -w * 0.5f - bw * 0.5f), v3(0, tb * 0.5f, 0)), f, 0.1f), v3(bw * 0.5f, (h + tb) * 0.5f + bw, 0.03f), yaw, 0, TEX_NONE, TEX_NONE, 0.3f, 0.95f, 1, 0);
    /* content: the world inside */
    memset(&va, 0, sizeof va);
    va.n[0] = f.x; va.n[1] = 0; va.n[2] = f.z;
    va.c[0] = va.c[1] = va.c[2] = va.c[3] = 255;
    vb = vc = vd = va;
    va.p[0] = a.x; va.p[1] = a.y; va.p[2] = a.z; va.uv[0] = 0.05f; va.uv[1] = 0.95f;
    vb.p[0] = bb.x; vb.p[1] = bb.y; vb.p[2] = bb.z; vb.uv[0] = 0.95f; vb.uv[1] = 0.95f;
    vc.p[0] = cc.x; vc.p[1] = cc.y; vc.p[2] = cc.z; vc.uv[0] = 0.95f; vc.uv[1] = 0.05f;
    vd.p[0] = d.x; vd.p[1] = d.y; vd.p[2] = d.z; vd.uv[0] = 0.05f; vd.uv[1] = 0.05f;
    {
        vec3 nn = v3cross(v3sub(bb, a), v3sub(cc, a));
        if (v3dot(nn, f) > 0) { mesh_add(m, tex, &va, &vb, &vc); mesh_add(m, tex, &va, &vc, &vd); }
        else { mesh_add(m, tex, &va, &vc, &vb); mesh_add(m, tex, &va, &vd, &vc); }
    }
}

static vec3 catmull(vec3 p0, vec3 p1, vec3 p2, vec3 p3, float t)
{
    float t2 = t * t, t3 = t2 * t;
    vec3 r;
    r.x = 0.5f * (2 * p1.x + (-p0.x + p2.x) * t + (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 + (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
    r.y = 0.5f * (2 * p1.y + (-p0.y + p2.y) * t + (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 + (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
    r.z = 0.5f * (2 * p1.z + (-p0.z + p2.z) * t + (2 * p0.z - 5 * p1.z + 4 * p2.z - p3.z) * t2 + (-p0.z + 3 * p1.z - 3 * p2.z + p3.z) * t3);
    return r;
}

void lb_track(const vec3 *pts, int n, float width, float wall_h, int sub, int tex, float r, float g, float b, int flags)
{
    int i, k, total = (n - 1) * sub + 1, s;
    vec3 *c = (vec3 *)malloc(sizeof(vec3) * (size_t)total);
    vec3 *L = (vec3 *)malloc(sizeof(vec3) * (size_t)total);
    vec3 *R = (vec3 *)malloc(sizeof(vec3) * (size_t)total);
    for (i = 0; i < n - 1; i++)
        for (k = 0; k < sub; k++) {
            vec3 p0 = pts[i > 0 ? i - 1 : 0], p1 = pts[i], p2 = pts[i + 1], p3 = pts[i + 2 < n ? i + 2 : n - 1];
            c[i * sub + k] = catmull(p0, p1, p2, p3, (float)k / sub);
        }
    c[total - 1] = pts[n - 1];
    for (s = 0; s < total; s++) {
        vec3 d = v3sub(c[s < total - 1 ? s + 1 : s], c[s > 0 ? s - 1 : s]);
        vec3 rt = v3norm(v3(d.z, 0, -d.x));
        L[s] = v3mad(c[s], rt, width * 0.5f);
        R[s] = v3mad(c[s], rt, -width * 0.5f);
    }
    for (s = 0; s < total - 1; s++) {
        vec3 l0 = L[s], l1 = L[s + 1], r0 = R[s], r1 = R[s + 1];
        vec3 up = v3(0, wall_h, 0), dn = v3(0, -0.6f, 0);
        float sh = (s / 4) & 1 ? 0.92f : 1.0f;
        /* floor: make sure it faces up */
        vec3 nn = v3cross(v3sub(r0, l0), v3sub(l1, l0));
        if (nn.y > 0) lb_quad(tex, l0, r0, r1, l1, r * sh, g * sh, b * sh, flags, 1);
        else lb_quad(tex, l0, l1, r1, r0, r * sh, g * sh, b * sh, flags, 1);
        /* underside (visual) */
        if (nn.y > 0) lb_quad(TEX_ICE, v3add(l0, dn), v3add(l1, dn), v3add(r1, dn), v3add(r0, dn), 0.6f, 0.7f, 0.8f, 0, 0);
        else lb_quad(TEX_ICE, v3add(l0, dn), v3add(r0, dn), v3add(r1, dn), v3add(l1, dn), 0.6f, 0.7f, 0.8f, 0, 0);
        /* walls: inner faces must face the track centre */
        {
            vec3 cen = v3lerp(c[s], c[s + 1], 0.5f);
            vec3 wn = v3cross(v3sub(l1, l0), up);
            vec3 mid = v3lerp(l0, l1, 0.5f);
            if (v3dot(wn, v3sub(cen, mid)) > 0) lb_quad(TEX_ICE, l0, l1, v3add(l1, up), v3add(l0, up), 0.75f, 0.85f, 1, 0, 1);
            else lb_quad(TEX_ICE, l0, v3add(l0, up), v3add(l1, up), l1, 0.75f, 0.85f, 1, 0, 1);
            wn = v3cross(v3sub(r1, r0), up);
            mid = v3lerp(r0, r1, 0.5f);
            if (v3dot(wn, v3sub(cen, mid)) > 0) lb_quad(TEX_ICE, r0, r1, v3add(r1, up), v3add(r0, up), 0.75f, 0.85f, 1, 0, 1);
            else lb_quad(TEX_ICE, r0, v3add(r0, up), v3add(r1, up), r1, 0.75f, 0.85f, 1, 0, 1);
            /* outer faces (visual) */
            if (v3dot(wn, v3sub(cen, mid)) > 0) lb_quad(TEX_ICE, r0, v3add(r0, up), v3add(r1, up), r1, 0.6f, 0.7f, 0.85f, 0, 0);
            else lb_quad(TEX_ICE, r0, r1, v3add(r1, up), v3add(r0, up), 0.6f, 0.7f, 0.85f, 0, 0);
            wn = v3cross(v3sub(l1, l0), up);
            mid = v3lerp(l0, l1, 0.5f);
            if (v3dot(wn, v3sub(cen, mid)) > 0) lb_quad(TEX_ICE, l0, v3add(l0, up), v3add(l1, up), l1, 0.6f, 0.7f, 0.85f, 0, 0);
            else lb_quad(TEX_ICE, l0, l1, v3add(l1, up), v3add(l0, up), 0.6f, 0.7f, 0.85f, 0, 0);
        }
    }
    free(c); free(L); free(R);
}
