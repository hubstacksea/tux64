/* vertical.c - UPTIME HEIGHTS (skyscrapers, all about height) and CORE DUMP CAVERNS (all about depth)
 *
 * UPTIME HEIGHTS: Tux starts on a rooftop at dusk, a layer of cloud far below his feet and the
 * street lights of the city deeper still. Gusts of wind sweep the girders between the towers, a
 * window washer's gondola climbs a facade, a tower crane swings its jib, an express elevator races
 * up the tallest tower and a skydive through rings ends in a rooftop pool.
 *
 * CORE DUMP CAVERNS: a giant shaft drilled down into the earth. A spiral ledge winds down past
 * glowing crystals, falling stalactites and a waterfall to a lake of magma 290 m below. The light
 * from the opening fades with depth; a mine cart plunges down its own spiral track and glowing
 * mushrooms bounce Tux up a chimney. */
#include "game.h"
#include <stdio.h>
#include <string.h>

#define PR 0.42f

int g_crystals_left;
static int crystals_total;
static float gust_t, track_len = 1;
static GLuint dl_cart, dl_mush, dl_crystal, dl_crystal_lit, dl_stalac;

static float hash2(int x, int y, int seed)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

/* ---------------------------------------------------------------- the cave layout (shared with levels.c) */
/* the spiral ledge: two turns from the surface (angle 0, y 0) down to the bottom (angle 4 pi, y -290) */
float cave_walk_y(float a) { return -290.0f * clampf(a, 0, CAVE_WALK_TURNS * TAU_F) / (CAVE_WALK_TURNS * TAU_F); }

vec3 cave_polar(float a, float r, float y) { return v3(sinf(a) * r, y, cosf(a) * r); }

/* the mine cart track: a steeper spiral at radius 27, flat at both stations */
vec3 cave_track_point(float u)
{
    float a = u * CAVE_TRACK_ANGLE, y;
    if (a < 0.3f) y = -4.0f;
    else if (a > CAVE_TRACK_ANGLE - 0.3f) y = -200.0f;
    else y = lerpf(-4.0f, -200.0f, (a - 0.3f) / (CAVE_TRACK_ANGLE - 0.6f));
    return cave_polar(a, 27.0f, y);
}

void vert_reset(void)
{
    int i;
    g_crystals_left = crystals_total = 0;
    gust_t = 0;
    track_len = 0;
    for (i = 0; i < 500; i++) track_len += v3dist(cave_track_point(i / 500.0f), cave_track_point((i + 1) / 500.0f));
    for (i = 0; i < nobj; i++)
        if (objs[i].active && objs[i].type == OT_CAVECRYSTAL) { g_crystals_left++; crystals_total++; }
}

/* ---------------------------------------------------------------- per-frame */
void vert_update(void)
{
    if (lv.tall_world == 1) {
        /* gusts sweep across the girders high above the street (crouch with Z to hold on). Every 7 s:
         * 4 s calm, a whistle warns of the gust, then it builds up, blows and dies down. */
        float ph;
        int on_girder = pl.pos.y > 100 && fabsf(pl.pos.z) < 6 && pl.pos.x > 15 && pl.pos.x < 54;
        gust_t += DT;
        ph = fmodf(gust_t, 7.0f);
        g_gust = ph > 4.0f ? sinf((ph - 4.0f) / 3.0f * PI_F) : 0;
        g_gust = g_gust * g_gust;
        if (on_girder && ph > 3.0f && ph - DT <= 3.0f) sfx_ex(SFX_THROW, 0.8f, 0.3f);   /* the warning whistle */
        g_wind = on_girder ? v3(0, 0, ((int)(gust_t / 7.0f) & 1 ? 1 : -1) * 3.2f * g_gust) : v3(0, 0, 0);
        if (on_girder && g_gust > 0.2f && g_frame % 20 == 0) sfx_ex(SFX_SLIDE, 0.5f * g_gust, 0.45f);
    }
    if (lv.tall_world == 2) {
        /* the deeper Tux goes, the darker it gets */
        float k = clampf(-pl.pos.y / 70.0f, 0, 1), heat = clampf((-pl.pos.y - 200) / 90.0f, 0, 1);
        static const float day[3] = { 0.75f, 0.85f, 0.95f };
        int c;
        for (c = 0; c < 3; c++) {
            float deep = c == 0 ? 0.05f + 0.25f * heat : (c == 1 ? 0.04f + 0.06f * heat : 0.07f);
            lv.fog[c] = lerpf(day[c], deep, k);
            lv.sky_bot[c] = lv.fog[c];
            lv.sky_top[c] = lerpf(c == 2 ? 0.95f : 0.55f, deep * 0.5f, k);
        }
        lv.fog_start = lerpf(80, 18, k);
        lv.fog_end = lerpf(320, 115, k);
        lv.ambient = lerpf(0.6f, 0.42f, k);
    }
}

/* ---------------------------------------------------------------- CORE DUMP CAVERNS objects */
static void cart_update(Obj *o)
{
    /* f[0] = position along the track (0..1), f[1] = speed (units/s); state 0 waiting, 1 riding, 2 arrived */
    float len = track_len;
    vec3 a, b;
    o->timer++;
    if (o->state == 0) {
        if (v3dist(o->pos, pl.pos) < 1.6f && pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE) {
            o->state = 1;
            o->f[1] = 6;
            sfx(SFX_DOOR);
            hud_message("All aboard! Hold on tight!", 90);
        }
    } else if (o->state == 1) {
        float slope;
        a = cave_track_point(o->f[0]);
        b = cave_track_point(minf(o->f[0] + 0.002f, 1));
        slope = (a.y - b.y) / maxf(v3dist(a, b), 0.01f);
        o->f[1] = clampf(o->f[1] + (slope * 26.0f - 1.5f) * DT, 7.0f, 30.0f);
        if (o->f[0] > 0.97f) o->f[1] = approachf(o->f[1], 4.0f, 30 * DT);
        o->f[0] += o->f[1] * DT / len;
        if (o->timer % 8 == 0) sfx_ex(SFX_STEP, 0.4f, 0.6f + o->f[1] * 0.03f);
        if (o->f[1] > 20 && o->timer % 3 == 0) fx_spawn(OT_FX_SPARKLE, o->pos, 0.6f);
        if (o->f[0] >= 1) {
            o->f[0] = 1;
            o->state = 2;
            pl.no_ctrl = 0;
            player_set_act(ACT_IDLE);
            player_bounce(9);
            sfx(SFX_THUD);
        }
    }
    a = cave_track_point(minf(o->f[0] + 0.001f, 1));
    b = cave_track_point(maxf(o->f[0] - 0.001f, 0));
    o->pos = cave_track_point(o->f[0]);
    o->yaw = atan2f(a.x - b.x, a.z - b.z);
    o->pitch = atan2f(a.y - b.y, v3distxz(a, b));
    if (o->state == 1) {
        /* Tux rides inside the cart */
        pl.no_ctrl = 1;
        if (pl.act != ACT_FROZEN) player_set_act(ACT_FROZEN);
        pl.pos = v3add(o->pos, v3(0, 0.45f, 0));
        pl.vel = v3(0, 0, 0);
        pl.floor_y = pl.pos.y;
        pl.yaw = o->yaw;
    }
}

static void stalac_update(Obj *o)
{
    float d = v3distxz(o->pos, pl.pos);
    o->timer++;
    switch (o->state) {
    case 0:   /* hanging; trembles when Tux walks underneath */
        if (d < 2.6f && pl.pos.y < o->pos.y && pl.pos.y > o->pos.y - 14) { o->state = 1; o->timer = 0; sfx_ex(SFX_BREAK, 0.4f, 1.6f); }
        break;
    case 1:
        o->f[0] = sinf(o->timer * 1.3f) * 0.06f;
        if (o->timer > 28) { o->state = 2; o->timer = 0; o->vel.y = 0; o->f[0] = 0; }
        break;
    case 2: {   /* falling */
        Surf *s;
        float fy;
        o->vel.y -= 40 * DT;
        o->pos.y += o->vel.y * DT;
        if (v3distxz(o->pos, pl.pos) < 0.7f + PR && pl.pos.y < o->pos.y && pl.pos.y + 1.6f > o->pos.y - 2.4f) player_hurt(2, o->pos);
        fy = col_floor(o->pos.x, o->pos.y, o->pos.z, &s);
        if ((s && o->pos.y - 2.4f <= fy) || o->timer > 200) {
            fx_spawn(OT_FX_EXPLODE, v3(o->pos.x, s ? fy + 0.3f : o->pos.y, o->pos.z), 0.4f);
            sfx(SFX_BREAK);
            o->state = 3;
            o->timer = 0;
        }
        break;
    }
    case 3:   /* grows back */
        if (o->timer > 480) { o->state = 0; o->pos = o->home; fx_spawn(OT_FX_SPARKLE, o->pos, 1.0f); }
        break;
    }
}

static void mush_update(Obj *o)
{
    float top = o->pos.y + 1.25f;
    o->f[0] = maxf(0, o->f[0] - DT * 3);
    if (player_in_air() && pl.vel.y < 0 && v3distxz(o->pos, pl.pos) < 1.6f && pl.pos.y <= top + 0.3f && pl.pos.y > top - 0.9f) {
        pl.pos.y = top;
        player_set_act(ACT_TRAMP);
        pl.vel.y = 27.5f;
        pl.peak_y = pl.pos.y;
        o->f[0] = 1;
        sfx_ex(SFX_BOUNCE, 0.9f, 0.8f);
    }
}

static void crystal_update(Obj *o)
{
    if (o->state == 1) return;
    if ((player_is_attacking() && v3distxz(o->pos, pl.pos) < 1.9f && fabsf(o->pos.y - pl.pos.y) < 2.2f) ||
        (v3distxz(o->pos, pl.pos) < 1.0f + PR && pl.pos.y > o->pos.y + 1.5f && !player_in_air())) {
        o->state = 1;
        g_crystals_left--;
        sfx(SFX_REDCOIN);
        fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(0, 1.5f, 0)), 1.5f);
        if (g_crystals_left > 0) {
            static char msg[64];
            snprintf(msg, sizeof msg, tr("A crystal lights up! %d left"), g_crystals_left);
            hud_message(msg, 90);
        } else {
            hud_message("All five crystals are glowing!", 150);
            star_spawn(v3add(o->pos, v3(0, 4.0f, 0)), 3, 1, o->pos);
        }
    }
}

void vert_obj_update(Obj *o)
{
    switch (o->type) {
    case OT_MINECART: cart_update(o); break;
    case OT_STALACTITE: stalac_update(o); break;
    case OT_MUSHROOM: mush_update(o); break;
    case OT_CAVECRYSTAL: crystal_update(o); break;
    }
}

/* ---------------------------------------------------------------- models */
static Mesh M;
static void mb_start(void) { mesh_init(&M); mb_begin(&M); }
static GLuint mb_finish(void) { GLuint d; mb_end(); d = mesh_compile(&M); mesh_free(&M); return d; }

static void build_models(void)
{
    int k;
    if (dl_cart) return;
    mb_start();   /* mine cart, front +z */
    mb_color(0.45f, 0.3f, 0.2f);
    mb_cuboid(-0.75f, 0.25f, -1.0f, 0.75f, 0.35f, 1.0f);
    mb_color(0.55f, 0.38f, 0.25f);
    mb_cuboid(-0.8f, 0.35f, -1.05f, -0.68f, 1.15f, 1.05f);
    mb_cuboid(0.68f, 0.35f, -1.05f, 0.8f, 1.15f, 1.05f);
    mb_cuboid(-0.8f, 0.35f, -1.05f, 0.8f, 1.15f, -0.93f);
    mb_cuboid(-0.8f, 0.35f, 0.93f, 0.8f, 1.15f, 1.05f);
    mb_color(0.35f, 0.35f, 0.38f);
    mb_cuboid(-0.85f, 1.05f, -1.1f, 0.85f, 1.18f, -0.98f);
    mb_cuboid(-0.85f, 1.05f, 0.98f, 0.85f, 1.18f, 1.1f);
    mb_color(0.2f, 0.2f, 0.22f);
    for (k = 0; k < 4; k++) {
        mb_push();
        mb_translate((k & 1) ? 0.62f : -0.62f, 0.22f, (k & 2) ? 0.65f : -0.65f);
        mb_rotate(90, 0, 0, 1);
        mb_scale(0.24f, 0.12f, 0.24f);
        mb_translate(0, -0.5f, 0);
        mb_cylinder(10, 1);
        mb_pop();
    }
    dl_cart = mb_finish();
    mb_start();   /* glowing mushroom: stem and a wide cap (top at y 1.25) */
    mb_color(0.9f, 0.88f, 0.8f);
    mb_push(); mb_scale(0.45f, 0.9f, 0.45f); mb_cylinder(10, 1); mb_pop();
    mb_color(0.65f, 0.3f, 0.95f);
    mb_push(); mb_translate(0, 0.75f, 0); mb_scale(1.6f, 0.5f, 1.6f); mb_hemisphere(14, 5); mb_pop();
    mb_color(0.95f, 0.85f, 1.0f);
    for (k = 0; k < 6; k++) mb_ellipsoid(sinf(k * 1.05f) * 0.95f, 1.08f, cosf(k * 1.05f) * 0.95f, 0.17f, 0.08f, 0.17f, 6);
    dl_mush = mb_finish();
    for (k = 0; k < 2; k++) {   /* crystal cluster, dark or glowing */
        int i;
        mb_start();
        if (k) mb_color(0.55f, 0.95f, 1.0f); else mb_color(0.18f, 0.22f, 0.32f);
        for (i = 0; i < 5; i++) {
            float a = i * 1.26f, tilt = i ? 22.0f : 0;
            mb_push();
            mb_rotate(DEG(a), 0, 1, 0);
            mb_rotate(tilt, 1, 0, 0);
            mb_scale(i ? 0.35f : 0.55f, i ? 1.8f : 2.8f, i ? 0.35f : 0.55f);
            mb_cylinder(6, 0);
            mb_translate(0, 1, 0);
            mb_scale(1, 0.35f, 1);
            mb_cone(6);
            mb_pop();
        }
        if (k) dl_crystal_lit = mb_finish(); else dl_crystal = mb_finish();
    }
    mb_start();   /* stalactite: a rock cone pointing down (tip at y -2.4) */
    mb_color(0.55f, 0.6f, 0.68f);
    mb_push(); mb_scale(0.7f, -2.4f, 0.7f); mb_cone(8); mb_pop();
    mb_color(0.8f, 0.9f, 1.0f);
    mb_push(); mb_translate(0, -1.4f, 0); mb_scale(0.32f, -1.0f, 0.32f); mb_cone(8); mb_pop();
    dl_stalac = mb_finish();
}

static void draw_at(GLuint dl, vec3 p, float yaw, float pitch, float s)
{
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(DEG(yaw), 0, 1, 0);
    if (pitch != 0) glRotatef(-DEG(pitch), 1, 0, 0);
    if (s != 1) glScalef(s, s, s);
    glCallList(dl);
    glPopMatrix();
}

void vert_obj_draw(Obj *o)
{
    build_models();
    switch (o->type) {
    case OT_MINECART: draw_at(dl_cart, o->pos, o->yaw, o->pitch, 1); break;
    case OT_STALACTITE:
        if (o->state == 3) break;
        draw_at(dl_stalac, v3add(o->pos, v3(o->f[0], 0, 0)), o->yaw, 0, 1);
        break;
    case OT_MUSHROOM: {
        float sq = 1 - 0.25f * sinf(o->f[0] * PI_F);
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glScalef(1 + (1 - sq) * 0.5f, sq, 1 + (1 - sq) * 0.5f);
        glDisable(GL_LIGHTING);
        glCallList(dl_mush);
        glEnable(GL_LIGHTING);
        glPopMatrix();
        break;
    }
    case OT_CAVECRYSTAL:
        if (o->state) glDisable(GL_LIGHTING);
        draw_at(o->state ? dl_crystal_lit : dl_crystal, o->pos, o->yaw, 0, 1);
        glEnable(GL_LIGHTING);
        break;
    }
}

/* ---------------------------------------------------------------- transparent effects */
static vec3 cam_r, cam_u;

static void glow(vec3 p, float size, float r, float g, float b, float a)
{
    gfx_billboard_quad(p, size, r, g, b, a, cam_r, cam_u);
}

static void city_alpha(void)
{
    int i, k;
    /* streams of car lights in the streets far below */
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_GLOW]);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (i = -5; i <= 5; i++)
        for (k = 0; k < 26; k++) {
            float lane = i * 60.0f + 8, ph = hash2(i, k, 3), sp = 22 + 10 * hash2(i, k, 4);
            float t = fmodf(ph * 700 + g_frame * DT * sp, 700) - 350;
            int dir = k & 1;
            float off = dir ? 2.5f : -2.5f, s = dir ? 1 : -1;
            vec3 p1 = v3(lane + off, 0.8f, t * s), p2 = v3(t * s, 0.8f, lane + off);
            float r = dir ? 1.0f : 1.0f, g = dir ? 0.95f : 0.25f, b = dir ? 0.8f : 0.2f;
            glow(p1, 3.0f, r, g, b, 0.9f);
            glow(p2, 3.0f, r, g, b, 0.9f);
        }
    /* blinking red aircraft warning lights on the tallest towers */
    if ((g_frame / 40) % 2 == 0) {
        glow(v3(0, 268.5f, -75), 4.0f, 1, 0.15f, 0.1f, 1);
        glow(v3(-66, 191, -30), 3.0f, 1, 0.15f, 0.1f, 1);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /* the cloud layer between the towers, far below the rooftops */
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_CLOUD]);
    for (i = 0; i < 70; i++) {
        float a = hash2(i, 1, 9) * TAU_F, r = 40 + hash2(i, 2, 9) * 300;
        float drift = fmodf(g_frame * DT * 1.5f + hash2(i, 3, 9) * 600, 600) - 300;
        vec3 c = v3(cosf(a) * r + drift * 0.3f, 82 + hash2(i, 4, 9) * 12, sinf(a) * r);
        float s = 30 + hash2(i, 5, 9) * 40;
        glBegin(GL_QUADS);
        glColor4f(1.0f, 0.82f, 0.75f, 0.55f);
        glTexCoord2f(0, 0); glVertex3f(c.x - s, c.y, c.z - s);
        glTexCoord2f(1, 0); glVertex3f(c.x + s, c.y, c.z - s);
        glTexCoord2f(1, 1); glVertex3f(c.x + s, c.y, c.z + s);
        glTexCoord2f(0, 1); glVertex3f(c.x - s, c.y, c.z + s);
        glEnd();
    }
    /* an airship slowly circling the skyline */
    {
        float a = g_frame * DT * 0.03f;
        vec3 p = v3(sinf(a) * 170, 165, cosf(a) * 170);
        int j;
        glDisable(GL_TEXTURE_2D);
        glPushMatrix();
        glTranslatef(p.x, p.y, p.z);
        glRotatef(DEG(a + PI_F / 2), 0, 1, 0);
        glScalef(7, 7, 22);
        for (j = 0; j < 8; j++) {
            float a0 = PI_F * j / 8 - PI_F / 2, a1 = PI_F * (j + 1) / 8 - PI_F / 2;
            glBegin(GL_QUAD_STRIP);
            for (k = 0; k <= 12; k++) {
                float b = TAU_F * k / 12, sh = 0.55f + 0.35f * cosf(b - 1.0f);
                glColor4f(0.85f * sh, 0.85f * sh, 0.9f * sh, 1);
                glVertex3f(cosf(a1) * sinf(b), cosf(a1) * cosf(b), sinf(a1));
                glVertex3f(cosf(a0) * sinf(b), cosf(a0) * cosf(b), sinf(a0));
            }
            glEnd();
        }
        glPopMatrix();
        glEnable(GL_TEXTURE_2D);
    }
}

static void cave_alpha(void)
{
    int i;
    glDisable(GL_FOG);
    /* daylight pouring in through the opening */
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (i = 0; i < 5; i++) {
        float a = i * 1.3f + 0.4f, r = 10 + i * 5;
        vec3 top = v3(sinf(a) * r, 8, cosf(a) * r), bot = v3(sinf(a) * r * 0.6f - 6, -120, cosf(a) * r * 0.6f + 4);
        float w = 4 + i;
        vec3 side = v3norm(v3cross(v3sub(bot, top), v3sub(cam.pos, top)));
        glBegin(GL_QUADS);
        glColor4f(1.0f, 0.97f, 0.85f, 0.10f);
        glVertex3f(top.x - side.x * w, top.y, top.z - side.z * w);
        glVertex3f(top.x + side.x * w, top.y, top.z + side.z * w);
        glColor4f(1.0f, 0.97f, 0.85f, 0.0f);
        glVertex3f(bot.x + side.x * w, bot.y, bot.z + side.z * w);
        glVertex3f(bot.x - side.x * w, bot.y, bot.z - side.z * w);
        glEnd();
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_GLOW]);
    /* crystals in the walls glow far into the dark */
    for (i = 0; i < 90; i++) {
        float a = hash2(i, 1, 5) * TAU_F, y = -30 - hash2(i, 2, 5) * 250, r = 39.5f;
        float tw = 0.75f + 0.25f * sinf(g_frame * 0.03f + i);
        vec3 p = cave_polar(a, r, y);
        int hue = i % 3;
        glow(p, 2.5f + hash2(i, 3, 5) * 2.5f, hue == 2 ? 0.8f : 0.3f, hue == 1 ? 1.0f : 0.7f, 1.0f, 0.5f * tw);
    }
    /* lit crystals and mushrooms */
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        if (o->type == OT_CAVECRYSTAL && o->state == 1) glow(v3add(o->pos, v3(0, 1.6f, 0)), 9.0f, 0.4f, 0.9f, 1.0f, 0.7f);
        if (o->type == OT_MUSHROOM) glow(v3add(o->pos, v3(0, 1.1f, 0)), 5.0f, 0.75f, 0.4f, 1.0f, 0.45f + 0.3f * o->f[0]);
    }
    /* the magma far below, and embers rising from it */
    for (i = 0; i < 12; i++) {
        float a = TAU_F * i / 12 + g_frame * 0.002f;
        glow(v3(sinf(a) * 18, -288, cosf(a) * 18), 40, 1.0f, 0.35f, 0.05f, 0.35f);
    }
    for (i = 0; i < 80; i++) {
        float ph = fmodf(hash2(i, 7, 3) + g_frame * 0.0025f * (0.6f + hash2(i, 8, 3)), 1.0f);
        float a = hash2(i, 9, 3) * TAU_F + ph * 2, r = 4 + hash2(i, 10, 3) * 30;
        glow(cave_polar(a, r, -292 + ph * 90), 0.8f, 1.0f, 0.55f, 0.15f, 0.9f * (1 - ph));
    }
    /* the waterfall spray where it disappears into the dark */
    for (i = 0; i < 20; i++) {
        float ph = fmodf(g_frame * 0.01f + i * 0.05f, 1.0f);
        glow(v3add(cave_polar(2.2f, 38.5f - ph * 2, -30 - ph * 60), v3(0, 0, 0)), 3.0f + ph * 3, 0.7f, 0.85f, 1.0f, 0.25f * (1 - ph));
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_FOG);
}

void vert_draw_alpha(void)
{
    float mv[16];
    if (!lv.tall_world) return;
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    cam_r = v3(mv[0], mv[4], mv[8]);
    cam_u = v3(mv[1], mv[5], mv[9]);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    if (lv.tall_world == 1) city_alpha();
    else cave_alpha();
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
}

/* altitude / depth meter: a scale bar with Tux's marker */
void vert_hud(void)
{
    char buf[64];
    float x = g_vw - 44, y0 = 120, y1 = 380, k;
    if (!lv.tall_world) return;
    if (lv.tall_world == 1) {
        k = clampf(pl.pos.y / 280.0f, 0, 1);
        snprintf(buf, sizeof buf, tr("ALT %dm"), (int)maxf(pl.pos.y, 0));
    } else {
        k = clampf(1 + pl.pos.y / 290.0f, 0, 1);
        snprintf(buf, sizeof buf, tr("DEPTH %dm"), (int)maxf(-pl.pos.y, 0));
    }
    gfx_rect(x, y0, 8, y1 - y0, 0.05f, 0.05f, 0.1f, 0.6f);
    if (lv.tall_world == 1) gfx_rect(x + 1, y1 - (y1 - y0) * k, 6, (y1 - y0) * k, 0.5f, 0.75f, 1.0f, 0.8f);
    else gfx_rect(x + 1, y0, 6, (y1 - y0) * (1 - k), 0.9f, 0.4f, 0.15f, 0.8f);
    gfx_rect(x - 6, y1 - (y1 - y0) * k - 2, 20, 4, 1, 1, 1, 1);
    gfx_text_shadow(x - gfx_text_width(buf, 1.5f) + 16, y0 - 24, 1.5f, buf, 0.9f, 0.95f, 1, 1);
}
