/* scale.c - NANO VILLAGE (Tux is a giant) and HUGEPAGE HOUSE (Tux is tiny)
 *
 * NANO VILLAGE: a Lilliput of tiny penguins. Tux strides over their houses, wades through their sea
 * and towers above clouds that drift at knee height. He tows the enemy fleet into the harbour like
 * Gulliver, carries a rain cloud over forest fires and stomps a tiny invading army into retreat.
 *
 * HUGEPAGE HOUSE: a giant's living room. A toaster launches Tux to the top shelf, a giant cat sleeps
 * on the star and only a ball of yarn will lure it away, and the giant's slippers stomp down the
 * hallway in front of the mouse hole. */
#include "game.h"
#include <stdio.h>
#include <string.h>

#define PR 0.42f   /* player radius */
#define PH 1.6f    /* player height */

int g_nano_ships_left, g_nano_fires_left, g_nano_catapults_left;
vec3 g_nano_harbor, g_nano_windmill;
static int ships_total, fires_total, catapults_total;
static int towed_n;
static int cat_star_out;
static float train_s, train_len;
static int was_air;
static int step_t;
static GLuint dl_ship, dl_ship_blue, dl_cata, dl_cata_arm, dl_loco, dl_car, dl_cat_body, dl_cat_head, dl_cat_sleep, dl_slipper, dl_toaster, dl_yarn;

/* ---------------------------------------------------------------- the tiny railway */
/* a closed loop around the village (u in 0..1) */
vec3 nano_track_point(float u)
{
    float a = TAU_F * u;
    return v3(2.0f + sinf(a) * 31.0f + sinf(a * 3) * 2.5f, 0, 2.0f + cosf(a) * 22.0f + cosf(a * 2) * 2.0f);
}

static float ground_at(float x, float z, float ytop)
{
    Surf *s;
    float y = col_floor(x, ytop, z, &s);
    return s ? y : 0;
}

static vec3 track_at(float s)
{
    float u = s / train_len;
    vec3 p;
    u -= floorf(u);
    p = nano_track_point(u);
    p.y = ground_at(p.x, p.z, 4.0f);
    return p;
}

void scale_reset(void)
{
    int i;
    g_nano_ships_left = g_nano_fires_left = g_nano_catapults_left = 0;
    ships_total = fires_total = catapults_total = 0;
    towed_n = 0;
    cat_star_out = 0;
    train_s = 0;
    was_air = 0;
    step_t = 0;
    train_len = 0;
    for (i = 0; i < 400; i++) train_len += v3distxz(nano_track_point(i / 400.0f), nano_track_point((i + 1) / 400.0f));
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        if (o->type == OT_NANOSHIP) { g_nano_ships_left++; ships_total++; }
        if (o->type == OT_NANOFIRE) { g_nano_fires_left++; fires_total++; }
        if (o->type == OT_NANOCATAPULT) { g_nano_catapults_left++; catapults_total++; }
    }
}

static vec3 tux_center(void) { return v3add(pl.pos, v3(0, 0.8f, 0)); }

static float hash2(int x, int y, int seed)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
}

static int tux_grounded(void) { return !player_in_air() && pl.act != ACT_SWIM; }

/* landing events, latched once per frame in scale_update (before the objects run) */
static int ev_pound, ev_land, prev_pound, prev_air;
static int gpound_now(void) { return ev_pound; }
static int landed_now(void) { return ev_land; }

static int tux_attacking_near(Obj *o, float r)
{
    if (!player_is_attacking()) return 0;
    return v3distxz(o->pos, pl.pos) < r + PR && fabsf(o->pos.y - pl.pos.y) < 1.4f;
}

void scale_update(void)
{
    int pound = pl.act == ACT_GPOUND_LAND, air = player_in_air();
    ev_pound = pound && !prev_pound;
    ev_land = prev_air && !air && pl.act != ACT_SWIM;
    prev_pound = pound;
    prev_air = air;
    if (!lv.scale_world) return;
    if (lv.scale_world == 1) {
        /* the giant's footsteps: every stride shakes the camera a little and kicks up dust */
        int air = player_in_air();
        if (was_air && !air && pl.act != ACT_SWIM) {
            float k = clampf((pl.peak_y - pl.pos.y) / 6.0f, 0.15f, 1.0f);
            cam.shake = maxf(cam.shake, (pl.act == ACT_GPOUND_LAND ? 0.5f : 0.22f) * k + 0.08f);
            Obj *ring = obj_spawn(OT_FX_SHOCKWAVE, pl.pos, 0, 0);
            if (ring) { ring->flags = 0; ring->f[2] = 1; ring->f[3] = pl.act == ACT_GPOUND_LAND ? 4.0f : 2.2f; }
            sfx_ex(SFX_THUD, 0.6f + 0.4f * k, 0.55f);
        }
        was_air = air;
        if (!air && pl.act == ACT_WALK && pl.fwd > 5.5f && pl.water_y < -1e8f) {
            if (++step_t >= 15) {
                vec3 foot = v3mad(pl.pos, yaw_dir(pl.yaw), 0.2f);
                step_t = 0;
                cam.shake = maxf(cam.shake, 0.06f);
                fx_spawn(OT_FX_SMOKE, v3add(foot, v3(0, 0.1f, 0)), 0.5f);
                sfx_ex(SFX_THUD, 0.35f, 0.7f);
            }
        } else step_t = 10;
    }
    if (lv.scale_world == 2) {
        /* the giant wall clock ticks through the whole room */
        if (g_frame % 60 == 0) sfx_ex(SFX_STEP, 0.4f, 2.6f);
        if (g_frame % 60 == 30) sfx_ex(SFX_STEP, 0.3f, 2.1f);
    }
}

/* ---------------------------------------------------------------- NANO VILLAGE objects */
static void folk_update(Obj *o)
{
    float d = v3distxz(o->pos, pl.pos), spd = 0;
    int near_run = d < 3.2f && tux_grounded() && pl.fwd > 5.0f;
    o->timer++;
    /* hop when the giant lands close by */
    if ((landed_now() || gpound_now()) && d < 7.0f && o->f[3] <= 0) o->f[2] = 3.0f + frand() * 1.5f;
    if (o->f[2] != 0 || o->f[3] > 0) {
        o->f[3] += o->f[2] * DT;
        o->f[2] -= 14.0f * DT;
        if (o->f[3] <= 0) { o->f[3] = 0; o->f[2] = 0; }
    }
    switch (o->state) {
    case 0:   /* strolling about */
        if (o->timer % 150 == (o->param * 37) % 150) o->f[1] = o->yaw + frands() * 2.5f;
        if (v3distxz(o->pos, o->home) > o->f[0]) o->f[1] = yaw_to(o->pos, o->home);
        o->yaw = approach_angle(o->yaw, o->f[1], 3 * DT);
        spd = 0.45f;
        if (near_run) { o->state = 1; o->timer = 0; sfx_ex(SFX_SQUAWK, 0.25f, 2.6f); }
        else if (d < 2.6f && tux_grounded() && pl.fwd < 1.0f) { o->state = 2; o->timer = 0; }
        break;
    case 1:   /* running away from the giant's feet */
        o->f[1] = yaw_to(pl.pos, o->pos);
        o->yaw = approach_angle(o->yaw, o->f[1], 9 * DT);
        spd = 2.4f;
        if (o->timer > 70) o->state = 0;
        break;
    case 2:   /* the giant is standing still: wave at him */
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 6 * DT);
        if (o->timer % 50 == 0 && o->f[3] <= 0) o->f[2] = 2.2f;
        if (d > 3.4f || near_run) { o->state = near_run ? 1 : 0; o->timer = 0; }
        break;
    }
    if (spd > 0) {
        vec3 np = v3mad(o->pos, yaw_dir(o->yaw), spd * DT);
        float y = ground_at(np.x, np.z, o->pos.y + 0.6f);
        if (water_level_at(np.x, np.z, y) > y || fabsf(y - o->pos.y) > 0.35f) o->f[1] = o->yaw + PI_F * (0.6f + frand() * 0.8f);
        else { o->pos = np; o->pos.y = y; }
    }
    /* do not get trodden on: step out of the way of the giant */
    if (d < 0.6f && fabsf(pl.pos.y - o->pos.y) < 1.0f) {
        vec3 away = v3sub(o->pos, pl.pos);
        away.y = 0;
        if (v3len(away) < 0.01f) away = v3(1, 0, 0);
        o->pos = v3mad(pl.pos, v3norm(away), 0.6f);
        o->pos.y = ground_at(o->pos.x, o->pos.z, o->pos.y + 0.6f);
        if (o->f[3] <= 0) o->f[2] = 3.5f;
    }
}

static int ship_water(vec3 p, float *wy)
{
    float y = water_level_at(p.x, p.z, 0);
    float fy = ground_at(p.x, p.z, y + 0.5f);
    if (y < -1e8f || fy > y - 0.12f) return 0;
    *wy = y;
    return 1;
}

static void shot_fire(vec3 from, int kind, float t)
{
    vec3 to = v3mad(tux_center(), pl.vel, t * 0.6f), v;
    Obj *s;
    v = v3scale(v3sub(to, from), 1.0f / t);
    v.y += 0.5f * 9.0f * t;
    s = obj_spawn(OT_NANOSHOT, from, atan2f(v.x, v.z), kind);
    if (s) { s->vel = v; s->flags |= OF_NOCULL; }
}

static void ship_update(Obj *o)
{
    float wy;
    o->timer++;
    o->f[0] += DT;
    switch (o->state) {
    case 0:   /* anchored with the enemy fleet: fires its little cannon at the giant */
        o->pos.y = o->home.y + sinf(o->f[0] * 1.7f + o->param) * 0.03f;
        o->yaw += sinf(o->f[0] * 0.5f + o->param) * 0.08f * DT;
        if (v3distxz(o->pos, pl.pos) < 13 && o->timer % 190 == (o->param * 41) % 190) {
            shot_fire(v3add(o->pos, v3(0, 0.25f, 0)), 0, 1.3f);
            fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(0, 0.25f, 0)), 0.35f);
            sfx_ex(SFX_BLAST, 0.35f, 2.2f);
        }
        if (v3distxz(o->pos, pl.pos) < 1.2f && pl.water_y > -1e8f && fabsf(pl.pos.y - o->pos.y) < 1.2f) {
            o->state = 1;
            o->i[1] = towed_n++;
            sfx(SFX_GRAB);
            {
                static char msg[64];
                snprintf(msg, sizeof msg, tr("Ship in tow! %d/%d"), towed_n, ships_total);
                hud_message(msg, 90);
            }
        }
        break;
    case 1: {   /* towed: the rope pulls it after Tux */
        float len = 2.4f + 0.55f * o->i[1], d = v3distxz(o->pos, pl.pos);
        if (d > len) {
            /* sail toward Tux; when land is in the way, veer around it */
            static const float veer[7] = { 0, 0.5f, -0.5f, 1.0f, -1.0f, 1.5f, -1.5f };
            float base = yaw_to(o->pos, pl.pos), step = minf(d - len, 8.0f * DT);
            int k;
            for (k = 0; k < 7; k++) {
                float a = base + veer[(k + o->i[2]) % 7];
                vec3 np = v3mad(o->pos, yaw_dir(a), step);
                if (ship_water(np, &wy)) {
                    o->pos = np;
                    o->pos.y = wy - 0.02f;
                    o->yaw = approach_angle(o->yaw, a, 3 * DT);
                    if (k == 0) o->i[2] = 0;
                    break;
                }
            }
            if (k == 7) o->i[2] = (o->i[2] + 1) % 7;
        }
        o->pos.y += sinf(o->f[0] * 2.3f) * 0.004f;
        if (v3distxz(o->pos, g_nano_harbor) < 6.0f) {
            int k = ships_total - g_nano_ships_left;
            o->state = 2;
            o->timer = 0;
            o->home = v3(g_nano_harbor.x - 4.0f + k * 2.0f, o->pos.y, g_nano_harbor.z + 1.5f);
            g_nano_ships_left--;
            sfx(SFX_REDCOIN);
            if (g_nano_ships_left > 0) {
                static char msg[64];
                snprintf(msg, sizeof msg, tr("Ship in the harbour! %d left"), g_nano_ships_left);
                hud_message(msg, 90);
            } else {
                hud_message("The whole enemy fleet is in the harbour!", 150);
                star_spawn(v3add(g_nano_harbor, v3(0, 1.2f, -1.0f)), 0, 1, o->pos);
            }
        }
        break;
    }
    case 2:   /* moored in the harbour */
        o->pos.x = approachf(o->pos.x, o->home.x, 2 * DT);
        o->pos.z = approachf(o->pos.z, o->home.z, 2 * DT);
        o->yaw = approach_angle(o->yaw, PI_F, 2 * DT);
        break;
    }
}

static void cloud_update(Obj *o)
{
    o->f[0] += DT * (0.15f + 0.05f * (o->param2 & 3));
    if (o->flags & OF_HELD) {
        o->pos = v3add(v3mad(pl.pos, yaw_dir(pl.yaw), 0.2f), v3(0, 2.3f, 0));
        o->yaw = pl.yaw;
        return;
    }
    if (o->param == 0) {
        o->pos = v3add(o->home, v3(sinf(o->f[0]) * 2.0f, sinf(o->f[0] * 1.7f) * 0.15f, cosf(o->f[0] * 0.8f) * 2.0f));
        return;
    }
    /* the rain cloud drifts back to its spot when dropped */
    {
        vec3 hp = v3add(o->home, v3(sinf(o->f[0]) * 0.6f, 0, cosf(o->f[0]) * 0.6f));
        o->pos = v3add(o->pos, v3scale(v3sub(hp, o->pos), minf(1.0f, 0.6f * DT)));
        if (o->state >= 100 && v3dist(o->pos, hp) < 0.3f) o->state = 0;
    }
}

static Obj *held_rain_cloud(void)
{
    Obj *c;
    if (pl.held < 0) return NULL;
    c = &objs[pl.held];
    return (c->active && c->type == OT_NANOCLOUD && c->param == 1) ? c : NULL;
}

static void fire_update(Obj *o)
{
    Obj *c;
    o->timer++;
    if (o->state == 1) return;
    if (o->timer % 9 == 0) fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(frands() * 0.8f, 0.9f, frands() * 0.8f)), 0.9f);
    if (o->timer % 40 == 0 && v3distxz(o->pos, pl.pos) < 14) sfx_ex(SFX_FIRE, 0.3f, 0.8f);
    c = held_rain_cloud();
    if (c && v3distxz(c->pos, o->pos) < 2.3f) {
        o->f[0] += DT / 1.6f;
        if (o->timer % 12 == 0) sfx_ex(SFX_SPLASH, 0.3f, 1.8f);
        if (o->f[0] >= 1) {
            o->state = 1;
            g_nano_fires_left--;
            fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(0, 0.6f, 0)), 2.5f);
            sfx(SFX_REDCOIN);
            if (g_nano_fires_left > 0) {
                static char msg[64];
                snprintf(msg, sizeof msg, tr("Fire out! %d left"), g_nano_fires_left);
                hud_message(msg, 90);
            } else {
                hud_message("All the forest fires are out!", 150);
                star_spawn(v3add(o->pos, v3(0, 2.0f, 0)), 1, 1, o->pos);
            }
        }
    }
    if (v3distxz(o->pos, pl.pos) < 1.0f + PR && pl.pos.y < o->pos.y + 1.0f) player_hurt(1, o->pos);
}

static void soldier_knock(Obj *o, vec3 from, float out, float up)
{
    vec3 d = v3sub(o->pos, from);
    d.y = 0;
    if (v3len(d) < 0.01f) d = v3(frands(), 0, frands());
    d = v3norm(d);
    o->vel = v3(d.x * out, up, d.z * out);
    o->state = 1;
    o->timer = 0;
    sfx_ex(SFX_SQUAWK, 0.3f, 2.4f);
}

static void soldier_update(Obj *o)
{
    float d = v3distxz(o->pos, pl.pos);
    o->timer++;
    switch (o->state) {
    case 0:   /* holding the line; the archers shoot volleys of tiny arrows */
        if (d < 16) o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 3 * DT);
        if ((o->param % 3) == 0 && d < 13 && d > 1.5f && o->timer % 210 == (o->param * 11) % 210)
            shot_fire(v3add(o->pos, v3(0, 0.3f, 0)), 1, 1.0f);
        if (gpound_now() && d < 4.5f) soldier_knock(o, pl.pos, 5.0f, 7.0f);
        else if (tux_attacking_near(o, 1.0f)) soldier_knock(o, pl.pos, 6.0f, 5.0f);
        else if (d < 0.5f && tux_grounded() && pl.fwd > 1.0f) soldier_knock(o, pl.pos, 4.0f, 4.0f);
        else if (g_nano_catapults_left == 0 && catapults_total > 0) { o->state = 2; o->timer = 0; }
        break;
    case 1: {   /* sent flying */
        float fy;
        o->pos = v3mad(o->pos, o->vel, DT);
        o->vel.y -= 16.0f * DT;
        o->yaw += 12 * DT;
        fy = ground_at(o->pos.x, o->pos.z, o->pos.y + 0.5f);
        if (o->pos.y <= fy && o->vel.y < 0) { o->pos.y = fy; o->state = 2; o->timer = 0; }
        break;
    }
    case 2: {   /* routed: run off, away from the giant, and vanish into the hills */
        vec3 np;
        o->yaw = approach_angle(o->yaw, yaw_to(pl.pos, o->pos), 8 * DT);
        np = v3mad(o->pos, yaw_dir(o->yaw), 2.6f * DT);
        np.y = ground_at(np.x, np.z, o->pos.y + 0.6f);
        if (water_level_at(np.x, np.z, np.y) < np.y) o->pos = np;
        if (o->timer > 150) { fx_spawn(OT_FX_SMOKE, o->pos, 0.4f); obj_kill(o); }
        break;
    }
    }
}

static void catapult_update(Obj *o)
{
    float d = v3distxz(o->pos, pl.pos);
    int hit = 0;
    o->timer++;
    if (o->state == 1) return;
    if (d < 22) o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 1.5f * DT);
    /* arm: wound back, then flung forward when it throws */
    if (o->f[0] > 0) o->f[0] -= DT * 4;
    if (d < 20 && d > 2.0f && o->timer % 250 == (o->param * 83) % 250) {
        o->f[0] = 1;
        shot_fire(v3add(o->pos, v3(0, 0.6f, 0)), 2, 1.6f);
        sfx_ex(SFX_THROW, 0.5f, 1.8f);
    }
    if (gpound_now() && d < 3.0f) hit = 1;
    if (tux_attacking_near(o, 0.7f)) hit = 1;
    if (player_in_air() && pl.vel.y < 0 && d < 0.9f && pl.pos.y > o->pos.y + 0.2f && pl.pos.y < o->pos.y + 1.0f) {
        hit = 1;
        player_bounce(10);
    }
    if (hit) {
        o->state = 1;
        g_nano_catapults_left--;
        fx_spawn(OT_FX_EXPLODE, v3add(o->pos, v3(0, 0.3f, 0)), 0.6f);
        sfx(SFX_BREAK);
        spawn_coins_burst(v3add(o->pos, v3(0, 0.5f, 0)), 3);
        if (g_nano_catapults_left > 0) {
            static char msg[64];
            snprintf(msg, sizeof msg, tr("Catapult smashed! %d left"), g_nano_catapults_left);
            hud_message(msg, 90);
        } else {
            hud_message("The tiny army is in full retreat!", 150);
            star_spawn(v3add(o->pos, v3(0, 2.2f, 0)), 2, 1, o->pos);
        }
    }
}

/* tiny arrows and cannonballs only sting a giant: lose health but keep going */
static void sting(vec3 from)
{
    if (pl.invinc > 0 || pl.act == ACT_DEATH || pl.act == ACT_STAR_DANCE) return;
    if (pl.health <= 1) { player_hurt(1, from); return; }
    pl.health--;
    pl.invinc = 90;
    sfx(SFX_HURT);
    sfx_ex(SFX_SQUAWK, 0.5f, 1.3f);
}

static void shot_update(Obj *o)
{
    float fy, wy, r = o->param == 2 ? 0.55f : 0.42f;
    o->timer++;
    o->pos = v3mad(o->pos, o->vel, DT);
    o->vel.y -= 9.0f * DT;
    o->yaw = atan2f(o->vel.x, o->vel.z);
    o->pitch = atan2f(o->vel.y, sqrtf(o->vel.x * o->vel.x + o->vel.z * o->vel.z));
    if (v3dist(o->pos, tux_center()) < r + 0.3f && pl.visible) {
        if (o->param == 2) player_hurt(1, o->pos);
        else sting(o->pos);
        fx_spawn(OT_FX_SMOKE, o->pos, 0.4f);
        obj_kill(o);
        return;
    }
    fy = ground_at(o->pos.x, o->pos.z, o->pos.y + 0.3f);
    wy = water_level_at(o->pos.x, o->pos.z, o->pos.y);
    if (o->pos.y < fy || (wy > -1e8f && o->pos.y < wy) || o->timer > 300) {
        if (wy > -1e8f && o->pos.y < wy) fx_spawn(OT_FX_BUBBLE, v3(o->pos.x, wy, o->pos.z), 0.4f);
        else fx_spawn(OT_FX_SMOKE, o->pos, 0.3f);
        obj_kill(o);
    }
}

static void train_update(Obj *o)
{
    vec3 a, b;
    float s;
    if (o->param == 0) {
        /* the locomotive drives the whole train; it stops and toots when the giant blocks the line */
        vec3 ahead = track_at(train_s + 1.0f);
        int blocked = v3distxz(ahead, pl.pos) < 1.3f && fabsf(pl.pos.y - ahead.y) < 1.5f;
        o->i[0] = blocked;
        if (!blocked) train_s += 2.6f * DT;
        else if (++o->timer % 45 == 1) sfx_ex(SFX_BUZZ, 0.35f, 2.4f);
        if (!blocked && g_frame % 16 == 0) fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(0, 0.5f, 0)), 0.45f);
    }
    s = train_s - o->param * 0.82f;
    a = track_at(s + 0.25f);
    b = track_at(s - 0.25f);
    o->pos = v3scale(v3add(a, b), 0.5f);
    o->yaw = atan2f(a.x - b.x, a.z - b.z);
}

/* ---------------------------------------------------------------- HUGEPAGE HOUSE objects */
static void toaster_update(Obj *o)
{
    float top = o->pos.y + o->f[6];
    int on = pl.platform == obj_index(o) && !player_in_air() && fabsf(pl.pos.x - o->pos.x) < o->f[5] - 0.4f &&
             fabsf(pl.pos.z - o->pos.z) < o->f[7];
    o->timer++;
    switch (o->state) {
    case 0:
        if (on && pl.act != ACT_STAR_DANCE && pl.act != ACT_DEATH) {
            o->state = 1;
            o->timer = 0;
            sfx_ex(SFX_DOOR, 0.8f, 1.6f);
            hud_message("Tux dropped into the toaster slot...", 80);
        }
        break;
    case 1:   /* lever down: Tux sits in the slot while the coils glow */
        pl.no_ctrl = 1;
        if (pl.act != ACT_FROZEN) player_set_act(ACT_FROZEN);
        pl.pos = v3(o->pos.x + clampf(pl.pos.x - o->pos.x, -1.5f, 1.5f), top - 0.7f, o->pos.z);
        pl.vel = v3(0, 0, 0);
        if (o->timer % 20 == 0) sfx_ex(SFX_STEP, 0.5f, 1.4f);
        if (o->timer >= 80) {
            o->state = 2;
            o->timer = 0;
            pl.no_ctrl = 0;
            pl.pos.y = top + 0.05f;
            player_set_act(ACT_TRAMP);
            pl.vel.y = 30.5f;
            pl.fwd = 0;
            pl.peak_y = pl.pos.y;
            sfx(SFX_BOUNCE);
            sfx_ex(SFX_DOOR, 1.0f, 2.2f);
            cam.shake = 0.15f;
        }
        break;
    case 2:
        if (o->timer > 60 && !on) o->state = 0;
        break;
    }
}

static void yarn_update(Obj *o)
{
    Surf *s;
    vec3 wn, np;
    float fy;
    if (o->flags & OF_HELD) {
        o->pos = v3add(v3mad(pl.pos, yaw_dir(pl.yaw), 0.4f), v3(0, 1.75f, 0));
        o->vel = v3(0, 0, 0);
        return;
    }
    np = v3mad(o->pos, o->vel, DT);
    o->vel.y -= 30.0f * DT;
    if (col_walls(&np, 0.8f, 0.8f, &wn, NULL)) {
        float vn = v3dot(o->vel, wn);
        if (vn < 0) o->vel = v3mad(o->vel, wn, -1.6f * vn);
    }
    fy = col_floor(np.x, np.y + 0.8f, np.z, &s);
    if (s && np.y <= fy) {
        np.y = fy;
        if (o->vel.y < -4) { o->vel.y *= -0.35f; sfx_ex(SFX_BOUNCE, 0.3f, 0.7f); }
        else o->vel.y = 0;
        o->vel.x *= 1.0f - 1.2f * DT;
        o->vel.z *= 1.0f - 1.2f * DT;
        if (o->state >= 100 && v3len(v3(o->vel.x, 0, o->vel.z)) < 0.5f) o->state = 0;
    }
    if (np.y < -20) { np = o->home; o->vel = v3(0, 0, 0); }
    /* rolling: spin around the axis across the direction of travel */
    o->f[0] += v3len(v3(o->vel.x, 0, o->vel.z)) * DT / 0.9f;
    if (v3len(v3(o->vel.x, 0, o->vel.z)) > 0.2f) o->yaw = atan2f(o->vel.x, o->vel.z);
    o->pos = np;
    if (v3distxz(o->pos, pl.pos) < 0.9f + PR && fabsf(pl.pos.y - o->pos.y) < 1.5f && !(o->flags & OF_HELD)) {
        vec3 d = v3sub(pl.pos, o->pos);
        d.y = 0;
        if (v3len(d) > 0.01f) pl.pos = v3mad(o->pos, v3norm(d), 0.9f + PR);
        pl.pos.y = maxf(pl.pos.y, pl.pos.y);
    }
}

static Obj *find_yarn(void)
{
    int i;
    for (i = 0; i < nobj; i++)
        if (objs[i].active && objs[i].type == OT_YARN) return &objs[i];
    return NULL;
}

static void cat_walk_to(Obj *o, vec3 t, float speed, float turn)
{
    vec3 np;
    o->yaw = approach_angle(o->yaw, yaw_to(o->pos, t), turn * DT);
    np = v3mad(o->pos, yaw_dir(o->yaw), speed * DT);
    /* the cat keeps to the open floor of the room (f[0..3] = x0 z0 x1 z1) */
    np.x = clampf(np.x, o->f[0], o->f[2]);
    np.z = clampf(np.z, o->f[1], o->f[3]);
    np.y = ground_at(np.x, np.z, o->pos.y + 1.6f);
    o->pos = np;
    o->f[4] += speed * DT;
}

static void cat_update(Obj *o)
{
    Obj *y = find_yarn();
    float d = v3distxz(o->pos, pl.pos);
    int yarn_moving = y && !(y->flags & OF_HELD) && (y->state >= 100 || v3len(y->vel) > 1.5f);
    o->timer++;
    if (o->i[1] > 0) o->i[1]--;
    if (y && yarn_moving && o->state != 4 && v3distxz(y->pos, o->pos) < 18) {
        o->state = 4;
        o->timer = 0;
        sfx_ex(SFX_SQUAWK, 0.9f, 0.5f);
        if (!cat_star_out) {
            cat_star_out = 1;
            star_spawn(v3add(o->home, v3(0, 1.4f, 0)), 2, 1, v3add(o->home, v3(0, 0.5f, 0)));
            hud_message("The cat was sleeping on a Kernel Star!", 150);
        }
    }
    switch (o->state) {
    case 0: {   /* asleep: running, jumping or pounding close by wakes it up */
        int noisy = (tux_grounded() && pl.fwd > 6.5f && d < 9) || ((landed_now() || gpound_now()) && d < 11) ||
                    (tux_attacking_near(o, 4.0f));
        if (g_frame % 140 == 0 && d < 30) sfx_ex(SFX_SLIDE, 0.25f, 0.35f);   /* purring snore */
        if (noisy) { o->state = 1; o->timer = 0; sfx_ex(SFX_ROAR, 0.6f, 1.8f); hud_message("The giant cat woke up!", 90); }
        break;
    }
    case 1:   /* stretching awake */
        if (o->timer > 40) { o->state = 2; o->timer = 0; }
        break;
    case 2:   /* chasing Tux; swats at him with a huge paw */
        if (pl.pos.y > o->pos.y + 3.5f) {
            /* out of reach on the furniture: sit below and wait */
            o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 3 * DT);
            if (o->timer > 240) { o->state = 3; o->timer = 0; }
            break;
        }
        cat_walk_to(o, pl.pos, d > 6 ? 7.0f : 4.5f, 2.6f);
        if (d < 4.6f && o->i[1] == 0) { o->i[1] = 50; o->i[2] = 22; sfx_ex(SFX_SQUAWK, 0.7f, 0.6f); }
        if (o->i[2] > 0 && --o->i[2] == 10 && v3distxz(v3mad(o->pos, yaw_dir(o->yaw), 3.0f), pl.pos) < 2.6f) player_hurt(1, o->pos);
        if (o->timer > 480 || d > 26) { o->state = 3; o->timer = 0; }
        break;
    case 3:   /* back to the cushion */
        cat_walk_to(o, o->home, 4.0f, 3.0f);
        if (v3distxz(o->pos, o->home) < 0.5f) {
            o->yaw = approach_angle(o->yaw, o->f[5], 4 * DT);
            if (o->timer > 60) { o->state = 0; o->timer = 0; o->pos = o->home; o->yaw = o->f[5]; }
        } else o->timer = 0;
        break;
    case 4:   /* playing with the yarn: chase it and bat it across the room */
        if (!y || (y->flags & OF_HELD)) { if (o->timer > 60) { o->state = 3; o->timer = 0; } break; }
        cat_walk_to(o, y->pos, 6.0f, 3.5f);
        if (v3distxz(o->pos, y->pos) < 3.4f && o->i[1] == 0) {
            float a = o->yaw + frands() * 1.6f;
            y->vel = v3(sinf(a) * 9.0f, 4.0f, cosf(a) * 9.0f);
            y->state = 100;
            o->i[1] = 40;
            o->i[2] = 22;
            sfx_ex(SFX_KICK, 0.7f, 0.7f);
        }
        if (o->timer > 18 * 60) { o->state = 3; o->timer = 0; }
        break;
    }
}

static void foot_update(Obj *o)
{
    float x0 = o->f[0], z0 = o->f[1], x1 = o->f[2], z1 = o->f[3];
    int inside = pl.pos.x > x0 - 2 && pl.pos.x < x1 + 2 && pl.pos.z > z0 - 2 && pl.pos.z < z1 + 2;
    o->timer++;
    switch (o->state) {
    case 0:   /* lifted high up, waiting for the next step */
        o->pos.y = 60;
        if (o->timer > o->i[0]) {
            vec3 t;
            if (inside) t = v3(pl.pos.x + frands() * 1.5f + pl.vel.x * 0.6f, 0, pl.pos.z + frands() * 1.5f + pl.vel.z * 0.6f);
            else t = v3(lerpf(x0, x1, frand()), 0, lerpf(z0, z1, frand()));
            o->pos.x = clampf(t.x, x0, x1);
            o->pos.z = clampf(t.z, z0, z1);
            o->state = 1;
            o->timer = 0;
        }
        break;
    case 1:   /* hovering: its shadow grows on the floor */
        o->pos.y = lerpf(60, 9, clampf(o->timer / 70.0f, 0, 1));
        if (o->timer > 75) { o->state = 2; o->timer = 0; o->vel.y = -10; }
        break;
    case 2:   /* STOMP */
        o->vel.y -= 120 * DT;
        o->pos.y += o->vel.y * DT;
        if (o->pos.y <= o->home.y) {
            float dx = pl.pos.x - o->pos.x, dz = pl.pos.z - o->pos.z, dist = sqrtf(dx * dx + dz * dz);
            o->pos.y = o->home.y;
            o->state = 3;
            o->timer = 0;
            sfx(SFX_THUD);
            sfx_ex(SFX_EXPLODE, 0.6f, 0.4f);
            {   /* a shockwave runs along the floor: jump over it */
                Obj *ring = obj_spawn(OT_FX_SHOCKWAVE, o->pos, 0, 0);
                if (ring) { ring->flags = 0; ring->f[3] = 14.0f; }
            }
            cam.shake = maxf(cam.shake, clampf(1.0f - dist / 40.0f, 0.1f, 0.6f));
            if (fabsf(dx) < 3.6f && fabsf(dz) < 6.6f && pl.pos.y < o->pos.y + 3.0f) {
                /* squashed: thrown clear of the slipper */
                pl.pos.x = o->pos.x + (dx < 0 ? -4.2f : 4.2f);
                player_hurt(2, o->pos);
            } else if (dist < 11 && tux_grounded()) {
                pl.yaw = yaw_to(pl.pos, o->pos);
                player_bounce(7.0f);
                pl.fwd = -4;
            }
        }
        break;
    case 3:   /* resting on the floor: Tux can even climb onto it */
        if (o->timer > 80) { o->state = 4; o->timer = 0; }
        break;
    case 4:
        o->pos.y += 9.0f * DT * (1 + o->timer * 0.05f);
        if (o->pos.y > 60) { o->state = 0; o->timer = 0; o->i[0] = 30 + (int)(frand() * 50); }
        break;
    }
}

void scale_obj_update(Obj *o)
{
    switch (o->type) {
    case OT_NANOFOLK: folk_update(o); break;
    case OT_NANOSHIP: ship_update(o); break;
    case OT_NANOCLOUD: cloud_update(o); break;
    case OT_NANOFIRE: fire_update(o); break;
    case OT_NANOSOLDIER: soldier_update(o); break;
    case OT_NANOCATAPULT: catapult_update(o); break;
    case OT_NANOSHOT: shot_update(o); break;
    case OT_NANOTRAIN: train_update(o); break;
    case OT_TOASTER: toaster_update(o); break;
    case OT_YARN: yarn_update(o); break;
    case OT_GIANTCAT: cat_update(o); break;
    case OT_GIANTFOOT: foot_update(o); break;
    }
}

/* ---------------------------------------------------------------- collision */
static void box_col(Obj *o, vec3 c, float hx, float hy, float hz, float yaw)
{
    vec3 p[8];
    int i, idx = obj_index(o);
    float cy = cosf(yaw), sy = sinf(yaw);
    for (i = 0; i < 8; i++) {
        vec3 l = v3((i & 1) ? hx : -hx, (i & 2) ? hy : -hy, (i & 4) ? hz : -hz);
        p[i] = v3add(c, v3(l.x * cy + l.z * sy, l.y, -l.x * sy + l.z * cy));
    }
#define Q(a, b, c2, d) col_dyn_add(p[a], p[b], p[c2], SF_NOCAM, idx); col_dyn_add(p[a], p[c2], p[d], SF_NOCAM, idx);
    Q(2, 6, 7, 3) Q(0, 1, 5, 4) Q(1, 3, 7, 5) Q(0, 4, 6, 2) Q(4, 5, 7, 6) Q(0, 2, 3, 1)
#undef Q
}

int scale_obj_solid(Obj *o)
{
    if (o->type == OT_GIANTCAT) {
        float h = o->state == 0 ? 1.5f : 2.0f;
        box_col(o, v3add(o->pos, v3(0, h, 0)), 2.2f, h, 3.6f, o->yaw);
        return 1;
    }
    if (o->type == OT_GIANTFOOT) {
        if (o->state >= 2 && o->pos.y < 8) box_col(o, v3add(o->pos, v3(0, 1.3f, 0)), 3.2f, 1.3f, 6.3f, 0);
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- models */
static Mesh M;
static void mb_start(void) { mesh_init(&M); mb_begin(&M); }
static GLuint mb_finish(void) { GLuint d; mb_end(); d = mesh_compile(&M); mesh_free(&M); return d; }

static void build_models(void)
{
    int k;
    if (dl_ship) return;
    for (k = 0; k < 2; k++) {   /* a little sailing warship (bow +z), ~1.3 long */
        mb_start();
        mb_color(0.45f, 0.27f, 0.13f);
        mb_cuboid(-0.2f, -0.08f, -0.6f, 0.2f, 0.12f, 0.5f);
        mb_ellipsoid(0, 0.02f, 0.5f, 0.2f, 0.1f, 0.22f, 8);
        mb_color(0.7f, 0.5f, 0.3f);
        mb_cuboid(-0.18f, 0.12f, -0.58f, 0.18f, 0.14f, 0.55f);
        mb_cuboid(-0.2f, 0.12f, -0.6f, 0.2f, 0.26f, -0.38f);   /* stern castle */
        mb_color(0.1f, 0.1f, 0.1f);
        mb_cuboid(-0.21f, 0.02f, -0.3f, 0.21f, 0.06f, 0.3f);    /* gun ports */
        mb_color(0.35f, 0.22f, 0.1f);
        mb_cuboid(-0.02f, 0.14f, 0.12f, 0.02f, 0.95f, 0.16f);
        mb_cuboid(-0.02f, 0.14f, -0.25f, 0.02f, 0.8f, -0.21f);
        mb_color(0.97f, 0.95f, 0.88f);
        mb_cuboid(-0.24f, 0.4f, 0.13f, 0.24f, 0.85f, 0.15f);
        mb_cuboid(-0.2f, 0.35f, -0.24f, 0.2f, 0.72f, -0.22f);
        if (k) mb_color(0.2f, 0.35f, 0.9f); else mb_color(0.9f, 0.2f, 0.2f);
        mb_cuboid(0, 0.86f, 0.14f, 0.18f, 0.96f, 0.15f);
        if (k) dl_ship_blue = mb_finish(); else dl_ship = mb_finish();
    }
    /* catapult: base with wheels (the throwing arm is a separate list) */
    mb_start();
    mb_color(0.55f, 0.35f, 0.18f);
    mb_cuboid(-0.22f, 0.06f, -0.32f, 0.22f, 0.14f, 0.32f);
    mb_cuboid(-0.2f, 0.14f, -0.05f, -0.15f, 0.42f, 0.05f);
    mb_cuboid(0.15f, 0.14f, -0.05f, 0.2f, 0.42f, 0.05f);
    mb_color(0.3f, 0.2f, 0.1f);
    for (k = 0; k < 4; k++) {
        mb_push();
        mb_translate((k & 1) ? 0.25f : -0.25f, 0.08f, (k & 2) ? 0.2f : -0.2f);
        mb_rotate(90, 0, 0, 1);
        mb_scale(0.08f, 0.04f, 0.08f);
        mb_translate(0, -0.5f, 0);
        mb_cylinder(8, 1);
        mb_pop();
    }
    dl_cata = mb_finish();
    mb_start();
    mb_color(0.6f, 0.4f, 0.2f);
    mb_cuboid(-0.025f, -0.02f, -0.02f, 0.025f, 0.02f, 0.5f);
    mb_color(0.4f, 0.4f, 0.42f);
    mb_cuboid(-0.06f, 0.0f, 0.42f, 0.06f, 0.06f, 0.55f);
    dl_cata_arm = mb_finish();
    /* locomotive (front +z) and a goods wagon */
    mb_start();
    mb_color(0.12f, 0.12f, 0.14f);
    mb_push(); mb_translate(0, 0.17f, -0.05f); mb_rotate(90, 1, 0, 0); mb_scale(0.12f, 0.45f, 0.12f); mb_translate(0, -0.5f, 0); mb_cylinder(10, 1); mb_pop();
    mb_cuboid(-0.13f, 0.04f, -0.32f, 0.13f, 0.08f, 0.3f);
    mb_color(0.8f, 0.15f, 0.12f);
    mb_cuboid(-0.13f, 0.08f, -0.34f, 0.13f, 0.36f, -0.18f);
    mb_color(0.15f, 0.15f, 0.17f);
    mb_cuboid(-0.14f, 0.36f, -0.36f, 0.14f, 0.39f, -0.16f);
    mb_cuboid(-0.03f, 0.25f, 0.08f, 0.03f, 0.4f, 0.14f);
    mb_color(1.0f, 0.85f, 0.3f);
    mb_cuboid(-0.035f, 0.15f, 0.2f, 0.035f, 0.2f, 0.23f);
    dl_loco = mb_finish();
    mb_start();
    mb_color(0.15f, 0.15f, 0.17f);
    mb_cuboid(-0.12f, 0.03f, -0.3f, 0.12f, 0.07f, 0.3f);
    mb_color(0.95f, 0.75f, 0.2f);
    mb_cuboid(-0.12f, 0.07f, -0.28f, 0.12f, 0.27f, 0.28f);
    mb_color(0.85f, 0.6f, 0.15f);
    mb_cuboid(-0.13f, 0.27f, -0.29f, 0.13f, 0.29f, 0.29f);
    dl_car = mb_finish();
    /* the giant cat: orange tabby (length along +z) */
    mb_start();
    mb_color(0.95f, 0.6f, 0.25f);
    mb_ellipsoid(0, 2.2f, 0, 2.0f, 1.6f, 3.4f, 16);
    mb_color(1.0f, 0.95f, 0.88f);
    mb_ellipsoid(0, 1.85f, 1.2f, 1.5f, 1.2f, 1.9f, 12);
    mb_color(0.85f, 0.45f, 0.15f);
    for (k = 0; k < 4; k++) mb_ellipsoid(0, 3.55f, -2.2f + k * 1.3f, 1.5f, 0.35f, 0.32f, 10);   /* stripes */
    mb_color(0.95f, 0.6f, 0.25f);
    for (k = 0; k < 4; k++) mb_ellipsoid((k & 1) ? 1.1f : -1.1f, 0.8f, (k & 2) ? 2.2f : -2.2f, 0.55f, 0.9f, 0.6f, 10);
    mb_color(1.0f, 0.95f, 0.9f);
    for (k = 0; k < 4; k++) mb_ellipsoid((k & 1) ? 1.1f : -1.1f, 0.2f, ((k & 2) ? 2.2f : -2.2f) + 0.3f, 0.6f, 0.3f, 0.75f, 10);
    dl_cat_body = mb_finish();
    mb_start();   /* head, origin at neck */
    mb_color(0.95f, 0.6f, 0.25f);
    mb_ellipsoid(0, 0.4f, 0.4f, 1.55f, 1.35f, 1.35f, 16);
    mb_push(); mb_translate(-0.85f, 1.35f, 0.3f); mb_scale(0.55f, 1.0f, 0.4f); mb_cone(8); mb_pop();
    mb_push(); mb_translate(0.85f, 1.35f, 0.3f); mb_scale(0.55f, 1.0f, 0.4f); mb_cone(8); mb_pop();
    mb_color(1.0f, 0.75f, 0.75f);
    mb_push(); mb_translate(-0.85f, 1.4f, 0.42f); mb_scale(0.3f, 0.7f, 0.15f); mb_cone(6); mb_pop();
    mb_push(); mb_translate(0.85f, 1.4f, 0.42f); mb_scale(0.3f, 0.7f, 0.15f); mb_cone(6); mb_pop();
    mb_color(1.0f, 0.95f, 0.9f);
    mb_ellipsoid(0, 0.0f, 1.45f, 0.75f, 0.5f, 0.45f, 10);
    mb_color(0.95f, 0.45f, 0.5f);
    mb_ellipsoid(0, 0.3f, 1.75f, 0.2f, 0.14f, 0.12f, 8);
    mb_color(0.85f, 0.45f, 0.15f);
    mb_ellipsoid(0, 1.25f, 0.6f, 0.9f, 0.25f, 0.6f, 10);
    dl_cat_head = mb_finish();
    mb_start();   /* asleep: a curled-up loaf with the tail wrapped around */
    mb_color(0.95f, 0.6f, 0.25f);
    mb_ellipsoid(0, 1.5f, 0, 2.3f, 1.5f, 3.3f, 16);
    mb_color(0.85f, 0.45f, 0.15f);
    for (k = 0; k < 4; k++) mb_ellipsoid(0, 2.8f, -2.0f + k * 1.25f, 1.7f, 0.3f, 0.3f, 10);
    mb_color(0.95f, 0.6f, 0.25f);
    for (k = 0; k < 7; k++) {
        float a = -0.6f + k * 0.42f;
        mb_ellipsoid(sinf(a) * 2.4f, 0.5f, -cosf(a) * 3.0f + 0.4f, 0.55f, 0.5f, 0.55f, 8);
    }
    dl_cat_sleep = mb_finish();
    /* the giant's slipper (sole at y 0, toe +z) */
    mb_start();
    mb_color(0.95f, 0.95f, 0.9f);
    mb_cuboid(-3.2f, 0, -6.2f, 3.2f, 0.6f, 6.2f);
    mb_color(0.3f, 0.4f, 0.75f);
    mb_ellipsoid(0, 1.4f, 2.0f, 3.1f, 1.6f, 4.2f, 16);
    mb_cuboid(-3.1f, 0.6f, -6.0f, 3.1f, 1.8f, 0.0f);
    mb_color(1.0f, 1.0f, 0.95f);
    mb_ellipsoid(0, 2.6f, 2.6f, 1.6f, 0.8f, 1.6f, 10);   /* pompom */
    mb_color(0.85f, 0.75f, 0.65f);   /* the giant's ankle and leg, disappearing upwards */
    mb_push(); mb_translate(0, 1.6f, -2.5f); mb_scale(2.6f, 70.0f, 2.6f); mb_cylinder(14, 0); mb_pop();
    mb_color(0.35f, 0.3f, 0.45f);
    mb_push(); mb_translate(0, 9.0f, -2.5f); mb_scale(3.0f, 70.0f, 3.0f); mb_cylinder(14, 0); mb_pop();
    dl_slipper = mb_finish();
    /* toaster body (centered, the slots and lever are drawn on top) */
    mb_start();
    mb_color(0.82f, 0.84f, 0.88f);
    mb_cuboid(-3.0f, -2.3f, -1.8f, 3.0f, 2.3f, 1.8f);
    mb_color(0.12f, 0.12f, 0.14f);
    mb_cuboid(-2.4f, 2.3f, -1.1f, 2.4f, 2.32f, -0.3f);
    mb_cuboid(-2.4f, 2.3f, 0.3f, 2.4f, 2.32f, 1.1f);
    mb_cuboid(-3.2f, -2.3f, -2.0f, 3.2f, -2.0f, 2.0f);
    mb_color(0.9f, 0.2f, 0.15f);
    mb_cuboid(-0.6f, -1.6f, 1.8f, 0.6f, -1.0f, 2.05f);   /* dial */
    dl_toaster = mb_finish();
    /* ball of yarn */
    mb_start();
    mb_color(0.9f, 0.2f, 0.3f);
    mb_ellipsoid(0, 0, 0, 0.9f, 0.9f, 0.9f, 14);
    mb_color(0.75f, 0.12f, 0.22f);
    for (k = 0; k < 3; k++) { mb_push(); mb_rotate(60.0f * k, 1, 0, 0.3f); mb_torus(0.88f, 0.06f, 18, 4); mb_pop(); }
    dl_yarn = mb_finish();
}

static void draw_at(GLuint dl, vec3 p, float yaw, float s)
{
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(DEG(yaw), 0, 1, 0);
    if (s != 1) glScalef(s, s, s);
    glCallList(dl);
    glPopMatrix();
}

static void draw_folk(Obj *o)
{
    TuxPose tp;
    static const int var[4] = { TUXV_NPC, TUXV_MOTHER, TUXV_BABY, TUXV_TUX };
    float walk = (o->state == 1 ? 0.6f : 0.25f) * g_frame + o->param;
    memset(&tp, 0, sizeof tp);
    tp.footL = sinf(walk) * 0.5f;
    tp.footR = -tp.footL;
    tp.flipL_z = tp.flipR_z = 0.25f;
    if (o->state == 1) tp.flipL_z = tp.flipR_z = 1.8f + sinf(walk) * 0.4f;
    if (o->state == 2) { tp.flipR_z = 2.6f + sinf(g_frame * 0.4f + o->param) * 0.4f; tp.footL = tp.footR = 0; }
    draw_tux_model(v3add(o->pos, v3(0, o->f[3], 0)), o->yaw, &tp, 0.17f, var[o->param & 3]);
}

static void draw_soldier(Obj *o)
{
    TuxPose tp;
    vec3 p = o->pos;
    memset(&tp, 0, sizeof tp);
    if (o->state == 1) { tp.spin = o->yaw * 2; tp.flipL_z = tp.flipR_z = 2.5f; }
    else if (o->state == 2) { tp.footL = sinf(g_frame * 0.7f) * 0.6f; tp.footR = -tp.footL; tp.flipL_z = tp.flipR_z = 2.0f; }
    else tp.flipR_z = 0.6f;
    draw_tux_model(p, o->yaw, &tp, 0.18f, TUXV_TUX);
    /* helmet and spear */
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(DEG(o->yaw), 0, 1, 0);
    glDisable(GL_TEXTURE_2D);
    glColor3f(0.6f, 0.62f, 0.68f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0, 1, 0);
    glVertex3f(0, 0.33f, 0);
    {
        int k;
        for (k = 0; k <= 8; k++) glVertex3f(sinf(TAU_F * k / 8) * 0.07f, 0.27f, cosf(TAU_F * k / 8) * 0.07f);
    }
    glEnd();
    if (o->state == 0) {
        glColor3f(0.4f, 0.28f, 0.15f);
        glBegin(GL_LINES);
        glVertex3f(-0.07f, 0.02f, 0.04f);
        glVertex3f(-0.07f, 0.45f, 0.04f);
        glEnd();
    }
    glPopMatrix();
}

static void draw_cat(Obj *o)
{
    float walk = o->f[4] * 1.2f;
    glPushMatrix();
    glTranslatef(o->pos.x, o->pos.y, o->pos.z);
    glRotatef(DEG(o->yaw), 0, 1, 0);
    if (o->state == 0) {
        float br = 1.0f + sinf(g_frame * 0.035f) * 0.03f;
        glPushMatrix();
        glScalef(1, br, 1);
        glCallList(dl_cat_sleep);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(0.9f, 1.0f, 2.6f);
        glRotatef(-25, 0, 1, 0);
        glRotatef(20, 0, 0, 1);
        glCallList(dl_cat_head);
        glPopMatrix();
    } else {
        float stretch = o->state == 1 ? sinf(clampf(o->timer / 40.0f, 0, 1) * PI_F) : 0;
        float swat = o->i[2] > 0 ? sinf(o->i[2] / 22.0f * PI_F) : 0;
        glPushMatrix();
        glTranslatef(0, sinf(walk * 2) * 0.08f, 0);
        glRotatef(-stretch * 12, 1, 0, 0);
        glCallList(dl_cat_body);
        glPopMatrix();
        glPushMatrix();
        glTranslatef(0, 3.2f + stretch * 0.3f, 3.3f);
        glRotatef(sinf(g_frame * 0.05f) * 8 + (o->state == 4 ? -15 : 0), 0, 1, 0);
        glCallList(dl_cat_head);
        glPopMatrix();
        /* tail: a chain of balls swaying upwards */
        {
            int k;
            glDisable(GL_TEXTURE_2D);
            for (k = 0; k < 8; k++) {
                float t = k / 7.0f, sw = sinf(g_frame * 0.06f + t * 2.0f) * 1.4f * t;
                glPushMatrix();
                glTranslatef(sw, 2.8f + t * 3.2f, -3.2f - t * 1.2f + t * t * 1.0f);
                glScalef(0.55f, 0.55f, 0.55f);
                glColor3f(k & 1 ? 0.85f : 0.95f, k & 1 ? 0.45f : 0.6f, k & 1 ? 0.15f : 0.25f);
                {   /* low-poly ball */
                    int i, j;
                    for (i = 0; i < 6; i++) {
                        float a0 = PI_F * i / 6 - PI_F / 2, a1 = PI_F * (i + 1) / 6 - PI_F / 2;
                        glBegin(GL_QUAD_STRIP);
                        for (j = 0; j <= 8; j++) {
                            float b = TAU_F * j / 8;
                            glNormal3f(cosf(a1) * sinf(b), sinf(a1), cosf(a1) * cosf(b));
                            glVertex3f(cosf(a1) * sinf(b), sinf(a1), cosf(a1) * cosf(b));
                            glNormal3f(cosf(a0) * sinf(b), sinf(a0), cosf(a0) * cosf(b));
                            glVertex3f(cosf(a0) * sinf(b), sinf(a0), cosf(a0) * cosf(b));
                        }
                        glEnd();
                    }
                }
                glPopMatrix();
            }
        }
        /* a raised paw while swatting */
        if (swat > 0) {
            glPushMatrix();
            glTranslatef(1.1f, 1.2f + swat * 2.0f, 2.6f + swat * 1.8f);
            glScalef(0.7f, 0.7f, 0.9f);
            glColor3f(1.0f, 0.95f, 0.9f);
            glBegin(GL_QUADS);
            glNormal3f(0, 1, 0);
            glVertex3f(-1, 0, -1); glVertex3f(-1, 0, 1); glVertex3f(1, 0, 1); glVertex3f(1, 0, -1);
            glEnd();
            glPopMatrix();
        }
    }
    glPopMatrix();
}

void scale_obj_draw(Obj *o)
{
    build_models();
    switch (o->type) {
    case OT_NANOFOLK: draw_folk(o); break;
    case OT_NANOSOLDIER: draw_soldier(o); break;
    case OT_NANOSHIP: draw_at(o->state == 2 ? dl_ship : dl_ship_blue, o->pos, o->yaw, 1); break;
    case OT_NANOCATAPULT:
        if (o->state == 1) {
            int k;
            for (k = 0; k < 3; k++) draw_at(dl_cata_arm, v3add(o->pos, v3(sinf(k * 2.1f) * 0.3f, 0.03f, cosf(k * 2.1f) * 0.3f)), k * 1.3f, 1);
            break;
        }
        draw_at(dl_cata, o->pos, o->yaw, 1);
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 0.36f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(-(40 - o->f[0] * 110), 1, 0, 0);
        glCallList(dl_cata_arm);
        glPopMatrix();
        break;
    case OT_NANOTRAIN: draw_at(o->param == 0 ? dl_loco : dl_car, o->pos, o->yaw, 1); break;
    case OT_NANOSHOT: {
        glDisable(GL_TEXTURE_2D);
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(-DEG(o->pitch), 1, 0, 0);
        if (o->param == 1) {   /* arrow */
            glColor3f(0.5f, 0.35f, 0.2f);
            glBegin(GL_LINES); glVertex3f(0, 0, -0.25f); glVertex3f(0, 0, 0.25f); glEnd();
            glColor3f(0.8f, 0.8f, 0.85f);
            glBegin(GL_TRIANGLES); glNormal3f(0, 1, 0); glVertex3f(-0.05f, 0, 0.2f); glVertex3f(0.05f, 0, 0.2f); glVertex3f(0, 0, 0.32f); glEnd();
        } else {
            float r = o->param == 2 ? 0.16f : 0.1f;
            glColor3f(o->param == 2 ? 0.55f : 0.15f, o->param == 2 ? 0.52f : 0.15f, o->param == 2 ? 0.48f : 0.17f);
            glBegin(GL_TRIANGLE_FAN);
            glNormal3f(0, 0, 1);
            glVertex3f(0, 0, r);
            {
                int k;
                for (k = 0; k <= 6; k++) glVertex3f(sinf(TAU_F * k / 6) * r, cosf(TAU_F * k / 6) * r, 0);
            }
            glEnd();
            glBegin(GL_TRIANGLE_FAN);
            glNormal3f(0, 0, -1);
            glVertex3f(0, 0, -r);
            {
                int k;
                for (k = 6; k >= 0; k--) glVertex3f(sinf(TAU_F * k / 6) * r, cosf(TAU_F * k / 6) * r, 0);
            }
            glEnd();
        }
        glPopMatrix();
        break;
    }
    case OT_TOASTER: {
        float lever = o->state == 1 ? -1.6f : 0;
        draw_at(dl_toaster, o->pos, o->yaw, 1);
        glDisable(GL_TEXTURE_2D);
        glPushMatrix();
        glTranslatef(o->pos.x + 3.0f, o->pos.y + 0.8f + lever, o->pos.z);
        glColor3f(0.15f, 0.15f, 0.17f);
        glBegin(GL_QUADS);
        glNormal3f(1, 0, 0);
        glVertex3f(0.35f, -0.25f, -0.5f); glVertex3f(0.35f, 0.25f, -0.5f); glVertex3f(0.35f, 0.25f, 0.5f); glVertex3f(0.35f, -0.25f, 0.5f);
        glNormal3f(0, 1, 0);
        glVertex3f(0, 0.25f, -0.5f); glVertex3f(0, 0.25f, 0.5f); glVertex3f(0.35f, 0.25f, 0.5f); glVertex3f(0.35f, 0.25f, -0.5f);
        glEnd();
        glPopMatrix();
        break;
    }
    case OT_YARN:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 0.9f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(DEG(o->f[0]), 1, 0, 0);
        glCallList(dl_yarn);
        glPopMatrix();
        break;
    case OT_GIANTCAT: draw_cat(o); break;
    case OT_GIANTFOOT: if (o->pos.y < 59) draw_at(dl_slipper, o->pos, 0, 1); break;
    }
}

/* ---------------------------------------------------------------- transparent effects */
static vec3 cam_r, cam_u;

static void glow(vec3 p, float size, float r, float g, float b, float a)
{
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_GLOW]);
    gfx_billboard_quad(p, size, r, g, b, a, cam_r, cam_u);
}

static void puff(vec3 p, float size, float r, float g, float b, float a)
{
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_CLOUD]);
    gfx_billboard_quad(p, size, r, g, b, a, cam_r, cam_u);
}

static void draw_cloud(Obj *o)
{
    int k;
    float grey = o->param == 1 ? 0.55f : 1.0f;
    for (k = 0; k < 6; k++) {
        float a = TAU_F * k / 6 + o->param2;
        vec3 p = v3add(o->pos, v3(sinf(a) * 0.55f, (k & 1) * 0.15f, cosf(a) * 0.4f));
        puff(p, 0.9f, grey, grey, grey * 1.05f, 0.85f);
    }
    puff(v3add(o->pos, v3(0, 0.25f, 0)), 1.2f, grey, grey, grey * 1.05f, 0.9f);
    if (o->param == 1 && (o->flags & OF_HELD)) {
        /* rain streaks down to the ground */
        float fy = ground_at(o->pos.x, o->pos.z, o->pos.y - 0.3f);
        glDisable(GL_TEXTURE_2D);
        glBegin(GL_LINES);
        for (k = 0; k < 40; k++) {
            float ph = fmodf(g_frame * 0.05f + k * 0.37f, 1.0f);
            float x = o->pos.x + (hash2(k, 1, 7) - 0.5f) * 1.6f, z = o->pos.z + (hash2(k, 2, 7) - 0.5f) * 1.6f;
            float y = lerpf(o->pos.y - 0.2f, fy, ph);
            glColor4f(0.6f, 0.75f, 1.0f, 0.7f);
            glVertex3f(x, y, z);
            glVertex3f(x, y - 0.25f, z);
        }
        glEnd();
        glEnable(GL_TEXTURE_2D);
    }
}

static void draw_flames(Obj *o)
{
    int k;
    if (o->state == 1) return;
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (k = 0; k < 7; k++) {
        float a = TAU_F * k / 7, fl = 0.75f + 0.25f * sinf(g_frame * 0.4f + k * 1.7f);
        float rad = k == 0 ? 0 : 0.7f * (1 - o->f[0] * 0.6f);
        vec3 p = v3add(o->pos, v3(sinf(a) * rad, 0.35f + fl * 0.3f, cosf(a) * rad));
        glow(p, 1.1f * fl * (1 - o->f[0] * 0.7f), 1.0f, 0.55f + 0.2f * fl, 0.15f, 0.9f);
        glow(v3add(p, v3(0, 0.35f, 0)), 0.6f * fl, 1.0f, 0.85f, 0.4f, 0.7f);
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

/* windmill blades, tiny birds and the ship ropes (NANO); clock hands, sunbeam and dust (HUGE) */
void scale_draw_alpha(void)
{
    int i;
    float mv[16];
    if (!lv.scale_world) return;
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    cam_r = v3(mv[0], mv[4], mv[8]);
    cam_u = v3(mv[1], mv[5], mv[9]);
    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_FALSE);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        if (o->type == OT_NANOCLOUD) draw_cloud(o);
        else if (o->type == OT_NANOFIRE) draw_flames(o);
        else if (o->type == OT_GIANTFOOT && (o->state == 1 || o->state == 2)) {
            /* the shadow of the descending slipper */
            float k = clampf((60 - o->pos.y) / 51.0f, 0, 1);
            gfx_shadow(v3(o->pos.x, o->home.y + 0.05f, o->pos.z), v3(0, 1, 0), 3.0f + k * 4.0f, 0.15f + 0.5f * k);
        }
    }
    glDisable(GL_TEXTURE_2D);
    if (lv.scale_world == 1) {
        /* ropes from Tux to the ships in tow */
        glLineWidth(2);
        glBegin(GL_LINES);
        for (i = 0; i < nobj; i++) {
            Obj *o = &objs[i];
            vec3 bow;
            if (!o->active || o->type != OT_NANOSHIP || o->state != 1) continue;
            bow = v3add(v3mad(o->pos, yaw_dir(o->yaw), 0.55f), v3(0, 0.12f, 0));
            glColor4f(0.75f, 0.6f, 0.35f, 1);
            glVertex3f(bow.x, bow.y, bow.z);
            glVertex3f(pl.pos.x, pl.pos.y + 0.7f, pl.pos.z);
        }
        glEnd();
        glLineWidth(1);
        /* windmill sails on the hill */
        {
            vec3 hub = g_nano_windmill;
            float a0 = g_frame * 0.02f;
            int k;
            glColor4f(0.95f, 0.92f, 0.85f, 1);
            glBegin(GL_QUADS);
            for (k = 0; k < 4; k++) {
                float a = a0 + k * PI_F / 2, c = cosf(a), s = sinf(a);
                vec3 p0 = v3add(hub, v3(c * 0.15f - s * 0.08f, s * 0.15f + c * 0.08f, 0));
                vec3 p1 = v3add(hub, v3(c * 1.3f - s * 0.08f, s * 1.3f + c * 0.08f, 0));
                vec3 p2 = v3add(hub, v3(c * 1.3f + s * 0.12f, s * 1.3f - c * 0.12f, 0));
                vec3 p3 = v3add(hub, v3(c * 0.15f + s * 0.12f, s * 0.15f - c * 0.12f, 0));
                glVertex3f(p0.x, p0.y, p0.z); glVertex3f(p1.x, p1.y, p1.z); glVertex3f(p2.x, p2.y, p2.z); glVertex3f(p3.x, p3.y, p3.z);
            }
            glEnd();
        }
        /* little flocks of birds circling at the giant's shoulder height */
        {
            static const float fl[3][4] = { { 0, 0, 9, 3.2f }, { -28, -8, 7, 4.0f }, { 30, 20, 10, 3.6f } };
            int f, k;
            glColor4f(0.15f, 0.15f, 0.2f, 1);
            glBegin(GL_LINES);
            for (f = 0; f < 3; f++)
                for (k = 0; k < 7; k++) {
                    float a = g_frame * 0.006f * (f & 1 ? -1 : 1) + k * 0.12f + f, r = fl[f][2] + (k % 3) * 0.6f;
                    vec3 p = v3(fl[f][0] + sinf(a) * r, fl[f][3] + sinf(g_frame * 0.03f + k) * 0.2f + (k % 2) * 0.3f, fl[f][1] + cosf(a) * r);
                    vec3 side = v3(cosf(a) * 0.12f, 0, -sinf(a) * 0.12f);
                    float flap = sinf(g_frame * 0.4f + k * 1.3f) * 0.08f;
                    glVertex3f(p.x - side.x, p.y + flap, p.z - side.z); glVertex3f(p.x, p.y, p.z);
                    glVertex3f(p.x, p.y, p.z); glVertex3f(p.x + side.x, p.y + flap, p.z + side.z);
                }
            glEnd();
        }
    }
    if (lv.scale_world == 2) {
        /* the wall clock's hands (clock face on the west wall at x = -35.8) */
        {
            float t = g_frame / 60.0f + 10 * 3600 + 8 * 60;
            float ah[3] = { t / 43200.0f * TAU_F, t / 3600.0f * TAU_F, t / 60.0f * TAU_F };
            float len[3] = { 3.0f, 4.4f, 4.8f }, wid[3] = { 0.35f, 0.25f, 0.08f };
            int k;
            glBegin(GL_QUADS);
            for (k = 0; k < 3; k++) {
                float c = cosf(ah[k]), s = sinf(ah[k]), x = -35.6f + k * 0.04f;
                if (k == 2) glColor4f(0.8f, 0.15f, 0.1f, 1); else glColor4f(0.1f, 0.1f, 0.12f, 1);
                glVertex3f(x, 22 - c * 0.6f - s * wid[k], -6 - s * 0.6f + c * wid[k]);
                glVertex3f(x, 22 + c * len[k] - s * wid[k], -6 + s * len[k] + c * wid[k]);
                glVertex3f(x, 22 + c * len[k] + s * wid[k], -6 + s * len[k] - c * wid[k]);
                glVertex3f(x, 22 - c * 0.6f + s * wid[k], -6 - s * 0.6f - c * wid[k]);
            }
            glEnd();
        }
        /* a shaft of sunlight from the window, with dust motes the size of Tux's head */
        {
            vec3 w0 = v3(10, 26, -35.5f), w1 = v3(24, 26, -35.5f), w2 = v3(24, 15, -35.5f), w3 = v3(10, 15, -35.5f);
            vec3 off = v3(-6, -15, 24);
            int k;
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            glBegin(GL_QUADS);
            glColor4f(1.0f, 0.9f, 0.6f, 0.13f); glVertex3f(w0.x, w0.y, w0.z); glVertex3f(w1.x, w1.y, w1.z);
            glColor4f(1.0f, 0.9f, 0.6f, 0.0f); glVertex3f(w1.x + off.x, w1.y + off.y, w1.z + off.z); glVertex3f(w0.x + off.x, w0.y + off.y, w0.z + off.z);
            glColor4f(1.0f, 0.9f, 0.6f, 0.13f); glVertex3f(w3.x, w3.y, w3.z); glVertex3f(w2.x, w2.y, w2.z);
            glColor4f(1.0f, 0.9f, 0.6f, 0.0f); glVertex3f(w2.x + off.x, w2.y + off.y, w2.z + off.z); glVertex3f(w3.x + off.x, w3.y + off.y, w3.z + off.z);
            glColor4f(1.0f, 0.9f, 0.6f, 0.13f); glVertex3f(w0.x, w0.y, w0.z); glVertex3f(w3.x, w3.y, w3.z);
            glColor4f(1.0f, 0.9f, 0.6f, 0.0f); glVertex3f(w3.x + off.x, w3.y + off.y, w3.z + off.z); glVertex3f(w0.x + off.x, w0.y + off.y, w0.z + off.z);
            glColor4f(1.0f, 0.9f, 0.6f, 0.13f); glVertex3f(w1.x, w1.y, w1.z); glVertex3f(w2.x, w2.y, w2.z);
            glColor4f(1.0f, 0.9f, 0.6f, 0.0f); glVertex3f(w2.x + off.x, w2.y + off.y, w2.z + off.z); glVertex3f(w1.x + off.x, w1.y + off.y, w1.z + off.z);
            glEnd();
            glEnable(GL_TEXTURE_2D);
            for (k = 0; k < 70; k++) {
                float u = hash2(k, 1, 31), v = hash2(k, 2, 31), w = fmodf(hash2(k, 3, 31) + g_frame * 0.0004f, 1.0f);
                vec3 p = v3add(v3lerp(v3lerp(w0, w1, u), v3lerp(w3, w2, u), v), v3scale(off, w));
                p.x += sinf(g_frame * 0.01f + k) * 0.6f;
                p.y += sinf(g_frame * 0.013f + k * 2) * 0.5f;
                glow(p, 0.35f + 0.25f * hash2(k, 4, 31), 1.0f, 0.95f, 0.8f, 0.5f * sinf(w * PI_F));
            }
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        /* Zzz bubbles above the sleeping cat */
        glEnable(GL_TEXTURE_2D);
        for (i = 0; i < nobj; i++) {
            Obj *o = &objs[i];
            int k;
            if (!o->active || o->type != OT_GIANTCAT || o->state != 0) continue;
            for (k = 0; k < 3; k++) {
                float ph = fmodf(g_frame * 0.006f + k / 3.0f, 1.0f);
                vec3 p = v3add(v3mad(o->pos, yaw_dir(o->yaw), 2.6f), v3(sinf(ph * 6 + k) * 0.6f, 3.5f + ph * 4.0f, 0));
                glow(p, 0.6f + ph * 1.2f, 0.85f, 0.9f, 1.0f, 0.6f * (1 - ph));
            }
        }
    }
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_LIGHTING);
}
