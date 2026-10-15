/* objects.c - collectibles, enemies, bosses, platforms, NPCs, effects */
#include "game.h"
#include <string.h>
#include <stdio.h>

Obj objs[MAX_OBJ];
int nobj;
vec3 g_race_path[40];
int g_race_n;
int g_ring_next;
static int ring_total;

static int held(int b);
extern int g_trace;

#define PR 0.42f   /* player radius */
#define PH 1.6f    /* player height */

int obj_index(Obj *o) { return (int)(o - objs); }

void objs_clear(void)
{
    memset(objs, 0, sizeof objs);
    nobj = 0;
    g_ring_next = 0;
    ring_total = 0;
}

void obj_kill(Obj *o)
{
    if (pl.held == obj_index(o)) pl.held = -1;
    if (pl.platform == obj_index(o)) pl.platform = -1;
    o->active = 0;
}

Obj *obj_spawn(int type, vec3 pos, float yaw, int param)
{
    int i;
    Obj *o = NULL;
    for (i = 0; i < nobj; i++) if (!objs[i].active) { o = &objs[i]; break; }
    if (!o) {
        if (nobj >= MAX_OBJ) return NULL;
        o = &objs[nobj++];
    }
    memset(o, 0, sizeof *o);
    o->type = type;
    o->active = 1;
    o->pos = o->home = o->prev_pos = pos;
    o->yaw = o->prev_yaw = yaw;
    o->param = param;
    o->scale = 1;
    o->radius = 0.6f;
    o->height = 1.0f;
    o->floor_y = -1e9f;
    switch (type) {
    case OT_COIN: case OT_REDCOIN: case OT_BLUECOIN: o->radius = 0.55f; o->height = 0.9f; o->flags = OF_SHADOW; break;
    case OT_ONEUP: o->radius = 0.6f; o->flags = OF_SHADOW; break;
    case OT_STAR: o->radius = 0.8f; o->height = 1.4f; o->flags = OF_SHADOW; break;
    case OT_BUG: o->radius = 0.7f; o->height = 1.0f; o->flags = OF_SHADOW; break;
    case OT_FORKBOMB: o->radius = 0.6f; o->height = 1.3f; o->flags = OF_SHADOW | OF_GRABBABLE; break;
    case OT_KING: o->radius = 1.6f; o->height = 3.6f; o->flags = OF_SHADOW; o->scale = 2.6f; break;
    case OT_CRUSHER: o->radius = 1.5f; o->height = 3.4f; o->flags = OF_SOLID | OF_SHADOW;
        o->f[5] = 1.5f; o->f[6] = 1.7f; o->f[7] = 1.5f; break;
    case OT_ROLLER: o->radius = 1.2f; o->height = 2.4f; o->flags = OF_SHADOW; break;
    case OT_PUFFER: o->radius = 0.7f; o->height = 1.2f; break;
    case OT_CRAB: o->radius = 0.8f; o->height = 0.9f; o->flags = OF_SHADOW; break;
    case OT_BRUTE: o->radius = 1.0f; o->height = 2.0f; o->flags = OF_SHADOW; break;
    case OT_SEGFAULT: o->radius = 2.4f; o->height = 4.5f; o->flags = OF_SHADOW; break;
    case OT_FIREBALL: o->radius = 0.45f; o->height = 0.8f; break;
    case OT_SPIKEBOMB: o->radius = 1.1f; o->height = 2.2f; o->flags = OF_SHADOW; break;
    case OT_PLATFORM: case OT_ROTPLAT: case OT_ELEVATOR: case OT_FALLPLAT:
        o->flags = OF_SOLID | OF_SHADOW; break;
    case OT_SIGN: o->radius = 0.6f; o->height = 1.7f; o->flags = OF_SHADOW; break;
    case OT_NPC: o->radius = 0.6f; o->height = 1.6f; o->flags = OF_SHADOW; break;
    case OT_MOTHER: o->radius = 0.8f; o->height = 2.3f; o->flags = OF_SHADOW; break;
    case OT_BABY: o->radius = 0.5f; o->height = 1.0f; o->flags = OF_SHADOW | OF_GRABBABLE; break;
    case OT_RACER: o->radius = 0.8f; o->height = 2.0f; o->flags = OF_SHADOW; break;
    case OT_RACEFLAG: o->radius = 1.5f; o->height = 4.0f; break;
    case OT_TRAMPOLINE: o->radius = 1.5f; o->height = 0.9f; o->flags = OF_SOLID | OF_SHADOW; break;
    case OT_TREE: case OT_POLE: o->radius = 0.35f; o->height = 4.0f; break;
    case OT_WARP: o->radius = 1.5f; o->height = 3.0f; break;
    case OT_DOOR: o->radius = 1.3f; o->height = 3.4f; o->flags = OF_SOLID;
        o->f[1] = 1; o->f[2] = 1; o->f[5] = 1.2f; o->f[6] = 1.7f; o->f[7] = 0.15f; break;
    case OT_STARDOOR:   /* star doors fill the 4 x 4 doorways of the castle */
        o->radius = 2.0f; o->height = 4.0f; o->flags = OF_SOLID;
        o->f[1] = 4.0f / 2.4f; o->f[2] = 4.0f / 3.4f; o->f[5] = 2.0f; o->f[6] = 2.0f; o->f[7] = 0.15f; break;
    case OT_CLAM: o->radius = 1.8f; o->height = 1.0f; break;
    case OT_RING: o->radius = 1.4f; o->height = 2.8f; ring_total++; break;
    case OT_CRATE: o->radius = 0.7f; o->height = 1.4f; o->flags = OF_SOLID | OF_SHADOW;
        o->f[5] = 0.7f; o->f[6] = 0.7f; o->f[7] = 0.7f; break;
    case OT_GRANDSTAR: o->radius = 1.5f; o->height = 3.0f; o->flags = OF_SHADOW; break;
    case OT_BLUESWITCH: o->radius = 1.0f; o->height = 0.4f; o->flags = OF_SHADOW; break;
    case OT_TIMERGOAL: o->radius = 4.0f; o->height = 4.0f; break;
    case OT_GHOST: o->radius = 0.7f; o->height = 1.7f; o->f[0] = 0.9f; break;
    case OT_GHOSTKING: o->radius = 1.7f; o->height = 4.0f; o->scale = 2.5f; break;
    case OT_TOPSWITCH: o->radius = 1.2f; o->height = 1.0f; break;
    case OT_HEART: o->radius = 0.9f; o->height = 1.4f; o->flags = OF_SHADOW; break;
    case OT_WATCHDOG: o->radius = 1.3f; o->height = 2.4f; o->flags = OF_SHADOW; break;
    case OT_CAGE: o->radius = 1.6f; o->height = 3.0f; o->flags = OF_SOLID; break;
    case OT_STEAM: o->radius = 0; o->height = 0; break;
    case OT_OOM: o->radius = 0.95f; o->height = 3.4f; o->flags = OF_SHADOW; break;
    case OT_MAZEEXIT: o->radius = 2.0f; o->height = 3.0f; break;
    case OT_ROCKETBOX: o->radius = 0.8f; o->height = 1.4f; o->flags = OF_SOLID;
        o->f[5] = 0.7f; o->f[6] = 0.7f; o->f[7] = 0.7f; break;
    }
    return o;
}

/* solid box helper: centre at pos, half extents in f[5..7] */
void obj_set_box(Obj *o, float hx, float hy, float hz, int tt, int ts, float r, float g, float b)
{
    o->f[5] = hx; o->f[6] = hy; o->f[7] = hz;
    o->radius = sqrtf(hx * hx + hz * hz);
    o->height = hy * 2;
    o->dl = model_box(hx, hy, hz, tt, ts, r, g, b);
}

void fx_spawn(int type, vec3 p, float scale)
{
    Obj *o = obj_spawn(type, p, 0, 0);
    if (!o) return;
    o->scale = scale;
    o->flags = 0;
    o->vel = v3(frands() * 0.5f, 1.0f + frand(), frands() * 0.5f);
    if (type == OT_FX_SPARKLE) o->vel = v3(frands() * 2, frands() * 2 + 1, frands() * 2);
}

void spawn_coins_burst(vec3 p, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        Obj *o = obj_spawn(OT_COIN, v3add(p, v3(0, 0.5f, 0)), 0, 0);
        float a = TAU_F * i / n + frand();
        if (!o) return;
        o->vel = v3(sinf(a) * 3.0f, 9.0f + frand() * 3, cosf(a) * 3.0f);
        o->state = 1;       /* moving coin */
        o->timer = 0;
    }
}

Obj *star_spawn(vec3 p, int star, int from_pos, vec3 from)
{
    Obj *o = obj_spawn(OT_STAR, p, 0, star);
    if (!o) return NULL;
    if (from_pos) {
        o->state = 1;
        o->f[0] = from.x; o->f[1] = from.y; o->f[2] = from.z;
        o->pos = from;
        sfx(SFX_STAR_APPEAR);
    }
    return o;
}

/* ---------------------------------------------------------------- helpers */
static int touching(Obj *o)
{
    float dx = pl.pos.x - o->pos.x, dz = pl.pos.z - o->pos.z, r = o->radius + PR;
    if (!pl.visible) return 0;
    if ((o->type == OT_WARP || o->type == OT_TIMERSTART) && o->f[5] > 0) {
        return fabsf(dx) < o->f[5] + PR && fabsf(dz) < o->f[7] + PR &&
               pl.pos.y + PH > o->pos.y && pl.pos.y < o->pos.y + o->height;
    }
    if (dx * dx + dz * dz > r * r) return 0;
    if (pl.pos.y > o->pos.y + o->height || pl.pos.y + PH < o->pos.y) return 0;
    return 1;
}

static float dist_player(Obj *o) { return v3dist(o->pos, pl.pos); }

static void push_player_out(Obj *o, float r)
{
    float dx = pl.pos.x - o->pos.x, dz = pl.pos.z - o->pos.z, d = sqrtf(dx * dx + dz * dz), rr = r + PR;
    if (d < rr && d > 1e-4f) {
        pl.pos.x = o->pos.x + dx / d * rr;
        pl.pos.z = o->pos.z + dz / d * rr;
    }
}

static int stomped_by_player(Obj *o)
{
    return pl.vel.y < -1.0f && pl.pos.y > o->pos.y + o->height * 0.45f && player_in_air();
}

static void player_push(vec3 from, float strength)
{
    if (pl.act == ACT_DEATH || pl.act == ACT_STAR_DANCE) return;
    pl.yaw = yaw_to(pl.pos, from);
    pl.fwd = -strength;
    if (player_in_air()) { player_set_act(ACT_AIR_KNOCK); pl.vel.y = 5; }
    else player_set_act(ACT_KNOCKBACK);
    pl.act_t = 12;
}

/* ground physics for walkers; returns 1 if on ground */
static int obj_physics(Obj *o, float grav, float radius)
{
    Surf *s;
    vec3 wn;
    float fy;
    vec3 np = v3mad(o->pos, o->vel, DT);
    o->vel.y -= grav * DT;
    if (o->vel.y < -25) o->vel.y = -25;
    if (radius > 0) {
        col_walls(&np, radius * 0.8f + 0.1f, radius, &wn, NULL);
    }
    fy = col_floor(np.x, np.y + 0.8f, np.z, &s);
    if (!s) { np.x = o->pos.x; np.z = o->pos.z; fy = col_floor(np.x, np.y + 0.8f, np.z, &s); o->vel.x = -o->vel.x; o->vel.z = -o->vel.z; }
    o->floor = s;
    o->floor_y = fy;
    o->pos = np;
    if (s && o->pos.y <= fy) {
        o->pos.y = fy;
        if (o->vel.y < 0) o->vel.y = 0;
        return 1;
    }
    return 0;
}

static int obj_hit_by_attack(Obj *o)
{
    float r = o->radius + PR + (pl.act == ACT_PUNCH || pl.act == ACT_JUMPKICK ? 0.9f : 0.5f);
    if (!player_is_attacking()) return 0;
    if (v3distxz(o->pos, pl.pos) > r) return 0;
    if (pl.pos.y > o->pos.y + o->height + 0.5f || pl.pos.y + PH < o->pos.y - 0.3f) return 0;
    if (pl.act == ACT_PUNCH || pl.act == ACT_JUMPKICK) {
        vec3 d = v3norm(v3sub(v3(o->pos.x, 0, o->pos.z), v3(pl.pos.x, 0, pl.pos.z)));
        if (v3dot(d, yaw_dir(pl.yaw)) < 0.2f) return 0;
    }
    return 1;
}

static void enemy_die(Obj *o, int coins, int squish)
{
    o->state = 90;
    o->timer = 0;
    o->i[7] = squish;
    sfx(squish ? SFX_STOMP : SFX_POP);
    fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(0, 0.4f, 0)), 1.0f);
    if (coins > 0) spawn_coins_burst(o->pos, coins);
}

/* ---------------------------------------------------------------- collectibles */
static void upd_coin(Obj *o)
{
    o->yaw += 3.5f * DT;
    if (o->type == OT_BLUECOIN && o->param == 1 && g_blue_switch_timer <= 0) return; /* hidden */
    if (o->state == 1) {
        o->timer++;
        o->vel.y -= 30 * DT;
        {
            Surf *s;
            vec3 np = v3mad(o->pos, o->vel, DT), wn;
            float fy;
            col_walls(&np, 0.3f, 0.3f, &wn, NULL);
            fy = col_floor(np.x, np.y + 0.5f, np.z, &s);
            if (!s) { np.x = o->pos.x; np.z = o->pos.z; }
            if (s && np.y < fy) { np.y = fy; o->vel.y = -o->vel.y * 0.5f; o->vel.x *= 0.6f; o->vel.z *= 0.6f; if (fabsf(o->vel.y) > 1) sfx_ex(SFX_COIN, 0.15f, 2.0f); }
            o->pos = np;
            if (o->pos.y < lv.death_y) { obj_kill(o); return; }
        }
        if (o->timer > 600) { obj_kill(o); return; }
        if (o->timer < 15) return;
    }
    if (touching(o)) {
        if (o->type == OT_REDCOIN) { game_red_coin(o->pos); }
        else game_add_coins(o->type == OT_BLUECOIN ? 5 : 1, o->pos);
        fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(0, 0.5f, 0)), 0.5f);
        obj_kill(o);
    }
}

static void upd_oneup(Obj *o)
{
    o->yaw += 3 * DT;
    o->pos.y = o->home.y + sinf(g_frame * 0.05f) * 0.2f;
    if (touching(o)) {
        pl.lives++;
        sfx(SFX_1UP);
        hud_message("1-UP!", 90);
        obj_kill(o);
    }
}

static void upd_star(Obj *o)
{
    o->yaw += 2.5f * DT;
    if (o->state == 1) {
        /* fly from source to destination */
        float t = clampf(o->timer / 90.0f, 0, 1);
        vec3 from = v3(o->f[0], o->f[1], o->f[2]);
        o->timer++;
        o->pos = v3lerp(from, o->home, t);
        o->pos.y += sinf(t * PI_F) * 4.0f;
        o->yaw += 10 * DT;
        if (g_frame % 3 == 0) fx_spawn(OT_FX_SPARKLE, o->pos, 0.4f);
        if (t >= 1) { o->state = 0; o->pos = o->home; }
        return;
    }
    o->pos.y = o->home.y + sinf(g_frame * 0.04f) * 0.15f;
    if (o->i[0] > 0) {   /* inside a clam: only collectable when it's open */
        Obj *c = &objs[o->i[0] - 1];
        if (c->active && c->f[0] < 0.5f) return;
    }
    if (touching(o) && pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE) {
        game_star_collected(o->param, o->pos);
        obj_kill(o);
    }
}

static void upd_grandstar(Obj *o)
{
    o->yaw += 2 * DT;
    o->pos.y = o->home.y + sinf(g_frame * 0.04f) * 0.3f;
    if (g_frame % 4 == 0) fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(frands() * 2, frands() * 2, frands() * 2)), 0.5f);
    if (touching(o) && pl.act != ACT_DEATH) {
        obj_kill(o);
        game_grand_star();
    }
}

static void upd_blueswitch(Obj *o)
{
    if (o->state == 0) {
        float dx = pl.pos.x - o->pos.x, dz = pl.pos.z - o->pos.z;
        if (dx * dx + dz * dz < 1.2f && pl.pos.y < o->pos.y + 0.6f && pl.pos.y > o->pos.y - 0.2f && !player_in_air()) {
            o->state = 1;
            g_blue_switch_timer = 60 * 18;
            sfx(SFX_POUND);
            sfx(SFX_BLUECOIN);
            hud_message("Blue fish appeared!", 120);
        }
    }
}

/* ---------------------------------------------------------------- enemies */
static void upd_bug(Obj *o)
{
    float d;
    if (o->state == 90) { if (++o->timer > 30) obj_kill(o); return; }
    d = dist_player(o);
    o->timer++;
    if (o->state == 0) {          /* wander */
        if (o->timer % 120 == 0) o->f[0] = o->yaw + frands() * 2.0f;
        o->yaw = approach_angle(o->yaw, o->f[0], 2 * DT);
        o->vel.x = sinf(o->yaw) * 1.5f;
        o->vel.z = cosf(o->yaw) * 1.5f;
        if (v3distxz(o->pos, o->home) > 6) o->f[0] = yaw_to(o->pos, o->home);
        if (d < 8 && fabsf(pl.pos.y - o->pos.y) < 3) { o->state = 1; o->vel.y = 5; }
    } else {                      /* chase */
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 3 * DT);
        o->vel.x = sinf(o->yaw) * 3.6f;
        o->vel.z = cosf(o->yaw) * 3.6f;
        if (d > 12 || v3distxz(o->pos, o->home) > 16) o->state = 0;
    }
    obj_physics(o, 30, 0.6f);
    if (o->pos.y < lv.death_y) { obj_kill(o); return; }
    if (obj_hit_by_attack(o)) { enemy_die(o, 1, 0); return; }
    if (touching(o)) {
        if (stomped_by_player(o) || pl.act == ACT_GPOUND) {
            enemy_die(o, 1, 1);
            player_bounce(held(BT_A) ? 15 : 10);
        } else player_hurt(1, o->pos);
    }
}

static int held(int b) { return (in.held & b) != 0; }

static void explode_at(vec3 p, int hurts)
{
    Obj *e = obj_spawn(OT_FX_EXPLODE, p, 0, 0);
    if (e) { e->flags = 0; e->f[0] = hurts ? 1.0f : 0.0f; }
    sfx(SFX_EXPLODE);
    cam.shake = maxf(cam.shake, v3dist(p, pl.pos) < 15 ? 0.3f : 0.1f);
}

static void upd_forkbomb(Obj *o)
{
    float d = dist_player(o);
    if (o->state == 90) {    /* respawn wait */
        if (o->param2 == 7) { obj_kill(o); return; }   /* forked by King Daemon */
        o->timer++;
        if (o->timer > 600) { o->pos = o->home; o->state = 0; o->timer = 0; o->vel = v3(0, 0, 0); }
        return;
    }
    if (o->flags & OF_HELD) {
        vec3 dd = yaw_dir(pl.yaw);
        o->pos = v3add(v3mad(pl.pos, dd, 0.6f), v3(0, 0.6f, 0));
        o->yaw = pl.yaw;
        if (o->state == 1) o->timer++;
        if (o->state == 1 && o->timer > 240) {
            pl.held = -1; o->flags &= ~OF_HELD;
            explode_at(o->pos, 1);
            o->state = 90; o->timer = 0; o->pos = v3(0, -1000, 0);
        }
        return;
    }
    if (o->state == 100 || o->state == 101) {   /* thrown / dropped */
        int g = obj_physics(o, 30, 0.5f);
        o->timer++;
        if ((g && o->state == 100 && o->timer > 3) || o->timer > 150) {
            explode_at(o->pos, 0);
            o->state = 90; o->timer = 0; o->pos = v3(0, -1000, 0);
            return;
        }
        if (o->state == 101 && g) { o->state = 1; o->timer = 0; }
        return;
    }
    o->timer++;
    if (o->state == 0) {
        if (o->timer % 150 == 0) o->f[0] = o->yaw + frands() * 2.5f;
        o->yaw = approach_angle(o->yaw, o->f[0], 1.5f * DT);
        o->vel.x = sinf(o->yaw) * 1.2f;
        o->vel.z = cosf(o->yaw) * 1.2f;
        if (v3distxz(o->pos, o->home) > 6) o->f[0] = yaw_to(o->pos, o->home);
        if (d < 7 && fabsf(pl.pos.y - o->pos.y) < 3) { o->state = 1; o->timer = 0; }
    } else if (o->state == 1) {
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 2.5f * DT);
        o->vel.x = sinf(o->yaw) * 3.4f;
        o->vel.z = cosf(o->yaw) * 3.4f;
        if (o->timer % 30 == 0) sfx_ex(SFX_STEP, 0.6f, 2.0f);
        if (o->timer > 240) {
            explode_at(o->pos, 1);
            o->state = 90; o->timer = 0; o->pos = v3(0, -1000, 0);
            return;
        }
    }
    obj_physics(o, 30, 0.5f);
    if (o->pos.y < lv.death_y) { o->state = 90; o->timer = 0; return; }
    if (obj_hit_by_attack(o) || (touching(o) && stomped_by_player(o))) {
        if (stomped_by_player(o)) player_bounce(10);
        o->vel = v3scale(yaw_dir(pl.yaw), 10);
        o->vel.y = 8;
        o->state = 100;
        o->timer = 0;
        sfx(SFX_KICK);
        return;
    }
    if (touching(o) && o->state == 1 && o->timer > 200) {
        explode_at(o->pos, 1);
        o->state = 90; o->timer = 0; o->pos = v3(0, -1000, 0);
    }
}

/* King Daemon: fork()s lit bombs at Tux. Pick one up (B) and throw it back at him: 3 hits.
 * f[0]: fork timer, hp: hits taken */
static int count_king_bombs(void)
{
    int i, n = 0;
    for (i = 0; i < nobj; i++)
        if (objs[i].active && objs[i].type == OT_FORKBOMB && objs[i].param2 == 7 && objs[i].state != 90) n++;
    return n;
}

static void upd_king(Obj *o)
{
    float d = dist_player(o);
    int i;
    if (o->state == 90) {
        o->timer++;
        o->scale = 2.6f * (1 - clampf(o->timer / 60.0f, 0, 1));
        if (o->timer % 8 == 0) fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(frands() * 2, 1 + frand() * 2, frands() * 2)), 1.0f);
        if (o->timer == 60) {
            star_spawn(v3add(o->home, v3(0, 1.2f, 0)), o->param, 1, o->pos);
            obj_kill(o);
        }
        return;
    }
    o->timer++;
    /* hit by one of his own bombs? */
    if (o->state == 1 || o->state == 2) {
        for (i = 0; i < nobj; i++) {
            Obj *b = &objs[i];
            if (!b->active || b->type != OT_FORKBOMB || b->state != 100) continue;
            if (v3distxz(b->pos, o->pos) < o->radius + 0.9f && b->pos.y > o->pos.y - 0.5f && b->pos.y < o->pos.y + o->height) {
                explode_at(b->pos, 0);
                b->state = 90; b->timer = 0; b->pos = v3(0, -1000, 0);
                o->hp++;
                sfx(SFX_ROAR);
                cam.shake = 0.5f;
                if (o->hp >= 3) {
                    o->state = 90; o->timer = 0;
                    dialog_open("KING DAEMON: Gah! Blown up by my own child\nprocesses... Fine, take the star!");
                    music_play(lv.music);
                    return;
                }
                o->state = 3; o->timer = 0;
                o->vel = v3scale(v3norm(v3sub(o->home, o->pos)), 3);
                o->vel.y = 9;
                hud_message(o->hp == 1 ? "KING DAEMON: Ow! That's MY bomb!" : "KING DAEMON: Grr... one more and I'm done!", 120);
                break;
            }
        }
    }
    switch (o->state) {
    case 0:   /* waiting for the player to arrive */
        if (d < 10 && pl.pos.y > o->pos.y - 0.5f && !player_in_air()) {
            o->state = 1; o->timer = 0;
            dialog_open("KING DAEMON: Who dares climb my peak?!\nI am the King of all background processes!\n"
                        "Behold my fork()! Bombs, bombs, more bombs!\nYou'd better not grab them and throw them\nback at me...");
            music_play(MUS_BOSS);
        }
        break;
    case 1:   /* stomp toward the player */
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 1.0f * DT);
        if (d > 5.0f) {
            o->vel.x = sinf(o->yaw) * 1.4f;
            o->vel.z = cosf(o->yaw) * 1.4f;
        } else { o->vel.x = o->vel.z = 0; }
        if (o->timer % 40 == 0) sfx_ex(SFX_THUD, 0.4f, 1.5f);
        if (o->timer > 150 - o->hp * 30 && count_king_bombs() < 2) { o->state = 2; o->timer = 0; }
        break;
    case 2:   /* fork(): spit out a lit bomb */
        o->vel.x = o->vel.z = 0;
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 2.0f * DT);
        if (o->timer == 30) {
            vec3 fd = yaw_dir(o->yaw);
            Obj *b = obj_spawn(OT_FORKBOMB, v3add(v3mad(o->pos, fd, 2.2f), v3(0, 2.5f, 0)), o->yaw, 0);
            if (b) {
                b->param2 = 7;
                b->home = o->home;
                b->state = 101;   /* falls, then starts walking with its fuse lit */
                b->vel = v3(fd.x * 5, 6, fd.z * 5);
                b->timer = 0;
            }
            sfx(SFX_POP);
            sfx_ex(SFX_ROAR, 0.5f, 1.3f);
        }
        if (o->timer > 60) { o->state = 1; o->timer = 0; }
        break;
    case 3:   /* stunned by a blast */
        if (o->timer > 70) { o->state = 1; o->timer = 0; o->vel = v3(0, 0, 0); }
        break;
    }
    obj_physics(o, 30, 1.2f);
    if (o->state == 3 && o->timer > 2) { o->vel.x *= 0.9f; o->vel.z *= 0.9f; }
    if (o->pos.y < o->home.y - 3.0f) {
        o->pos = o->home;
        o->vel = v3(0, 0, 0);
        o->state = 3; o->timer = 0;
        sfx(SFX_JUMP3);
        fx_spawn(OT_FX_SMOKE, o->pos, 2.0f);
    }
    if (o->state != 0 && v3distxz(o->pos, o->home) > 9) {
        o->vel.x = sinf(yaw_to(o->pos, o->home)) * 2;
        o->vel.z = cosf(yaw_to(o->pos, o->home)) * 2;
    }
    if (touching(o)) {
        vec3 fd = yaw_dir(o->yaw), tp = v3norm(v3sub(pl.pos, o->pos));
        if (v3dot(fd, tp) > 0.3f && o->state == 1) player_hurt(1, o->pos);
        else push_player_out(o, o->radius);
    }
}

static void upd_crusher(Obj *o)
{
    float top = o->home.y, bot = o->f[0];   /* f[0] = resting y of centre at the bottom */
    o->timer++;
    switch (o->state) {
    case 0:
        if (o->timer > 60 + o->param) { o->state = 1; o->timer = 0; o->vel.y = 0; }
        break;
    case 1:
        o->vel.y -= 45 * DT;
        o->pos.y += o->vel.y * DT;
        if (o->pos.y <= bot) {
            o->pos.y = bot;
            o->state = 2; o->timer = 0;
            if (v3dist(o->pos, pl.pos) < 20) { sfx_ex(SFX_THUD, clampf(1.2f - v3dist(o->pos, pl.pos) / 20, 0.1f, 1), 1); cam.shake = 0.2f; }
        }
        break;
    case 2:
        if (o->timer > 50) { o->state = 3; o->timer = 0; }
        break;
    case 3:
        o->pos.y += 3.0f * DT;
        if (o->pos.y >= top) { o->pos.y = top; o->state = 0; o->timer = 0; }
        break;
    }
    /* crush check: player underneath while falling */
    if (o->state == 1) {
        float dx = fabsf(pl.pos.x - o->pos.x), dz = fabsf(pl.pos.z - o->pos.z);
        if (dx < o->f[5] + 0.2f && dz < o->f[7] + 0.2f && pl.pos.y + PH > o->pos.y - o->f[6] && pl.pos.y < o->pos.y) {
            player_hurt(3, v3add(pl.pos, v3(0, 0, 0.01f)));
            pl.pos.x += (pl.pos.x > o->pos.x ? 1 : -1) * (o->f[5] + 0.6f - dx);
        }
    }
}

static void upd_rollergen(Obj *o)
{
    o->timer++;
    if (o->timer > o->param) {
        Obj *r = obj_spawn(OT_ROLLER, o->pos, 0, 0);
        o->timer = 0;
        if (r) {
            r->f[0] = o->f[0]; r->f[1] = o->f[1]; r->f[2] = o->f[2];
            r->home = o->pos;
        }
    }
}

static void upd_roller(Obj *o)
{
    vec3 end = v3(o->f[0], o->f[1], o->f[2]);
    vec3 d = v3sub(end, o->home);
    float len = v3len(v3(d.x, 0, d.z)), sp = 5.5f;
    Surf *s;
    float fy;
    o->timer++;
    if (o->state == 0) {
        vec3 dir = v3norm(v3(d.x, 0, d.z));
        o->pos.x += dir.x * sp * DT;
        o->pos.z += dir.z * sp * DT;
        o->pitch += sp * DT / 1.2f;
        o->yaw = atan2f(dir.x, dir.z);
        fy = col_floor(o->pos.x, o->pos.y + 1.0f, o->pos.z, &s);
        if (s && o->pos.y - fy < 1.0f) o->pos.y = fy;
        else { o->vel.y -= 30 * DT; o->pos.y += o->vel.y * DT; }
        if (v3distxz(o->pos, o->home) > len) { fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(0, 1, 0)), 2.0f); obj_kill(o); return; }
    } else {
        o->vel.y -= 30 * DT;
        o->pos.y += o->vel.y * DT;
        o->pos.x += sinf(o->yaw) * sp * DT;
        o->pos.z += cosf(o->yaw) * sp * DT;
        if (o->timer > 60 * 30 || o->pos.y < lv.death_y || o->pos.y < o->f[1] - 12) { obj_kill(o); return; }
    }
    if (touching(o)) {
        if (pl.act == ACT_GPOUND || stomped_by_player(o)) player_bounce(12);
        else player_hurt(2, o->pos);
    }
}

static void upd_puffer(Obj *o)
{
    float d = dist_player(o);
    float wy = water_level_at(o->pos.x, o->pos.z, o->pos.y);
    if (o->state == 90) { if (++o->timer > 30) obj_kill(o); return; }
    o->timer++;
    if (d < 10 && pl.act == ACT_SWIM) {
        vec3 dir = v3norm(v3sub(v3add(pl.pos, v3(0, 0.6f, 0)), o->pos));
        o->vel = v3lerp(o->vel, v3scale(dir, 2.6f), 2 * DT);
        o->yaw = atan2f(dir.x, dir.z);
    } else {
        vec3 tgt = v3add(o->home, v3(sinf(o->timer * 0.01f) * 3, sinf(o->timer * 0.02f), cosf(o->timer * 0.01f) * 3));
        vec3 dir = v3sub(tgt, o->pos);
        o->vel = v3lerp(o->vel, v3scale(v3norm(dir), minf(v3len(dir), 1.5f)), 2 * DT);
        if (v3len(v3(o->vel.x, 0, o->vel.z)) > 0.1f) o->yaw = atan2f(o->vel.x, o->vel.z);
    }
    o->pos = v3mad(o->pos, o->vel, DT);
    if (wy > -1e8f && o->pos.y > wy - 0.8f) o->pos.y = wy - 0.8f;
    {
        Surf *s;
        float fy = col_floor(o->pos.x, o->pos.y + 0.5f, o->pos.z, &s);
        if (s && o->pos.y < fy + 0.6f) o->pos.y = fy + 0.6f;
    }
    o->scale = 1.0f + sinf(g_frame * 0.1f) * 0.08f;
    if (obj_hit_by_attack(o)) { enemy_die(o, 2, 0); return; }
    if (touching(o)) player_hurt(1, o->pos);
}

static void upd_crab(Obj *o)
{
    float d;
    if (o->state == 90) { if (++o->timer > 30) obj_kill(o); return; }
    d = dist_player(o);
    o->timer++;
    {
        float side = sinf(o->timer * 0.02f) > 0 ? 1.0f : -1.0f;
        vec3 r = v3(cosf(o->yaw), 0, -sinf(o->yaw));
        o->vel.x = r.x * side * 2.0f;
        o->vel.z = r.z * side * 2.0f;
        if (d < 6) {
            vec3 t = v3norm(v3sub(pl.pos, o->pos));
            o->vel.x = t.x * 3.0f; o->vel.z = t.z * 3.0f;
            o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 2 * DT);
        }
    }
    obj_physics(o, 30, 0.6f);
    if (v3distxz(o->pos, o->home) > 8) { o->pos.x = lerpf(o->pos.x, o->home.x, 0.02f); o->pos.z = lerpf(o->pos.z, o->home.z, 0.02f); }
    if (o->pos.y < lv.death_y) { obj_kill(o); return; }
    if (obj_hit_by_attack(o)) { enemy_die(o, 1, 0); return; }
    if (touching(o)) {
        if (stomped_by_player(o) || pl.act == ACT_GPOUND) { enemy_die(o, 1, 1); player_bounce(10); }
        else player_hurt(1, o->pos);
    }
}

static void upd_brute(Obj *o)
{
    float d = dist_player(o);
    int g;
    if (o->state == 90) {   /* sinking into freezing water */
        o->timer++;
        o->pos.y -= 1.0f * DT;
        if (o->timer % 6 == 0) fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(frands(), 1.5f, frands())), 0.8f);
        if (o->timer == 70) {
            star_spawn(v3add(o->home, v3(0, 1.0f, 0)), o->param, 1, o->pos);
            obj_kill(o);
        }
        return;
    }
    o->timer++;
    /* approach the player when close */
    if (d < 14 && o->state == 0 && fabsf(pl.pos.y - o->pos.y) < 4) {
        vec3 t = v3norm(v3sub(v3(pl.pos.x, 0, pl.pos.z), v3(o->pos.x, 0, o->pos.z)));
        o->yaw = approach_angle(o->yaw, atan2f(t.x, t.z), 2.5f * DT);
        o->vel.x += sinf(o->yaw) * 4.5f * DT;
        o->vel.z += cosf(o->yaw) * 4.5f * DT;
    }
    if (o->state == 1 && o->timer > 60) o->state = 0;
    {
        float sp = v3len(v3(o->vel.x, 0, o->vel.z)), mx = o->state == 1 ? 14.0f : 3.2f;
        if (sp > mx) { o->vel.x *= mx / sp; o->vel.z *= mx / sp; }
        o->vel.x = approachf(o->vel.x, 0, 4.0f * DT * fabsf(o->vel.x) / maxf(sp, 0.01f));
        o->vel.z = approachf(o->vel.z, 0, 4.0f * DT * fabsf(o->vel.z) / maxf(sp, 0.01f));
    }
    g = obj_physics(o, 30, 0.9f);
    if (g && o->floor && (o->floor->flags & SF_BURN)) {
        o->state = 90; o->timer = 0;
        sfx(SFX_ROAR);
        sfx(SFX_SPLASH);
        hud_message("The Ice Brute is frozen solid!", 120);
        return;
    }
    if (o->pos.y < lv.death_y) { o->pos = o->home; o->vel = v3(0, 0, 0); }
    if (obj_hit_by_attack(o) || (touching(o) && (pl.act == ACT_GPOUND || stomped_by_player(o)))) {
        vec3 t = v3norm(v3sub(v3(o->pos.x, 0, o->pos.z), v3(pl.pos.x, 0, pl.pos.z)));
        float k = (pl.act == ACT_DIVE || pl.act == ACT_BELLY_SLIDE) ? 13.0f : 10.0f;
        if (stomped_by_player(o)) { player_bounce(10); k = 5; }
        o->vel.x = t.x * k; o->vel.z = t.z * k;
        o->state = 1; o->timer = 0;
        sfx(SFX_PUNCH);
        sfx_ex(SFX_ROAR, 0.4f, 1.6f);
        player_push(o->pos, 3);
        return;
    }
    if (touching(o)) {
        vec3 t = v3norm(v3sub(v3(o->pos.x, 0, o->pos.z), v3(pl.pos.x, 0, pl.pos.z)));
        float rel = v3len(v3(o->vel.x, 0, o->vel.z));
        player_push(o->pos, 6 + rel);
        o->vel.x = t.x * 2.5f; o->vel.z = t.z * 2.5f;
        o->state = 1; o->timer = 20;
        sfx(SFX_BOUNCE);
    }
}

/* -------- Segfault (final boss) --------
 * He can't be hurt directly. Bait his charge so he rams one of the spike mines around the arena: 3 hits. */
static void upd_segfault(Obj *o)
{
    float d = dist_player(o);
    vec3 center = o->home;
    int i;
    o->timer++;
    switch (o->state) {
    case 0:
        if (d > 24 || fabsf(pl.pos.y - o->pos.y) > 4) { o->timer = 0; break; }
        if (o->timer == 1) {
            dialog_open("SEGFAULT: So the little penguin made it to the core!\n"
                        "I'll dump your core and free() your memory!\n"
                        "Stand still and let me charge you down!\n...Just mind my spike mines. Ramming them\nhurts even me.");
        }
        if (o->timer == 2) music_play(MUS_BOSS);
        if (!dialog_active() && o->timer > 30) { o->state = 1; o->timer = 0; sfx(SFX_ROAR); cam.shake = 0.5f; }
        break;
    case 1:   /* stalk the player, keeping a little distance and walking around his own mines */
    {
        float sp = d > 7 ? 2.0f + o->hp * 0.6f : 0.0f;
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 1.3f * DT);
        o->vel.x = sinf(o->yaw) * sp;
        o->vel.z = cosf(o->yaw) * sp;
        for (i = 0; i < nobj; i++) {
            Obj *b = &objs[i];
            vec3 away;
            float bd;
            if (!b->active || b->type != OT_SPIKEBOMB || b->state != 0) continue;
            bd = v3distxz(b->pos, o->pos);
            if (bd > 5.0f) continue;
            away = v3norm(v3(o->pos.x - b->pos.x, 0, o->pos.z - b->pos.z));
            o->vel.x += away.x * 3.0f * (5.0f - bd);
            o->vel.z += away.z * 3.0f * (5.0f - bd);
        }
        if (o->timer > 100 - o->hp * 15) {
            float r = frand();
            o->timer = 0;
            if (d >= 6) o->state = r < 0.7f ? 3 : (r < 0.88f ? 2 : 4);
            else o->state = r < 0.7f ? 4 : 2;
            if (o->state == 4) { o->vel.y = 18; o->vel.x = o->vel.z = 0; sfx(SFX_ROAR); }
        }
        break;
    }
    case 2:   /* fire breath */
        o->vel.x = o->vel.z = 0;
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 1.0f * DT);
        if (o->timer > 30 && o->timer < 80 && o->timer % 7 == 0) {
            vec3 mouth = v3add(v3mad(o->pos, yaw_dir(o->yaw), 3.0f), v3(0, 2.6f, 0));
            Obj *f = obj_spawn(OT_FIREBALL, mouth, 0, 0);
            if (f) {
                float a = o->yaw + frands() * 0.25f;
                f->vel = v3(sinf(a) * 11, -2.0f, cosf(a) * 11);
                f->timer = 0;
            }
            if (o->timer % 21 == 0) sfx(SFX_FIRE);
        }
        if (o->timer > 100) { o->state = 1; o->timer = 0; }
        break;
    case 3:   /* charge: aims during the wind-up, then runs straight */
        if (o->timer < 40) {
            o->vel.x = o->vel.z = 0;
            o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 3 * DT);
            if (o->timer == 1) { sfx(SFX_ROAR); hud_message("Segfault is about to charge!", 60); }
        } else {
            o->vel.x = sinf(o->yaw) * 12; o->vel.z = cosf(o->yaw) * 12;
            if (o->timer % 10 == 0) sfx_ex(SFX_THUD, 0.5f, 1.4f);
            for (i = 0; i < nobj; i++) {
                Obj *b = &objs[i];
                if (!b->active || b->type != OT_SPIKEBOMB || b->state != 0) continue;
                if (v3distxz(b->pos, o->pos) < o->radius + b->radius + 0.2f) {
                    explode_at(b->pos, 0);
                    obj_kill(b);
                    {   /* a fresh mine grows back later */
                        Obj *r = obj_spawn(OT_SPIKEBOMB, v3add(b->home, v3(0, -30, 0)), 0, 0);
                        if (r) { r->home = b->home; r->state = 1; r->timer = 0; }
                    }
                    o->hp++;
                    sfx(SFX_ROAR);
                    cam.shake = 0.7f;
                    if (o->hp >= 3) {
                        o->state = 7; o->timer = 0;
                        o->pos = v3lerp(o->pos, center, 0.3f);
                        o->pos.y = center.y;
                        music_stop();
                        return;
                    }
                    o->state = 6; o->timer = 0;
                    o->vel = v3scale(v3norm(v3sub(center, o->pos)), 6);
                    o->vel.y = 10;
                    hud_message(o->hp == 1 ? "Segfault: OUCH! My own mine..." : "Segfault: Grrr! Not again...", 120);
                    break;
                }
            }
            if (o->state != 3) break;
        }
        if (o->timer > 100 || v3distxz(o->pos, center) > 17) { o->state = 1; o->timer = 0; o->vel.x = o->vel.z = 0; }
        break;
    case 4:   /* jump + shockwave */
        break;
    case 6:   /* hit recovery */
        if (o->timer > 90) { o->state = 1; o->timer = 0; }
        break;
    case 7:   /* defeated */
        o->vel = v3(0, 0, 0);
        if (o->timer % 7 == 0) {
            fx_spawn(OT_FX_EXPLODE, v3add(o->pos, v3(frands() * 3, 1 + frand() * 3, frands() * 3)), 0.8f);
            sfx_ex(SFX_EXPLODE, 0.5f, 0.8f + frand() * 0.4f);
        }
        o->scale = 1 - clampf((o->timer - 120) / 60.0f, 0, 1);
        if (o->timer == 1) {
            dialog_open("SEGFAULT: Impossible... Core dumped...\n"
                        "This isn't over, penguin! I'll be back...\nin the next kernel release!");
        }
        if (o->timer == 180) {
            Obj *gs = obj_spawn(OT_GRANDSTAR, v3add(center, v3(0, 0.6f, 0)), 0, 0);
            (void)gs;
            sfx(SFX_STAR_APPEAR);
            music_play(MUS_STARGET);
            obj_kill(o);
            return;
        }
        return;
    case 8:   /* jumping back to the arena */
    {
        float t = clampf(o->timer / 70.0f, 0, 1);
        vec3 from = v3(o->f[1], o->f[2], o->f[3]);
        vec3 to = v3add(center, v3(0, 0, 0));
        o->pos = v3lerp(from, to, t);
        o->pos.y = lerpf(from.y, to.y, t) + sinf(t * PI_F) * 12;
        if (t >= 1) { o->state = 1; o->timer = 0; o->vel = v3(0, 0, 0); sfx(SFX_THUD); cam.shake = 0.4f; }
        return;
    }
    }
    /* generic physics for walking states */
    {
        int g = obj_physics(o, 30, 2.0f);
        if (o->state == 4 && g && o->timer > 5) {
            Obj *sw = obj_spawn(OT_FX_SHOCKWAVE, o->pos, 0, 0);
            if (sw) { sw->flags = 0; sw->f[0] = 2.0f; }
            sfx(SFX_POUND);
            cam.shake = 0.6f;
            o->state = 1; o->timer = 0;
        }
        if (o->state == 6 && g) { o->vel.x = o->vel.z = 0; }
        if (v3distxz(o->pos, center) > 17.5f) {
            vec3 t = v3norm(v3sub(center, o->pos));
            o->pos.x += t.x * 0.15f; o->pos.z += t.z * 0.15f;
        }
        if (o->pos.y < center.y - 6) {
            o->state = 8; o->timer = 0;
            o->f[1] = o->pos.x; o->f[2] = o->pos.y; o->f[3] = o->pos.z;
        }
    }
    if (touching(o) && o->state != 6 && o->state != 0) {
        vec3 fd = yaw_dir(o->yaw), tp = v3norm(v3sub(pl.pos, o->pos));
        if (v3dot(fd, tp) > -0.4f || o->state == 3) player_hurt(o->state == 3 ? 2 : 1, o->pos);
        else push_player_out(o, o->radius);
    }
}

static void upd_fireball(Obj *o)
{
    o->timer++;
    o->pos = v3mad(o->pos, o->vel, DT);
    {
        Surf *s;
        float fy = col_floor(o->pos.x, o->pos.y + 0.5f, o->pos.z, &s);
        if (s && o->pos.y < fy + 0.3f) { o->pos.y = fy + 0.3f; o->vel.y = 0; }
    }
    if (o->timer > 150) { obj_kill(o); return; }
    if (touching(o)) {
        if (pl.invinc == 0 && pl.act != ACT_DEATH) {
            player_hurt(1, o->pos);
            if (pl.act == ACT_KNOCKBACK || pl.act == ACT_AIR_KNOCK) { pl.vel.y = 18; player_set_act(ACT_BURN_JUMP); pl.invinc = 90; }
        }
    }
}

static void upd_spikebomb(Obj *o)
{
    o->yaw += 0.3f * DT;
    if (touching(o)) {
        explode_at(o->pos, 0);
        player_hurt(2, o->pos);
        obj_kill(o);
        {
            /* respawn a mine after a while so the boss is always beatable */
            Obj *r = obj_spawn(OT_SPIKEBOMB, v3add(o->home, v3(0, -30, 0)), 0, 0);
            if (r) { r->home = o->home; r->state = 1; r->timer = 0; }
        }
    }
    if (o->state == 1) {
        if (++o->timer > 420) { o->pos = o->home; o->state = 0; fx_spawn(OT_FX_SMOKE, o->pos, 1.0f); }
    }
}

static void upd_flamebar(Obj *o)
{
    int i, n = o->param;
    o->yaw = wrap_angle(o->yaw + o->f[0] * DT);
    for (i = 1; i <= n; i++) {
        vec3 p = v3add(o->pos, v3scale(yaw_dir(o->yaw), i * 0.9f));
        if (v3distxz(p, pl.pos) < 0.45f + PR && pl.pos.y < p.y + 0.5f && pl.pos.y + PH > p.y - 0.5f) {
            if (pl.invinc == 0 && pl.act != ACT_DEATH) {
                player_hurt(1, o->pos);
                if (pl.act == ACT_KNOCKBACK || pl.act == ACT_AIR_KNOCK) { pl.vel.y = 18; player_set_act(ACT_BURN_JUMP); pl.invinc = 90; }
            }
        }
    }
}

/* ---------------------------------------------------------------- platforms */
static void upd_platform(Obj *o)
{
    float period = o->f[3] > 0 ? o->f[3] : 6.0f;
    float k = 0.5f - 0.5f * cosf((o->t + o->f[4]) * TAU_F / period);
    o->t += DT;
    o->pos = v3add(o->home, v3(o->f[0] * k, o->f[1] * k, o->f[2] * k));
}

static void upd_rotplat(Obj *o)
{
    o->yaw = wrap_angle(o->yaw + o->f[3] * DT);
}

static void upd_elevator(Obj *o)
{
    int on = pl.platform == obj_index(o) && !player_in_air();
    float top = o->home.y + o->f[0];
    if (on) { o->timer = 0; o->pos.y = approachf(o->pos.y, top, 3.5f * DT); }
    else {
        o->timer++;
        if (o->timer > 90) o->pos.y = approachf(o->pos.y, o->home.y, 3.0f * DT);
    }
}

static void upd_fallplat(Obj *o)
{
    int on = pl.platform == obj_index(o) && !player_in_air();
    switch (o->state) {
    case 0: if (on) { o->state = 1; o->timer = 0; } break;
    case 1:
        o->timer++;
        o->pos.x = o->home.x + sinf(o->timer * 1.5f) * 0.05f;
        if (o->timer > 40) { o->state = 2; o->timer = 0; o->vel.y = 0; }
        break;
    case 2:
        o->timer++;
        o->vel.y -= 20 * DT;
        o->pos.y += o->vel.y * DT;
        if (o->timer > 180) { o->state = 3; o->timer = 0; o->pos = v3(o->home.x, -9999, o->home.z); }
        break;
    case 3:
        if (++o->timer > 120) { o->state = 0; o->pos = o->home; fx_spawn(OT_FX_SPARKLE, o->pos, 1.0f); }
        break;
    }
}

/* ---------------------------------------------------------------- NPCs */
static void upd_baby(Obj *o)
{
    if (o->flags & OF_HELD) {
        vec3 dd = yaw_dir(pl.yaw);
        o->pos = v3add(v3mad(pl.pos, dd, 0.55f), v3(0, 0.5f, 0));
        o->yaw = pl.yaw;
        if (g_frame % 120 == 0) sfx_ex(SFX_TEXT, 0.6f, 1.6f);
        return;
    }
    if (o->state == 50) { o->yaw += 2 * DT; return; }   /* delivered */
    if (o->state == 100 || o->state == 101) {
        if (obj_physics(o, 30, 0.4f)) { o->state = 0; o->vel = v3(0, 0, 0); }
    } else {
        o->timer++;
        if (o->timer % 160 == 0) o->f[0] = o->yaw + frands() * 3;
        o->yaw = approach_angle(o->yaw, o->f[0], 2 * DT);
        o->vel.x = sinf(o->yaw) * 0.6f;
        o->vel.z = cosf(o->yaw) * 0.6f;
        if (v3distxz(o->pos, o->home) > 3 && v3distxz(o->home, o->pos) < 40) o->f[0] = yaw_to(o->pos, o->home);
        obj_physics(o, 30, 0.4f);
    }
    if (o->pos.y < lv.death_y || o->pos.y < -50) { o->pos = o->home; o->vel = v3(0, 0, 0); }
    if (touching(o)) push_player_out(o, o->radius);
}

static void upd_mother(Obj *o)
{
    int i;
    o->t += DT;
    if (o->state == 1) return;
    for (i = 0; i < nobj; i++) {
        Obj *b = &objs[i];
        if (!b->active || b->type != OT_BABY || (b->flags & OF_HELD) || b->state == 50) continue;
        if (v3dist(b->pos, o->pos) < 4.5f) {
            b->state = 50;
            b->pos = v3add(o->pos, v3(1.2f, 0, 0.8f));
            o->state = 1;
            dialog_open("INIT: Welcome home, little process! I'll adopt\nyou, just like every orphan. Thank you, Tux!\nPlease take this Kernel Star!");
            star_spawn(v3add(o->pos, v3(0, 2.6f, 0)), o->param, 1, o->pos);
            break;
        }
    }
    if (touching(o)) push_player_out(o, o->radius);
}

static void upd_racer(Obj *o)
{
    o->timer++;
    if (o->state == 0 || o->state >= 3) {
        if (touching(o)) push_player_out(o, o->radius);
        if (o->state == 0) o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 2 * DT);
        return;
    }
    if (o->state == 1) {       /* countdown */
        if (o->timer == 1) hud_message("3", 50);
        if (o->timer == 60) hud_message("2", 50);
        if (o->timer == 120) hud_message("1", 50);
        if (o->timer == 180) { hud_message("GO!", 60); o->state = 2; o->timer = 0; o->i[0] = 1; g_race_state = 1; sfx(SFX_MENU_OK); music_play(MUS_RACE); }
        if (o->timer % 60 == 1) sfx(SFX_MENU_MOVE);
        return;
    }
    if (o->state == 2 && o->i[0] < g_race_n) {
        vec3 tgt = g_race_path[o->i[0]];
        vec3 d = v3sub(tgt, o->pos);
        float sp = o->f[0] > 0 ? o->f[0] : 7.4f;
        float dl = v3len(v3(d.x, 0, d.z));
        if (dl < 0.6f) { o->i[0]++; return; }
        o->yaw = approach_angle(o->yaw, atan2f(d.x, d.z), 6 * DT);
        o->pos.x += d.x / dl * sp * DT;
        o->pos.z += d.z / dl * sp * DT;
        {
            Surf *s;
            float fy = col_floor(o->pos.x, o->pos.y + 1.5f, o->pos.z, &s);
            if (s && fy >= o->pos.y - 0.8f) { o->pos.y = fy; o->vel.y = 0; }
            else {
                o->vel.y -= 30 * DT;
                o->pos.y += o->vel.y * DT;
                if (s && o->pos.y < fy) { o->pos.y = fy; o->vel.y = 0; }
            }
        }
        o->t += DT * sp;
        if (o->i[0] >= g_race_n) {
            o->state = 3;
            if (g_race_state == 1) {
                g_race_state = 3;
                dialog_open("QUICK GNU: Ha! Free as in speed! You lose!\nTalk to me at the bottom to try again.");
                music_play(lv.music);
            }
        }
    }
}

static void upd_raceflag(Obj *o)
{
    if (touching(o) && g_race_state == 1) {
        int i;
        g_race_state = 2;
        music_play(lv.music);
        for (i = 0; i < nobj; i++) {
            if (objs[i].active && objs[i].type == OT_RACER) objs[i].state = 4;
        }
        dialog_open("QUICK GNU: Whoa, you beat me! You're quick\nfor a flightless bird. Here, you earned this!");
        star_spawn(v3add(o->pos, v3(2.5f, 0.8f, 0)), o->param, 1, pl.pos);
    }
}

/* ---------------------------------------------------------------- level objects */
static void upd_trampoline(Obj *o)
{
    /* f[0]: squash amount (set by the player on each bounce), springs back */
    o->f[0] = approachf(o->f[0], 0, 4.0f * DT);
}

static void upd_pole(Obj *o)
{
    float dx = pl.pos.x - o->pos.x, dz = pl.pos.z - o->pos.z;
    float d = sqrtf(dx * dx + dz * dz);
    if (pl.act == ACT_POLE) return;
    if (d < o->radius + PR && pl.pos.y < o->pos.y + o->height && pl.pos.y + PH > o->pos.y) {
        if (player_in_air() && pl.act != ACT_GPOUND && pl.act != ACT_AIR_KNOCK && pl.act != ACT_BURN_JUMP &&
            pl.act != ACT_HOLD_JUMP && pl.act != ACT_HOLD_FALL && pl.held < 0) {
            pl.pole = obj_index(o);
            pl.pole_h = clampf(pl.pos.y - o->pos.y, 0, o->height - 0.8f);
            pl.act_f = atan2f(-dx, -dz);
            player_set_act(ACT_POLE);
            pl.act_f = atan2f(-dx, -dz);
            sfx(SFX_GRAB);
        } else if (d > 1e-4f) {
            pl.pos.x = o->pos.x + dx / d * (o->radius + PR);
            pl.pos.z = o->pos.z + dz / d * (o->radius + PR);
        }
    }
}

static void upd_warp(Obj *o)
{
    if (pl.act == ACT_DEATH || pl.act == ACT_FROZEN || pl.act == ACT_STAR_DANCE || g_fade > 0) return;
    if (touching(o)) {
        int need = o->i[1];
        if (total_stars() < need) {
            static char msg[256];
            if (o->timer <= 0) {
                snprintf(msg, sizeof msg, tr("This portal needs %d Kernel Stars.\nYou have %d."), need, total_stars());
                dialog_open(msg);
                sfx(SFX_BUZZ);
            }
            o->timer = 60;
            player_push(o->pos, 6);
            return;
        }
        sfx(SFX_WARP);
        if (o->param >= LV_PEAK && cur_level < LV_PEAK) g_entry_painting = o->i[2];
        warp_to(o->param, o->param2, o->i[0]);
    }
    if (o->timer > 0) o->timer--;
}

static void upd_door(Obj *o)
{
    /* f[0] = open angle */
    if (o->state == 1) {
        o->f[0] = approachf(o->f[0], 1.6f, 4 * DT);
        o->timer++;
        if (o->timer > 40) { o->state = 2; }
        return;
    }
    if (o->state == 2) {
        if (o->timer++ > 160 && !touching(o)) { o->f[0] = approachf(o->f[0], 0, 3 * DT); if (o->f[0] <= 0) { o->state = 0; o->timer = 0; } }
        return;
    }
    {
        /* touching the door's front/back */
        vec3 rel = v3sub(pl.pos, o->pos);
        vec3 f = yaw_dir(o->yaw), r = v3(f.z, 0, -f.x);
        float fz = v3dot(rel, f), rx = v3dot(rel, r);
        if (fabsf(rx) < 1.2f * o->f[1] + 0.1f && fabsf(fz) < 0.75f && pl.pos.y < o->pos.y + 2.5f && pl.pos.y > o->pos.y - 0.5f && pl.act != ACT_DEATH) {
            if (o->type == OT_STARDOOR && total_stars() < o->param) {
                static char msg[256];
                if (o->timer <= 0) {
                    snprintf(msg, sizeof msg, tr("This star door needs %d Kernel Star%s.\nYou have %d."), o->param, (o->param == 1 || g_lang != LANG_EN) ? "" : "s", total_stars());
                    dialog_open(msg);
                    sfx(SFX_BUZZ);
                }
                o->timer = 90;
                return;
            }
            if (o->param2 >= 0 && o->type == OT_DOOR) {
                sfx(SFX_DOOR);
                warp_to(o->param2, o->i[0], 0);
                o->state = 1;
                return;
            }
            o->state = 1; o->timer = 0;
            sfx(SFX_DOOR);
        }
        if (o->timer > 0) o->timer--;
    }
}

static void upd_clam(Obj *o)
{
    /* f[0] = openness 0..1 */
    o->t += DT;
    {
        float c = fmodf(o->t, 6.0f);
        float target = (c < 3.5f) ? 1.0f : 0.0f;
        float prev = o->f[0];
        o->f[0] = approachf(o->f[0], target, (target > 0 ? 1.0f : 2.5f) * DT);
        if (prev > 0.5f && o->f[0] <= 0.5f && v3dist(o->pos, pl.pos) < 12) sfx_ex(SFX_THUD, 0.5f, 1.6f);
        if (target == 0 && o->f[0] > 0.1f && touching(o) && pl.pos.y < o->pos.y + 1.2f) player_hurt(1, o->pos);
    }
}

static void upd_ring(Obj *o)
{
    o->yaw += 0.5f * DT;
    if (o->state == 1) { o->scale += 3 * DT; if (o->scale > 2) obj_kill(o); return; }
    if (v3dist(v3add(pl.pos, v3(0, 0.8f, 0)), o->pos) < 1.6f) {
        if (o->param == g_ring_next) {
            g_ring_next++;
            o->state = 1;
            sfx_ex(SFX_REDCOIN, 1.0f, 1.0f + g_ring_next * 0.12f);
            if (g_ring_next >= ring_total) {
                star_spawn(o->pos, o->param2, 1, pl.pos);
                hud_message("All rings cleared!", 120);
            }
        }
    }
}

static void upd_crate(Obj *o)
{
    float dx = fabsf(pl.pos.x - o->pos.x), dz = fabsf(pl.pos.z - o->pos.z);
    int hit = 0;
    if (dx < 1.6f && dz < 1.6f && pl.pos.y < o->pos.y + 1.6f && pl.pos.y > o->pos.y - 1.7f) {
        if (pl.act == ACT_PUNCH && pl.act_t > 2 && pl.act_t < 10) {
            vec3 d = v3norm(v3sub(v3(o->pos.x, 0, o->pos.z), v3(pl.pos.x, 0, pl.pos.z)));
            if (v3dot(d, yaw_dir(pl.yaw)) > 0.3f) hit = 1;
        }
        if (pl.act == ACT_DIVE || pl.act == ACT_JUMPKICK || (pl.act == ACT_BELLY_SLIDE && pl.fwd > 4)) hit = 1;
        if ((pl.act == ACT_GPOUND_LAND && pl.act_t < 3) && pl.platform == obj_index(o)) hit = 1;
    }
    if (pl.act == ACT_GPOUND_LAND && pl.act_t < 3 && dx < 0.9f && dz < 0.9f && fabsf(pl.pos.y - (o->pos.y + 0.7f)) < 0.3f) hit = 1;
    if (hit) {
        sfx(SFX_BREAK);
        fx_spawn(OT_FX_SMOKE, o->pos, 1.2f);
        if (o->param > 0) spawn_coins_burst(o->pos, o->param);
        else if (o->param == -1) { Obj *u = obj_spawn(OT_ONEUP, v3add(o->pos, v3(0, 0.5f, 0)), 0, 0); (void)u; }
        obj_kill(o);
    }
}

static void upd_rocketbox(Obj *o)
{
    float dx = fabsf(pl.pos.x - o->pos.x), dz = fabsf(pl.pos.z - o->pos.z);
    int hit = 0;
    if (o->state == 1) {
        if (++o->timer > 60 * 25) { o->state = 0; fx_spawn(OT_FX_SPARKLE, o->pos, 1.0f); }
        return;
    }
    /* break the crate open: punch, kick, dive, or land / ground pound on it */
    if (dx < 1.5f && dz < 1.5f && pl.pos.y < o->pos.y + 1.2f && pl.pos.y > o->pos.y - 2.5f) {
        if (pl.act == ACT_PUNCH && pl.act_t > 2 && pl.act_t < 10) hit = 1;
        if (pl.act == ACT_DIVE || pl.act == ACT_JUMPKICK) hit = 1;
    }
    if (!player_in_air() && pl.platform == obj_index(o)) hit = 1;
    if (hit) {
        o->state = 1; o->timer = 0;
        sfx(SFX_BREAK);
        sfx(SFX_1UP);
        fx_spawn(OT_FX_SMOKE, o->pos, 1.2f);
        pl.rocket_t = 60 * 40;
        if (!player_in_air()) player_bounce(10);
        hud_message("ROCKET PACK! In mid-air: press A to ignite, hold A to fly!", 200);
        if (g_race_state != 1) music_play(MUS_RACE);
    }
}

/* ---- ghosts ("zombie processes"): out of phase with the world, so attacks pass right through them.
 * A GROUND POUND next to one shakes it into the real world for a few seconds: then it can be hit.
 * f[0]: opacity, f[2]: frames left materialised ---- */
static int ghost_shaken(Obj *o, float radius)
{
    return pl.act == ACT_GPOUND_LAND && pl.act_t == 1 && v3distxz(o->pos, pl.pos) < radius &&
           fabsf(o->pos.y - pl.pos.y) < 3.5f;
}

static void ghost_materialise(Obj *o, int frames)
{
    o->f[2] = (float)frames;
    sfx_ex(SFX_DROWN, 0.6f, 0.7f);
    fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(0, 1.0f, 0)), 1.2f);
}

static int count_type(int type, int param)
{
    int i, n = 0;
    for (i = 0; i < nobj; i++)
        if (objs[i].active && objs[i].type == type && objs[i].param == param && objs[i].state != 90) n++;
    return n;
}

static void ghost_float(Obj *o, vec3 target, float speed)
{
    vec3 d = v3sub(target, o->pos);
    float l = v3len(d);
    if (l > 0.05f) o->pos = v3mad(o->pos, d, minf(speed * DT, l) / l);
    {
        Surf *s;
        float fy = col_floor(o->pos.x, o->pos.y + 1.0f, o->pos.z, &s);
        if (s && o->pos.y < fy + 0.2f) o->pos.y = fy + 0.2f;
    }
}

static void upd_ghost(Obj *o)
{
    float d = dist_player(o);
    int solid;
    if (o->state == 90) {
        o->timer++;
        o->scale = 1 - o->timer / 30.0f;
        o->pos.y += 2 * DT;
        if (o->timer >= 30) {
            obj_kill(o);
            if (o->param == 1 && count_type(OT_GHOST, 1) == 0) {
                int i;
                for (i = 0; i < nobj; i++)
                    if (objs[i].active && objs[i].type == OT_STARMARKER && objs[i].param == o->param2) {
                        star_spawn(objs[i].pos, objs[i].param, 1, o->pos);
                        hud_message("All the yard zombies are busted!", 150);
                    }
            }
        }
        return;
    }
    o->timer++;
    if (ghost_shaken(o, 6.0f)) ghost_materialise(o, 240);
    solid = o->f[2] > 0;
    if (solid) o->f[2] -= 1;
    o->f[1] = (float)solid;
    o->f[0] = approachf(o->f[0], solid ? 0.95f : 0.3f, 3 * DT);
    if (!solid) {
        if (d < 16) {
            o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 3 * DT);
            ghost_float(o, v3add(pl.pos, v3(0, 0.3f + sinf(o->timer * 0.05f) * 0.3f, 0)), 2.2f);
        } else {
            vec3 t = v3add(o->home, v3(sinf(o->timer * 0.01f + o->home.x) * 4, sinf(o->timer * 0.03f) * 0.5f, cosf(o->timer * 0.013f) * 4));
            o->yaw = approach_angle(o->yaw, yaw_to(o->pos, t), 2 * DT);
            ghost_float(o, t, 1.2f);
        }
        if (o->timer % 140 == 0 && d < 14) sfx_ex(SFX_DROWN, 0.35f, 1.6f);
        if (touching(o)) player_hurt(1, o->pos);
    } else {
        /* dazed in the real world: sinks to the floor and wobbles */
        Surf *fs;
        float fy = col_floor(o->pos.x, o->pos.y + 1.0f, o->pos.z, &fs);
        o->yaw += 4 * DT;
        if (fs && o->pos.y > fy + 0.2f) o->pos.y = maxf(fy + 0.2f, o->pos.y - 3 * DT);
        if (obj_hit_by_attack(o) || (touching(o) && stomped_by_player(o))) {
            if (stomped_by_player(o)) player_bounce(10);
            o->state = 90; o->timer = 0;
            sfx(SFX_POP);
            sfx_ex(SFX_DROWN, 0.6f, 2.0f);
            spawn_coins_burst(o->pos, 1);
            return;
        }
    }
}

static void upd_ghostking(Obj *o)
{
    float d = dist_player(o);
    int solid;
    o->timer++;
    switch (o->state) {
    case 0:
        if (d < 13 && fabsf(pl.pos.y - o->pos.y) < 4) {
            o->state = 2; o->timer = 0;
            dialog_open("ZOMBIE KING: Hee hee hee... Welcome to my manor!\n"
                        "Nobody can kill a zombie process! Your\nattacks go right through me... unless you\n"
                        "shake me into the real world. But how?");
            music_play(MUS_BOSS);
        }
        return;
    case 1:   /* recoiling after a hit */
    {
        vec3 away = v3norm(v3sub(o->pos, pl.pos));
        away.y = 0.2f;
        ghost_float(o, v3mad(o->pos, away, 4), 6.0f);
        if (v3distxz(o->pos, o->home) > 10) ghost_float(o, o->home, 6.0f);
        o->f[0] = approachf(o->f[0], 0.3f, 2 * DT);
        if (o->timer > 70) { o->state = 2; o->timer = 0; o->f[2] = 0; }
        return;
    }
    case 90:
        o->scale = 2.5f * (1 - clampf(o->timer / 60.0f, 0, 1));
        if (o->timer % 6 == 0) fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(frands() * 2, 1 + frand() * 3, frands() * 2)), 0.8f);
        if (o->timer == 60) {
            star_spawn(v3add(o->home, v3(0, 0.6f, 0)), o->param, 1, o->pos);
            music_play(lv.music);
            obj_kill(o);
        }
        return;
    }
    if (ghost_shaken(o, 7.0f)) ghost_materialise(o, 180 - o->hp * 30);
    solid = o->f[2] > 0;
    if (solid) o->f[2] -= 1;
    o->f[1] = (float)solid;
    o->f[0] = approachf(o->f[0], solid ? 0.95f : 0.3f, 3 * DT);
    if (!solid) {
        o->yaw = approach_angle(o->yaw, yaw_to(o->pos, pl.pos), 2 * DT);
        ghost_float(o, v3add(pl.pos, v3(0, -0.5f, 0)), 2.6f + o->hp * 0.5f);
        if (o->timer % 100 == 0) sfx_ex(SFX_ROAR, 0.4f, 1.5f);
        if (touching(o)) player_hurt(2, o->pos);
    } else {
        Surf *fs;
        float fy = col_floor(o->pos.x, o->pos.y + 1.0f, o->pos.z, &fs);
        o->yaw += 3 * DT;
        if (fs && o->pos.y > fy + 0.2f) o->pos.y = maxf(fy + 0.2f, o->pos.y - 3 * DT);
        if (obj_hit_by_attack(o) || (touching(o) && stomped_by_player(o))) {
            if (stomped_by_player(o)) player_bounce(12);
            o->hp++;
            o->f[2] = 0;
            sfx(SFX_PUNCH);
            sfx(SFX_ROAR);
            if (o->hp >= 3) {
                o->state = 90; o->timer = 0;
                dialog_open("ZOMBIE KING: Nooo! I've been... reaped!\nTake your star and get out of my manor!");
            } else { o->state = 1; o->timer = 0; hud_message(o->hp == 1 ? "Ow! 2 more hits!" : "Ouch! One more!", 120); }
            return;
        }
    }
    if (v3distxz(o->pos, o->home) > 12) ghost_float(o, o->home, 3.0f);
}

static void upd_topswitch(Obj *o)
{
    int i, all = 1, any_done = 0;
    if (o->state == 0 && !player_in_air() && v3distxz(o->pos, pl.pos) < 1.5f && fabsf(pl.pos.y - o->pos.y) < 0.5f) {
        o->state = 1;
        sfx(SFX_REDCOIN);
        fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(0, 0.5f, 0)), 1.0f);
    }
    if (o->state != 1) return;
    for (i = 0; i < nobj; i++) {
        Obj *s = &objs[i];
        if (!s->active || s->type != OT_TOPSWITCH) continue;
        if (s->state == 0) all = 0;
        if (s->i[1]) any_done = 1;
    }
    if (all && !any_done) {
        for (i = 0; i < nobj; i++) if (objs[i].active && objs[i].type == OT_TOPSWITCH) objs[i].i[1] = 1;
        for (i = 0; i < nobj; i++)
            if (objs[i].active && objs[i].type == OT_STARMARKER && objs[i].param == o->param) {
                star_spawn(objs[i].pos, objs[i].param, 1, o->pos);
                hud_message("All four obelisks are shining!", 150);
            }
    }
}

static void upd_heart(Obj *o)
{
    float d = dist_player(o);
    o->yaw += (d < 4 ? 8.0f : 2.0f) * DT;
    o->pos.y = o->home.y + sinf(g_frame * 0.05f) * 0.15f;
    if (touching(o) && g_frame % 8 == 0 && pl.health < 8) { player_heal(1); sfx_ex(SFX_COIN, 0.4f, 0.8f); }
}

/* ---- watchdog: a robot guard dog. It lunges at intruders; right after a lunge it is out of
 * breath and can be kicked. "Kicking the watchdog" 3 times resets it and unlocks the gate. ---- */
static Obj *find_type(int type)
{
    int i;
    for (i = 0; i < nobj; i++) if (objs[i].active && objs[i].type == type) return &objs[i];
    return NULL;
}

static void upd_watchdog(Obj *o)
{
    vec3 base = o->home;
    float dp = v3distxz(pl.pos, base);
    o->timer++;
    obj_physics(o, 30, 1.0f);
    switch (o->state) {
    case 0: {   /* patrol around its spot in front of the gate */
        float a = o->timer * 0.02f;
        vec3 t = v3add(base, v3(sinf(a) * 3.0f, 0, cosf(a) * 3.0f));
        vec3 d = v3sub(t, o->pos);
        d.y = 0;
        o->vel.x = o->vel.z = 0;
        if (v3len(d) > 0.1f) {
            vec3 n = v3norm(d);
            o->vel.x = n.x * 2.5f;
            o->vel.z = n.z * 2.5f;
        }
        o->yaw = approach_angle(o->yaw, dp < 12 ? yaw_to(o->pos, pl.pos) : atan2f(d.x, d.z), 3 * DT);
        if (dp < 11 && o->timer > 70 && fabsf(pl.pos.y - base.y) < 4) {
            o->state = 1; o->timer = 0;
            o->f[0] = pl.pos.x; o->f[2] = pl.pos.z;
            o->vel = v3(0, 0, 0);
            sfx_ex(SFX_ROAR, 0.6f, 1.9f);
        }
        break;
    }
    case 1: {   /* crouch, then lunge at where Tux was */
        vec3 d = v3sub(v3(o->f[0], 0, o->f[2]), o->pos);
        d.y = 0;
        o->yaw = atan2f(d.x, d.z);
        if (o->timer == 14) { o->vel.y = 5; sfx_ex(SFX_JUMP, 0.6f, 0.7f); }
        if (o->timer > 14) {
            vec3 n = v3norm(d);
            o->vel.x = n.x * 17;
            o->vel.z = n.z * 17;
        } else o->vel.x = o->vel.z = 0;
        if ((o->timer > 14 && v3len(d) < 0.8f) || o->timer > 50 || v3distxz(o->pos, base) > 8.0f) {
            o->state = 2; o->timer = 0;
            o->vel.x = o->vel.z = 0;
            sfx_ex(SFX_THUD, 0.6f, 1.5f);
        }
        break;
    }
    case 2:     /* out of breath: kick it now! */
        o->vel.x = o->vel.z = 0;
        if (o->timer % 20 == 0) fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(0, 2.4f, 0)), 0.4f);
        if (obj_hit_by_attack(o) || (touching(o) && stomped_by_player(o))) {
            if (stomped_by_player(o)) player_bounce(12);
            o->hp++;
            sfx(SFX_KICK);
            sfx_ex(SFX_SQUAWK, 0.7f, 1.6f);
            o->vel = v3scale(v3norm(v3sub(base, o->pos)), 8);
            o->vel.y = 6;
            if (o->hp >= 3) {
                Obj *cage = find_type(OT_CAGE);
                o->state = 4; o->timer = 0;
                if (cage) {
                    fx_spawn(OT_FX_SMOKE, cage->pos, 2.0f);
                    fx_spawn(OT_FX_SMOKE, v3add(cage->pos, v3(0, 1.5f, 0)), 2.0f);
                    obj_kill(cage);
                }
                sfx(SFX_STAR_APPEAR);
                hud_message("Watchdog kicked! It resets and unlocks the gate.", 180);
            } else {
                o->state = 0; o->timer = 0;
                hud_message(o->hp == 1 ? "You kicked the watchdog! (1/3)" : "You kicked the watchdog! (2/3)", 120);
            }
            return;
        }
        if (o->timer > 100) { o->state = 0; o->timer = 0; }
        break;
    case 4:     /* reset: lies down and sleeps by the open gate */
        o->vel.x = o->vel.z = 0;
        if (o->timer % 50 == 0) fx_spawn(OT_FX_BUBBLE, v3add(o->pos, v3(0, 2.2f, 0)), 0.5f);
        return;
    }
    /* stay near its post */
    if (v3distxz(o->pos, base) > 8.0f) {
        vec3 n = v3norm(v3sub(v3(o->pos.x, 0, o->pos.z), v3(base.x, 0, base.z)));
        o->pos.x = base.x + n.x * 8.0f;
        o->pos.z = base.z + n.z * 8.0f;
    }
    if (touching(o) && o->state != 2) player_hurt(o->state == 1 ? 2 : 1, o->pos);
    else if (touching(o)) push_player_out(o, o->radius);
}

/* ---- the OOM Killer: an unstoppable hunter in the labyrinth ----
 * It sees with a 120 degree cone and cannot see through walls (ray test against the level).
 * states: 0 patrol, 1 chase, 2 go to the last place Tux was seen, 3 look around, 4 cool-down after a hit
 * f[0..2]: last seen position, i[0..1]: patrol target cell */
static int oom_sees(Obj *o)
{
    vec3 eye = v3add(o->pos, v3(0, 2.8f, 0)), tgt = v3add(pl.pos, v3(0, 1.0f, 0)), d = v3sub(tgt, eye);
    vec3 n;
    float dist = v3len(d), t;
    if (pl.act == ACT_DEATH || !pl.visible) return 0;
    if (dist > 30) return 0;
    if (dist > 2.5f) {   /* within 2.5 it hears / smells you even from behind */
        vec3 dh = v3norm(v3(d.x, 0, d.z));
        if (v3dot(dh, yaw_dir(o->yaw)) < 0.5f) return 0;
    }
    if (col_ray(eye, tgt, &t, &n, 1)) return 0;   /* a wall is in the way */
    return 1;
}

static void oom_walk_to(Obj *o, vec3 target, float speed)
{
    int cx, cz, tx, tz, nx, nz;
    vec3 goal = target, d;
    float l;
    if (maze_cell(o->pos, &cx, &cz) && maze_cell(target, &tx, &tz) && (cx != tx || cz != tz)) {
        if (maze_next_step(cx, cz, tx, tz, &nx, &nz)) {
            vec3 c = maze_center(cx, cz);
            /* stay near the corridor centre line before turning into the next cell */
            if (nx != cx && fabsf(o->pos.z - c.z) > 0.4f) goal = v3(o->pos.x, 0, c.z);
            else if (nz != cz && fabsf(o->pos.x - c.x) > 0.4f) goal = v3(c.x, 0, o->pos.z);
            else goal = maze_center(nx, nz);
        }
    }
    d = v3sub(goal, o->pos);
    d.y = 0;
    l = v3len(d);
    if (l > 0.05f) {
        o->pos = v3mad(o->pos, d, minf(speed * DT, l) / l);
        o->yaw = approach_angle(o->yaw, atan2f(d.x, d.z), 7 * DT);
    }
    o->t += speed * DT;
}

static void upd_oom(Obj *o)
{
    int sees = oom_sees(o);
    vec3 last = v3(o->f[0], 0, o->f[2]);
    o->timer++;
    if (sees && o->state != 4) {
        if (o->state != 1) {
            sfx(SFX_ROAR);
            hud_message("The OOM Killer spotted you! RUN!", 120);
            if (music_current() != MUS_BOSS) music_play(MUS_BOSS);
        }
        g_maze_spotted = 1;
        o->state = 1;
        o->f[0] = pl.pos.x; o->f[2] = pl.pos.z;
        o->timer = 0;
    }
    switch (o->state) {
    case 0: {   /* patrol between random cells */
        vec3 tgt = maze_center(o->i[0], o->i[1]);
        if (v3distxz(o->pos, tgt) < 0.5f || o->timer > 60 * 25) {
            o->i[0] = (int)(frand() * g_maze.w); o->i[1] = (int)(frand() * g_maze.h);
            o->timer = 0;
        }
        oom_walk_to(o, tgt, 3.2f);
        break;
    }
    case 1:     /* chase */
        if (sees) {
            if (v3distxz(o->pos, pl.pos) < 7.0f) {
                vec3 d = v3sub(pl.pos, o->pos);
                d.y = 0;
                o->yaw = approach_angle(o->yaw, atan2f(d.x, d.z), 8 * DT);
                o->pos = v3mad(o->pos, v3norm(d), 7.0f * DT);
                o->t += 7.0f * DT;
            } else oom_walk_to(o, pl.pos, 7.0f);
        } else { o->state = 2; o->timer = 0; }
        break;
    case 2:     /* go where Tux was last seen */
        oom_walk_to(o, last, 6.0f);
        if (v3distxz(o->pos, last) < 0.8f || o->timer > 60 * 12) { o->state = 3; o->timer = 0; sfx_ex(SFX_ROAR, 0.4f, 1.5f); }
        break;
    case 3:     /* look around */
        o->yaw = wrap_angle(o->yaw + 2.2f * DT);
        if (o->timer > 170) {
            o->state = 0; o->timer = 0;
            o->i[0] = (int)(frand() * g_maze.w); o->i[1] = (int)(frand() * g_maze.h);
            hud_message("The OOM Killer lost sight of you...", 100);
            if (music_current() == MUS_BOSS) music_play(lv.music);
        }
        break;
    case 4:     /* cool-down after catching Tux */
        if (o->timer > 150) { o->state = 3; o->timer = 0; }
        break;
    }
    /* collision with walls (it walks along corridor centres, but be safe) */
    {
        vec3 wn;
        col_walls(&o->pos, 1.0f, 0.9f, &wn, NULL);
        col_walls(&o->pos, 2.5f, 0.9f, &wn, NULL);
    }
    /* it cannot be defeated: attacks just bounce off */
    if (touching(o) && o->state != 4) {
        if (player_is_attacking() || stomped_by_player(o)) sfx(SFX_BUZZ);
        player_hurt(3, o->pos);
        if (pl.act != ACT_DEATH) hud_message("kill -9 !", 90);
        o->state = 4; o->timer = 0;
    } else if (touching(o)) push_player_out(o, o->radius);
}

static void upd_mazeexit(Obj *o)
{
    o->t += DT;
    if (!touching(o) || o->state) return;
    o->state = 1;
    if (!g_maze_spotted) {
        star_spawn(v3add(o->pos, v3(0, 0.6f, 0)), o->param, 1, pl.pos);
        hud_message("You slipped through unseen!", 150);
    } else {
        dialog_open("EXIT: The OOM Killer saw you on the way...\nCome back and reach this exit without being\nseen even once to earn the Kernel Star.");
    }
}

static void upd_timerstart(Obj *o)
{
    if (o->state == 0 && touching(o)) {
        o->state = 1;
        g_course_timer = 0;
        g_timer_on = 1;
        sfx(SFX_MENU_OK);
    }
}

static void upd_timergoal(Obj *o)
{
    if (o->state == 0 && touching(o)) {
        static char msg[256];
        int t = g_course_timer;
        o->state = 1;
        g_timer_on = 0;
        snprintf(msg, sizeof msg, "TIME %d'%02d\"%d", t / 3600, (t / 60) % 60, (t % 60) / 6);
        hud_message(msg, 240);
        star_spawn(v3add(o->pos, v3(-2.5f, 0.6f, 0)), o->param, 1, pl.pos);
        if (t < o->param2 * 60) star_spawn(v3add(o->pos, v3(2.5f, 0.6f, 0)), o->param + 1, 1, pl.pos);
        else hud_message("Finish under 21 seconds for a bonus star!", 240);
    }
}

/* ---------------------------------------------------------------- effects */
static void upd_fx(Obj *o)
{
    o->timer++;
    switch (o->type) {
    case OT_FX_SPARKLE:
        o->pos = v3mad(o->pos, o->vel, DT);
        if (o->timer > 30) obj_kill(o);
        break;
    case OT_FX_SMOKE: case OT_FX_SNOWPUFF:
        o->pos = v3mad(o->pos, o->vel, DT);
        if (o->timer > 40) obj_kill(o);
        break;
    case OT_FX_BUBBLE: {
        float wy = water_level_at(o->pos.x, o->pos.z, o->pos.y);
        o->pos.y += 2.0f * DT;
        o->pos.x += sinf(o->timer * 0.2f) * 0.01f;
        if (o->timer > 90 || (wy > -1e8f && o->pos.y > wy)) obj_kill(o);
        break;
    }
    case OT_FX_EXPLODE:
        if (o->timer == 2 && o->f[0] > 0 && v3dist(o->pos, pl.pos) < 2.8f) player_hurt(2, o->pos);
        if (o->timer > 30) obj_kill(o);
        break;
    case OT_FX_SHOCKWAVE: {
        float r = o->timer * 0.18f;
        o->f[1] = r;
        if (!player_in_air() && fabsf(v3distxz(o->pos, pl.pos) - r) < 0.7f && fabsf(pl.pos.y - o->pos.y) < 1.0f) player_hurt(1, o->pos);
        if (r > 22) obj_kill(o);
        break;
    }
    }
}

/* ---------------------------------------------------------------- public: interactions */
int objs_try_grab(void)
{
    int i, best = -1;
    float bd = 1e9f;
    vec3 f = yaw_dir(pl.yaw);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        vec3 d;
        float dist;
        if (!o->active || !(o->flags & OF_GRABBABLE) || (o->flags & OF_HELD)) continue;
        if (o->type == OT_FORKBOMB && (o->state == 90 || o->state == 100)) continue;
        d = v3sub(o->pos, pl.pos);
        d.y = 0;
        dist = v3len(d);
        if (dist > o->radius + 1.1f || fabsf(o->pos.y - pl.pos.y) > 1.5f) continue;
        if (dist > 0.3f && v3dot(v3scale(d, 1.0f / dist), f) < 0.3f) continue;
        if (o->type == OT_BABY && o->state == 50) continue;
        if (dist < bd) { bd = dist; best = i; }
    }
    if (best >= 0) objs[best].timer = 0;
    return best;
}

int objs_try_talk(void)
{
    int i;
    vec3 f = yaw_dir(pl.yaw);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        vec3 d;
        float dist;
        if (!o->active) continue;
        if (o->type != OT_SIGN && o->type != OT_NPC && o->type != OT_MOTHER && o->type != OT_RACER) continue;
        d = v3sub(o->pos, pl.pos);
        d.y = 0;
        dist = v3len(d);
        if (dist > o->radius + 1.4f || fabsf(o->pos.y - pl.pos.y) > 1.5f) continue;
        if (dist > 0.3f && v3dot(v3scale(d, 1.0f / dist), f) < 0.2f) continue;
        if (o->type == OT_RACER) {
            if (o->state == 0) {
                dialog_open("QUICK GNU: Hey penguin! Let's race to the\ntop of Penguin Peak! The flag at the summit\n"
                            "is the finish line. No rockets! Ready...");
                o->state = 1; o->timer = 0;
                pl.yaw = yaw_to(pl.pos, o->pos);
                player_set_act(ACT_FROZEN);
                return 1;
            }
            if (o->state == 3 && v3distxz(o->pos, o->home) > 5) {
                dialog_open("QUICK GNU: I'm waiting for you at the bottom!");
                return 1;
            }
            if (o->state == 3) { o->state = 1; o->timer = 0; o->pos = o->home; dialog_open("QUICK GNU: A rematch? Sure! Ready..."); player_set_act(ACT_FROZEN); return 1; }
            if (o->state == 4) { dialog_open("QUICK GNU: You're fast! Want to race again\nsome other time?"); player_set_act(ACT_FROZEN); return 1; }
            return 0;
        }
        if (o->type == OT_MOTHER && o->state == 1) dialog_open("INIT: Thank you so much, Tux! All my child\nprocesses are safe and sound.");
        else if (o->type == OT_NPC && o->param == 1 && !has_star(CRS_SECRET, 1)) {
            if (total_stars() >= 8 && o->state == 0) {
                dialog_open("PENGUIN: Wow, Tux! You already found 8 Kernel\nStars! I found this one behind the castle\n"
                            "wardrobe... it's yours! Keep going!");
                star_spawn(v3add(o->pos, v3(0, 2.6f, 0)), 1, 1, o->pos);
                o->state = 1;
            } else if (o->state == 0) {
                dialog_open("PENGUIN: I've got a little secret for you...\nCome back when you have 8 Kernel Stars!");
            } else continue;
        }
        else if (o->text) dialog_open(o->text);
        else continue;
        pl.yaw = yaw_to(pl.pos, o->pos);
        player_set_act(ACT_FROZEN);
        if (o->type == OT_SIGN) sfx(SFX_MENU_OK); else sfx_ex(SFX_SQUAWK, 0.6f, 1.4f);
        return 1;
    }
    return 0;
}

void objs_update(void)
{
    int i;
    if (g_blue_switch_timer > 0) {
        g_blue_switch_timer--;
        if (g_blue_switch_timer > 0 && g_blue_switch_timer < 180 && g_blue_switch_timer % 20 == 0) sfx_ex(SFX_TEXT, 0.8f, 0.7f);
    }
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        o->prev_pos = o->pos;
        o->prev_yaw = o->yaw;
        switch (o->type) {
        case OT_COIN: case OT_REDCOIN: case OT_BLUECOIN: upd_coin(o); break;
        case OT_ONEUP: upd_oneup(o); break;
        case OT_STAR: upd_star(o); break;
        case OT_GRANDSTAR: upd_grandstar(o); break;
        case OT_BLUESWITCH: upd_blueswitch(o); break;
        case OT_BUG: upd_bug(o); break;
        case OT_FORKBOMB: upd_forkbomb(o); break;
        case OT_KING: upd_king(o); break;
        case OT_CRUSHER: upd_crusher(o); break;
        case OT_ROLLERGEN: upd_rollergen(o); break;
        case OT_ROLLER: upd_roller(o); break;
        case OT_PUFFER: upd_puffer(o); break;
        case OT_CRAB: upd_crab(o); break;
        case OT_BRUTE: upd_brute(o); break;
        case OT_SEGFAULT: upd_segfault(o); break;
        case OT_FIREBALL: upd_fireball(o); break;
        case OT_SPIKEBOMB: upd_spikebomb(o); break;
        case OT_FLAMEBAR: upd_flamebar(o); break;
        case OT_PLATFORM: upd_platform(o); break;
        case OT_ROTPLAT: upd_rotplat(o); break;
        case OT_ELEVATOR: upd_elevator(o); break;
        case OT_FALLPLAT: upd_fallplat(o); break;
        case OT_SIGN: case OT_NPC:
            if (touching(o)) push_player_out(o, o->radius);
            if (o->type == OT_NPC) o->t += DT;
            break;
        case OT_BABY: upd_baby(o); break;
        case OT_MOTHER: upd_mother(o); break;
        case OT_RACER: upd_racer(o); break;
        case OT_RACEFLAG: upd_raceflag(o); break;
        case OT_TRAMPOLINE: upd_trampoline(o); break;
        case OT_TREE: case OT_POLE: upd_pole(o); break;
        case OT_WARP: upd_warp(o); break;
        case OT_DOOR: case OT_STARDOOR: upd_door(o); break;
        case OT_CLAM: upd_clam(o); break;
        case OT_RING: upd_ring(o); break;
        case OT_CRATE: upd_crate(o); break;
        case OT_TIMERGOAL: upd_timergoal(o); break;
        case OT_TIMERSTART: upd_timerstart(o); break;
        case OT_ROCKETBOX: upd_rocketbox(o); break;
        case OT_GHOST: upd_ghost(o); break;
        case OT_GHOSTKING: upd_ghostking(o); break;
        case OT_TOPSWITCH: upd_topswitch(o); break;
        case OT_HEART: upd_heart(o); break;
        case OT_WATCHDOG: upd_watchdog(o); break;
        case OT_OOM: upd_oom(o); break;
        case OT_MAZEEXIT: upd_mazeexit(o); break;
        case OT_STEAM:   /* hot spring steam: puffs rising from a disc of radius param */
            if (g_frame % 5 == 0) {
                float a = frand() * TAU_F, r = sqrtf(frand()) * o->param;
                fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(sinf(a) * r, 0.2f, cosf(a) * r)), 1.4f);
            }
            break;
        case OT_DECOR:
            if (o->radius > 0 && touching(o)) push_player_out(o, o->radius);
            break;
        default:
            if (o->type >= OT_FX_SPARKLE) upd_fx(o);
            break;
        }
    }
}

/* dynamic collision for solid objects */
static void add_box_col(Obj *o, vec3 c, float hx, float hy, float hz, float yaw)
{
    vec3 p[8];
    int i;
    float cy = cosf(yaw), sy = sinf(yaw);
    int idx = obj_index(o);
    int fl = (o->type == OT_DOOR || o->type == OT_STARDOOR) ? 0 : SF_NOCAM;
    for (i = 0; i < 8; i++) {
        vec3 l = v3((i & 1) ? hx : -hx, (i & 2) ? hy : -hy, (i & 4) ? hz : -hz);
        p[i] = v3add(c, v3(l.x * cy + l.z * sy, l.y, -l.x * sy + l.z * cy));
    }
#define Q(a, b, c2, d) col_dyn_add(p[a], p[b], p[c2], fl, idx); col_dyn_add(p[a], p[c2], p[d], fl, idx);
    Q(2, 6, 7, 3) Q(0, 1, 5, 4) Q(1, 3, 7, 5) Q(0, 4, 6, 2) Q(4, 5, 7, 6) Q(0, 2, 3, 1)
#undef Q
}

void objs_collision(void)
{
    int i;
    col_dyn_clear();
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active || !(o->flags & OF_SOLID)) continue;
        if (o->type == OT_DOOR || o->type == OT_STARDOOR) {
            if (o->state != 0) continue;
            add_box_col(o, v3add(o->pos, v3(0, o->f[6], 0)), o->f[5], o->f[6], o->f[7], o->yaw);
            continue;
        }
        if (o->type == OT_CRATE) {
            add_box_col(o, v3add(o->pos, v3(0, 0.7f, 0)), 0.7f, 0.7f, 0.7f, o->yaw);
            continue;
        }
        if (o->type == OT_FALLPLAT && o->state == 3) continue;
        if (o->type == OT_TRAMPOLINE) {
            add_box_col(o, v3add(o->pos, v3(0, 0.45f, 0)), 1.25f, 0.45f, 1.25f, 0);
            continue;
        }
        if (o->type == OT_ROCKETBOX) {
            if (o->state == 0) add_box_col(o, o->pos, 0.7f, 0.7f, 0.7f, 0);
            continue;
        }
        if (o->type == OT_CAGE) {
            /* eight wall quads approximating the round cage */
            int k;
            for (k = 0; k < 8; k++) {
                float a0 = TAU_F * k / 8, a1 = TAU_F * (k + 1) / 8;
                vec3 p0 = v3(o->pos.x + sinf(a0) * 1.55f, o->pos.y - 0.5f, o->pos.z + cosf(a0) * 1.55f);
                vec3 p1 = v3(o->pos.x + sinf(a1) * 1.55f, o->pos.y - 0.5f, o->pos.z + cosf(a1) * 1.55f);
                vec3 q0 = v3add(p0, v3(0, 3.4f, 0)), q1 = v3add(p1, v3(0, 3.4f, 0));
                col_dyn_add(p0, p1, q1, SF_NOCAM, obj_index(o));
                col_dyn_add(p0, q1, q0, SF_NOCAM, obj_index(o));
                col_dyn_add(p0, q0, q1, SF_NOCAM, obj_index(o));
                col_dyn_add(p0, q1, p1, SF_NOCAM, obj_index(o));
            }
            continue;
        }
        add_box_col(o, o->pos, o->f[5], o->f[6], o->f[7], o->yaw);
    }
}

/* ---------------------------------------------------------------- drawing */
static vec3 cam_right, cam_up;

static void draw_glow(vec3 p, float size, float r, float g, float b, float a)
{
    glBindTexture(GL_TEXTURE_2D, g_tex[TEX_GLOW]);
    gfx_billboard_quad(p, size, r, g, b, a, cam_right, cam_up);
}

static void draw_model_at(GLuint dl, vec3 p, float yaw, float scale)
{
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(DEG(yaw), 0, 1, 0);
    if (scale != 1) glScalef(scale, scale, scale);
    glCallList(dl);
    glPopMatrix();
}

static void draw_npc_penguin(Obj *o, int variant, float scale)
{
    TuxPose tp;
    memset(&tp, 0, sizeof tp);
    tp.flipL_z = tp.flipR_z = 0.2f + (variant == TUXV_BABY ? sinf(g_frame * 0.3f) * 0.3f : 0);
    tp.body_squash = sinf(g_frame * 0.06f + o->home.x) * 0.02f;
    if (o->type == OT_BABY && !(o->flags & OF_HELD)) {
        tp.footL = sinf(g_frame * 0.2f) * 0.4f; tp.footR = -tp.footL;
        tp.body_roll = sinf(g_frame * 0.2f) * 0.1f;
    }
    if (o->type == OT_BABY && (o->flags & OF_HELD)) { tp.flipL_z = tp.flipR_z = 1.5f + sinf(g_frame * 0.5f) * 0.5f; }
    draw_tux_model(o->pos, o->yaw, &tp, scale, variant);
}

static void draw_obj(Obj *o)
{
    switch (o->type) {
    case OT_COIN: case OT_REDCOIN: case OT_BLUECOIN: {
        GLuint dl = o->type == OT_COIN ? mdl[MDL_COIN] : (o->type == OT_REDCOIN ? mdl[MDL_COIN_RED] : mdl[MDL_COIN_BLUE]);
        if (o->type == OT_BLUECOIN && o->param == 1 && g_blue_switch_timer <= 0) return;
        if (o->state == 1 && o->timer > 480 && (o->timer / 4) % 2) return;
        if (o->type == OT_BLUECOIN && o->param == 1 && g_blue_switch_timer < 180 && (g_frame / 4) % 2) return;
        draw_model_at(dl, v3add(o->pos, v3(0, 0.45f, 0)), o->yaw, 1.15f);
        break;
    }
    case OT_ONEUP: draw_model_at(mdl[MDL_FISH_GREEN], v3add(o->pos, v3(0, 0.5f, 0)), o->yaw, 1.3f); break;
    case OT_STAR: {
        int got = has_star(star_course(), o->param);
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 0.7f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glScalef(0.75f, 0.75f, 0.75f);
        if (got) {
            glEnable(GL_BLEND);
            glDepthMask(GL_FALSE);
            glCallList(mdl[MDL_STAR_GHOST]);
            glDepthMask(GL_TRUE);
        } else glCallList(mdl[MDL_STAR]);
        glPopMatrix();
        break;
    }
    case OT_GRANDSTAR:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 1.0f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glScalef(2.0f, 2.0f, 2.0f);
        glCallList(mdl[MDL_STAR]);
        glPopMatrix();
        break;
    case OT_BLUESWITCH:
        if (o->state == 0) draw_model_at(mdl[MDL_SWITCH], o->pos, 0, 1);
        else {
            glPushMatrix(); glTranslatef(o->pos.x, o->pos.y, o->pos.z); glScalef(1, 0.4f, 1); glCallList(mdl[MDL_SWITCH]); glPopMatrix();
        }
        break;
    case OT_BUG: {
        float sq = o->state == 90 ? (o->i[7] ? 0.2f : 1.0f) : 1.0f;
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + (o->state == 90 && !o->i[7] ? o->timer * 0.05f : 0), o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        if (o->state != 90) glRotatef(sinf(g_frame * 0.4f + o->home.x) * 8, 0, 0, 1);
        glScalef(1, sq, 1);
        glCallList(mdl[MDL_BUG]);
        glPopMatrix();
        break;
    }
    case OT_FORKBOMB: {
        int lit = (o->state == 1 && (o->timer / (o->timer > 150 ? 4 : 10)) % 2) || o->state == 100;
        if (o->state == 90) return;
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(sinf(g_frame * 0.3f) * 6, 0, 0, 1);
        glCallList(mdl[lit ? MDL_BOMB_LIT : MDL_BOMB]);
        glPopMatrix();
        if (o->state == 1) { glEnable(GL_TEXTURE_2D); draw_glow(v3add(o->pos, v3(0, 1.6f, 0)), 0.3f, 1, 0.8f, 0.3f, 0.9f); glDisable(GL_TEXTURE_2D); }
        break;
    }
    case OT_KING:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        if (o->state == 3) glRotatef(sinf(g_frame * 0.5f) * 10, 0, 0, 1);
        else if (o->state == 1) glRotatef(sinf(g_frame * 0.15f) * 5, 0, 0, 1);
        glScalef(o->scale, o->scale, o->scale);
        glCallList(mdl[MDL_KING]);
        glPopMatrix();
        break;
    case OT_CRUSHER: draw_model_at(mdl[MDL_CRUSHER], o->pos, o->yaw, 1); break;
    case OT_ROLLER:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 1.2f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(DEG(o->pitch), 1, 0, 0);
        glScalef(1.2f, 1.2f, 1.2f);
        glCallList(mdl[MDL_ROLLER]);
        glPopMatrix();
        break;
    case OT_PUFFER:
        if (o->state == 90) return;
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 0.6f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glScalef(0.6f * o->scale, 0.6f * o->scale, 0.6f * o->scale);
        glCallList(mdl[MDL_PUFFER]);
        glPopMatrix();
        break;
    case OT_CRAB:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        if (o->state == 90) glScalef(1, o->i[7] ? 0.2f : 1, 1);
        else glRotatef(sinf(g_frame * 0.3f) * 5, 0, 0, 1);
        glCallList(mdl[MDL_CRAB]);
        glPopMatrix();
        break;
    case OT_BRUTE:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(sinf(g_frame * 0.2f) * 4, 0, 0, 1);
        glCallList(mdl[MDL_BRUTE]);
        glPopMatrix();
        break;
    case OT_SEGFAULT: {
        float s = o->scale;
        int flash = o->state == 6 && (o->timer / 4) % 2;
        if (flash) return;
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glScalef(s, s, s);
        glCallList(mdl[MDL_SEG_BODY]);
        glPushMatrix();
        glTranslatef(0, 3.5f, 0.1f);
        if (o->state == 2) glRotatef(15, 1, 0, 0);
        if (o->state == 3 && o->timer < 40) glTranslatef(sinf(o->timer * 1.7f) * 0.08f, 0, 0);   /* glitching before a charge */
        glRotatef(sinf(g_frame * 0.05f) * 10, 0, 1, 0);
        glCallList(mdl[MDL_SEG_HEAD]);
        glPopMatrix();
        glPopMatrix();
        break;
    }
    case OT_SPIKEBOMB:
        if (o->state == 1) return;
        draw_model_at(mdl[MDL_SPIKEBOMB], v3add(o->pos, v3(0, 1.1f, 0)), o->yaw, 1.1f);
        break;
    case OT_PLATFORM: case OT_ROTPLAT: case OT_ELEVATOR: case OT_FALLPLAT:
        if (o->type == OT_FALLPLAT && o->state == 3) return;
        if (o->dl) draw_model_at(o->dl, o->pos, o->yaw, 1);
        break;
    case OT_SIGN: draw_model_at(mdl[MDL_SIGN], o->pos, o->yaw, 1); break;
    case OT_NPC: draw_npc_penguin(o, TUXV_NPC, 1.0f); break;
    case OT_MOTHER: draw_model_at(mdl[MDL_INIT], o->pos, o->yaw, 1.0f); break;
    case OT_BABY: draw_model_at(mdl[MDL_CHILDPROC], v3add(o->pos, v3(0, (o->flags & OF_HELD) ? 0 : fabsf(sinf(g_frame * 0.15f)) * 0.08f, 0)), o->yaw, 1.0f); break;
    case OT_RACER: {
        float bob = (o->state == 2) ? fabsf(sinf(o->t * 2.5f)) * 0.2f : 0;
        draw_model_at(mdl[MDL_GNU], v3add(o->pos, v3(0, bob, 0)), o->yaw, 1);
        break;
    }
    case OT_RACEFLAG: draw_model_at(mdl[MDL_FLAG], o->pos, o->yaw, 1); break;
    case OT_TRAMPOLINE:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glScalef(1, 1 - 0.3f * sinf(o->f[0] * PI_F), 1);
        glCallList(mdl[MDL_TRAMPOLINE]);
        glPopMatrix();
        break;
    case OT_TREE:
        draw_model_at(o->param == 1 ? mdl[MDL_PINE_SNOW] : (o->param == 2 ? mdl[MDL_TREE] : mdl[MDL_PINE]), o->pos, o->yaw, 1.0f);
        break;
    case OT_POLE:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glScalef(1.4f, o->height, 1.4f);
        glCallList(mdl[MDL_POLE]);
        glPopMatrix();
        break;
    case OT_DOOR: case OT_STARDOOR:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glTranslatef(-1.2f * o->f[1], 0, 0);
        glRotatef(DEG(o->f[0]), 0, 1, 0);
        glTranslatef(1.2f * o->f[1], 0, 0);
        glScalef(o->f[1], o->f[2], 1);
        if (cur_level == LV_LOBBY) glCallList(mdl[o->type == OT_STARDOOR ? MDL_CYSTARDOOR : MDL_CYDOOR]);
        else glCallList(mdl[o->type == OT_STARDOOR ? MDL_STARDOOR : MDL_DOOR]);
        glPopMatrix();
        if (o->type == OT_STARDOOR && total_stars() < o->param) {
            /* number of stars needed, drawn as little stars over the door */
            int k, n = o->param > 10 ? 10 : o->param;
            for (k = 0; k < n; k++) {
                vec3 f = yaw_dir(o->yaw), r = v3(f.z, 0, -f.x);
                vec3 p = v3add(v3mad(v3mad(o->pos, r, (k - (n - 1) * 0.5f) * 0.45f), f, 0.25f), v3(0, 3.55f, 0));
                glPushMatrix();
                glTranslatef(p.x, p.y, p.z);
                glRotatef(DEG(o->yaw), 0, 1, 0);
                glScalef(0.18f, 0.18f, 0.18f);
                glCallList(mdl[MDL_STAR]);
                glPopMatrix();
            }
        }
        break;
    case OT_CLAM:
        glDisable(GL_CULL_FACE);
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 0.7f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glCallList(mdl[MDL_CLAM_BOT]);
        glTranslatef(0, 0, -1.8f);
        glRotatef(-o->f[0] * 70, 1, 0, 0);
        glCallList(mdl[MDL_CLAM_TOP]);
        glPopMatrix();
        glEnable(GL_CULL_FACE);
        break;
    case OT_RING:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glScalef(o->scale, o->scale, o->scale);
        if (o->param == g_ring_next) glColor3f(1, 1, 1);
        glCallList(mdl[MDL_RING]);
        glPopMatrix();
        break;
    case OT_CRATE: draw_model_at(mdl[MDL_CRATE], o->pos, o->yaw, 1); break;
    case OT_ROCKETBOX: if (o->state == 0) draw_model_at(mdl[MDL_ROCKETBOX], o->pos, o->yaw, 1); break;
    case OT_TOPSWITCH: draw_model_at(mdl[o->state ? MDL_TOPSWITCH_ON : MDL_TOPSWITCH], o->pos, 0, 1); break;
    case OT_HEART: draw_model_at(mdl[MDL_HEART], v3add(o->pos, v3(0, 0.7f, 0)), o->yaw, 1.4f); break;
    case OT_OOM: {
        float sw = sinf(o->t * 1.4f);
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + fabsf(sw) * 0.12f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glRotatef(sw * 5, 0, 0, 1);
        glCallList(mdl[MDL_OOM]);
        glPopMatrix();
        break;
    }
    case OT_MAZEEXIT:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + 1.6f, o->pos.z);
        glRotatef(o->t * 60, 0, 1, 0);
        glCallList(mdl[MDL_RING]);
        glPopMatrix();
        break;
    case OT_CAGE: draw_model_at(mdl[MDL_CAGE], o->pos, 0, 1); break;
    case OT_WATCHDOG: {
        float lunge = o->state == 1 && o->timer < 14 ? 0.85f : 1.0f;   /* crouch before the lunge */
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        if (o->state == 2) glRotatef(sinf(o->timer * 0.5f) * 6, 0, 0, 1);   /* panting */
        glScalef(1.3f, 1.3f * (o->state == 4 ? 0.6f : lunge), 1.3f);
        glCallList(mdl[MDL_WATCHDOG]);
        glPopMatrix();
        break;
    }
    case OT_GHOST: case OT_GHOSTKING: {
        float sc = o->type == OT_GHOSTKING ? o->scale : o->scale;
        int fade = o->f[0] < 0.6f;
        int flash = o->type == OT_GHOSTKING && o->state == 1 && (o->timer / 4) % 2;
        if (flash) break;
        glEnable(GL_BLEND);
        if (fade) glDepthMask(GL_FALSE);
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y + sinf(g_frame * 0.06f + o->home.x) * 0.15f, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        glScalef(sc, sc, sc);
        glCallList(mdl[fade ? MDL_GHOST_FADE : MDL_GHOST]);
        glPopMatrix();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        break;
    }
    case OT_TIMERGOAL: break;
    case OT_DECOR:
        if (o->param < 0) {   /* statue of Tux */
            TuxPose tp;
            memset(&tp, 0, sizeof tp);
            tp.flipL_z = 2.6f; tp.flipR_z = 0.3f; tp.head_pitch = -0.15f;
            draw_tux_model(o->pos, o->yaw, &tp, o->scale, TUXV_TUX);
        } else draw_model_at(mdl[o->param], o->pos, o->yaw, o->scale);
        break;
    case OT_WARP:
        if (o->i[3]) {
            glPushMatrix();
            glTranslatef(o->pos.x, o->pos.y + 1.6f, o->pos.z);
            glRotatef(g_frame * 3.0f, 0, 1, 0);
            glScalef(1.1f, 1.1f, 1.1f);
            glCallList(mdl[MDL_RING]);
            glRotatef(90, 0, 1, 0);
            glCallList(mdl[MDL_RING]);
            glPopMatrix();
        }
        break;
    }
}

static void draw_shadow_for(vec3 p, float r)
{
    Surf *s;
    float fy = col_floor(p.x, p.y + 0.3f, p.z, &s);
    float h;
    if (!s) return;
    h = p.y - fy;
    if (h > 25) return;
    gfx_shadow(v3(p.x, fy, p.z), s->n, r * (1 - h * 0.02f), 0.55f * (1 - h / 25));
}

void objs_draw(void)
{
    int i;
    float mv[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mv);
    cam_right = v3(mv[0], mv[4], mv[8]);
    cam_up = v3(mv[1], mv[5], mv[9]);
    glEnable(GL_LIGHTING);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        float d;
        if (!o->active || o->type >= OT_FX_SPARKLE) continue;
        d = v3dist(o->pos, cam.pos);
        if (d > lv.fog_end + 20 && o->type != OT_GRANDSTAR) continue;
        draw_obj(o);
    }
    /* shadows */
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2, -2);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active || !(o->flags & OF_SHADOW)) continue;
        if (v3dist(o->pos, cam.pos) > 60) continue;
        if (o->type == OT_FORKBOMB && o->state == 90) continue;
        if (o->type == OT_BLUECOIN && o->param == 1 && g_blue_switch_timer <= 0) continue;
        if (o->type == OT_PLATFORM || o->type == OT_ROTPLAT || o->type == OT_ELEVATOR || o->type == OT_FALLPLAT || o->type == OT_CRATE) continue;
        draw_shadow_for(o->pos, o->radius * (o->type == OT_KING ? 0.9f : 1.0f));
    }
    if (pl.visible && pl.act != ACT_SWIM) draw_shadow_for(pl.pos, 0.6f);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}

void objs_draw_alpha(void)
{
    int i;
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);
    glEnable(GL_TEXTURE_2D);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        switch (o->type) {
        case OT_FX_SPARKLE: draw_glow(o->pos, 0.5f * o->scale * (1 - o->timer / 30.0f), 1, 0.95f, 0.6f, 1); break;
        case OT_FX_EXPLODE: {
            float k = o->timer / 30.0f;
            draw_glow(v3add(o->pos, v3(0, 1, 0)), (1.5f + k * 3.5f) * o->scale, 1, 0.6f, 0.2f, 1 - k);
            draw_glow(v3add(o->pos, v3(0, 1, 0)), (0.8f + k * 2.0f) * o->scale, 1, 1, 0.7f, 1 - k);
            break;
        }
        case OT_FIREBALL:
            draw_glow(o->pos, 0.7f + sinf(g_frame * 0.5f + i) * 0.1f, 0.3f, 0.7f, 1.0f, 1);   /* frost bolt */
            draw_glow(o->pos, 0.35f, 0.85f, 0.95f, 1.0f, 1);
            break;
        case OT_FLAMEBAR: {
            int k;
            for (k = 1; k <= o->param; k++) {
                vec3 p = v3add(o->pos, v3scale(yaw_dir(o->yaw), k * 0.9f));
                draw_glow(p, 0.6f, 0.3f, 0.7f, 1.0f, 1);   /* frost bar */
                draw_glow(p, 0.3f, 0.85f, 0.95f, 1.0f, 1);
            }
            break;
        }
        case OT_OOM: {
            vec3 e = v3add(v3add(o->pos, v3(0, 2.98f, 0)), v3scale(yaw_dir(o->yaw), 0.65f));
            float c = o->state == 1 ? 1.0f : 0.6f;
            draw_glow(e, 0.5f + 0.2f * c, 1, 0.15f, 0.1f, c);
            if (o->state == 1) draw_glow(v3add(o->pos, v3(0, 4.1f, 0)), 0.6f + 0.15f * sinf(g_frame * 0.4f), 1, 0.2f, 0.1f, 1);
            if (o->state == 2 || o->state == 3) draw_glow(v3add(o->pos, v3(0, 4.1f, 0)), 0.5f, 1, 0.85f, 0.2f, 0.9f);
            break;
        }
        case OT_MAZEEXIT:
            draw_glow(v3add(o->pos, v3(0, 1.6f, 0)), 2.0f, 0.4f, 1.0f, 0.6f, 0.6f);
            break;
        case OT_WARP:
            if (o->i[3]) draw_glow(v3add(o->pos, v3(0, 1.6f, 0)), 2.2f + sinf(g_frame * 0.1f) * 0.3f, 0.7f, 0.3f, 1.0f, 0.8f);
            break;
        case OT_STAR: case OT_GRANDSTAR:
            draw_glow(v3add(o->pos, v3(0, o->type == OT_STAR ? 0.7f : 1.0f, 0)), o->type == OT_STAR ? 1.4f : 3.5f, 1, 0.9f, 0.4f, 0.35f);
            break;
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (i = 0; i < nobj; i++) {
        Obj *o = &objs[i];
        if (!o->active) continue;
        switch (o->type) {
        case OT_FX_SMOKE: case OT_FX_SNOWPUFF: {
            float k = o->timer / 40.0f;
            glBindTexture(GL_TEXTURE_2D, g_tex[TEX_CLOUD]);
            gfx_billboard_quad(o->pos, (0.4f + k * 0.8f) * o->scale, 0.85f, 0.85f, 0.85f, 0.8f * (1 - k), cam_right, cam_up);
            break;
        }
        case OT_OOM: {
            /* its field of view, drawn on the floor (shortened by walls) */
            int k;
            vec3 eye = v3add(o->pos, v3(0, 1.0f, 0));
            glDisable(GL_TEXTURE_2D);
            glBegin(GL_TRIANGLE_FAN);
            glColor4f(1, 0.1f, 0.1f, o->state == 1 ? 0.3f : 0.18f);
            glVertex3f(o->pos.x, o->pos.y + 0.06f, o->pos.z);
            glColor4f(1, 0.1f, 0.1f, 0.0f);
            for (k = 0; k <= 12; k++) {
                float a = o->yaw - PI_F / 3 + (2 * PI_F / 3) * k / 12.0f, t = 1;
                vec3 n, end = v3mad(eye, yaw_dir(a), 9.0f);
                if (col_ray(eye, end, &t, &n, 1)) t *= 0.98f; else t = 1;
                glVertex3f(o->pos.x + sinf(a) * 9 * t, o->pos.y + 0.06f, o->pos.z + cosf(a) * 9 * t);
            }
            glEnd();
            glEnable(GL_TEXTURE_2D);
            break;
        }
        case OT_FX_BUBBLE:
            glBindTexture(GL_TEXTURE_2D, g_tex[TEX_GLOW]);
            gfx_billboard_quad(o->pos, 0.15f * o->scale + 0.1f, 0.8f, 0.9f, 1, 0.8f, cam_right, cam_up);
            break;
        case OT_FX_SHOCKWAVE: {
            int k;
            float r = o->f[1];
            glDisable(GL_TEXTURE_2D);
            glColor4f(1, 0.6f, 0.9f, 0.7f);
            glBegin(GL_QUAD_STRIP);
            for (k = 0; k <= 48; k++) {
                float a = TAU_F * k / 48;
                glVertex3f(o->pos.x + sinf(a) * (r - 0.25f), o->pos.y + 0.1f, o->pos.z + cosf(a) * (r - 0.25f));
                glVertex3f(o->pos.x + sinf(a) * (r + 0.25f), o->pos.y + 0.6f, o->pos.z + cosf(a) * (r + 0.25f));
            }
            glEnd();
            glEnable(GL_TEXTURE_2D);
            break;
        }
        }
    }
    glDisable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);
    glEnable(GL_LIGHTING);
}
