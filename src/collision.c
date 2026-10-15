/* collision.c - static grid + dynamic surface list */
#include "collision.h"
#include <stdlib.h>
#include <string.h>

#define CELL 4.0f
#define MAX_DYN 4096

static Surf *surfs;
static int nsurf, capsurf;
static int *cell_start, *cell_items;
static int grid_w, grid_h;
static float grid_x0, grid_z0;
static unsigned *ray_stamp;
static unsigned ray_id;

static Surf dyn[MAX_DYN];
static int ndyn;

void col_reset(void)
{
    free(surfs); surfs = NULL; nsurf = capsurf = 0;
    free(cell_start); cell_start = NULL;
    free(cell_items); cell_items = NULL;
    free(ray_stamp); ray_stamp = NULL;
    grid_w = grid_h = 0;
    ndyn = 0;
}

int col_static_count(void) { return nsurf; }

static int make_surf(Surf *s, vec3 a, vec3 b, vec3 c, int flags)
{
    vec3 n = v3cross(v3sub(b, a), v3sub(c, a));
    float l = v3len(n);
    if (l < 1e-6f) return 0;
    n = v3scale(n, 1.0f / l);
    s->v[0] = a; s->v[1] = b; s->v[2] = c;
    s->n = n;
    s->d = -v3dot(n, a);
    s->minx = minf(a.x, minf(b.x, c.x)); s->maxx = maxf(a.x, maxf(b.x, c.x));
    s->miny = minf(a.y, minf(b.y, c.y)); s->maxy = maxf(a.y, maxf(b.y, c.y));
    s->minz = minf(a.z, minf(b.z, c.z)); s->maxz = maxf(a.z, maxf(b.z, c.z));
    if (n.y > 0.45f) s->type = SURF_FLOOR;
    else if (n.y < -0.45f) s->type = SURF_CEIL;
    else s->type = SURF_WALL;
    s->flags = (unsigned char)flags;
    s->owner = -1;
    s->param = 0;
    return 1;
}

void col_add_tri(vec3 a, vec3 b, vec3 c, int flags, int param)
{
    Surf s;
    if (!make_surf(&s, a, b, c, flags)) return;
    s.param = (short)param;
    if (nsurf >= capsurf) {
        capsurf = capsurf ? capsurf * 2 : 4096;
        surfs = (Surf *)realloc(surfs, sizeof(Surf) * capsurf);
    }
    surfs[nsurf++] = s;
}

static void cell_range(const Surf *s, int *x0, int *z0, int *x1, int *z1)
{
    float pad = s->type == SURF_WALL ? 1.2f : 0.05f;
    *x0 = (int)floorf((s->minx - pad - grid_x0) / CELL);
    *x1 = (int)floorf((s->maxx + pad - grid_x0) / CELL);
    *z0 = (int)floorf((s->minz - pad - grid_z0) / CELL);
    *z1 = (int)floorf((s->maxz + pad - grid_z0) / CELL);
    if (*x0 < 0) *x0 = 0;
    if (*z0 < 0) *z0 = 0;
    if (*x1 >= grid_w) *x1 = grid_w - 1;
    if (*z1 >= grid_h) *z1 = grid_h - 1;
}

void col_build_grid(void)
{
    float minx = 1e9f, maxx = -1e9f, minz = 1e9f, maxz = -1e9f;
    int i, x, z, total = 0;
    int *cnt;
    for (i = 0; i < nsurf; i++) {
        minx = minf(minx, surfs[i].minx); maxx = maxf(maxx, surfs[i].maxx);
        minz = minf(minz, surfs[i].minz); maxz = maxf(maxz, surfs[i].maxz);
    }
    if (nsurf == 0) { minx = minz = -1; maxx = maxz = 1; }
    grid_x0 = minx - 4; grid_z0 = minz - 4;
    grid_w = (int)((maxx - minx + 8) / CELL) + 1;
    grid_h = (int)((maxz - minz + 8) / CELL) + 1;
    cnt = (int *)calloc((size_t)grid_w * grid_h + 1, sizeof(int));
    for (i = 0; i < nsurf; i++) {
        int x0, z0, x1, z1;
        cell_range(&surfs[i], &x0, &z0, &x1, &z1);
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++) { cnt[z * grid_w + x]++; total++; }
    }
    free(cell_start);
    free(cell_items);
    cell_start = (int *)malloc(sizeof(int) * ((size_t)grid_w * grid_h + 1));
    cell_items = (int *)malloc(sizeof(int) * (size_t)(total + 1));
    cell_start[0] = 0;
    for (i = 0; i < grid_w * grid_h; i++) cell_start[i + 1] = cell_start[i] + cnt[i];
    memset(cnt, 0, sizeof(int) * (size_t)grid_w * grid_h);
    for (i = 0; i < nsurf; i++) {
        int x0, z0, x1, z1;
        cell_range(&surfs[i], &x0, &z0, &x1, &z1);
        for (z = z0; z <= z1; z++)
            for (x = x0; x <= x1; x++) {
                int c = z * grid_w + x;
                cell_items[cell_start[c] + cnt[c]++] = i;
            }
    }
    free(cnt);
    free(ray_stamp);
    ray_stamp = (unsigned *)calloc((size_t)nsurf + 1, sizeof(unsigned));
    ray_id = 0;
}

void col_dyn_clear(void) { ndyn = 0; }

void col_dyn_add(vec3 a, vec3 b, vec3 c, int flags, int owner)
{
    if (ndyn >= MAX_DYN) return;
    if (make_surf(&dyn[ndyn], a, b, c, flags)) {
        dyn[ndyn].owner = (short)owner;
        ndyn++;
    }
}

static int cell_of(float x, float z)
{
    int cx = (int)floorf((x - grid_x0) / CELL), cz = (int)floorf((z - grid_z0) / CELL);
    if (!cell_start || cx < 0 || cz < 0 || cx >= grid_w || cz >= grid_h) return -1;
    return cz * grid_w + cx;
}

static int inside_xz(const Surf *s, float x, float z)
{
    const vec3 *v = s->v;
    float c0 = (v[1].x - v[0].x) * (z - v[0].z) - (v[1].z - v[0].z) * (x - v[0].x);
    float c1 = (v[2].x - v[1].x) * (z - v[1].z) - (v[2].z - v[1].z) * (x - v[1].x);
    float c2 = (v[0].x - v[2].x) * (z - v[2].z) - (v[0].z - v[2].z) * (x - v[2].x);
    const float e = 1e-4f;
    return (c0 >= -e && c1 >= -e && c2 >= -e) || (c0 <= e && c1 <= e && c2 <= e);
}

static void test_floor(const Surf *s, float x, float y, float z, float *best, Surf **out)
{
    float h;
    if (s->type != SURF_FLOOR && !(s->type == SURF_WALL && s->n.y > 0.1f)) return;
    if (x < s->minx - 0.001f || x > s->maxx + 0.001f || z < s->minz - 0.001f || z > s->maxz + 0.001f) return;
    if (s->miny > y) return;
    if (!inside_xz(s, x, z)) return;
    h = -(s->n.x * x + s->n.z * z + s->d) / s->n.y;
    if (h <= y && h > *best) { *best = h; if (out) *out = (Surf *)s; }
}

float col_floor(float x, float y, float z, Surf **out)
{
    float best = -1e9f;
    int c = cell_of(x, z), i;
    if (out) *out = NULL;
    if (c >= 0)
        for (i = cell_start[c]; i < cell_start[c + 1]; i++) test_floor(&surfs[cell_items[i]], x, y, z, &best, out);
    for (i = 0; i < ndyn; i++) test_floor(&dyn[i], x, y, z, &best, out);
    return best;
}

static int any_below(const Surf *s, float x, float y, float z)
{
    float h;
    if (s->type == SURF_CEIL) return 0;
    if (s->n.y < 0.05f) return 0;
    if (x < s->minx || x > s->maxx || z < s->minz || z > s->maxz) return 0;
    if (s->miny > y) return 0;
    if (!inside_xz(s, x, z)) return 0;
    h = -(s->n.x * x + s->n.z * z + s->d) / s->n.y;
    return h <= y;
}

/* is there any (floor or steep) surface below this point? used to tell steep slopes from out of bounds */
int col_any_below(float x, float y, float z)
{
    int c = cell_of(x, z), i;
    if (c >= 0)
        for (i = cell_start[c]; i < cell_start[c + 1]; i++) if (any_below(&surfs[cell_items[i]], x, y, z)) return 1;
    for (i = 0; i < ndyn; i++) if (any_below(&dyn[i], x, y, z)) return 1;
    return 0;
}

static void test_ceil(const Surf *s, float x, float y, float z, float *best, Surf **out)
{
    float h;
    if (s->type != SURF_CEIL) return;
    if (x < s->minx || x > s->maxx || z < s->minz || z > s->maxz) return;
    if (s->maxy < y) return;
    if (!inside_xz(s, x, z)) return;
    h = -(s->n.x * x + s->n.z * z + s->d) / s->n.y;
    if (h >= y && h < *best) { *best = h; if (out) *out = (Surf *)s; }
}

float col_ceil(float x, float y, float z, Surf **out)
{
    float best = 1e9f;
    int c = cell_of(x, z), i;
    if (out) *out = NULL;
    if (c >= 0)
        for (i = cell_start[c]; i < cell_start[c + 1]; i++) test_ceil(&surfs[cell_items[i]], x, y, z, &best, out);
    for (i = 0; i < ndyn; i++) test_ceil(&dyn[i], x, y, z, &best, out);
    return best;
}

static vec3 closest_on_tri(vec3 p, vec3 a, vec3 b, vec3 c)
{
    vec3 ab = v3sub(b, a), ac = v3sub(c, a), ap = v3sub(p, a), bp, cp;
    float d1 = v3dot(ab, ap), d2 = v3dot(ac, ap), d3, d4, d5, d6, va, vb, vc, v, w, denom;
    if (d1 <= 0 && d2 <= 0) return a;
    bp = v3sub(p, b);
    d3 = v3dot(ab, bp); d4 = v3dot(ac, bp);
    if (d3 >= 0 && d4 <= d3) return b;
    vc = d1 * d4 - d3 * d2;
    if (vc <= 0 && d1 >= 0 && d3 <= 0) { v = d1 / (d1 - d3); return v3mad(a, ab, v); }
    cp = v3sub(p, c);
    d5 = v3dot(ab, cp); d6 = v3dot(ac, cp);
    if (d6 >= 0 && d5 <= d6) return c;
    vb = d5 * d2 - d1 * d6;
    if (vb <= 0 && d2 >= 0 && d6 <= 0) { w = d2 / (d2 - d6); return v3mad(a, ac, w); }
    va = d3 * d6 - d5 * d4;
    if (va <= 0 && (d4 - d3) >= 0 && (d5 - d6) >= 0) {
        w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        return v3mad(b, v3sub(c, b), w);
    }
    denom = 1.0f / (va + vb + vc);
    v = vb * denom; w = vc * denom;
    return v3add(a, v3add(v3scale(ab, v), v3scale(ac, w)));
}

static int wall_push(const Surf *s, vec3 *c, float r, vec3 *wn, float *bestpush)
{
    float dist;
    vec3 cp, dh, nh;
    float l;
    if (s->type != SURF_WALL) return 0;
    if (c->y < s->miny || c->y > s->maxy) return 0;
    if (c->x < s->minx - r || c->x > s->maxx + r || c->z < s->minz - r || c->z > s->maxz + r) return 0;
    dist = v3dot(s->n, *c) + s->d;
    if (dist > r || dist < -0.3f) return 0;
    nh = v3(s->n.x, 0, s->n.z);
    l = v3len(nh);
    if (l < 1e-4f) return 0;
    nh = v3scale(nh, 1.0f / l);
    cp = closest_on_tri(*c, s->v[0], s->v[1], s->v[2]);
    if (dist >= 0) {
        float hl;
        dh = v3sub(*c, cp);
        dh.y = 0;
        hl = v3len(dh);
        if (hl >= r) return 0;
        if (hl < 1e-4f) {
            /* (almost) on the plane: push along the horizontal normal */
            *c = v3mad(*c, nh, minf((r - dist) / l, r * 2));
        } else {
            *c = v3mad(*c, v3scale(dh, 1.0f / hl), r - hl);
        }
    } else {
        /* slightly behind: only if projection lands inside the triangle */
        vec3 proj = v3mad(*c, s->n, -dist);
        if (v3dist(closest_on_tri(proj, s->v[0], s->v[1], s->v[2]), proj) > 0.01f) return 0;
        *c = v3mad(*c, nh, r - dist);
    }
    if (r - dist > *bestpush) { *bestpush = r - dist; *wn = nh; }
    return 1;
}

int col_walls(vec3 *pos, float yoff, float radius, vec3 *wn, Surf **wall)
{
    vec3 c = v3(pos->x, pos->y + yoff, pos->z);
    int hits = 0, i, cell, iter;
    float best = -1;
    vec3 n = v3(0, 0, 0);
    if (wall) *wall = NULL;
    for (iter = 0; iter < 2; iter++) {
        cell = cell_of(c.x, c.z);
        if (cell >= 0)
            for (i = cell_start[cell]; i < cell_start[cell + 1]; i++) {
                const Surf *s = &surfs[cell_items[i]];
                float pb = best;
                if (wall_push(s, &c, radius, &n, &best)) { hits++; if (wall && best != pb) *wall = (Surf *)s; }
            }
        for (i = 0; i < ndyn; i++) {
            float pb = best;
            if (wall_push(&dyn[i], &c, radius, &n, &best)) { hits++; if (wall && best != pb) *wall = &dyn[i]; }
        }
        if (!hits) break;
    }
    pos->x = c.x;
    pos->z = c.z;
    if (wn) *wn = n;
    return hits;
}

static int ray_tri(const Surf *s, vec3 o, vec3 d, float *t)
{
    vec3 e1 = v3sub(s->v[1], s->v[0]), e2 = v3sub(s->v[2], s->v[0]);
    vec3 p = v3cross(d, e2), q, tv;
    float det = v3dot(e1, p), inv, u, v, tt;
    if (det > -1e-7f && det < 1e-7f) return 0;
    inv = 1.0f / det;
    tv = v3sub(o, s->v[0]);
    u = v3dot(tv, p) * inv;
    if (u < 0 || u > 1) return 0;
    q = v3cross(tv, e1);
    v = v3dot(d, q) * inv;
    if (v < 0 || u + v > 1) return 0;
    tt = v3dot(e2, q) * inv;
    if (tt < 0 || tt > *t) return 0;
    *t = tt;
    return 1;
}

int col_ray(vec3 a, vec3 b, float *tout, vec3 *nout, int ignore_nocam)
{
    vec3 d = v3sub(b, a);
    float t = 1.0f;
    int hit = 0, i, x, z, x0, x1, z0, z1;
    const Surf *hs = NULL;
    if (!cell_start) return 0;
    ray_id++;
    x0 = (int)floorf((minf(a.x, b.x) - grid_x0) / CELL); x1 = (int)floorf((maxf(a.x, b.x) - grid_x0) / CELL);
    z0 = (int)floorf((minf(a.z, b.z) - grid_z0) / CELL); z1 = (int)floorf((maxf(a.z, b.z) - grid_z0) / CELL);
    if (x0 < 0) x0 = 0;
    if (z0 < 0) z0 = 0;
    if (x1 >= grid_w) x1 = grid_w - 1;
    if (z1 >= grid_h) z1 = grid_h - 1;
    for (z = z0; z <= z1; z++)
        for (x = x0; x <= x1; x++) {
            int c = z * grid_w + x;
            for (i = cell_start[c]; i < cell_start[c + 1]; i++) {
                int si = cell_items[i];
                const Surf *s = &surfs[si];
                if (ray_stamp[si] == ray_id) continue;
                ray_stamp[si] = ray_id;
                if (ignore_nocam && (s->flags & SF_NOCAM)) continue;
                if (ray_tri(s, a, d, &t)) { hit = 1; hs = s; }
            }
        }
    for (i = 0; i < ndyn; i++) {
        const Surf *s = &dyn[i];
        if (s->owner >= 0 && (s->flags & SF_NOCAM)) continue;
        if (ray_tri(s, a, d, &t)) { hit = 1; hs = s; }
    }
    if (hit) {
        if (tout) *tout = t;
        if (nout) *nout = hs->n;
    }
    return hit;
}
