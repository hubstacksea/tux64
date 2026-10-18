/* player.c - Tux: SM64-style action state machine, physics and animation */
#include "game.h"
#include <string.h>
#include <stdio.h>

Player pl;
extern int g_trace;

#define GRAV       38.0f
#define TERM_VEL  -24.0f
#define RUN_SPEED  9.0f
#define RAD_T      0.42f
#define TUX_H      1.6f

static float int_yaw, int_mag;

int player_in_air(void) { return pl.act >= ACT_FIRST_AIR && pl.act < ACT_SWIM; }

void player_set_act(int a)
{
    if (a == pl.act) { pl.act_t = 0; pl.act_s = 0; return; }
    pl.prev_act = pl.act;
    pl.act = a;
    pl.act_t = 0;
    pl.act_s = 0;
    pl.act_f = 0;
    if (a >= ACT_FIRST_AIR && a < ACT_SWIM && !(pl.prev_act >= ACT_FIRST_AIR && pl.prev_act < ACT_SWIM))
        pl.peak_y = pl.pos.y;
}

static void space_enter(void);

void player_init(vec3 pos, float yaw)
{
    int lives = pl.lives, coins = pl.coins, health = pl.health;
    memset(&pl, 0, sizeof pl);
    pl.lives = lives; pl.coins = coins; pl.health = health;
    pl.pos = pos;
    pl.yaw = yaw;
    pl.held = -1;
    pl.platform = -1;
    pl.pole = -1;
    pl.visible = 1;
    pl.boots = cur_level == LV_SEA;   /* the dash boots never come off on the AURORA SEA */
    pl.act = ACT_FALL;
    pl.peak_y = pos.y;
    pl.floor_y = col_floor(pos.x, pos.y + 1.0f, pos.z, &pl.floor);
    if (pl.floor && pos.y - pl.floor_y < 0.5f) { pl.pos.y = pl.floor_y; pl.act = ACT_IDLE; }
    if (lv.space) space_enter();   /* STARDUST ORBIT: on the rocket from the start */
}

float water_level_at(float x, float z, float y)
{
    int i;
    for (i = 0; i < lv.nwater; i++) {
        WaterBox *w = &lv.water[i];
        if (x >= w->x0 && x <= w->x1 && z >= w->z0 && z <= w->z1 && y >= w->ybot - 2.0f) return w->y;
    }
    return -1e9f;
}

static int water_warm_at(float x, float z)
{
    int i;
    for (i = 0; i < lv.nwater; i++) {
        WaterBox *w = &lv.water[i];
        if (x >= w->x0 && x <= w->x1 && z >= w->z0 && z <= w->z1) return w->warm;
    }
    return 0;
}

void player_heal(int n) { pl.heal_queue += n; }

int player_is_attacking(void)
{
    switch (pl.act) {
    case ACT_FLY: return 1;
    case ACT_PUNCH: return pl.act_t >= 2 && pl.act_t <= 12;
    case ACT_JUMPKICK: return pl.act_t < 18;
    case ACT_DIVE: return 1;
    case ACT_BELLY_SLIDE: return v3len(v3(pl.vel.x, 0, pl.vel.z)) > 4.0f;
    case ACT_GPOUND: return pl.act_s == 1;
    case ACT_GPOUND_LAND: return pl.act_t < 4;
    case ACT_ROLLOUT: return 0;
    case ACT_SWIM: return pl.act_s == 2;
    }
    return 0;
}

static int is_unhurtable(void)
{
    return pl.invinc > 0 || pl.act == ACT_DEATH || pl.act == ACT_DROWN || pl.act == ACT_STAR_DANCE ||
           pl.act == ACT_FROZEN || pl.act == ACT_KNOCKBACK || pl.act == ACT_AIR_KNOCK;
}

static void drop_held(int throwit);

void player_hurt(int dmg, vec3 from)
{
    float ay;
    if (is_unhurtable()) return;
    if (pl.held >= 0) drop_held(0);
    pl.health -= dmg;
    pl.invinc = 120;
    sfx(SFX_HURT);
    sfx_ex(SFX_SQUAWK, 0.8f, 0.9f);
    ay = yaw_to(pl.pos, from);
    pl.yaw = ay;
    if (pl.health <= 0) {
        pl.health = 0;
        player_set_act(pl.act == ACT_SWIM ? ACT_DROWN : ACT_DEATH);
        return;
    }
    if (pl.act == ACT_SWIM) {
        pl.vel = v3scale(yaw_dir(ay), -5);
        return;
    }
    if (player_in_air()) {
        player_set_act(ACT_AIR_KNOCK);
        pl.vel.y = 9;
        pl.fwd = -7;
    } else {
        player_set_act(ACT_KNOCKBACK);
        pl.fwd = -8;
    }
    if (pl.pole >= 0) pl.pole = -1;
}

void player_bounce(float vy)
{
    pl.vel.y = vy;
    player_set_act(ACT_BOUNCE);
    pl.kicked = 0;
}

static void compute_intended(void)
{
    int_mag = in.mag;
    if (lv.side_view) {   /* TTY WORLD 1-1: only left and right */
        int_mag = fabsf(in.sx) > 0.2f ? minf(fabsf(in.sx) * 1.1f, 1.0f) : 0;
        if (pl.no_ctrl) int_mag = 0;
        int_yaw = int_mag > 0 ? (in.sx > 0 ? PI_F / 2 : -PI_F / 2) : pl.yaw;
        return;
    }
    if (pl.no_ctrl) int_mag = 0;
    if (int_mag > 0.01f) int_yaw = wrap_angle(camera_yaw() - atan2f(in.sx, in.sy));
    else int_yaw = pl.yaw;
}

static int pressed(int b) { return !pl.no_ctrl && (in.pressed & b); }
static int held(int b) { return !pl.no_ctrl && (in.held & b); }

/* ------------------------------------------------------------------ steps */
enum { GSTEP_NONE, GSTEP_LEFT, GSTEP_WALL, GSTEP_BLOCKED };
enum { ASTEP_NONE, ASTEP_LAND, ASTEP_WALL, ASTEP_CEIL };

static int ground_step(void)
{
    vec3 np = v3(pl.pos.x + pl.vel.x * DT, pl.pos.y, pl.pos.z + pl.vel.z * DT);
    vec3 w1, w2;
    int h1, h2;
    Surf *fl;
    float fy, cy;
    h1 = col_walls(&np, 0.6f, RAD_T, &w1, NULL);
    h2 = col_walls(&np, 1.3f, RAD_T, &w2, NULL);
    fy = col_floor(np.x, np.y + 0.7f, np.z, &fl);
    if (!fl) {
        if (!col_any_below(np.x, np.y + 0.7f, np.z)) return GSTEP_BLOCKED;
        /* only steep ground here: start falling / sliding off it */
        pl.pos = np;
        pl.floor = NULL;
        pl.floor_y = -1e9f;
        return GSTEP_LEFT;
    }
    cy = col_ceil(np.x, fy + 0.5f, np.z, NULL);
    if (cy - fy < 1.3f) return GSTEP_BLOCKED;
    pl.floor = fl;
    if (np.y - fy > 0.7f) {
        pl.pos = np;
        pl.floor_y = fy;
        return GSTEP_LEFT;
    }
    pl.pos = v3(np.x, fy, np.z);
    pl.floor_y = fy;
    pl.vel.y = 0;
    if (h1 || h2) { pl.wall_n = h2 ? w2 : w1; return GSTEP_WALL; }
    return GSTEP_NONE;
}

static void tramp_bounce(Obj *t);

static int air_step(void)
{
    float spd = v3len(pl.vel) * DT;
    int n = (int)(spd / 0.25f) + 1, i, res = ASTEP_NONE;
    for (i = 0; i < n; i++) {
        vec3 np = v3mad(pl.pos, pl.vel, DT / n);
        vec3 w1, w2;
        int h1, h2;
        Surf *fl, *cl;
        float fy, cy;
        h1 = col_walls(&np, 0.5f, RAD_T, &w1, NULL);
        h2 = col_walls(&np, 1.3f, RAD_T, &w2, NULL);
        fy = col_floor(np.x, np.y + 0.6f, np.z, &fl);
        if (!fl && !col_any_below(np.x, np.y + 0.6f, np.z)) {
            np.x = pl.pos.x; np.z = pl.pos.z;
            fy = col_floor(np.x, np.y + 0.6f, np.z, &fl);
        }
        cy = col_ceil(np.x, np.y + 0.8f, np.z, &cl);
        if (np.y + TUX_H > cy) {
            /* moving sideways into a low ceiling (e.g. the edge of a slab): block like a wall
             * instead of squeezing Tux down underneath it */
            float cy_old = col_ceil(pl.pos.x, pl.pos.y + 0.8f, pl.pos.z, NULL);
            if (cy_old > cy + 0.05f && (np.x != pl.pos.x || np.z != pl.pos.z)) {
                np.x = pl.pos.x; np.z = pl.pos.z;
                fy = col_floor(np.x, np.y + 0.6f, np.z, &fl);
                cy = cy_old;
                if (res != ASTEP_CEIL) res = ASTEP_WALL;
            }
        }
        if (fl && np.y <= fy) {
            if (pl.vel.y <= 0 && fl->owner >= 0 && objs[fl->owner].type == OT_TRAMPOLINE && fl->n.y > 0.7f) {
                pl.pos = v3(np.x, fy, np.z);
                tramp_bounce(&objs[fl->owner]);
                return ASTEP_NONE;
            }
            if (pl.vel.y <= 0) {
                pl.pos = v3(np.x, fy, np.z);
                pl.floor = fl;
                pl.floor_y = fy;
                return ASTEP_LAND;
            }
            np.y = fy;
        }
        if (np.y + TUX_H > cy) {
            np.y = cy - TUX_H;
            if (pl.vel.y > 0) pl.vel.y = 0;
            res = ASTEP_CEIL;
            if (fl && np.y < fy) { np = pl.pos; }
        }
        pl.pos = np;
        pl.floor = fl;
        pl.floor_y = fl ? fy : -1e9f;
        if (h1 || h2) {
            pl.wall_n = h2 ? w2 : w1;
            if (res != ASTEP_CEIL) res = ASTEP_WALL;
        }
    }
    return res;
}

static void set_vel_from_fwd(void)
{
    vec3 d = yaw_dir(pl.yaw);
    pl.vel.x = d.x * pl.fwd;
    pl.vel.z = d.z * pl.fwd;
}

static void apply_gravity(float mul)
{
    pl.vel.y -= GRAV * mul * DT;
    if (pl.vel.y < TERM_VEL) pl.vel.y = TERM_VEL;
}

static int floor_flags(void) { return pl.floor ? pl.floor->flags : 0; }
static float floor_ny(void) { return pl.floor ? pl.floor->n.y : 1.0f; }

static int floor_is_slidey(void)
{
    int f = floor_flags();
    float ny = floor_ny();
    if (f & SF_SLIDE) return 1;
    if (f & SF_NOSLIP) return 0;
    if (f & SF_SLIPPERY) return ny < 0.9f;
    return ny < 0.72f;
}

/* ------------------------------------------------------------------ transitions */
static void do_jump(int type)
{
    float vy = 12.6f + 0.25f * maxf(pl.fwd, 0);
    pl.kicked = 0;
    switch (type) {
    case ACT_JUMP: sfx(SFX_JUMP); break;
    case ACT_DJUMP: vy = 15.4f + 0.25f * maxf(pl.fwd, 0); sfx(SFX_JUMP2); break;
    case ACT_TJUMP: vy = 20.5f; sfx(SFX_JUMP3); sfx_ex(SFX_SQUAWK, 0.7f, 1.3f); break;
    case ACT_BACKFLIP: vy = 19.5f; pl.fwd = -4.5f; sfx(SFX_BACKFLIP); break;
    case ACT_SIDEFLIP: vy = 19.0f; pl.yaw = int_yaw; pl.fwd = 4.0f; sfx(SFX_BACKFLIP); break;
    case ACT_LONGJUMP: vy = 9.5f; pl.fwd = minf(pl.fwd * 1.5f, 14.5f); sfx(SFX_JUMP2); break;
    case ACT_HOLD_JUMP: vy = 12.0f; sfx(SFX_JUMP); break;
    case ACT_ROLLOUT: vy = 11.0f; sfx(SFX_JUMP); break;
    }
    if (pl.sink > 0) vy *= 1.0f - minf(pl.sink, 1.2f) * 0.35f;
    pl.vel.y = vy;
    player_set_act(type);
}

static void chain_jump(void)
{
    if (pl.chain_timer < 10 && pl.jump_chain == 1) do_jump(ACT_DJUMP);
    else if (pl.chain_timer < 10 && pl.jump_chain == 2 && pl.fwd > 6.0f) do_jump(ACT_TJUMP);
    else do_jump(ACT_JUMP);
    pl.jump_chain = 0;
}

static int try_interact_b(void);
static void enter_fly(void);
static void skate_enter(int airborne);
static void sky_enter(void);

static void enter_fall(void) { player_set_act(ACT_FALL); }

static void begin_belly_slide(float speed)
{
    vec3 d = yaw_dir(pl.yaw);
    pl.vel.x = d.x * speed;
    pl.vel.z = d.z * speed;
    player_set_act(ACT_BELLY_SLIDE);
}

/* common ground checks - return 1 if action changed */
static int ground_common(int allow_jump)
{
    if (floor_is_slidey() && pl.act != ACT_BELLY_SLIDE) {
        begin_belly_slide(maxf(pl.fwd, 1.0f));
        return 1;
    }
    if (pl.floor && (pl.floor->flags & SF_BURN)) {
        pl.health -= 2;
        pl.invinc = 30;
        sfx(SFX_HURT);
        sfx(SFX_FIRE);
        pl.vel.y = 22;
        player_set_act(ACT_BURN_JUMP);
        if (pl.health <= 0) { pl.health = 0; player_set_act(ACT_DEATH); }
        return 1;
    }
    if (allow_jump && pressed(BT_A)) { chain_jump(); return 1; }
    return 0;
}

static void walk_speed(void)
{
    float maxs = RUN_SPEED * int_mag;
    int slip = floor_flags() & SF_SLIPPERY;
    vec3 n = pl.floor ? pl.floor->n : v3(0, 1, 0);
    vec3 d = yaw_dir(pl.yaw);
    float turn = pl.fwd < 3.0f ? 18.0f : 8.0f;
    if (slip) turn *= 0.5f;
    if (pl.fwd <= 0) pl.fwd += (slip ? 6.0f : 22.0f) * DT;
    else if (pl.fwd <= maxs) pl.fwd += ((slip ? 7.0f : 18.0f) - pl.fwd * (slip ? 0.6f : 1.2f)) * DT;
    else pl.fwd -= (slip ? 3.0f : 20.0f) * DT;
    if (pl.fwd > maxs && pl.fwd - maxs < 0.2f) pl.fwd = maxs;
    if (n.y < 0.98f) pl.fwd += (n.x * d.x + n.z * d.z) * 14.0f * DT;
    if (pl.fwd > RUN_SPEED * 1.4f) pl.fwd = RUN_SPEED * 1.4f;
    pl.yaw = approach_angle(pl.yaw, int_yaw, turn * DT);
    if (slip) {
        /* velocity lags behind facing on ice */
        vec3 t = v3scale(yaw_dir(pl.yaw), pl.fwd);
        pl.vel.x = approachf(pl.vel.x, t.x, 9.0f * DT);
        pl.vel.z = approachf(pl.vel.z, t.z, 9.0f * DT);
    } else {
        set_vel_from_fwd();
    }
}

static void decel_ground(float rate)
{
    int slip = floor_flags() & SF_SLIPPERY;
    if (slip) rate *= 0.15f;
    pl.fwd = approachf(pl.fwd, 0, rate * DT);
    if (slip) {
        pl.vel.x = approachf(pl.vel.x, 0, rate * DT);
        pl.vel.z = approachf(pl.vel.z, 0, rate * DT);
    } else set_vel_from_fwd();
}

/* ------------------------------------------------------------------ ground actions */
static void act_idle(void)
{
    if (ground_common(1)) return;
    if (pressed(BT_B) && try_interact_b()) return;
    if (pressed(BT_B)) { pl.punch_n = 0; player_set_act(ACT_PUNCH); return; }
    if (pressed(BT_Z) || held(BT_Z)) { player_set_act(ACT_CROUCH); return; }
    if (int_mag > 0.1f) { player_set_act(ACT_WALK); return; }
    decel_ground(30);
    if (ground_step() == GSTEP_LEFT) enter_fall();
    pl.sleep_t++;
}

static void act_walk(void)
{
    float d;
    if (ground_common(1)) return;
    if (pressed(BT_B) && try_interact_b()) return;
    if (pressed(BT_B)) {
        if (pl.fwd > 6.5f) {
            pl.vel.y = 7.0f;
            pl.fwd = minf(pl.fwd + 4.5f, 15.0f);
            sfx(SFX_DIVE);
            player_set_act(ACT_DIVE);
            set_vel_from_fwd();
            return;
        }
        pl.punch_n = 0;
        player_set_act(ACT_PUNCH);
        return;
    }
    if (pressed(BT_Z)) {
        if (pl.fwd > 5.0f) { player_set_act(ACT_CROUCH_SLIDE); return; }
        player_set_act(ACT_CROUCH);
        return;
    }
    d = fabsf(angle_diff(int_yaw, pl.yaw));
    if (int_mag > 0.5f && pl.fwd > 6.0f && d > 2.1f && !(floor_flags() & SF_SLIPPERY)) {
        player_set_act(ACT_SKID);
        sfx(SFX_SKID);
        return;
    }
    if (int_mag < 0.05f) {
        decel_ground(28);
        if (pl.fwd <= 0.05f && v3len(v3(pl.vel.x, 0, pl.vel.z)) < 0.1f) { player_set_act(ACT_IDLE); }
    } else {
        walk_speed();
    }
    switch (ground_step()) {
    case GSTEP_LEFT: enter_fall(); break;
    case GSTEP_WALL:
        if (v3dot(yaw_dir(pl.yaw), pl.wall_n) < -0.7f) pl.fwd = minf(pl.fwd, 2.0f);
        break;
    case GSTEP_BLOCKED: pl.fwd = 0; break;
    }
    pl.anim += pl.fwd * DT * 1.7f + DT * 2;
    pl.step_t += pl.fwd * DT;
    if (pl.step_t > 1.4f) { pl.step_t = 0; sfx_ex(SFX_STEP, 0.6f, 0.9f + frand() * 0.2f); }
    pl.sleep_t = 0;
}

static void act_skid(void)
{
    if (pressed(BT_A)) { do_jump(ACT_SIDEFLIP); return; }
    pl.fwd = approachf(pl.fwd, 0, 30 * DT);
    set_vel_from_fwd();
    if (ground_step() == GSTEP_LEFT) { enter_fall(); return; }
    if (pl.fwd <= 0.01f) {
        pl.yaw = int_yaw;
        pl.fwd = 0;
        player_set_act(int_mag > 0.1f ? ACT_WALK : ACT_IDLE);
    }
}

static void act_landing(void)
{
    pl.chain_timer++;
    if (ground_common(1)) return;
    if (pressed(BT_B)) { if (try_interact_b()) return; pl.punch_n = 0; player_set_act(ACT_PUNCH); return; }
    if (pressed(BT_Z)) { player_set_act(pl.fwd > 5 ? ACT_CROUCH_SLIDE : ACT_CROUCH); return; }
    if (int_mag > 0.1f && pl.act_t > 1) walk_speed();
    else decel_ground(30);
    if (ground_step() == GSTEP_LEFT) { enter_fall(); return; }
    if (pl.act_t > 7) {
        player_set_act(int_mag > 0.1f ? ACT_WALK : ACT_IDLE);
        /* keep the chain window alive a bit after landing */
    }
}

static void act_crouch(void)
{
    if (ground_common(0)) return;
    if (pressed(BT_A)) { do_jump(ACT_BACKFLIP); return; }
    if (!held(BT_Z)) { player_set_act(ACT_IDLE); return; }
    if (int_mag > 0.3f && pl.act_t > 6) { player_set_act(ACT_CRAWL); return; }
    decel_ground(40);
    if (ground_step() == GSTEP_LEFT) enter_fall();
}

static void act_crawl(void)
{
    if (ground_common(0)) return;
    if (pressed(BT_A)) { do_jump(ACT_BACKFLIP); return; }
    if (!held(BT_Z)) { player_set_act(int_mag > 0.1f ? ACT_WALK : ACT_IDLE); return; }
    if (int_mag < 0.1f) { player_set_act(ACT_CROUCH); pl.act_t = 7; return; }
    pl.yaw = approach_angle(pl.yaw, int_yaw, 6 * DT);
    pl.fwd = 2.5f * int_mag;
    set_vel_from_fwd();
    if (ground_step() == GSTEP_LEFT) enter_fall();
    pl.anim += DT * 6;
}

static void act_crouch_slide(void)
{
    if (ground_common(0)) return;
    if (pressed(BT_A)) { do_jump(pl.fwd > 3.0f ? ACT_LONGJUMP : ACT_BACKFLIP); return; }
    if (pressed(BT_B)) { begin_belly_slide(pl.fwd + 2); sfx(SFX_DIVE); return; }
    pl.yaw = approach_angle(pl.yaw, int_yaw, 1.5f * DT * int_mag);
    pl.fwd = approachf(pl.fwd, 0, 9 * DT);
    set_vel_from_fwd();
    if (ground_step() == GSTEP_LEFT) { enter_fall(); return; }
    if (pl.fwd < 1.0f || pl.act_t > 40) player_set_act(held(BT_Z) ? ACT_CROUCH : ACT_IDLE);
}

static void act_punch(void)
{
    if (ground_common(0)) return;
    if (pressed(BT_A)) { chain_jump(); return; }
    if (pl.act_t == 1) { sfx(pl.punch_n == 2 ? SFX_KICK : SFX_PUNCH); if (pl.fwd < 2) pl.fwd = 2.5f; }
    if (pressed(BT_B) && pl.act_t > 4 && pl.punch_n < 2) pl.act_s = 1;   /* queue next */
    pl.fwd = approachf(pl.fwd, 0, 15 * DT);
    set_vel_from_fwd();
    if (ground_step() == GSTEP_LEFT) { enter_fall(); return; }
    if (pl.act_t >= (pl.punch_n == 2 ? 20 : 14)) {
        if (pl.act_s == 1) {
            int n = pl.punch_n + 1;
            player_set_act(ACT_PUNCH);
            pl.punch_n = n;
        } else player_set_act(int_mag > 0.1f ? ACT_WALK : ACT_IDLE);
    }
}

static void act_belly_slide(void)
{
    vec3 n = pl.floor ? pl.floor->n : v3(0, 1, 0);
    int f = floor_flags();
    float sp, fric, maxsp, steer;
    vec3 hv;
    if (pl.floor && (pl.floor->flags & SF_BURN)) { ground_common(0); return; }
    if (pressed(BT_A)) {
        sp = v3len(v3(pl.vel.x, 0, pl.vel.z));
        pl.fwd = minf(sp, 12.0f);
        if (sp > 0.5f) pl.yaw = atan2f(pl.vel.x, pl.vel.z);
        do_jump(ACT_ROLLOUT);
        return;
    }
    /* gravity along slope */
    pl.vel.x += n.x * GRAV * 0.9f * DT;
    pl.vel.z += n.z * GRAV * 0.9f * DT;
    hv = v3(pl.vel.x, 0, pl.vel.z);
    sp = v3len(hv);
    fric = 6.0f;
    maxsp = 18.0f;
    if (f & SF_SLIPPERY) fric = 1.2f;
    if (f & SF_SLIDE) { fric = 0.6f; maxsp = 24.0f; }
    if (!floor_is_slidey() && n.y > 0.95f) fric *= 1.5f;
    /* steering */
    steer = (f & SF_SLIDE) ? 2.2f : 1.5f;
    if (sp > 0.3f && int_mag > 0.1f) {
        float vy = atan2f(pl.vel.x, pl.vel.z);
        float nd = approach_angle(vy, int_yaw, steer * int_mag * DT);
        float along = cosf(angle_diff(int_yaw, vy));
        pl.vel.x = sinf(nd) * sp;
        pl.vel.z = cosf(nd) * sp;
        if (along > 0) sp += along * int_mag * 2.0f * DT;
    }
    sp = approachf(sp, 0, fric * DT);
    if (sp > maxsp) sp = maxsp;
    if (v3len(v3(pl.vel.x, 0, pl.vel.z)) > 1e-4f) {
        hv = v3norm(v3(pl.vel.x, 0, pl.vel.z));
        pl.vel.x = hv.x * sp;
        pl.vel.z = hv.z * sp;
        if (sp > 0.3f) pl.yaw = approach_angle(pl.yaw, atan2f(pl.vel.x, pl.vel.z), 10 * DT);
    }
    pl.fwd = sp;
    pl.slide_snd += sp * DT;
    if (pl.slide_snd > 3.0f) { pl.slide_snd = 0; sfx_ex(SFX_SLIDE, 0.5f, 1.0f); }
    switch (ground_step()) {
    case GSTEP_LEFT:
        pl.fwd = sp;
        player_set_act(ACT_FALL);
        pl.act_s = 1;   /* fall in belly pose */
        return;
    case GSTEP_WALL: {
        float into = v3dot(v3(pl.vel.x, 0, pl.vel.z), pl.wall_n);
        if (into < 0) {
            pl.vel.x -= pl.wall_n.x * into * 1.2f;
            pl.vel.z -= pl.wall_n.z * into * 1.2f;
        }
        break;
    }
    case GSTEP_BLOCKED:
        pl.vel.x = pl.vel.z = 0;
        break;
    }
    if (sp < 0.6f && !floor_is_slidey() && pl.act_t > 10) {
        pl.fwd = 0;
        player_set_act(ACT_IDLE);
    }
}

static void act_knockback(void)
{
    pl.fwd = approachf(pl.fwd, 0, 14 * DT);
    set_vel_from_fwd();
    if (ground_step() == GSTEP_LEFT) { player_set_act(ACT_FALL); return; }
    if (pl.act_t > 30) player_set_act(ACT_IDLE);
}

static void act_gpound_land(void)
{
    if (pl.act_t == 0) {
        sfx(SFX_POUND);
        cam.shake = 0.3f;
        fx_spawn(OT_FX_SMOKE, pl.pos, 1.2f);
    }
    pl.fwd = 0;
    pl.vel.x = pl.vel.z = 0;
    if (pl.act_t > 6 && pressed(BT_A)) { chain_jump(); return; }
    if (pl.act_t > 18) player_set_act(ACT_IDLE);
}

/* ------------------------------------------------------------------ holding / interaction */
static void drop_held(int throwit)
{
    Obj *o;
    if (pl.held < 0) return;
    o = &objs[pl.held];
    o->flags &= ~OF_HELD;
    if (throwit) {
        vec3 d = yaw_dir(pl.yaw);
        float spd = 12.0f;
        o->vel = v3(d.x * spd + pl.vel.x * 0.3f, 7.0f, d.z * spd + pl.vel.z * 0.3f);
        o->state = 100;   /* generic "thrown" state */
        o->timer = 0;
        sfx(SFX_THROW);
    } else {
        vec3 d = yaw_dir(pl.yaw);
        o->pos = v3mad(pl.pos, d, 1.0f);
        o->pos.y = pl.pos.y + 0.2f;
        o->vel = v3(0, 0, 0);
        o->state = 101;   /* dropped */
        o->timer = 0;
    }
    pl.held = -1;
}

int objs_try_grab(void);    /* objects.c */
int objs_try_talk(void);

static int try_interact_b(void)
{
    int g;
    if (objs_try_talk()) return 1;
    g = objs_try_grab();
    if (g >= 0) {
        Obj *o = &objs[g];
        pl.held = g;
        o->flags |= OF_HELD;
        sfx(SFX_GRAB);
        player_set_act(ACT_HOLD_IDLE);
        pl.fwd = 0;
        return 1;
    }
    return 0;
}

static void act_hold(void)
{
    float maxs = 6.5f;
    if (pl.held < 0 || !objs[pl.held].active) { pl.held = -1; player_set_act(ACT_IDLE); return; }
    if (floor_is_slidey()) { drop_held(0); begin_belly_slide(pl.fwd); return; }
    if (pressed(BT_A)) { do_jump(ACT_HOLD_JUMP); return; }
    if (pressed(BT_B)) { player_set_act(ACT_THROW); return; }
    if (pressed(BT_Z)) { drop_held(0); player_set_act(ACT_IDLE); return; }
    if (int_mag > 0.1f) {
        pl.act = ACT_HOLD_WALK;
        pl.yaw = approach_angle(pl.yaw, int_yaw, 8 * DT);
        pl.fwd = approachf(pl.fwd, maxs * int_mag, 14 * DT);
        pl.anim += pl.fwd * DT * 1.7f;
    } else {
        pl.act = ACT_HOLD_IDLE;
        pl.fwd = approachf(pl.fwd, 0, 25 * DT);
    }
    set_vel_from_fwd();
    if (ground_step() == GSTEP_LEFT) player_set_act(ACT_HOLD_FALL);
}

static void act_throw(void)
{
    pl.fwd = approachf(pl.fwd, 0, 20 * DT);
    set_vel_from_fwd();
    if (pl.act_t == 4) drop_held(1);
    if (ground_step() == GSTEP_LEFT) { enter_fall(); return; }
    if (pl.act_t > 14) player_set_act(ACT_IDLE);
}

static void air_control(int allow_turn, float turn_rate)
{
    float d = angle_diff(int_yaw, pl.yaw);
    float fc = cosf(d) * int_mag, sc = sinf(d) * int_mag;
    vec3 f, r;
    float cap = RUN_SPEED;
    if (pl.act == ACT_LONGJUMP || pl.act == ACT_DIVE || pl.act == ACT_ROLLOUT) cap = 15.0f;
    if (fc > 0) { if (pl.fwd < cap) pl.fwd = minf(pl.fwd + fc * 13.5f * DT, cap); }
    else pl.fwd += fc * 13.5f * DT;
    if (allow_turn) pl.yaw = approach_angle(pl.yaw, int_yaw, turn_rate * int_mag * DT);
    if (pl.fwd > cap) pl.fwd -= 6 * DT;
    if (pl.fwd < -RUN_SPEED * 0.6f) pl.fwd += 8 * DT;
    if (int_mag < 0.05f) pl.fwd = approachf(pl.fwd, 0, 2.5f * DT);
    f = yaw_dir(pl.yaw);
    r = v3(f.z, 0, -f.x);   /* yaw - 90deg : screen-consistent side */
    pl.vel.x = f.x * pl.fwd + r.x * sc * 3.0f;
    pl.vel.z = f.z * pl.fwd + r.z * sc * 3.0f;
}

static int try_ledge(void)
{
    vec3 d = yaw_dir(pl.yaw);
    vec3 probe = v3mad(pl.pos, d, RAD_T + 0.4f);
    Surf *s;
    float top = col_floor(probe.x, pl.pos.y + 2.1f, probe.z, &s), c;
    if (!s || s->n.y < 0.85f) return 0;
    if (top < pl.pos.y + 1.0f || top > pl.pos.y + 1.95f) return 0;
    /* there must be room to stand on the ledge (also rejects tops hidden right under a ceiling) */
    c = col_ceil(probe.x, top - 0.05f, probe.z, NULL);
    if (c - top < 1.7f) return 0;
    c = col_ceil(pl.pos.x, pl.pos.y + 0.8f, pl.pos.z, NULL);
    if (c < top + 0.5f) return 0;
    if (s->owner >= 0) return 0;
    pl.ledge_pos = v3(probe.x, top, probe.z);
    pl.pos.y = top - 1.55f;
    pl.vel = v3(0, 0, 0);
    pl.fwd = 0;
    sfx(SFX_LEDGE);
    player_set_act(ACT_LEDGE_HANG);
    return 1;
}

static void land_damage(void)
{
    float fall = pl.peak_y - pl.pos.y;
    if (lv.side_view) return;   /* TTY WORLD 1-1: like the old 2D games, no fall damage */
    if (fall > 11.0f && !(floor_flags() & SF_SNOW) && pl.act != ACT_TRAMP) {
        pl.health -= fall > 20 ? 4 : 3;
        sfx(SFX_HURT);
        sfx(SFX_THUD);
        cam.shake = 0.4f;
        if (pl.health <= 0) { pl.health = 0; player_set_act(ACT_DEATH); return; }
        player_set_act(ACT_KNOCKBACK);
        pl.fwd = 0;
        pl.act_t = 5;
    }
}

/* returns 1 if the action changed */
static int air_common(int land_act, int can_wallkick, int can_dive, int can_pound)
{
    int r;
    /* rocket pack: press A in mid-air to ignite (not while lining up a trampoline bounce) */
    if (pl.rocket_t > 0 && pressed(BT_A) && pl.wall_timer == 0 && pl.act_t > 3 && pl.held < 0 &&
        !(pl.act == ACT_TRAMP && pl.vel.y < 0)) {
        enter_fly();
        return 1;
    }
    if (can_pound && pressed(BT_Z)) {
        player_set_act(ACT_GPOUND);
        pl.vel = v3(0, 0, 0);
        pl.fwd = 0;
        sfx(SFX_BACKFLIP);
        return 1;
    }
    if (can_dive && pressed(BT_B)) {
        if (objs_try_grab() >= 0 && 0) return 1;
        if (pl.fwd > 5.0f || pl.act == ACT_LONGJUMP) {
            pl.fwd = minf(pl.fwd + 4.0f, 15.0f);
            pl.vel.y = maxf(pl.vel.y, 3.0f);
            sfx(SFX_DIVE);
            player_set_act(ACT_DIVE);
            return 1;
        } else if (!pl.kicked) {
            pl.kicked = 1;
            pl.vel.y = maxf(pl.vel.y, 5.0f);
            sfx(SFX_KICK);
            player_set_act(ACT_JUMPKICK);
            return 1;
        }
    }
    if (can_wallkick && pl.wall_timer > 0 && pressed(BT_A)) {
        vec3 d = yaw_dir(pl.yaw);
        vec3 rd = v3sub(d, v3scale(pl.wall_n, 2 * v3dot(d, pl.wall_n)));
        pl.yaw = atan2f(rd.x, rd.z);
        pl.fwd = 7.5f;
        pl.vel.y = 19.0f;
        pl.wall_timer = 0;
        pl.kicked = 0;
        sfx(SFX_WALLKICK);
        fx_spawn(OT_FX_SMOKE, v3mad(pl.pos, pl.wall_n, -0.3f), 0.5f);
        player_set_act(ACT_WALLKICK);
        return 1;
    }
    r = air_step();
    if (pl.pos.y > pl.peak_y) pl.peak_y = pl.pos.y;
    if (r == ASTEP_LAND) {
        land_damage();
        if (pl.act == ACT_DEATH || pl.act == ACT_KNOCKBACK) return 1;
        pl.vel.y = 0;
        if (land_act == ACT_LANDING) {
            pl.chain_timer = 0;
            pl.jump_chain = pl.act == ACT_JUMP ? 1 : (pl.act == ACT_DJUMP ? 2 : 0);
            sfx_ex(SFX_LAND, 0.8f, 1.0f);
        }
        if (land_act == ACT_BELLY_SLIDE) begin_belly_slide(pl.fwd);
        else player_set_act(land_act);
        if (floor_is_slidey() && land_act != ACT_BELLY_SLIDE && pl.act != ACT_GPOUND_LAND) begin_belly_slide(maxf(pl.fwd, 1));
        return 1;
    }
    if (r == ASTEP_WALL) {
        float into = v3dot(yaw_dir(pl.yaw), pl.wall_n);
        if (into < -0.5f && pl.fwd > 3.0f) {
            if (pl.vel.y < 0 && try_ledge()) return 1;
            if (pl.fwd > 13.5f) {
                /* bonk */
                pl.fwd = -3;
                pl.vel.y = minf(pl.vel.y, 3);
                sfx(SFX_THUD);
                player_set_act(ACT_AIR_KNOCK);
                return 1;
            }
            if (can_wallkick) {
                pl.wall_timer = 10;
                pl.fwd = 0;
            } else {
                pl.fwd = 0;
            }
        } else if (into < -0.3f && pl.vel.y < 0 && pl.fwd >= 0) {
            if (try_ledge()) return 1;
        }
    }
    if (pl.wall_timer > 0) pl.wall_timer--;
    return 0;
}

static void act_jump(void)
{
    int turn = pl.act == ACT_JUMP || pl.act == ACT_DJUMP || pl.act == ACT_FALL || pl.act == ACT_BOUNCE || pl.act == ACT_TRAMP;
    int wk = pl.act != ACT_BACKFLIP;
    if (pl.act == ACT_JUMP || pl.act == ACT_DJUMP || pl.act == ACT_HOLD_JUMP) {
        if (!held(BT_A) && pl.vel.y > 6.0f) pl.vel.y = 6.0f;
    }
    if (pl.act == ACT_BOUNCE && !held(BT_A) && pl.vel.y > 8.0f) pl.vel.y = 8.0f;
    air_control(turn, pl.act == ACT_FALL ? 2.5f : 2.0f);
    apply_gravity(pl.act == ACT_TJUMP ? 0.95f : 1.0f);
    air_common(pl.act == ACT_FALL && pl.act_s == 1 ? ACT_BELLY_SLIDE : ACT_LANDING, wk, 1, 1);
}

static void act_longjump(void)
{
    air_control(0, 0);
    apply_gravity(0.85f);
    air_common(ACT_LANDING, 1, 0, 1);
}

static void act_dive(void)
{
    air_control(0, 0);
    apply_gravity(1.0f);
    pl.fwd = approachf(pl.fwd, 4, 1.0f * DT);
    air_common(ACT_BELLY_SLIDE, 0, 0, 0);
}

static void act_jumpkick(void)
{
    air_control(0, 0);
    apply_gravity(1.0f);
    air_common(ACT_LANDING, 0, 0, 1);
}

static void act_gpound(void)
{
    if (pl.act_s == 0) {
        pl.vel = v3(0, 0, 0);
        if (pl.act_t > 14) { pl.act_s = 1; pl.vel.y = -26.0f; }
        return;
    }
    pl.vel.y = -26.0f;
    pl.vel.x = pl.vel.z = 0;
    if (air_step() == ASTEP_LAND) {
        float fall = pl.peak_y - pl.pos.y;
        pl.peak_y = pl.pos.y + minf(fall, 10.9f);   /* ground pound never deals fall damage */
        player_set_act(ACT_GPOUND_LAND);
    }
}

static void act_air_knock(void)
{
    pl.vel.x = yaw_dir(pl.yaw).x * pl.fwd;
    pl.vel.z = yaw_dir(pl.yaw).z * pl.fwd;
    apply_gravity(1.0f);
    if (air_step() == ASTEP_LAND) {
        land_damage();
        if (pl.act == ACT_DEATH) return;
        player_set_act(ACT_KNOCKBACK);
        pl.fwd = -3;
        pl.act_t = 15;
    }
}

static void act_burn_jump(void)
{
    air_control(1, 1.5f);
    apply_gravity(1.0f);
    if (pl.act_t % 6 == 0) fx_spawn(OT_FX_SMOKE, v3add(pl.pos, v3(0, 0.5f, 0)), 0.4f);
    if (air_step() == ASTEP_LAND) {
        if (pl.floor && (pl.floor->flags & SF_BURN)) {
            ground_common(0);
        } else player_set_act(ACT_LANDING);
    }
}

/* ---- trampoline: each well-timed A press (just before touching it) bounces Tux higher ---- */
static void tramp_bounce(Obj *t)
{
    static const float vy[4] = { 13.0f, 21.0f, 30.0f, 44.0f };
    int good = pl.a_age <= 10;
    if (pl.act != ACT_TRAMP) pl.tramp_combo = 0;
    if (pl.act == ACT_GPOUND) pl.tramp_combo = pl.tramp_combo < 1 ? 2 : 3;
    else if (good) pl.tramp_combo = pl.tramp_combo < 3 ? pl.tramp_combo + 1 : 3;
    else pl.tramp_combo = 0;
    if (pl.held >= 0) pl.tramp_combo = 0;
    pl.vel.y = vy[pl.tramp_combo];
    pl.fwd *= 0.5f;
    pl.vel.x *= 0.5f; pl.vel.z *= 0.5f;
    pl.peak_y = pl.pos.y;
    pl.kicked = 0;
    pl.a_age = 99;
    t->f[0] = 1.0f;   /* squash the mat */
    sfx_ex(SFX_BOUNCE, 0.9f, 0.7f + pl.tramp_combo * 0.2f);
    if (pl.tramp_combo == 3) { sfx(SFX_JUMP3); fx_spawn(OT_FX_SPARKLE, pl.pos, 1.0f); }
    if (pl.held >= 0) { player_set_act(ACT_HOLD_JUMP); return; }
    player_set_act(ACT_TRAMP);
}

/* ---- rocket pack flight: hold A to fire the rocket, stick steers (forward = nose down) ---- */
static void enter_fly(void)
{
    float hs = v3len(v3(pl.vel.x, 0, pl.vel.z));
    if (hs > 0.5f) pl.yaw = atan2f(pl.vel.x, pl.vel.z);
    pl.vel.y = maxf(pl.vel.y, 4.0f);
    pl.pitch = 0.25f;
    pl.fwd = hs;
    sfx(SFX_BLAST);
    cam.shake = maxf(cam.shake, 0.15f);
    fx_spawn(OT_FX_SMOKE, pl.pos, 1.0f);
    player_set_act(ACT_FLY);
}

int player_rocket_firing(void) { return (pl.act == ACT_FLY && held(BT_A) && !pl.no_ctrl) || pl.act == ACT_SKYFLY || pl.act == ACT_SPACEFLY; }

static void act_fly(void)
{
    int r, thrust;
    vec3 d;
    if (pl.rocket_t <= 0 || pressed(BT_Z) || pressed(BT_B)) {
        pl.fwd = v3len(v3(pl.vel.x, 0, pl.vel.z)) * 0.6f;
        pl.pitch = 0;
        player_set_act(ACT_FALL);
        return;
    }
    thrust = held(BT_A) && !pl.no_ctrl;
    pl.pitch = approachf(pl.pitch, -in.sy * 0.9f + (thrust ? 0.1f : -0.3f), 1.8f * DT);
    pl.yaw = wrap_angle(pl.yaw - in.sx * 2.0f * DT);
    pl.roll = approachf(pl.roll, in.sx * 0.5f, 3 * DT);
    d = v3(sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch), cosf(pl.yaw) * cosf(pl.pitch));
    if (thrust) {
        /* the rocket carries Tux's weight while it burns: level flight at pitch 0 */
        pl.vel = v3mad(pl.vel, d, 32.0f * DT);
        pl.vel = v3scale(pl.vel, 1 - 1.5f * DT);
        pl.act_s = 0;
        if (pl.act_t % 3 == 0) {
            vec3 nz = v3add(v3mad(pl.pos, yaw_dir(pl.yaw), -0.6f), v3(0, 0.5f, 0));
            fx_spawn(OT_FX_SMOKE, nz, 0.45f);
        }
        if (pl.act_t % 5 == 0) sfx_ex(SFX_FIRE, 0.35f, 0.6f);
    } else {
        /* engine idle: Tux arcs down; after a while he just falls */
        pl.vel.y -= GRAV * 0.6f * DT;
        pl.vel.x *= 1 - 0.6f * DT; pl.vel.z *= 1 - 0.6f * DT;
        if (++pl.act_s > 50) {
            pl.fwd = v3len(v3(pl.vel.x, 0, pl.vel.z));
            pl.pitch = 0;
            player_set_act(ACT_FALL);
            return;
        }
    }
    if (pl.vel.y < TERM_VEL) pl.vel.y = TERM_VEL;
    pl.fwd = v3len(v3(pl.vel.x, 0, pl.vel.z));
    r = air_step();
    pl.peak_y = pl.pos.y;
    if (r == ASTEP_LAND) {
        pl.pitch = 0; pl.roll = 0;
        if (pl.fwd > 9) begin_belly_slide(pl.fwd * 0.7f);
        else player_set_act(ACT_LANDING);
        return;
    }
    if (r == ASTEP_WALL) {
        pl.pitch = 0; pl.roll = 0;
        pl.fwd = -3;
        pl.vel.y = 0;
        sfx(SFX_THUD);
        player_set_act(ACT_AIR_KNOCK);
        return;
    }
    if (r == ASTEP_CEIL) { pl.pitch = -0.3f; if (pl.vel.y > 0) pl.vel.y = 0; }
}

static void act_hold_air(void)
{
    if (pl.held < 0) { player_set_act(ACT_FALL); return; }
    if (pl.act == ACT_HOLD_JUMP && !held(BT_A) && pl.vel.y > 6.0f) pl.vel.y = 6.0f;
    if (pressed(BT_B)) { drop_held(1); player_set_act(ACT_FALL); return; }
    air_control(1, 2.0f);
    apply_gravity(1.0f);
    if (air_step() == ASTEP_LAND) {
        land_damage();
        if (pl.act == ACT_DEATH || pl.act == ACT_KNOCKBACK) return;
        player_set_act(ACT_HOLD_IDLE);
    }
    if (pl.pos.y > pl.peak_y) pl.peak_y = pl.pos.y;
}

/* ------------------------------------------------------------------ ledge / pole */
static void act_ledge_hang(void)
{
    vec3 d = yaw_dir(pl.yaw);
    float back;
    pl.vel = v3(0, 0, 0);
    back = cosf(angle_diff(int_yaw, pl.yaw)) * int_mag;
    if (pl.act_t > 8 && (pressed(BT_A) || back > 0.6f)) { player_set_act(ACT_LEDGE_CLIMB); sfx(SFX_LEDGE); return; }
    if (pl.act_t > 8 && (pressed(BT_Z) || back < -0.6f)) {
        pl.pos = v3mad(pl.pos, d, -0.3f);
        pl.fwd = 0;
        player_set_act(ACT_FALL);
        pl.peak_y = pl.pos.y;
        return;
    }
}

static void act_ledge_climb(void)
{
    vec3 d = yaw_dir(pl.yaw);
    pl.vel = v3(0, 0, 0);
    if (pl.act_t >= 18) {
        Surf *s;
        vec3 np = v3mad(pl.ledge_pos, d, 0.35f);
        float fy = col_floor(np.x, pl.ledge_pos.y + 0.5f, np.z, &s);
        if (s) np.y = fy;
        else np = pl.ledge_pos;
        pl.pos = np;
        pl.floor = s;
        pl.floor_y = np.y;
        player_set_act(ACT_IDLE);
    }
}

static void act_pole(void)
{
    Obj *o;
    float top;
    if (pl.pole < 0) { player_set_act(ACT_FALL); return; }
    o = &objs[pl.pole];
    top = o->height - 0.8f;
    if (pressed(BT_A)) {
        pl.yaw = wrap_angle(pl.act_f + PI_F);
        pl.pos = v3mad(pl.pos, yaw_dir(pl.yaw), 0.4f);
        pl.fwd = 5.0f;
        pl.vel.y = 15.0f;
        pl.pole = -1;
        sfx(SFX_JUMP);
        player_set_act(ACT_JUMP);
        return;
    }
    if (pressed(BT_Z)) {
        pl.yaw = wrap_angle(pl.act_f + PI_F);
        pl.pos = v3mad(pl.pos, yaw_dir(pl.yaw), 0.5f);
        pl.pole = -1;
        pl.fwd = 0;
        player_set_act(ACT_FALL);
        return;
    }
    pl.pole_h += in.sy * (in.sy > 0 ? 3.0f : 5.0f) * DT;
    if (pl.pole_h > top) pl.pole_h = top;
    if (pl.pole_h < 0) {
        pl.pole_h = 0;
        if (in.sy < -0.5f) { pl.pole = -1; player_set_act(ACT_IDLE); pl.yaw = wrap_angle(pl.act_f); return; }
    }
    pl.act_f = wrap_angle(pl.act_f - in.sx * 3.0f * DT);
    {
        vec3 off = v3scale(yaw_dir(pl.act_f + PI_F), 0.5f);
        pl.pos = v3add(v3(o->pos.x, o->pos.y + pl.pole_h, o->pos.z), off);
        pl.yaw = pl.act_f;
    }
    if (fabsf(in.sy) > 0.2f) pl.anim += DT * 6;
    pl.vel = v3(0, 0, 0);
    pl.peak_y = pl.pos.y;
}

/* ------------------------------------------------------------------ water */
static void act_swim(void)
{
    float wy = pl.water_y;
    float target_sp, sp;
    vec3 hv;
    int r;
    pl.peak_y = pl.pos.y;
    if (wy < -1e8f) { player_set_act(ACT_FALL); return; }
    /* surface */
    if (pl.pos.y > wy - 1.2f) {
        if (pl.boots && pressed(BT_A)) {
            pl.pos.y = wy;
            pl.vel.y = 9.0f;
            sfx(SFX_SPLASH);
            fx_spawn(OT_FX_BUBBLE, pl.pos, 1.0f);
            skate_enter(1);
            pl.vel.y = 9.0f;
            return;
        }
        if (pressed(BT_A) && !held(BT_Z)) {
            pl.vel.y = 15.0f;
            pl.pos.y = wy - 0.9f;   /* pop out of the water */
            pl.fwd = minf(v3len(v3(pl.vel.x, 0, pl.vel.z)), 7.0f);
            if (int_mag > 0.1f) pl.yaw = int_yaw;
            sfx(SFX_SPLASH);
            fx_spawn(OT_FX_BUBBLE, pl.pos, 1.0f);
            player_set_act(ACT_JUMP);
            return;
        }
    }
    /* dash (penguin torpedo) */
    if (pressed(BT_B) && pl.act_s != 2) {
        pl.act_s = 2;
        pl.act_f = 0.45f;
        sfx(SFX_SWIM);
        if (int_mag > 0.1f) pl.yaw = int_yaw;
    }
    if (pl.act_s == 2) {
        pl.act_f -= DT;
        if (pl.act_f <= 0) pl.act_s = 0;
    }
    if (int_mag > 0.1f) pl.yaw = approach_angle(pl.yaw, int_yaw, 4.0f * DT);
    target_sp = 6.5f * int_mag;
    if (held(BT_A)) target_sp = maxf(target_sp, 3.0f) + 2.0f;
    if (pl.act_s == 2) target_sp = 13.0f;
    hv = v3(pl.vel.x, 0, pl.vel.z);
    sp = v3len(hv);
    sp = approachf(sp, target_sp, (pl.act_s == 2 ? 40.0f : 10.0f) * DT);
    pl.vel.x = sinf(pl.yaw) * sp;
    pl.vel.z = cosf(pl.yaw) * sp;
    /* vertical */
    if (held(BT_A)) pl.vel.y = approachf(pl.vel.y, 4.5f, 14 * DT);
    else if (held(BT_Z)) pl.vel.y = approachf(pl.vel.y, -5.5f, 14 * DT);
    else pl.vel.y = approachf(pl.vel.y, pl.pos.y > wy - 1.6f ? 0.0f : -0.7f, 6 * DT);
    if (pl.act_s == 2 && held(BT_Z)) pl.vel.y = -6.0f;
    if (pressed(BT_A)) { sfx_ex(SFX_SWIM, 0.6f, 1.2f); pl.anim += 1.0f; }
    pl.pitch = approachf(pl.pitch, -pl.vel.y * 0.08f, 3 * DT);
    r = air_step();
    if (pl.pos.y > wy - 1.1f) { pl.pos.y = wy - 1.1f; if (pl.vel.y > 0) pl.vel.y = 0; }
    if (r == ASTEP_LAND) {
        pl.vel.y = 0;
        /* walk out of shallow water */
        if (pl.floor_y > wy - 1.25f) {
            pl.pitch = 0;
            pl.fwd = sp * 0.5f;
            player_set_act(int_mag > 0.1f ? ACT_WALK : ACT_IDLE);
            return;
        }
    }
    pl.anim += (sp * 0.3f + 1.0f) * DT * 2;
    /* breath (a hot spring heals instead) */
    if (water_warm_at(pl.pos.x, pl.pos.z)) {
        pl.breath_t += DT;
        if (pl.breath_t > 0.4f) { pl.breath_t = 0; if (pl.health < 8) { pl.health++; sfx_ex(SFX_COIN, 0.3f, 0.7f); } }
    } else if (pl.pos.y + 1.4f < wy) {
        pl.breath_t += DT;
        if (pl.breath_t > 3.0f) {
            pl.breath_t = 0;
            pl.health--;
            sfx(SFX_DROWN);
            if (pl.health <= 0) { pl.health = 0; player_set_act(ACT_DROWN); return; }
        }
        if (g_frame % 50 == 0) fx_spawn(OT_FX_BUBBLE, v3add(pl.pos, v3(0, 1.4f, 0)), 0.3f);
    } else {
        pl.breath_t += DT;
        if (pl.breath_t > 0.5f) { pl.breath_t = 0; if (pl.health < 8) pl.health++; }
    }
}

/* ------------------------------------------------------------------ dash boots (AURORA SEA) */
#define SK_BASE   21.0f       /* cruising speed with no gate power */
#define SK_POWER  1.8f        /* extra top speed per gate power level (0..5) */
#define SK_BOOST  9.0f

static float skate_max(void)
{
    float m = SK_BASE + SK_POWER * g_sea_power;
    if (pl.boost_t > 0) m += SK_BOOST;
    return m;
}

/* height Tux skims on: solid ground/ice, or the water surface (dash boots run on water) */
static float skate_surface(float x, float y, float z, Surf **fl, int *on_water)
{
    float fy = col_floor(x, y + 0.9f, z, fl);
    float wy = water_level_at(x, z, y);
    if (!*fl) fy = -1e9f;
    *on_water = 0;
    if (wy > -1e8f && wy >= fy - 0.02f) { *on_water = 1; *fl = NULL; return wy; }
    return fy;
}

static void skate_enter(int airborne)
{
    float hs = v3len(v3(pl.vel.x, 0, pl.vel.z));
    int a = pl.act;
    player_set_act(ACT_SKATE);
    pl.act_s = airborne;
    pl.fwd = maxf(hs, a == ACT_SWIM ? 6.0f : pl.fwd);
    pl.ground_vy = 0;
    pl.trick = 0;
    pl.pitch = 0;
}

static void skate_land(void)
{
    if (pl.trick == 1) {          /* landed mid-spin: wipe out */
        pl.fwd *= 0.35f;
        sfx(SFX_THUD);
        cam.shake = 0.25f;
        hud_message("Wipeout!", 60);
    } else if (pl.trick == 2) {
        if (pl.boost_t < 75) pl.boost_t = 75;
        sfx_ex(SFX_BLUECOIN, 0.8f, 1.2f);
        hud_message("TRICK! Speed boost!", 60);
    }
    if (pl.vel.y < -16) { pl.fwd *= 0.85f; cam.shake = 0.15f; }
    pl.trick = 0;
    pl.act_s = 0;
    pl.vel.y = 0;
    pl.ground_vy = 0;
    if (pl.water_y > -1e8f && pl.pos.y <= pl.water_y + 0.05f) {
        sfx(SFX_SPLASH);
        fx_spawn(OT_FX_SNOWPUFF, pl.pos, 1.2f);
        fx_spawn(OT_FX_SNOWPUFF, v3add(pl.pos, v3(0.4f, 0, 0.3f)), 1.0f);
    } else sfx_ex(SFX_LAND, 0.8f, 1.2f);
}

static void act_skate(void)
{
    float maxs = skate_max(), steer = pl.no_ctrl ? 0 : in.sx, throttle = pl.no_ctrl ? 0 : in.sy;
    int air = pl.act_s == 1, sub, nsub, onw = 0;
    vec3 want, start = pl.pos;
    Surf *fl = NULL;
    float sy = pl.pos.y;
    pl.peak_y = pl.pos.y;
    if (pl.no_ctrl) throttle = -1;   /* countdown / warps: hold still */
    /* steering ("tank" style, like a jet ski): left/right turns, back brakes, Z carves */
    if (!air) {
        float rate = 2.5f - 0.025f * pl.fwd;
        if (held(BT_Z)) { rate *= 1.7f; pl.fwd = approachf(pl.fwd, 0, 9 * DT); }
        pl.yaw = wrap_angle(pl.yaw - steer * rate * DT);
        pl.roll = approachf(pl.roll, steer * clampf(pl.fwd / 20.0f, 0, 1) * (held(BT_Z) ? 0.7f : 0.4f), 5 * DT);
        if (pl.floor && pl.floor->n.y < 0.98f) {   /* uphill slows, downhill speeds up */
            vec3 d = yaw_dir(pl.yaw);
            pl.fwd += (pl.floor->n.x * d.x + pl.floor->n.z * d.z) * 10.0f * DT;
        }
        if (throttle < -0.4f) pl.fwd = approachf(pl.fwd, 0, 28 * DT);
        else if (pl.fwd < maxs) pl.fwd = minf(pl.fwd + (pl.boost_t > 0 ? 40.0f : 14.0f - pl.fwd * 0.25f) * DT, maxs);
        else pl.fwd = approachf(pl.fwd, maxs, 8 * DT);
        if (pressed(BT_A)) {
            pl.act_s = 1; air = 1;
            pl.vel.y = maxf(pl.ground_vy, 0) + 11.5f;
            sfx(SFX_JUMP);
            fx_spawn(OT_FX_SNOWPUFF, pl.pos, 0.8f);
        } else if (pressed(BT_B) && pl.fwd < 3.0f && try_interact_b()) return;
    } else {
        pl.yaw = wrap_angle(pl.yaw - steer * 1.3f * DT);
        pl.roll = approachf(pl.roll, 0, 3 * DT);
        if (pressed(BT_B) && pl.trick == 0 && pl.vel.y > -6.0f) {   /* mid-air spin trick */
            pl.trick = 1; pl.trick_t = 0;
            sfx(SFX_BACKFLIP);
        }
        if (pl.trick == 1) {
            pl.trick_t += DT / 0.55f;
            if (pl.trick_t >= 1) { pl.trick = 2; pl.trick_t = 1; }
        }
        if (held(BT_Z)) pl.vel.y -= 30 * DT;   /* dive back to the water */
    }
    /* velocity drifts toward the facing direction: grip is weaker on the water */
    want = v3scale(yaw_dir(pl.yaw), pl.fwd);
    {
        float grip = air ? 4.0f : (pl.floor && (pl.floor->flags & SF_SLIPPERY) ? 30.0f : 55.0f);
        pl.vel.x = approachf(pl.vel.x, want.x, grip * DT * maxf(1.0f, fabsf(want.x - pl.vel.x) * 0.3f));
        pl.vel.z = approachf(pl.vel.z, want.z, grip * DT * maxf(1.0f, fabsf(want.z - pl.vel.z) * 0.3f));
    }
    if (air) {
        pl.vel.y -= GRAV * 0.85f * DT;
        if (pl.vel.y < TERM_VEL) pl.vel.y = TERM_VEL;
    }
    nsub = 1 + (int)(v3len(pl.vel) * DT / 0.2f);
    for (sub = 0; sub < nsub; sub++) {
        float dt = DT / nsub, cy;
        vec3 np = v3(pl.pos.x + pl.vel.x * dt, pl.pos.y + (air ? pl.vel.y * dt : 0), pl.pos.z + pl.vel.z * dt);
        vec3 w1, w2;
        int h1 = col_walls(&np, 0.6f, RAD_T, &w1, NULL), h2 = col_walls(&np, 1.3f, RAD_T, &w2, NULL);
        if (h1 || h2) {
            vec3 n = h2 ? w2 : w1, hv = v3(pl.vel.x, 0, pl.vel.z);
            float sp = v3len(hv), into = sp > 0.1f ? v3dot(v3scale(hv, 1.0f / sp), n) : 0;
            if (into < -0.6f && sp > 12.0f) {
                /* head-on crash: bounce back */
                pl.vel.x = n.x * 6; pl.vel.z = n.z * 6;
                pl.fwd = 2.0f;
                pl.yaw = atan2f(n.x, n.z);
                pl.boost_t = 0;
                sfx(SFX_THUD);
                cam.shake = 0.3f;
                fx_spawn(OT_FX_SMOKE, v3mad(np, n, -0.4f), 0.6f);
            } else if (into < 0) {
                /* glance off the wall */
                hv = v3sub(hv, v3scale(n, v3dot(hv, n)));
                pl.vel.x = hv.x; pl.vel.z = hv.z;
                pl.fwd *= 1.0f - 0.45f * -into;
                if (v3len(hv) > 1) pl.yaw = approach_angle(pl.yaw, atan2f(hv.x, hv.z), 0.2f);
            }
        }
        sy = skate_surface(np.x, np.y, np.z, &fl, &onw);
        if (!air) {
            if (sy > np.y + 0.9f || sy < -1e8f) {
                np.x = pl.pos.x; np.z = pl.pos.z;    /* too steep to climb: stop */
                pl.fwd *= 0.5f;
                sy = skate_surface(np.x, np.y, np.z, &fl, &onw);
            }
            {
                /* where Tux would be if he just flew on: when the surface drops away faster than
                 * that (a ramp lip, the whale's ridge, an edge) he takes off with the slope's speed */
                float yb = pl.pos.y + pl.ground_vy * dt - 0.5f * GRAV * 0.85f * dt * dt, rise = sy - pl.pos.y;
                if (sy < yb - 0.06f && rise < 0.01f) {
                    air = 1; pl.act_s = 1;
                    pl.vel.y = pl.ground_vy > 0 ? pl.ground_vy * 1.15f : pl.ground_vy;
                    np.y = yb;
                    if (pl.vel.y > 4) sfx_ex(SFX_JUMP2, 0.7f, 0.9f);
                } else {
                    /* a small step up (floe edge) does not count as climbing a slope */
                    pl.ground_vy = rise > 0.2f ? 0 : clampf(rise / dt, -40, 40);
                    np.y = sy;
                }
            }
        } else if (np.y <= sy && (pl.vel.y <= 0 || sy - np.y < 0.6f)) {
            np.y = sy;
            pl.pos = np;
            skate_land();
            air = 0;
        }
        cy = col_ceil(np.x, np.y + 0.8f, np.z, NULL);
        if (np.y + TUX_H > cy) { np.y = maxf(cy - TUX_H, sy); if (pl.vel.y > 0) pl.vel.y = 0; }
        pl.pos = np;
    }
    pl.floor = air ? NULL : fl;
    pl.floor_y = sy;
    if (!air) pl.vel.y = 0;
    sea_player_moved(start, pl.pos);
    /* sinking: dash boots only skim the water while moving */
    if (!air && onw && pl.fwd < 2.0f && throttle < -0.4f && !pl.no_ctrl && pl.act == ACT_SKATE) {
        pl.pos.y = pl.water_y - 1.15f;
        pl.vel = v3(0, 0, 0);
        sfx(SFX_SPLASH);
        player_set_act(ACT_SWIM);
        pl.breath_t = 0;
        return;
    }
    if (pl.boost_t > 0) pl.boost_t--;
    /* spray and sounds */
    if (!air && pl.fwd > 6) {
        pl.slide_snd += DT * pl.fwd;
        if (pl.slide_snd > 9) { pl.slide_snd = 0; sfx_ex(onw ? SFX_SWIM : SFX_SLIDE, 0.35f, 0.8f + pl.fwd * 0.02f); }
        if (onw && g_frame % 3 == 0) {
            vec3 b = v3mad(pl.pos, yaw_dir(pl.yaw), -0.6f);
            fx_spawn(OT_FX_SNOWPUFF, v3add(b, v3(frands() * 0.4f, 0.1f, frands() * 0.4f)), 0.35f + 0.012f * pl.fwd);
        }
    }
    pl.anim += pl.fwd * DT * 0.6f + DT;
}

/* ------------------------------------------------------------------ sky flight (FROSTCLOUD SKYWAY) */
static void sky_enter(void)
{
    player_set_act(ACT_SKYFLY);
    pl.sky_s = 0; pl.sky_y = 0; pl.sky_vs = 0; pl.sky_vy = 0;
    pl.sky_dash = 1.0f;
    pl.pitch = 0; pl.roll = 0;
}

static void act_skyfly(void)
{
    float dir = sky_screen_dir(pl.sky_s), sx = pl.no_ctrl ? 0 : in.sx, sy = pl.no_ctrl ? 0 : in.sy;
    float mag = sqrtf(sx * sx + sy * sy);
    int dash;
    vec3 t;
    pl.rocket_t = 5000;
    if (mag > 1) { sx /= mag; sy /= mag; mag = 1; }
    dash = held(BT_A) && pl.sky_dash > 0.01f && mag > 0.2f;
    if (mag > 0.15f) {
        /* swing the velocity toward the stick direction: a floaty, banking flight */
        float target = (dash ? 25.0f : 12.5f) * mag, acc = dash ? 55.0f : 24.0f;
        float dvs = sx * dir / mag * target - pl.sky_vs, dvy = sy / mag * target - pl.sky_vy;
        float l = sqrtf(dvs * dvs + dvy * dvy), step = minf(l, acc * DT);
        if (l > 1e-4f) { pl.sky_vs += dvs / l * step; pl.sky_vy += dvy / l * step; }
    } else {
        /* let go: drift to a hover */
        pl.sky_vs *= 1 - 1.4f * DT;
        pl.sky_vy *= 1 - 1.4f * DT;
        pl.sky_vy += sinf(g_frame * 0.05f) * 0.6f * DT;
    }
    if (dash) {
        pl.sky_dash = maxf(pl.sky_dash - 0.28f * DT, 0);
        if (pl.act_t % 4 == 0) fx_spawn(OT_FX_SPARKLE, v3add(pl.pos, v3(0, 0.8f, 0)), 0.8f);
        if (pressed(BT_A)) sfx(SFX_BLAST);
    } else if (!held(BT_A)) pl.sky_dash = minf(pl.sky_dash + 0.1f * DT, 1);
    pl.sky_s += pl.sky_vs * DT;
    pl.sky_y += pl.sky_vy * DT;
    if (pl.sky_y != sky_clamp_y(pl.sky_y)) { pl.sky_y = sky_clamp_y(pl.sky_y); pl.sky_vy = 0; }
    pl.pos = sky_world(pl.sky_s, pl.sky_y);
    t = sky_tangent(pl.sky_s);
    pl.vel = v3add(v3scale(t, pl.sky_vs), v3(0, pl.sky_vy, 0));
    if (fabsf(pl.sky_vs) > 0.8f) {
        vec3 f = v3scale(t, pl.sky_vs > 0 ? 1.0f : -1.0f);
        pl.yaw = approach_angle(pl.yaw, atan2f(f.x, f.z), 9 * DT);
    }
    {
        float hs = fabsf(pl.sky_vs), want = (hs + fabsf(pl.sky_vy) > 2) ? atan2f(pl.sky_vy, hs) : 0;
        pl.pitch = approachf(pl.pitch, clampf(want, -1.3f, 1.3f), 4 * DT);
        pl.roll = approachf(pl.roll, clampf(sx * dir * 0.2f, -0.5f, 0.5f), 3 * DT);
    }
    pl.fwd = sqrtf(pl.sky_vs * pl.sky_vs + pl.sky_vy * pl.sky_vy);
    pl.floor = NULL;
    pl.peak_y = pl.pos.y;
    if (pl.act_t % 3 == 0 && pl.fwd > 3) fx_spawn(OT_FX_SMOKE, v3add(v3mad(pl.pos, yaw_dir(pl.yaw), -0.5f), v3(0, 0.6f, 0)), 0.35f);
    if (pl.act_t % 8 == 0 && pl.fwd > 3) sfx_ex(SFX_FIRE, 0.2f, 0.8f + pl.fwd * 0.02f);
    if (!pl.no_ctrl) sky_trail_push(pl.sky_s, pl.sky_y);
}

/* ------------------------------------------------------------------ space flight (STARDUST ORBIT) */
#define SP_CRUISE 24.0f
#define SP_BOOST  54.0f
#define SP_BRAKE  9.0f
#define SP_RING   78.0f
#define SP_LIMIT  330.0f

static void space_enter(void)
{
    player_set_act(ACT_SPACEFLY);
    pl.sp_speed = SP_CRUISE * 0.5f;
    pl.sp_stun = 0;
    pl.sp_turn = 0;
    pl.boost_t = 0;
    pl.pitch = 0; pl.roll = 0;
}

static vec3 space_dir(void)
{
    return v3(sinf(pl.yaw) * cosf(pl.pitch), sinf(pl.pitch), cosf(pl.yaw) * cosf(pl.pitch));
}

/* the rocket hit the planet, the station or a rock: glance off it */
static void space_crash(vec3 n)
{
    vec3 d = space_dir();
    float dn = v3dot(d, n);
    if (dn >= 0) return;
    d = v3norm(v3mad(d, n, -(dn < -0.25f ? 1.6f : 1.05f) * dn));
    pl.yaw = atan2f(d.x, d.z);
    pl.pitch = clampf(asinf(clampf(d.y, -1, 1)), -1.3f, 1.3f);
    if (dn < -0.25f) {
        if (pl.sp_speed > 30) {
            pl.sp_stun = 18;
            cam.shake = maxf(cam.shake, 0.35f);
            fx_spawn(OT_FX_SMOKE, pl.pos, 1.0f);
            sfx(SFX_THUD);
        } else sfx_ex(SFX_THUD, 0.5f, 1.3f);
        pl.sp_speed *= dn < -0.6f ? 0.4f : 0.7f;
        pl.boost_t = 0;
    }
}

static void act_spacefly(void)
{
    int ctl = !pl.no_ctrl && pl.sp_stun == 0;
    float sx = ctl ? in.sx : 0, sy = ctl ? in.sy : 0, target, rate;
    int boost = ctl && held(BT_A), brake = ctl && held(BT_B);
    vec3 d;
    pl.rocket_t = 5000;
    if (pl.sp_stun > 0) pl.sp_stun--;
    /* Z: a quick half loop to fly back the other way */
    if (ctl && pressed(BT_Z) && pl.sp_turn == 0) { pl.sp_turn = 24; sfx(SFX_BACKFLIP); }
    if (pl.sp_turn > 0) {
        pl.sp_turn--;
        pl.yaw = wrap_angle(pl.yaw + PI_F / 24);
        pl.pitch *= 0.92f;
        sx = 0;
    }
    /* steering: left/right turns and banks, forward dives, back climbs; let go and the nose levels out */
    {
        float turn = 2.2f - minf(pl.sp_speed, 80) * 0.008f;
        pl.yaw = wrap_angle(pl.yaw - sx * turn * DT);
        if (fabsf(sy) > 0.1f) pl.pitch = clampf(pl.pitch - sy * 1.8f * DT, -1.3f, 1.3f);
        else pl.pitch = approachf(pl.pitch, 0, 0.35f * DT);
        pl.roll = approachf(pl.roll, -sx * 0.8f, 4 * DT);   /* bank into the turn */
    }
    /* too far out: the rocket swings back toward the planet */
    {
        float r = v3len(pl.pos);
        if (r > SP_LIMIT) {
            vec3 to = v3scale(pl.pos, -1.0f / r);
            pl.yaw = approach_angle(pl.yaw, atan2f(to.x, to.z), 1.6f * DT);
            pl.pitch = approachf(pl.pitch, clampf(asinf(clampf(to.y, -1, 1)), -1.2f, 1.2f), 1.0f * DT);
            if (pl.act_t % 150 == 1) hud_message("Too far out! Heading back to orbit.", 120);
        }
    }
    /* speed: cruise all the time, A boosts, B brakes; boost rings shoot Tux forward */
    target = boost ? SP_BOOST : brake ? SP_BRAKE : SP_CRUISE;
    if (pl.boost_t > 0) { pl.boost_t--; target = maxf(target, SP_RING); }
    rate = pl.sp_speed < target ? (boost || pl.boost_t > 0 ? 75.0f : 30.0f) : (brake ? 60.0f : 25.0f);
    pl.sp_speed = approachf(pl.sp_speed, target, rate * DT);
    if (boost && pressed(BT_A)) { sfx(SFX_BLAST); cam.shake = maxf(cam.shake, 0.1f); }
    /* move, sweeping the path ahead so even full speed never tunnels through a wall */
    d = space_dir();
    pl.vel = v3scale(d, pl.sp_speed);
    {
        vec3 c0 = v3add(pl.pos, v3(0, 0.8f, 0)), n, wn;
        float l = pl.sp_speed * DT, t, fy, cy;
        Surf *s;
        if (col_ray(c0, v3mad(c0, d, l + 0.9f), &t, &n, 0)) {
            float go = maxf(t * (l + 0.9f) - 0.9f, 0);
            c0 = v3mad(c0, d, go);
            space_crash(n);
        } else c0 = v3mad(c0, d, l);
        pl.pos = v3sub(c0, v3(0, 0.8f, 0));
        if (col_walls(&pl.pos, 0.8f, 0.85f, &wn, NULL)) space_crash(wn);
        fy = col_floor(pl.pos.x, pl.pos.y + 0.8f, pl.pos.z, &s);
        if (s && pl.pos.y < fy + 0.05f) { pl.pos.y = fy + 0.05f; space_crash(s->n); }
        cy = col_ceil(pl.pos.x, pl.pos.y + 0.8f, pl.pos.z, &s);
        if (s && cy < pl.pos.y + 1.6f) { pl.pos.y = cy - 1.6f; space_crash(s->n); }
    }
    pl.fwd = pl.sp_speed;
    pl.floor = NULL;
    pl.peak_y = pl.pos.y;
    if (pl.act_t % 3 == 0) fx_spawn(OT_FX_SMOKE, v3add(v3mad(pl.pos, d, -0.7f), v3(0, 0.7f, 0)), boost ? 0.3f : 0.18f);
    if (boost && pl.act_t % 3 == 0) fx_spawn(OT_FX_SPARKLE, v3add(v3mad(pl.pos, d, -1.0f), v3(0, 0.8f, 0)), 0.9f);
    if (pl.act_t % 6 == 0) sfx_ex(SFX_FIRE, boost || pl.boost_t ? 0.4f : 0.22f, 0.7f + pl.sp_speed * 0.012f);
}

static void act_death(void)
{
    pl.vel.x = pl.vel.z = 0;
    if (pl.act == ACT_DROWN) {
        pl.vel.y = 1.0f;
        air_step();
        if (pl.pos.y > pl.water_y - 1.1f) pl.pos.y = pl.water_y - 1.1f;
    } else if (lv.space) {
        pl.vel = v3(0, 0, 0);
    } else if (player_in_air() || pl.pos.y > pl.floor_y + 0.05f) {
        apply_gravity(1.0f);
        if (air_step() == ASTEP_LAND) pl.vel.y = 0;
    }
    if (pl.act_t == 1) { music_play(MUS_DEATH); sfx_ex(SFX_SQUAWK, 1.0f, 0.7f); }
    if (pl.act_t == 150) game_player_died();
}

static void act_frozen(void)
{
    pl.vel.x = pl.vel.z = 0;
    pl.fwd = 0;
    if (pl.pos.y > pl.floor_y + 0.05f && pl.water_y < -1e8f && !lv.space) {
        apply_gravity(1.0f);
        if (air_step() == ASTEP_LAND) pl.vel.y = 0;
    }
    if (!dialog_active() && pl.act_t > 2 && !pl.no_ctrl) player_set_act(ACT_IDLE);
}

static void act_star_dance(void)
{
    pl.vel.x = pl.vel.z = 0;
    pl.fwd = 0;
    if (lv.space) pl.vel = v3(0, 0, 0);
    else if (pl.pos.y > pl.floor_y + 0.05f) {
        apply_gravity(1.0f);
        if (air_step() == ASTEP_LAND) pl.vel.y = 0;
    }
    pl.yaw = approach_angle(pl.yaw, wrap_angle(camera_yaw() + PI_F), 6 * DT);
}

/* ------------------------------------------------------------------ main update */
static void platform_carry(void)
{
    Obj *o;
    float dyaw, c, s;
    vec3 rel;
    if (pl.platform < 0) return;
    o = &objs[pl.platform];
    if (!o->active) { pl.platform = -1; return; }
    if (player_in_air() && pl.act != ACT_FROZEN) { pl.platform = -1; return; }
    dyaw = angle_diff(o->yaw, o->prev_yaw);
    rel = v3sub(pl.pos, o->prev_pos);
    c = cosf(dyaw); s = sinf(dyaw);
    rel = v3(rel.x * c + rel.z * s, rel.y, -rel.x * s + rel.z * c);
    {
        vec3 np = v3add(o->pos, rel);
        vec3 wn;
        col_walls(&np, 0.6f, RAD_T, &wn, NULL);
        pl.pos = np;
    }
    pl.yaw = wrap_angle(pl.yaw + dyaw);
}

void player_update(void)
{
    compute_intended();
    if (pl.invinc > 0 && pl.act != ACT_DEATH) pl.invinc--;
    if (pressed(BT_A)) pl.a_age = 0;
    else if (pl.a_age < 1000) pl.a_age++;
    if (pl.rocket_t > 0) {
        pl.rocket_t--;
        if (pl.rocket_t == 180) hud_message("The rocket is running out of fuel!", 120);
        if (pl.rocket_t == 0) { sfx(SFX_POP); if (music_current() == MUS_RACE && g_race_state != 1) music_play(lv.music); }
    }
    if (pl.heal_queue > 0 && g_frame % 4 == 0) {
        pl.heal_queue--;
        if (pl.health < 8) pl.health++;
    }
    platform_carry();

    /* water */
    pl.water_y = water_level_at(pl.pos.x, pl.pos.z, pl.pos.y);
    if (pl.water_y > -1e8f && pl.pos.y < pl.water_y - 1.0f && pl.act != ACT_SWIM && pl.act != ACT_DROWN &&
        !(player_in_air() && pl.vel.y > 0) &&
        pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE) {
        if (pl.vel.y < -4) { sfx(SFX_SPLASH); fx_spawn(OT_FX_BUBBLE, v3(pl.pos.x, pl.water_y, pl.pos.z), 1.2f); }
        if (pl.held >= 0) drop_held(0);
        pl.vel.y *= 0.3f;
        pl.pole = -1;
        player_set_act(ACT_SWIM);
        pl.breath_t = 0;
    }
    if (pl.act != ACT_SWIM && pl.act != ACT_DROWN && pl.act != ACT_FLY && pl.act != ACT_SKYFLY && pl.act != ACT_SPACEFLY) pl.pitch = approachf(pl.pitch, 0, 4 * DT);
    if (pl.act != ACT_FLY && pl.act != ACT_SKATE && pl.act != ACT_SKYFLY && pl.act != ACT_SPACEFLY) pl.roll = approachf(pl.roll, 0, 4 * DT);
    if (cur_level == LV_SKY && pl.act != ACT_SKYFLY && pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE) sky_enter();
    if (lv.space && pl.act != ACT_SPACEFLY && pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE) space_enter();
    /* dash boots: Tux never walks, he dashes - and normal jumps land back on the water */
    if (pl.boots) {
        switch (pl.act) {
        case ACT_IDLE: case ACT_WALK: case ACT_SKID: case ACT_LANDING: case ACT_CROUCH: case ACT_CRAWL:
        case ACT_CROUCH_SLIDE: case ACT_PUNCH: case ACT_BELLY_SLIDE: case ACT_GPOUND_LAND:
            skate_enter(0);
            break;
        default:
            if (player_in_air() && pl.act != ACT_FLY && pl.vel.y <= 0 && pl.water_y > -1e8f && pl.pos.y <= pl.water_y + 0.05f) {
                pl.pos.y = pl.water_y;
                skate_enter(0);
                pl.fwd = maxf(pl.fwd, 8.0f);
                sfx(SFX_SPLASH);
            }
        }
    }

    pl.act_t++;
    switch (pl.act) {
    case ACT_IDLE: act_idle(); break;
    case ACT_WALK: act_walk(); break;
    case ACT_SKID: act_skid(); break;
    case ACT_LANDING: act_landing(); break;
    case ACT_CROUCH: act_crouch(); break;
    case ACT_CRAWL: act_crawl(); break;
    case ACT_CROUCH_SLIDE: act_crouch_slide(); break;
    case ACT_PUNCH: act_punch(); break;
    case ACT_BELLY_SLIDE: act_belly_slide(); break;
    case ACT_KNOCKBACK: act_knockback(); break;
    case ACT_GPOUND_LAND: act_gpound_land(); break;
    case ACT_HOLD_IDLE: case ACT_HOLD_WALK: act_hold(); break;
    case ACT_THROW: act_throw(); break;
    case ACT_LEDGE_HANG: act_ledge_hang(); break;
    case ACT_LEDGE_CLIMB: act_ledge_climb(); break;
    case ACT_POLE: act_pole(); break;
    case ACT_JUMP: case ACT_DJUMP: case ACT_TJUMP: case ACT_BACKFLIP: case ACT_SIDEFLIP:
    case ACT_FALL: case ACT_WALLKICK: case ACT_ROLLOUT: case ACT_BOUNCE: case ACT_TRAMP:
        act_jump(); break;
    case ACT_LONGJUMP: act_longjump(); break;
    case ACT_DIVE: act_dive(); break;
    case ACT_JUMPKICK: act_jumpkick(); break;
    case ACT_GPOUND: act_gpound(); break;
    case ACT_AIR_KNOCK: act_air_knock(); break;
    case ACT_BURN_JUMP: act_burn_jump(); break;
    case ACT_HOLD_JUMP: case ACT_HOLD_FALL: act_hold_air(); break;
    case ACT_FLY: act_fly(); break;
    case ACT_SWIM: act_swim(); break;
    case ACT_SKATE: act_skate(); break;
    case ACT_SKYFLY: act_skyfly(); break;
    case ACT_SPACEFLY: act_spacefly(); break;
    case ACT_DEATH: case ACT_DROWN: act_death(); break;
    case ACT_FROZEN: act_frozen(); break;
    case ACT_STAR_DANCE: act_star_dance(); break;
    }
    if (pl.act != ACT_LANDING && pl.act != ACT_WALK && pl.act != ACT_IDLE) {
        if (pl.act < ACT_FIRST_AIR) pl.jump_chain = 0;
    }
    if (pl.act == ACT_WALK || pl.act == ACT_IDLE) {
        pl.chain_timer++;
        if (pl.chain_timer > 10) pl.jump_chain = 0;
    }

    /* standing on a trampoline */
    if (!player_in_air() && pl.floor && pl.floor->owner >= 0 && objs[pl.floor->owner].type == OT_TRAMPOLINE &&
        pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE && pl.act != ACT_SWIM) tramp_bounce(&objs[pl.floor->owner]);
    /* platform we stand on */
    if (!player_in_air() && pl.floor && pl.floor->owner >= 0) pl.platform = pl.floor->owner;
    else if (pl.act != ACT_FROZEN && pl.act != ACT_STAR_DANCE) pl.platform = -1;

    /* death plane */
    if (pl.pos.y < lv.death_y && pl.act != ACT_DEATH) {
        pl.health = 0;
        pl.act = ACT_DEATH;
        pl.act_t = 140;
        pl.visible = 0;
        game_player_died();
    }
    /* blizzard wind pushes Tux around, unless he shelters behind something */
    if (v3len(g_wind) > 0.01f && pl.act != ACT_SWIM && pl.act != ACT_POLE && pl.act != ACT_LEDGE_HANG &&
        pl.act != ACT_LEDGE_CLIMB && pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE &&
        pl.act != ACT_FROZEN && pl.act != ACT_DROWN && pl.act != ACT_CROUCH && !pl.no_ctrl) {
        vec3 head = v3add(pl.pos, v3(0, 1.0f, 0)), up = v3mad(head, v3norm(g_wind), -6.0f), wn;
        float t;
        vec3 n;
        if (!col_ray(head, up, &t, &n, 0)) {
            vec3 np = v3mad(pl.pos, g_wind, DT * (player_in_air() ? 1.4f : 0.8f));
            col_walls(&np, 0.6f, RAD_T, &wn, NULL);
            col_walls(&np, 1.3f, RAD_T, &wn, NULL);
            if (col_any_below(np.x, np.y + 0.7f, np.z)) { pl.pos.x = np.x; pl.pos.z = np.z; }
        }
    }
    /* deep powder snow */
    if (!player_in_air() && pl.floor && (pl.floor->flags & SF_POWDER) && pl.act != ACT_DEATH &&
        pl.act != ACT_SWIM && pl.act != ACT_STAR_DANCE && pl.act != ACT_FROZEN) {
        pl.sink += 0.55f * DT;
        if (pl.fwd > 2.5f) pl.fwd = 2.5f;
        if (pl.act == ACT_BELLY_SLIDE) { pl.vel.x *= 0.9f; pl.vel.z *= 0.9f; }
        if (g_frame % 20 == 0) sfx_ex(SFX_SLIDE, 0.4f, 0.6f);
        if (pl.sink > 1.5f) {
            pl.health = 0;
            pl.sink = 1.6f;
            sfx(SFX_HURT);
            player_set_act(ACT_DEATH);
        }
    } else if (pl.act != ACT_DEATH) pl.sink = approachf(pl.sink, 0, 3 * DT);
    if (pl.health > 8) pl.health = 8;
    if (lv.side_view) { pl.pos.z = 0; pl.vel.z = 0; }   /* TTY WORLD 1-1 is a plane */
}

/* ------------------------------------------------------------------ drawing */
void player_draw(void)
{
    TuxPose tp;
    float t = pl.anim;
    float sp = pl.fwd;
    if (!pl.visible) return;
    if (pl.invinc > 0 && (pl.invinc / 3) % 2 == 0 && pl.act != ACT_DEATH) return;
    memset(&tp, 0, sizeof tp);
    tp.flipL_z = 0.15f;
    tp.flipR_z = 0.15f;
    switch (pl.act) {
    case ACT_IDLE:
        tp.body_squash = sinf(g_frame * 0.05f) * 0.015f;
        tp.flipL_z = tp.flipR_z = 0.12f + sinf(g_frame * 0.05f) * 0.04f;
        if (pl.sleep_t > 600) {
            tp.head_pitch = 0.5f; tp.eyes_closed = 1;
            tp.body_pitch = 0.15f;
        } else tp.head_yaw = sinf(g_frame * 0.013f) * 0.4f * (pl.sleep_t > 120);
        break;
    case ACT_WALK: case ACT_LANDING: case ACT_HOLD_WALK: {
        float s = sinf(t * 2.2f);
        float a = clampf(sp / RUN_SPEED, 0, 1);
        tp.footL = s * 0.9f * a + s * 0.2f;
        tp.footR = -tp.footL;
        tp.body_roll = s * 0.12f;
        tp.body_pitch = 0.2f * a;
        tp.lift = fabsf(cosf(t * 2.2f)) * 0.08f * a;
        tp.flipL_z = 0.3f + 0.5f * a;
        tp.flipR_z = 0.3f + 0.5f * a;
        tp.flipL_x = -s * 0.4f * a;
        tp.flipR_x = s * 0.4f * a;
        if (pl.act == ACT_LANDING && pl.act_t < 4) tp.body_squash = -0.12f;
        if (pl.act == ACT_HOLD_WALK) { tp.flipL_x = tp.flipR_x = -1.4f; tp.flipL_z = tp.flipR_z = 0.2f; }
        break;
    }
    case ACT_SKID:
        tp.body_pitch = -0.35f; tp.footL = tp.footR = 0.6f; tp.flipL_z = tp.flipR_z = 1.0f;
        break;
    case ACT_CROUCH: case ACT_CROUCH_SLIDE: case ACT_CRAWL:
        tp.body_squash = -0.3f; tp.lift = -0.1f; tp.flipL_z = tp.flipR_z = 0.5f;
        if (pl.act == ACT_CRAWL) { tp.footL = sinf(t * 3) * 0.5f; tp.footR = -tp.footL; }
        break;
    case ACT_PUNCH: {
        float k = clampf(sinf(pl.act_t / 14.0f * PI_F), 0, 1);
        if (pl.punch_n == 0) { tp.flipL_x = -1.6f * k; tp.flipL_z = 0.2f; tp.body_roll = -0.2f * k; }
        else if (pl.punch_n == 1) { tp.flipR_x = -1.6f * k; tp.flipR_z = 0.2f; tp.body_roll = 0.2f * k; }
        else { k = clampf(sinf(pl.act_t / 20.0f * PI_F), 0, 1); tp.footL = 1.4f * k; tp.body_pitch = -0.3f * k; tp.lift = 0.15f * k; }
        break;
    }
    case ACT_BELLY_SLIDE: case ACT_DIVE:
        tp.body_pitch = 1.45f;
        tp.lift = 0.25f;
        tp.head_pitch = -1.0f;
        tp.flipL_z = tp.flipR_z = 1.2f;
        tp.flipL_x = tp.flipR_x = 0.6f;
        tp.footL = tp.footR = -0.9f;
        break;
    case ACT_TRAMP:
        if (pl.tramp_combo >= 2 && pl.vel.y > 0) { tp.flipL_z = tp.flipR_z = 2.9f; tp.footL = tp.footR = 0; break; }
        /* fall through */
    case ACT_JUMP: case ACT_DJUMP: case ACT_BOUNCE: case ACT_HOLD_JUMP: case ACT_HOLD_FALL: case ACT_FALL: {
        float up = clampf(pl.vel.y / 15.0f, -1, 1);
        if (pl.act == ACT_FALL && pl.act_s == 1) {
            tp.body_pitch = 1.45f; tp.lift = 0.25f; tp.head_pitch = -1.0f; tp.flipL_z = tp.flipR_z = 1.2f;
            break;
        }
        tp.flipL_z = tp.flipR_z = 1.0f + up * 0.9f + (pl.act == ACT_FALL ? sinf(g_frame * 0.6f) * 0.4f : 0);
        tp.footL = 0.5f; tp.footR = -0.3f;
        tp.body_pitch = -0.1f * up;
        if (pl.act == ACT_DJUMP) tp.spin = (1 - clampf(pl.act_t / 25.0f, 0, 1)) * TAU_F;
        if (pl.act == ACT_HOLD_JUMP || pl.act == ACT_HOLD_FALL) { tp.flipL_x = tp.flipR_x = -1.4f; tp.flipL_z = tp.flipR_z = 0.2f; }
        break;
    }
    case ACT_TJUMP: case ACT_ROLLOUT: case ACT_WALLKICK: {
        float k = clampf(pl.act_t / (pl.act == ACT_TJUMP ? 40.0f : 25.0f), 0, 1);
        tp.flip = k * TAU_F;
        tp.flipL_z = tp.flipR_z = 1.2f;
        tp.body_squash = -0.15f * sinf(k * PI_F);
        break;
    }
    case ACT_BACKFLIP: {
        float k = clampf(pl.act_t / 38.0f, 0, 1);
        tp.flip = -k * TAU_F;
        tp.flipL_z = tp.flipR_z = 1.3f;
        break;
    }
    case ACT_SIDEFLIP: {
        float k = clampf(pl.act_t / 35.0f, 0, 1);
        tp.body_roll = k * TAU_F;
        tp.flipL_z = tp.flipR_z = 1.4f;
        break;
    }
    case ACT_LONGJUMP:
        tp.body_pitch = 0.9f; tp.flipL_z = tp.flipR_z = 1.4f; tp.flipL_x = tp.flipR_x = 0.8f;
        tp.footL = tp.footR = -0.7f; tp.lift = 0.3f;
        break;
    case ACT_JUMPKICK: {
        float k = clampf(sinf(pl.act_t / 18.0f * PI_F), 0, 1);
        tp.footL = 1.6f * k; tp.body_pitch = -0.4f * k; tp.flipL_z = tp.flipR_z = 1.0f;
        break;
    }
    case ACT_GPOUND:
        if (pl.act_s == 0) tp.flip = clampf(pl.act_t / 14.0f, 0, 1) * TAU_F;
        tp.body_squash = -0.2f;
        tp.flipL_z = tp.flipR_z = 0.6f;
        tp.footL = tp.footR = 0.6f;
        break;
    case ACT_GPOUND_LAND:
        tp.body_squash = pl.act_t < 6 ? -0.3f : 0;
        tp.flipL_z = tp.flipR_z = 1.4f;
        break;
    case ACT_KNOCKBACK: case ACT_AIR_KNOCK:
        tp.body_pitch = -0.5f; tp.flipL_z = tp.flipR_z = 1.5f; tp.head_pitch = -0.4f;
        tp.footL = tp.footR = 0.7f;
        break;
    case ACT_BURN_JUMP:
        tp.flipL_z = tp.flipR_z = 2.4f + sinf(g_frame * 0.8f) * 0.4f;
        tp.footL = sinf(g_frame * 0.9f); tp.footR = -tp.footL;
        tp.body_pitch = -0.2f;
        break;
    case ACT_HOLD_IDLE:
        tp.flipL_x = tp.flipR_x = -1.4f; tp.flipL_z = tp.flipR_z = 0.2f;
        break;
    case ACT_THROW: {
        float k = clampf(pl.act_t / 8.0f, 0, 1);
        tp.flipL_x = tp.flipR_x = -1.4f - k * 0.8f;
        tp.body_pitch = 0.3f * k;
        break;
    }
    case ACT_LEDGE_HANG:
        tp.flipL_z = tp.flipR_z = 2.9f; tp.flipL_x = tp.flipR_x = -0.3f;
        tp.footL = sinf(g_frame * 0.1f) * 0.2f; tp.footR = -tp.footL;
        break;
    case ACT_LEDGE_CLIMB: {
        float k = clampf(pl.act_t / 18.0f, 0, 1);
        tp.lift = k * 1.5f;
        tp.flipL_z = tp.flipR_z = 2.9f - k * 2.0f;
        tp.body_pitch = 0.6f * sinf(k * PI_F);
        tp.footL = -0.8f * k;
        break;
    }
    case ACT_POLE:
        tp.flipL_z = tp.flipR_z = 1.3f; tp.flipL_x = tp.flipR_x = -1.0f;
        tp.footL = sinf(t * 2) * 0.4f; tp.footR = -tp.footL;
        tp.body_pitch = 0.1f;
        break;
    case ACT_SWIM: {
        float s = sinf(t * 2.0f);
        tp.body_pitch = 1.3f + pl.pitch;
        tp.lift = 0.6f;
        tp.head_pitch = -1.0f;
        tp.flipL_z = tp.flipR_z = 1.0f + s * 0.6f;
        tp.flipL_x = tp.flipR_x = 0.6f;
        tp.footL = s * 0.4f; tp.footR = -s * 0.4f;
        if (pl.act_s == 2) { tp.flipL_z = tp.flipR_z = 0.3f; tp.flipL_x = tp.flipR_x = 1.2f; }
        if (pl.pos.y > pl.water_y - 1.25f && pl.act_s != 2) { tp.body_pitch = 0.4f; tp.lift = 0.3f; tp.head_pitch = -0.3f; }
        break;
    }
    case ACT_DEATH: case ACT_DROWN: {
        float k = clampf(pl.act_t / 40.0f, 0, 1);
        tp.spin = k * 3 * TAU_F * (pl.act_t < 40);
        if (pl.act_t >= 40) {
            float k2 = clampf((pl.act_t - 40) / 20.0f, 0, 1);
            tp.body_pitch = -1.5f * k2;
            tp.lift = 0.3f * k2;
        }
        tp.flipL_z = tp.flipR_z = 1.5f;
        tp.eyes_closed = 1;
        break;
    }
    case ACT_STAR_DANCE: {
        float k = pl.act_t / 60.0f;
        if (k < 1) { tp.spin = k * TAU_F; tp.lift = sinf(k * PI_F) * 1.2f; }
        tp.flipL_z = 2.8f;
        tp.flipR_z = 0.4f + (k > 1 ? sinf(pl.act_t * 0.2f) * 0.3f : 0);
        break;
    }
    case ACT_FROZEN:
        tp.flipL_z = tp.flipR_z = 0.15f;
        break;
    case ACT_SKATE: {
        float s = sinf(t * 3.0f), k = clampf(pl.fwd / 20.0f, 0, 1);
        tp.body_pitch = 0.35f * k;
        tp.body_roll = -pl.roll;
        tp.flipL_z = tp.flipR_z = 0.5f + 0.7f * k;
        tp.flipL_x = tp.flipR_x = 0.5f * k;
        tp.footL = s * 0.7f * k + 0.1f;
        tp.footR = -tp.footL;
        tp.lift = fabsf(cosf(t * 3.0f)) * 0.1f * k;
        if (pl.act_s == 1) {
            float up = clampf(pl.vel.y / 15.0f, -1, 1);
            tp.footL = 0.4f; tp.footR = -0.2f;
            tp.flipL_z = tp.flipR_z = 1.4f + up * 0.6f;
            tp.lift = 0;
            if (pl.trick) { tp.spin = pl.trick_t * TAU_F; tp.flipL_z = tp.flipR_z = 2.6f; }
        }
        if (held(BT_Z) && pl.act_s == 0) { tp.body_squash = -0.15f; tp.body_roll = -pl.roll * 1.4f; }
        break;
    }
    case ACT_SKYFLY:
    case ACT_SPACEFLY:
    case ACT_FLY:
        /* riding the rocket: leaning into the flight direction, flippers back */
        tp.body_pitch = 1.1f - pl.pitch * 0.9f;
        tp.body_roll = -pl.roll;
        tp.lift = 0.4f;
        tp.head_pitch = -0.8f;
        tp.flipL_z = tp.flipR_z = 0.5f;
        tp.flipL_x = tp.flipR_x = 0.9f + sinf(g_frame * 0.4f) * 0.05f;
        tp.footL = tp.footR = -0.5f;
        break;
    }
    tp.rocket = pl.rocket_t > 0 && (pl.rocket_t > 180 || (pl.rocket_t / 6) % 2);
    if (pl.rocket_t > 0 && player_rocket_firing()) tp.rocket = 2;
    if (pl.boots) tp.boots = pl.boost_t > 0 ? 2 : 1;
    if (pl.act == ACT_JUMP || pl.act == ACT_DJUMP) tp.lift += 0;
    draw_tux_model(v3(pl.pos.x, pl.pos.y - pl.sink, pl.pos.z), pl.yaw, &tp, 1.0f, TUXV_TUX);
}
