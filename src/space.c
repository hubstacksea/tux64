/* space.c - STARDUST ORBIT: free 3D rocket flight around an ice planet
 *
 * The rocket never runs out of fuel here. Tux cruises all the time, A boosts, B brakes and Z does a
 * quick turn. The stage has a timed course of rings, a comet to chase, ice meteors that only break
 * when rammed at full boost, and asteroids drifting in a belt around the planet. */
#include "game.h"
#include <stdio.h>
#include <string.h>

int g_ring_run_next, g_ring_run_t, g_meteors_left;
static int ring_idx[SPACE_RINGS];
static int ring_star_out;
static int bump_msg_t;
static vec3 cam_off;
static int cam_ok;

/* ---------------------------------------------------------------- the ring course */
vec3 space_ring_pos(int k)
{
    float t = TAU_F * k / SPACE_RINGS, r = 100.0f + 20.0f * sinf(3 * t);
    return v3(sinf(t) * r, 6.0f + 28.0f * sinf(2 * t), cosf(t) * r);
}

Obj *space_ring(int k)
{
    if (k < 0 || k >= SPACE_RINGS || ring_idx[k] < 0) return NULL;
    return &objs[ring_idx[k]];
}

static vec3 ring_normal(Obj *o)
{
    return v3(sinf(o->yaw) * cosf(o->pitch), sinf(o->pitch), cosf(o->yaw) * cosf(o->pitch));
}

void space_reset(void)
{
    int i;
    g_ring_run_next = 0;
    g_ring_run_t = 0;
    g_meteors_left = 0;
    ring_star_out = 0;
    bump_msg_t = 0;
    cam_ok = 0;
    for (i = 0; i < SPACE_RINGS; i++) ring_idx[i] = -1;
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        if (o->type == OT_SPACERING && o->param >= 0 && o->param < SPACE_RINGS) ring_idx[o->param] = i;
        if (o->type == OT_METEOR) g_meteors_left++;
    }
}

static vec3 tux_center(void) { return v3add(pl.pos, v3(0, 0.8f, 0)); }

static vec3 fly_dir(void)
{
    return v3(sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch), cosf(pl.yaw) * cosf(pl.pitch));
}

/* Tux bounced off something round (asteroid, meteor) */
static void bounce_off(vec3 c, float r)
{
    vec3 tc = tux_center(), n = v3norm(v3sub(tc, c)), d = fly_dir();
    float dn = v3dot(d, n);
    pl.pos = v3sub(v3mad(c, n, r + 0.95f), v3(0, 0.8f, 0));
    if (dn < 0) d = v3norm(v3mad(d, n, -1.8f * dn));
    pl.yaw = atan2f(d.x, d.z);
    pl.pitch = clampf(asinf(clampf(d.y, -1, 1)), -1.3f, 1.3f);
    pl.sp_speed *= 0.5f;
    pl.sp_stun = 14;
    cam.shake = maxf(cam.shake, 0.25f);
    sfx(SFX_THUD);
}

static void ring_run_stop(void)
{
    g_ring_run_next = 0;
    g_ring_run_t = 0;
}

void space_update(void)
{
    int i;
    if (cur_level != LV_SPACE) return;
    if (bump_msg_t > 0) bump_msg_t--;
    /* the ring run clock */
    if (g_ring_run_t > 0 && pl.act == ACT_SPACEFLY) {
        g_ring_run_t--;
        if (g_ring_run_t > 0 && g_ring_run_t <= 5 * 60 && g_ring_run_t % 60 == 0) sfx(SFX_MENU_MOVE);
        if (g_ring_run_t == 0) {
            ring_run_stop();
            sfx(SFX_BUZZ);
            hud_message("TIME UP! Start again from the green ring.", 150);
        }
    }
    /* fish fly into Tux when he passes close by */
    if (pl.act == ACT_SPACEFLY) {
        vec3 tc = tux_center();
        for (i = 0; i < nobj; i++) {
            Obj *o = &objs[i];
            vec3 d;
            float l;
            if (!o->active || (o->type != OT_COIN && o->type != OT_REDCOIN && o->type != OT_ONEUP && o->type != OT_HEART)) continue;
            d = v3sub(tc, o->pos);
            l = v3len(d);
            if (l > 4.5f || l < 0.01f) continue;
            o->pos = v3mad(o->pos, d, minf(1.0f, (30.0f + pl.sp_speed) * DT / l));
            o->home = o->pos;
        }
    }
}

/* ---------------------------------------------------------------- objects */
static void comet_place(Obj *o)
{
    float a = o->f[0], r = 140.0f + 45.0f * sinf(2 * a);
    r += 5.0f * sinf(9 * a);   /* it weaves as it flies */
    o->pos = v3(cosf(a) * r, 40.0f * sinf(a + 1.0f) + 5.0f * sinf(6 * a), sinf(a) * r);
}

void space_obj_update(Obj *o)
{
    vec3 tc = tux_center();
    int flying = pl.act == ACT_SPACEFLY && pl.visible;
    o->t += DT;
    switch (o->type) {
    case OT_SPACERING: {
        vec3 n = ring_normal(o), rel = v3sub(tc, o->pos);
        float along = v3dot(rel, n), rad = v3len(v3mad(rel, n, -along));
        float R = o->param >= 0 ? 4.0f : 3.2f;
        if (o->timer > 0) o->timer--;
        if (!flying || o->timer > 0 || fabsf(along) > 1.8f || rad > R + 0.2f) break;
        o->timer = 45;
        if (o->param < 0) {   /* boost ring */
            pl.boost_t = 80;
            sfx(SFX_BLAST);
            cam.shake = maxf(cam.shake, 0.15f);
            fx_spawn(OT_FX_SPARKLE, o->pos, 2.0f);
            break;
        }
        if (o->param == 0 && g_ring_run_t == 0) {
            g_ring_run_next = 1;
            g_ring_run_t = SPACE_RING_TIME;
            pl.sp_speed = maxf(pl.sp_speed, 50.0f);
            sfx(SFX_BLUECOIN);
            hud_message("RING RUN! Fly through all 20 rings in 35 seconds!", 150);
            fx_spawn(OT_FX_SPARKLE, o->pos, 2.5f);
            break;
        }
        if (g_ring_run_t > 0 && o->param == g_ring_run_next) {
            g_ring_run_next++;
            pl.sp_speed = maxf(pl.sp_speed, 50.0f);
            sfx_ex(SFX_BLUECOIN, 1.0f, 0.8f + 0.04f * o->param);
            fx_spawn(OT_FX_SPARKLE, o->pos, 2.5f);
            if (g_ring_run_next >= SPACE_RINGS) {
                /* the star appears straight ahead of Tux */
                vec3 at = v3mad(tc, fly_dir(), 18.0f);
                ring_run_stop();
                ring_star_out = 1;
                star_spawn(at, 1, 0, at);
                sfx(SFX_STAR_APPEAR);
                fx_spawn(OT_FX_SPARKLE, at, 3.0f);
                hud_message("RING RUN CLEAR!", 150);
            }
        }
        break;
    }
    case OT_COMET: {
        /* it flies a little faster when Tux gets close: only a full boost catches it */
        float d = v3dist(o->pos, tc), sp = d < 30 ? 45.0f : 36.0f;
        float r = 140.0f + 45.0f * sinf(2 * o->f[0]);
        o->f[0] += sp / r * DT;
        comet_place(o);
        if (g_frame % 2 == 0) fx_spawn(OT_FX_SPARKLE, o->pos, 1.2f);
        if (flying && d < 2.8f) {
            sfx(SFX_STAR_APPEAR);
            fx_spawn(OT_FX_EXPLODE, o->pos, 1.5f);
            hud_message("You caught the comet!", 120);
            star_spawn(v3mad(o->pos, fly_dir(), 4.0f), 2, 0, o->pos);
            obj_kill(o);
        }
        break;
    }
    case OT_METEOR: {
        float d = v3dist(o->pos, tc);
        o->yaw += 0.4f * DT;
        o->pitch += 0.25f * DT;
        o->pos.y = o->home.y + sinf(o->t * 0.7f + o->home.x) * 0.8f;
        if (!flying || d > o->radius + 0.9f) break;
        if (pl.sp_speed > 40.0f) {
            /* smashed! */
            fx_spawn(OT_FX_EXPLODE, o->pos, 3.0f);
            fx_spawn(OT_FX_SPARKLE, o->pos, 3.0f);
            sfx(SFX_EXPLODE);
            sfx(SFX_BREAK);
            cam.shake = maxf(cam.shake, 0.5f);
            game_add_coins(5, o->pos);
            g_meteors_left--;
            if (g_meteors_left <= 0) {
                hud_message("All the ice meteors are smashed!", 150);
                { vec3 at = v3mad(tc, fly_dir(), 18.0f); star_spawn(at, 4, 0, at); sfx(SFX_STAR_APPEAR); fx_spawn(OT_FX_SPARKLE, at, 3.0f); }
            } else {
                static char msg[64];
                snprintf(msg, sizeof msg, tr("ICE METEOR SMASHED! %d left"), g_meteors_left);
                hud_message(msg, 120);
            }
            obj_kill(o);
        } else {
            bounce_off(o->pos, o->radius);
            if (bump_msg_t == 0) { hud_message("Too slow! Ram it at full boost (hold A)!", 120); bump_msg_t = 240; }
        }
        break;
    }
    case OT_ASTEROID: {
        float d;
        if (o->state == 1) {   /* smashed: drifts back in a while */
            if (++o->timer > 20 * 60) { o->state = 0; o->timer = 0; }
            break;
        }
        o->f[0] += o->f[3] * DT;
        o->pos = v3(sinf(o->f[0]) * o->f[1], o->f[2] + sinf(o->t * 0.5f + o->f[1]) * 1.5f, cosf(o->f[0]) * o->f[1]);
        o->yaw += o->f[5] * DT;
        o->pitch += o->f[5] * 0.7f * DT;
        d = v3dist(o->pos, tc);
        if (!flying || d > o->radius + 0.9f) break;
        if (pl.sp_speed > 40.0f) {
            fx_spawn(OT_FX_EXPLODE, o->pos, 1.5f);
            sfx(SFX_BREAK);
            cam.shake = maxf(cam.shake, 0.3f);
            game_add_coins(3, o->pos);
            o->state = 1; o->timer = 0;
        } else bounce_off(o->pos, o->radius);
        break;
    }
    }
}

/* ---------------------------------------------------------------- drawing */
/* a lumpy low-poly rock (unit size), cached per seed */
static GLuint rock_dl[4];
static float hash3(int a, int b, int c)
{
    unsigned h = (unsigned)a * 73856093u ^ (unsigned)b * 19349663u ^ (unsigned)c * 83492791u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
    return (h & 0xFFFF) / 65535.0f;
}

static vec3 rock_v(int seed, int i, int j, int sl, int st)
{
    float th = TAU_F * (i % sl) / sl, ph = PI_F * j / st - PI_F / 2;
    float k = (j == 0 || j == st) ? 0.9f : 0.78f + 0.4f * hash3(seed, i % sl, j);
    return v3(cosf(ph) * sinf(th) * k, sinf(ph) * k, cosf(ph) * cosf(th) * k);
}

static GLuint rock_list(int seed)
{
    int i, j, sl = 8, st = 6;
    if (rock_dl[seed]) return rock_dl[seed];
    rock_dl[seed] = glGenLists(1);
    glNewList(rock_dl[seed], GL_COMPILE);
    glBegin(GL_TRIANGLES);
    for (j = 0; j < st; j++)
        for (i = 0; i < sl; i++) {
            vec3 a = rock_v(seed, i, j, sl, st), b = rock_v(seed, i + 1, j, sl, st);
            vec3 c = rock_v(seed, i + 1, j + 1, sl, st), d = rock_v(seed, i, j + 1, sl, st);
            vec3 n1 = v3norm(v3cross(v3sub(c, a), v3sub(b, a))), n2 = v3norm(v3cross(v3sub(d, a), v3sub(c, a)));
            if (v3dot(n1, a) < 0) n1 = v3scale(n1, -1);
            if (v3dot(n2, a) < 0) n2 = v3scale(n2, -1);
            glNormal3f(n1.x, n1.y, n1.z);
            glVertex3f(a.x, a.y, a.z); glVertex3f(c.x, c.y, c.z); glVertex3f(b.x, b.y, b.z);
            glNormal3f(n2.x, n2.y, n2.z);
            glVertex3f(a.x, a.y, a.z); glVertex3f(d.x, d.y, d.z); glVertex3f(c.x, c.y, c.z);
        }
    glEnd();
    glEndList();
    return rock_dl[seed];
}

static void draw_torus(float R, float r, int seg, int seg2)
{
    int i, j;
    for (i = 0; i < seg; i++) {
        float a0 = TAU_F * i / seg, a1 = TAU_F * (i + 1) / seg;
        glBegin(GL_QUAD_STRIP);
        for (j = 0; j <= seg2; j++) {
            float b = TAU_F * j / seg2, c = cosf(b), s = sinf(b);
            glNormal3f(cosf(a1) * c, sinf(a1) * c, s);
            glVertex3f(cosf(a1) * (R + r * c), sinf(a1) * (R + r * c), r * s);
            glNormal3f(cosf(a0) * c, sinf(a0) * c, s);
            glVertex3f(cosf(a0) * (R + r * c), sinf(a0) * (R + r * c), r * s);
        }
        glEnd();
    }
}

void space_obj_draw(Obj *o)
{
    glPushMatrix();
    glTranslatef(o->pos.x, o->pos.y, o->pos.z);
    switch (o->type) {
    case OT_SPACERING: {
        float pulse = 0.5f + 0.5f * sinf(g_frame * 0.25f), r, g, b;
        int lit;
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(-DEG(o->pitch), 1, 0, 0);
        glDisable(GL_LIGHTING);
        glDisable(GL_TEXTURE_2D);
        if (o->param < 0) { r = 1.0f; g = 0.45f + 0.3f * pulse; b = 0.1f; }
        else {
            lit = g_ring_run_t > 0 ? o->param == g_ring_run_next : o->param == 0;
            if (lit && g_ring_run_t == 0) { r = 0.2f; g = 0.8f + 0.2f * pulse; b = 0.3f; }
            else if (lit) { r = 1.0f; g = 0.85f + 0.15f * pulse; b = 0.2f; }
            else if (g_ring_run_t > 0 && o->param < g_ring_run_next) { r = 0.25f; g = 0.3f; b = 0.4f; }
            else { r = 0.35f; g = 0.6f; b = 0.95f; }
        }
        glColor3f(r, g, b);
        draw_torus(o->param >= 0 ? 4.0f : 3.2f, o->param >= 0 ? 0.35f : 0.45f, 28, 6);
        glEnable(GL_LIGHTING);
        break;
    }
    case OT_COMET:
        glRotatef(o->t * 90, 0.3f, 1, 0.2f);
        glScalef(1.4f, 1.4f, 1.4f);
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.8f, 0.95f, 1.0f);
        glCallList(rock_list(1));
        break;
    case OT_METEOR:
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(DEG(o->pitch), 1, 0, 0);
        glScalef(o->radius, o->radius, o->radius);
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.55f, 0.85f, 1.0f);
        glCallList(rock_list(2));
        glScalef(0.7f, 1.25f, 0.7f);
        glColor3f(0.85f, 0.97f, 1.0f);
        glCallList(rock_list(3));
        break;
    case OT_ASTEROID:
        if (o->state == 1) break;
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(DEG(o->pitch), 1, 0, 0);
        glScalef(o->radius, o->radius * 0.85f, o->radius);
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.75f, 0.68f, 0.64f);
        glCallList(rock_list(o->param & 3));
        break;
    }
    glPopMatrix();
}

static vec3 cam_r, cam_u;
static void glow(vec3 p, float size, float r, float g, float b, float a)
{
    gfx_billboard_quad(p, size, r, g, b, a, cam_r, cam_u);
}

void space_draw_alpha(void)
{
    int i;
    float mv[16], sp;
    vec3 d;
    if (!lv.space) return;
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    cam_r = v3(mv[0], mv[4], mv[8]);
    cam_u = v3(mv[1], mv[5], mv[9]);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_GLOW]);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        if (o->type == OT_COMET) {
            int k;
            float a0 = o->f[0];
            glow(o->pos, 3.5f, 0.5f, 0.85f, 1.0f, 0.9f);
            /* the tail streams behind it along its orbit */
            for (k = 1; k < 26; k++) {
                Obj tmp = *o;
                float f = 1 - k / 26.0f;
                tmp.f[0] = a0 - k * 0.006f;
                comet_place(&tmp);
                glow(tmp.pos, 1.0f + k * 0.18f, 0.35f, 0.7f + 0.3f * f, 1.0f, 0.55f * f);
            }
        } else if (o->type == OT_METEOR) {
            glow(o->pos, o->radius * 2.4f, 0.3f, 0.6f, 1.0f, 0.35f + 0.1f * sinf(g_frame * 0.1f + i));
        } else if (o->type == OT_SPACERING && o->param < 0) {
            glow(o->pos, 2.0f, 1.0f, 0.5f, 0.15f, 0.4f);
        } else if (o->type == OT_SPACERING && g_ring_run_t > 0 && o->param == g_ring_run_next) {
            glow(o->pos, 5.5f, 1.0f, 0.85f, 0.2f, 0.25f + 0.15f * sinf(g_frame * 0.25f));
        } else if (o->type == OT_SPACERING && g_ring_run_t == 0 && o->param == 0) {
            glow(o->pos, 5.5f, 0.2f, 1.0f, 0.4f, 0.25f + 0.15f * sinf(g_frame * 0.25f));
        }
    }
    /* rocket flame when boosting */
    if (pl.act == ACT_SPACEFLY && pl.visible) {
        vec3 f = fly_dir(), e = v3mad(tux_center(), f, -0.9f);
        float k = clampf((pl.sp_speed - 20) / 50.0f, 0, 1.2f);
        glow(e, 0.6f + 0.9f * k + 0.1f * sinf(g_frame * 0.9f), 1.0f, 0.55f, 0.15f, 0.8f);
        glow(v3mad(e, f, -0.6f * k), 0.4f + 0.5f * k, 1.0f, 0.9f, 0.5f, 0.7f);
    }
    /* space dust streaking past: the faster Tux goes, the longer the streaks */
    glDisable(GL_TEXTURE_2D);
    sp = pl.act == ACT_SPACEFLY ? pl.sp_speed : 0;
    d = pl.act == ACT_SPACEFLY ? fly_dir() : v3(0, 0, 1);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    for (i = 0; i < 220; i++) {
        float hx = hash3(i, 1, 7) * 60, hy = hash3(i, 2, 7) * 60, hz = hash3(i, 3, 7) * 60;
        vec3 p = v3(fmodf(hx - cam.pos.x, 60.0f), fmodf(hy - cam.pos.y, 60.0f), fmodf(hz - cam.pos.z, 60.0f));
        float len = clampf((sp - 12) * 0.05f, 0.08f, 3.5f), dist, a;
        if (p.x < 0) p.x += 60;
        if (p.y < 0) p.y += 60;
        if (p.z < 0) p.z += 60;
        p = v3add(v3sub(p, v3(30, 30, 30)), cam.pos);
        dist = v3dist(p, cam.pos);
        a = clampf(1.0f - dist / 30.0f, 0, 1) * clampf(0.25f + sp / 60.0f, 0, 1);
        glColor4f(0.7f, 0.85f, 1.0f, a);
        glVertex3f(p.x, p.y, p.z);
        glColor4f(0.7f, 0.85f, 1.0f, 0);
        glVertex3f(p.x - d.x * len, p.y - d.y * len, p.z - d.z * len);
    }
    glEnd();
    glLineWidth(1);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

/* the backdrop: drawn around the camera with depth testing off (game.c draw_sky) */
void space_draw_sky(void)
{
    int i, j;
    vec3 sun = v3norm(v3(0.35f, 0.85f, 0.4f)), gp = v3norm(v3(-0.7f, 0.18f, -0.68f));
    float mv[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    cam_r = v3(mv[0], mv[4], mv[8]);
    cam_u = v3(mv[1], mv[5], mv[9]);
    glEnable(GL_BLEND);
    /* stars in every direction, a few bright ones and a milky band */
    glPointSize(1);
    glBegin(GL_POINTS);
    for (i = 0; i < 1400; i++) {
        float u = hash3(i, 11, 3) * 2 - 1, th = hash3(i, 12, 3) * TAU_F, s = sqrtf(1 - u * u), b;
        vec3 p = v3(s * cosf(th), u, s * sinf(th));
        if (i < 700) { p.y = p.y * 0.15f + p.x * 0.3f; p = v3norm(p); }   /* the band */
        b = 0.35f + 0.65f * hash3(i, 13, 3);
        glColor4f(b, b, b * (0.85f + 0.3f * hash3(i, 14, 3)), 1);
        glVertex3f(p.x * 45, p.y * 45, p.z * 45);
    }
    glEnd();
    glPointSize(2.5f);
    glBegin(GL_POINTS);
    for (i = 0; i < 90; i++) {
        float u = hash3(i, 21, 3) * 2 - 1, th = hash3(i, 22, 3) * TAU_F, s = sqrtf(1 - u * u);
        float tw = 0.6f + 0.4f * sinf(g_frame * 0.06f + i * 1.7f);
        glColor4f(tw, tw * 0.95f, tw * (0.8f + 0.2f * hash3(i, 23, 3)), 1);
        glVertex3f(s * cosf(th) * 45, u * 45, s * sinf(th) * 45);
    }
    glEnd();
    glPointSize(1);
    /* nebulae and the sun */
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_GLOW]);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (i = 0; i < 14; i++) {
        vec3 c = v3norm(v3(-0.4f + 0.12f * i * cosf(i * 1.3f), 0.35f + 0.25f * sinf(i * 2.1f), 0.8f - 0.05f * i));
        glow(v3scale(c, 44), 7 + 3 * hash3(i, 31, 3), 0.55f + 0.3f * (i % 3 == 0), 0.2f + 0.25f * (i % 3 == 1), 0.85f, 0.18f);
    }
    for (i = 0; i < 9; i++) {
        vec3 c = v3norm(v3(0.6f - 0.1f * i, -0.45f + 0.2f * sinf(i * 1.7f), -0.55f + 0.08f * i));
        glow(v3scale(c, 44), 6 + 3 * hash3(i, 32, 3), 0.15f, 0.6f + 0.2f * (i & 1), 0.75f, 0.16f);
    }
    glow(v3scale(sun, 44), 9, 1.0f, 0.85f, 0.55f, 0.6f);
    glow(v3scale(sun, 44), 3.5f, 1.0f, 1.0f, 0.9f, 1.0f);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /* a giant striped planet far away, lit from the sun's side */
    {
        vec3 c = v3scale(gp, 40), ax = v3norm(v3cross(gp, v3(0, 1, 0))), ay = v3cross(ax, gp);
        float R = 6.5f;
        for (j = 0; j < 12; j++) {
            float p0 = PI_F * j / 12 - PI_F / 2, p1 = PI_F * (j + 1) / 12 - PI_F / 2;
            glBegin(GL_QUAD_STRIP);
            for (i = 0; i <= 20; i++) {
                float th = TAU_F * i / 20;
                int k;
                for (k = 0; k < 2; k++) {
                    float ph = k ? p1 : p0;
                    vec3 n = v3add(v3add(v3scale(ax, cosf(ph) * cosf(th)), v3scale(ay, sinf(ph))), v3scale(gp, -cosf(ph) * sinf(th)));
                    float l = clampf(v3dot(n, sun) * 0.8f + 0.35f, 0.08f, 1), band = 0.75f + 0.25f * sinf(ph * 9.0f);
                    glColor4f(0.85f * l * band, 0.6f * l * band, 0.95f * l, 1);
                    glVertex3f(c.x + n.x * R, c.y + n.y * R, c.z + n.z * R);
                }
            }
            glEnd();
        }
        /* its ring */
        glBegin(GL_QUAD_STRIP);
        for (i = 0; i <= 40; i++) {
            float th = TAU_F * i / 40;
            vec3 dir = v3add(v3scale(ax, cosf(th)), v3scale(v3norm(v3add(v3scale(ay, 0.3f), v3scale(gp, 0.95f))), sinf(th)));
            glColor4f(0.9f, 0.8f, 0.95f, 0.5f);
            glVertex3f(c.x + dir.x * R * 1.4f, c.y + dir.y * R * 1.4f, c.z + dir.z * R * 1.4f);
            glColor4f(0.7f, 0.6f, 0.9f, 0.1f);
            glVertex3f(c.x + dir.x * R * 2.0f, c.y + dir.y * R * 2.0f, c.z + dir.z * R * 2.0f);
        }
        glEnd();
    }
}

/* ---------------------------------------------------------------- camera */
/* a chase camera right behind the rocket; the field of view widens with speed */
void space_camera(void)
{
    vec3 f = fly_dir(), c = tux_center(), want, aim, n;
    float sp = pl.sp_speed, dist = 8.0f + minf(sp, 80) * 0.035f, t, fov;
    if (cam.mode != CAM_FOLLOW) return;
    want = v3add(v3scale(f, -dist), v3(0, 2.6f - fabsf(f.y) * 1.0f, 0));
    aim = v3add(v3mad(c, f, 8.0f), v3(0, 1.4f, 0));
    if (!cam_ok) { cam_off = want; cam_ok = 1; cam.at = aim; }
    cam_off = v3lerp(cam_off, want, minf(1.0f, (pl.sp_turn > 0 ? 3.0f : 6.0f) * DT));
    cam.pos = v3add(c, cam_off);
    if (col_ray(c, cam.pos, &t, &n, 1)) cam.pos = v3lerp(c, cam.pos, maxf(t - 0.08f, 0.1f));
    cam.at = v3lerp(cam.at, aim, minf(1.0f, 12.0f * DT));
    cam.yaw = yaw_to(cam.pos, pl.pos);
    fov = 55.0f + clampf((sp - 24) * 0.5f, -4, 26);
    cam.fov = approachf(cam.fov, fov, 40.0f * DT);
    if (cam.shake > 0) {
        cam.pos = v3add(cam.pos, v3(frands() * cam.shake, frands() * cam.shake, frands() * cam.shake));
        cam.shake -= DT;
    }
}
