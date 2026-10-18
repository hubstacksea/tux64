/* ice.c - ICE STACK CLIMBER: the complete system of the classic mountain-climbing game, Tux style
 *
 * The mountain is a grid 32 tiles wide that wraps around at the screen edges. Every fourth row is a
 * floor of blocks; Tux breaks the blocks above him by jumping into them head (and hammer) first, and
 * climbs through the holes. Jumps are fixed: the height never changes and the sideways speed is
 * locked at take-off. B swings the hammer.
 *   - little seals (the "Topi") walk the floors and fill holes with blocks of ice they push in
 *   - birds dive across the upper floors, icicles grow under the floors and drop
 *   - stay too long on one floor and the Ice Brute (in sunglasses) stomps the screen upward
 *   - conveyor floors, slippery ice and drifting clouds
 *   - floor 8 leads to the timed bonus stage: fish to collect and, at the very top, the condor
 *     carrying a Kernel Star - jump up and grab it to clear the mountain
 * The screen only ever scrolls up; falling below it costs health. Points are scored as in the
 * original and shown on screen. */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include "ice_maps.h"

#define IW 32                       /* tiles across */
#define IROWS 64
#define IT 0.8f                     /* tile size */
#define HWID (IW * IT * 0.5f)       /* half the mountain width */
#define WRAPW (IW * IT)
#define BONUS_ROW 33
#define CONDOR_ROW (BONUS_ROW + 26)
#define P_HW 0.47f                  /* Tux's half width: he needs a hole two tiles wide */
#define P_H 1.5f
#define WALK 3.4f
#define GRAV 32.0f
#define JUMPV 16.8f
#define SCORE_STAR 8000

enum { TE, TBRK, THARD, TCONVR, TCONVL, TSLIP, TPUSHED, TPLAT };

typedef struct { float x, bx; int row, dir, state, timer, hole; float vy, y; } Topi;
typedef struct { float x, y, vx, ph; int alive, timer; } Bird;
typedef struct { float x, w, vx; int row; } Cloud;
typedef struct { int col, row, state, timer; float grow, y; } Icicle;
typedef struct { float x, y; int got, kind; } Fish;

int g_ice_score, g_ice_mountain;
static unsigned char grid[IROWS][IW];
static Topi topi[12];
static int ntopi;
static Bird birds[3];
static Cloud clouds[10];
static int nclouds;
static Icicle icicles[16];
static int nicicles;
static Fish fish[16];
static int nfish, fish_left;
static float px, py, vx, vy, jvx, cam_bot, cam_dist;
static int ground, face, hammer_t, on_cloud, cam_floor, best_floor, stall_t, bird_t;
static int bonus, bonus_t, end_t, cleared, score_star_given, walk_t;
static struct { int active, state, timer; float x, y, vy; } bear;
static float condor_x, condor_dir, bonus_cam;   /* bonus_cam: the camera follows each platform landed on */

/* ---------------------------------------------------------------- the grid */
static int wrapc(int c) { c %= IW; return c < 0 ? c + IW : c; }
static int tile(int c, int r) { if (r <= 0) return THARD; if (r >= IROWS) return TE; return grid[r][wrapc(c)]; }
static int solid(int c, int r) { return tile(c, r) != TE; }
static int breakable(int t) { return t == TBRK || t == TSLIP || t == TPUSHED; }
static int colf(float x) { return (int)floorf((x + HWID) / IT); }   /* unwrapped column */
static int rowf(float y) { return (int)floorf(y / IT); }
static float col_x(int c) { return c * IT - HWID + IT * 0.5f; }   /* centre of an (unwrapped) column */
static float wrapx(float x) { while (x < -HWID) x += WRAPW; while (x >= HWID) x -= WRAPW; return x; }
static float dxw(float a, float b) { return wrapx(a - b); }   /* wrapped difference a - b */

static void load_mountain(int m)
{
    const char **fl = m == 2 ? mountain2 : (m == 3 ? mountain3 : mountain1);
    int k, c, r;
    memset(grid, 0, sizeof grid);
    ntopi = nclouds = nicicles = nfish = 0;
    for (c = 0; c < IW; c++) grid[0][c] = THARD;
    for (k = 0; k < 8; k++) {
        r = 4 * (k + 1);
        for (c = 0; c < IW; c++) {
            char ch = fl[k][c];
            int t = TBRK;
            switch (ch) {
            case '.': case 'C': case 'c': t = TE; break;
            case 'H': t = THARD; break;
            case '>': t = TCONVR; break;
            case '<': t = TCONVL; break;
            case '~': t = TSLIP; break;
            }
            grid[r][c] = (unsigned char)t;
            if (ch == 'T' && ntopi < 12) {
                Topi *t2 = &topi[ntopi++];
                memset(t2, 0, sizeof *t2);
                t2->x = col_x(c); t2->row = r; t2->dir = (c & 1) ? 1 : -1;
            }
            if (ch == 'I' && nicicles < 16) { Icicle *ic = &icicles[nicicles++]; memset(ic, 0, sizeof *ic); ic->col = c; ic->row = r; }
            if ((ch == 'C' || ch == 'c') && (c == 0 || fl[k][c - 1] != ch) && nclouds < 10) {
                Cloud *cl = &clouds[nclouds++];
                cl->row = r; cl->w = 4 * IT; cl->x = col_x(c) + 1.5f * IT; cl->vx = ch == 'C' ? 1.8f : -1.8f;
            }
        }
    }
    for (k = 0; k < 27; k++) {
        r = BONUS_ROW + k;
        for (c = 0; c < IW; c++) {
            char ch = bonus_rows[k][c];
            if (ch == '=') grid[r][c] = TPLAT;
            if (ch == 'F' && nfish < 16) { Fish *f = &fish[nfish++]; f->x = col_x(c); f->y = r * IT + 0.1f; f->got = 0; f->kind = nfish % 4; }
            if ((ch == 'C' || ch == 'c') && nclouds < 10) {
                Cloud *cl = &clouds[nclouds++];
                cl->row = r - 1; cl->w = 4 * IT; cl->x = col_x(c); cl->vx = ch == 'C' ? 1.5f : -1.5f;
            }
        }
    }
    fish_left = nfish;
}

/* ---------------------------------------------------------------- setup */
void ice_reset(void)
{
    extern int g_selected_star;
    static const int mountain_of_star[5] = { 1, 2, 3, 1, 2 };
    g_ice_mountain = mountain_of_star[g_selected_star >= 0 && g_selected_star < 5 ? g_selected_star : 0];
    load_mountain(g_ice_mountain);
    px = col_x(15); py = IT; vx = vy = jvx = 0;
    ground = 1; face = 1; hammer_t = 0; on_cloud = -1;
    cam_floor = 0; best_floor = 0; stall_t = 0; bird_t = 0; walk_t = 0;
    bonus = 0; bonus_t = 0; end_t = 0; cleared = 0; score_star_given = 0;
    g_ice_score = 0;
    memset(birds, 0, sizeof birds);
    memset(&bear, 0, sizeof bear);
    condor_x = 0; condor_dir = 1; bonus_cam = 0;
    cam_bot = -IT;
    cam_dist = 16;
    pl.pos = v3(px, py, 0);
    pl.yaw = PI_F / 2;
}

static void add_score(int n) { g_ice_score += n; }

static float view_h(void) { return 2 * cam_dist * tanf(25.0f * PI_F / 180); }

/* lose health; respawn on the lowest floor on screen */
static void miss(void)
{
    int k, c, i;
    if (pl.invinc > 0 || pl.act == ACT_DEATH || pl.act == ACT_STAR_DANCE || cleared) return;
    sfx(SFX_HURT);
    sfx_ex(SFX_SQUAWK, 0.8f, 0.9f);
    if (bonus) {   /* in the bonus stage a fall just ends it */
        hud_message("You fell! The bonus stage is over.", 150);
        end_t = 150;
        cleared = 2;
        return;
    }
    pl.health -= 2;
    if (pl.health <= 0) {
        pl.health = 0;
        player_set_act(ACT_DEATH);
        return;
    }
    pl.invinc = 150;
    k = cam_floor;
    for (i = 0; i < IW; i++) {
        c = 16 + ((i & 1) ? (i + 1) / 2 : -(i / 2));
        if (solid(c, 4 * k) && !solid(c, 4 * k + 1) && !solid(c, 4 * k + 2) && !solid(c + 1, 4 * k + 1)) break;
    }
    px = col_x(wrapc(c)) + IT * 0.5f;
    py = (4 * k + 1) * IT;
    vx = vy = jvx = 0;
    ground = 1;
    on_cloud = -1;
}

/* the block Tux's head (and hammer) runs into */
static void head_hit(int r)
{
    int cc = colf(px), c, t, best = -999;
    float dbest = 1e9f;
    for (c = colf(px - P_HW); c <= colf(px + P_HW); c++) {
        float d = fabsf(col_x(c) - px);
        if (solid(c, r) && d < dbest) { dbest = d; best = c; }
    }
    if (best == -999) return;
    (void)cc;
    t = tile(best, r);
    if (breakable(t)) {
        grid[r][wrapc(best)] = TE;
        add_score(10);
        sfx_ex(SFX_BREAK, 0.8f, 1.3f);
        fx_spawn(OT_FX_SMOKE, v3(col_x(best), r * IT + 0.4f, 0.5f), 0.5f);
    } else sfx_ex(SFX_THUD, 0.6f, 1.6f);
}

static int body_hits_wall(float x, float y)
{
    int r, c0 = colf(x - P_HW), c1 = colf(x + P_HW), c;
    for (r = rowf(y + 0.05f); r <= rowf(y + P_H - 0.05f); r++)
        for (c = c0; c <= c1; c++) if (solid(c, r)) return 1;
    return 0;
}

static int floor_under(float x, float y, int *type)
{
    int r = rowf(y - 0.05f), c;
    for (c = colf(x - P_HW + 0.08f); c <= colf(x + P_HW - 0.08f); c++)
        if (solid(c, r) && fabsf(y - (r + 1) * IT) < 0.12f) { if (type) *type = tile(colf(x), r) != TE ? tile(colf(x), r) : tile(c, r); return r; }
    return -1;
}

static int cloud_under(float x, float y)
{
    int i;
    for (i = 0; i < nclouds; i++) {
        Cloud *cl = &clouds[i];
        float top = (cl->row + 1) * IT;
        if (fabsf(y - top) < 0.12f && fabsf(dxw(x, cl->x)) < cl->w * 0.5f + 0.2f) return i;
    }
    return -1;
}

static void landed(int r)
{
    int f = r / 4;
    if (r % 4 == 0 && r <= 32 && f > best_floor) { best_floor = f; stall_t = 0; }
    if (r % 4 == 0 && r <= 32 && f - 1 > cam_floor) { cam_floor = f - 1; stall_t = 0; bear.active = 0; }
    if (r >= BONUS_ROW && !bonus) {
        bonus = 1;
        bonus_t = 40 * 60;
        memset(birds, 0, sizeof birds);   /* no enemies in the bonus stage */
        bear.active = 0;
        hud_message("BONUS STAGE! Catch the condor at the top!", 150);
        music_play(MUS_RACE);
    }
    if (bonus) bonus_cam = maxf(bonus_cam, (r - 3) * IT);   /* in the bonus stage the screen follows every platform */
}

static void move_horiz(float dx, float *v)
{
    float nx = px + dx;
    if (body_hits_wall(nx, py)) {
        if (dx > 0) nx = (colf(px + P_HW + dx)) * IT - HWID - P_HW - 0.002f;
        else nx = (colf(px - P_HW + dx) + 1) * IT - HWID + P_HW + 0.002f;
        if (body_hits_wall(nx, py)) nx = px;
        if (v) *v = 0;
    }
    px = nx;
}

static void player_step(void)
{
    int dir = 0, lock = pl.no_ctrl || cleared == 1;   /* after the condor: wait for the star to come down */
    if (!lock) dir = in.sx > 0.3f ? 1 : (in.sx < -0.3f ? -1 : 0);
    if (hammer_t > 0) {
        hammer_t--;
        if (hammer_t == 12) {   /* the hammer comes down: hit whatever is in front */
            int i;
            float hx = px + face * 0.9f, hy = py + 0.7f;
            for (i = 0; i < ntopi; i++) {
                Topi *t = &topi[i];
                if (t->state == 3 || t->state == 4) continue;
                if (t->state == 2 && fabsf(dxw(t->bx, hx)) < 0.7f && fabsf(t->row * IT + IT + 0.4f - hy) < 0.8f) {
                    t->state = 1; t->timer = 0; add_score(50); sfx(SFX_BREAK);
                    fx_spawn(OT_FX_SMOKE, v3(t->bx, hy, 0.5f), 0.6f);
                    continue;
                }
                if (fabsf(dxw(t->x, hx)) < 0.8f && fabsf((t->row + 1) * IT + 0.4f - hy) < 0.9f) {
                    t->state = 3; t->vy = 8; t->y = (t->row + 1) * IT; t->timer = 0; t->dir = face;
                    add_score(400); sfx(SFX_KICK);
                }
            }
            for (i = 0; i < 3; i++) {
                Bird *b = &birds[i];
                if (b->alive && fabsf(dxw(b->x, hx)) < 0.9f && fabsf(b->y - hy) < 0.9f) {
                    b->alive = 0; add_score(800); sfx(SFX_POP);
                    fx_spawn(OT_FX_SPARKLE, v3(b->x, b->y, 0.5f), 0.8f);
                }
            }
        }
        dir = 0;
    }
    if (ground) {
        int t = TE, r;
        float target = dir * WALK, cv = 0;
        r = floor_under(px, py, &t);
        if (r < 0 && on_cloud < 0) on_cloud = cloud_under(px, py);
        if (t == TSLIP) vx = approachf(vx, target, (dir ? 2.5f : 0.8f) * DT);
        else vx = target;
        if (t == TCONVR) cv = 1.6f;
        if (t == TCONVL) cv = -1.6f;
        if (dir) face = dir;
        if (dir && ++walk_t % 14 == 0) sfx_ex(SFX_STEP, 0.4f, 1.3f);
        if (!lock && (in.pressed & BT_A) && hammer_t == 0) {
            vy = JUMPV;
            jvx = vx;
            ground = 0;
            on_cloud = -1;
            sfx(SFX_JUMP);
        } else if (!lock && (in.pressed & BT_B) && hammer_t == 0) {
            hammer_t = 20;
            sfx_ex(SFX_PUNCH, 0.8f, 0.9f);
        } else {
            move_horiz((vx + cv) * DT, &vx);
            if (on_cloud >= 0) move_horiz(clouds[on_cloud].vx * DT, NULL);
            if (floor_under(px, py, NULL) < 0) {
                on_cloud = cloud_under(px, py);
                if (on_cloud < 0) { ground = 0; jvx = vx * 0.5f; vy = 0; }
            }
        }
    } else {
        float ny;
        vy -= GRAV * DT;
        if (vy < -18) vy = -18;
        move_horiz(jvx * DT, NULL);   /* the sideways speed of a jump never changes */
        ny = py + vy * DT;
        if (vy > 0) {
            int r = rowf(ny + P_H), c, hit = 0;
            for (c = colf(px - P_HW); c <= colf(px + P_HW); c++) if (solid(c, r)) hit = 1;
            if (hit) { head_hit(r); vy = 0; ny = r * IT - P_H - 0.001f; }
        } else {
            int r = rowf(ny), c, hit = 0, i;
            for (c = colf(px - P_HW + 0.08f); c <= colf(px + P_HW - 0.08f); c++) if (solid(c, r)) hit = 1;
            if (hit && py >= (r + 1) * IT - 0.05f) {
                ny = (r + 1) * IT; ground = 1; vy = 0; vx = jvx; on_cloud = -1;
                if (tile(colf(px), r) != TSLIP) vx = 0;
                landed(r);
                sfx_ex(SFX_LAND, 0.5f, 1.2f);
            }
            for (i = 0; i < nclouds && !ground; i++) {
                Cloud *cl = &clouds[i];
                float top = (cl->row + 1) * IT;
                if (py >= top - 0.02f && ny < top && fabsf(dxw(px, cl->x)) < cl->w * 0.5f + 0.2f) {
                    ny = top; ground = 1; vy = 0; vx = 0; on_cloud = i;
                    landed(cl->row + 1);
                }
            }
        }
        py = ny;
    }
    px = wrapx(px);
}

/* ---------------------------------------------------------------- enemies and gimmicks */
static void touch_check(float x, float y, float r)
{
    if (fabsf(dxw(x, px)) < r + P_HW * 0.8f && y > py - 0.2f && y < py + P_H + 0.2f) miss();
}

static void topi_update(Topi *t)
{
    float floor_top = (t->row + 1) * IT;
    float vis_lo = cam_bot - 2, vis_hi = cam_bot + view_h() + 2;
    if (floor_top < vis_lo - 4 || floor_top > vis_hi + 6) return;
    t->timer++;
    switch (t->state) {
    case 0: {   /* walking the floor; a hole ahead means: go and fetch some ice */
        int c = colf(t->x + t->dir * 0.45f);
        t->x = wrapx(t->x + t->dir * 1.5f * DT);
        if (!solid(c, t->row)) { t->state = 1; t->timer = 0; t->hole = wrapc(c); t->dir = -t->dir; }
        touch_check(t->x, floor_top + 0.3f, 0.45f);
        break;
    }
    case 1:   /* walking away to fetch a block of ice; it soon comes back pushing one */
        t->x = wrapx(t->x + t->dir * 2.0f * DT);
        if (!solid(colf(t->x + t->dir * 0.45f), t->row)) t->dir = -t->dir;
        touch_check(t->x, floor_top + 0.3f, 0.45f);
        if (t->timer > 110) {
            t->state = 2; t->timer = 0;
            t->dir = dxw(col_x(t->hole), t->x) > 0 ? 1 : -1;
            t->bx = wrapx(t->x + t->dir * 0.9f);
        }
        break;
    case 2: {   /* pushing the ice block; it drops into the first hole it reaches */
        int c;
        t->x = wrapx(t->x + t->dir * 1.3f * DT);
        t->bx = wrapx(t->x + t->dir * 0.9f);
        c = colf(t->bx);
        if (!solid(c, t->row) && fabsf(dxw(col_x(c), t->bx)) < 0.15f) {
            grid[t->row][wrapc(c)] = TPUSHED;
            sfx_ex(SFX_THUD, 0.5f, 1.2f);
            t->state = 0; t->timer = 0;
        }
        if (t->timer > 600) { t->state = 0; t->timer = 0; }
        touch_check(t->x, floor_top + 0.3f, 0.45f);
        touch_check(t->bx, floor_top + 0.4f, 0.4f);
        break;
    }
    case 3:   /* knocked off by the hammer */
        t->y += t->vy * DT;
        t->vy -= 25 * DT;
        t->x = wrapx(t->x + t->dir * 3 * DT);
        if (t->y < floor_top - 8) { t->state = 4; t->timer = 0; }
        break;
    case 4:   /* comes back after a while from the edge */
        if (t->timer > 420) { t->state = 0; t->x = t->dir > 0 ? -HWID + 0.4f : HWID - 0.4f; t->dir = -t->dir; t->timer = 0; }
        break;
    }
}

static void birds_update(void)
{
    int i, maxb = g_ice_mountain >= 2 ? 2 : 1, first = g_ice_mountain == 1 ? 4 : 3;
    if (!bonus && cam_floor >= first - 1 && ++bird_t > 420) {
        for (i = 0; i < maxb; i++) if (!birds[i].alive) {
            Bird *b = &birds[i];
            b->alive = 1; b->timer = 0;
            b->vx = (g_frame & 1) ? 3.0f : -3.0f;
            b->x = b->vx > 0 ? -HWID : HWID - 0.1f;
            b->y = py + 2.5f;
            b->ph = 0;
            bird_t = 0;
            sfx_ex(SFX_SQUAWK, 0.5f, 2.2f);
            break;
        }
    }
    for (i = 0; i < 3; i++) {
        Bird *b = &birds[i];
        if (!b->alive) continue;
        b->timer++;
        b->ph += DT;
        b->x = wrapx(b->x + b->vx * DT);
        /* swoops down toward Tux and back up */
        b->y += ((py + 1.0f + sinf(b->ph * 2.2f) * 1.8f) - b->y) * minf(1, 1.2f * DT);
        if (b->timer > 720) { b->y += 6 * DT; if (b->y > cam_bot + view_h() + 3) b->alive = 0; }
        else touch_check(b->x, b->y, 0.45f);
    }
}

static void bear_update(void)
{
    if (bonus || cleared) return;
    if (!bear.active) {
        if (++stall_t > 40 * 60 && cam_floor < 7) {
            bear.active = 1; bear.state = 0; bear.timer = 0;
            bear.x = -HWID + 1; bear.y = (4 * cam_floor + 1) * IT;
            hud_message("The Ice Brute is coming! Climb!", 120);
            sfx(SFX_ROAR);
        }
        return;
    }
    bear.timer++;
    bear.y = (4 * cam_floor + 1) * IT;
    if (bear.state == 0) {   /* walk in, then stomp every few seconds */
        bear.x = wrapx(bear.x + 1.2f * DT);
        if (bear.timer % 180 == 120) { bear.state = 1; bear.vy = 7; }
    }
    if (bear.state == 1) {
        static float hop;
        hop += bear.vy * DT;
        bear.vy -= 20 * DT;
        if (hop <= 0) {
            hop = 0;
            bear.state = 0;
            cam_floor++;   /* STOMP: the whole screen is pushed up a floor */
            cam.shake = 0.4f;
            sfx(SFX_THUD);
            sfx_ex(SFX_EXPLODE, 0.5f, 0.6f);
            if (cam_floor >= 7) bear.active = 0;
        }
    }
    touch_check(bear.x, bear.y + 0.8f, 0.8f);
}

static void icicles_update(void)
{
    int i;
    for (i = 0; i < nicicles; i++) {
        Icicle *ic = &icicles[i];
        float x = col_x(ic->col), top = ic->row * IT;
        if (!solid(ic->col, ic->row)) { ic->state = 3; continue; }   /* its block was broken */
        if (top < cam_bot || top > cam_bot + view_h() + 1) continue;
        switch (ic->state) {
        case 0:
            ic->grow = minf(1, ic->grow + DT / 8.0f);
            if (ic->grow >= 1 && fabsf(dxw(x, px)) < 2.5f && py < top) { ic->state = 1; ic->timer = 0; }
            break;
        case 1:
            if (++ic->timer > 30) { ic->state = 2; ic->y = top - 0.1f; sfx_ex(SFX_BREAK, 0.4f, 2.0f); }
            break;
        case 2: {
            int r;
            ic->y -= 9 * DT;
            touch_check(x, ic->y - 0.4f, 0.25f);
            r = rowf(ic->y - 0.8f);
            if (solid(ic->col, r) && r < ic->row) { ic->state = 0; ic->grow = 0; fx_spawn(OT_FX_SMOKE, v3(x, ic->y - 0.6f, 0.5f), 0.4f); sfx_ex(SFX_BREAK, 0.4f, 2.4f); }
            if (ic->y < cam_bot - 3) { ic->state = 0; ic->grow = 0; }
            break;
        }
        }
    }
}

static void bonus_update(void)
{
    int i;
    for (i = 0; i < nfish; i++) {
        Fish *f = &fish[i];
        if (f->got) continue;
        if (fabsf(dxw(f->x, px)) < 0.7f && f->y > py - 0.5f && f->y < py + P_H) {
            f->got = 1;
            fish_left--;
            add_score(300);
            game_add_coins(1, v3(f->x, f->y, 0));
            sfx_ex(SFX_REDCOIN, 1.0f, 1.0f + (nfish - fish_left) * 0.06f);
            if (fish_left == 0 && bonus && cleared == 0) {
                hud_message("Every fish in the bonus stage!", 150);
                star_spawn(v3(px, py + 2.2f, 0), 3, 1, v3(f->x, f->y, 0));
            }
        }
    }
    if (!bonus || cleared) return;
    if (bonus_t > 0) bonus_t--;
    if (bonus_t > 0 && bonus_t <= 600 && bonus_t % 60 == 0) sfx_ex(SFX_TEXT, 0.8f, 1.4f);
    if (bonus_t == 0) { hud_message("TIME UP! The condor flew away...", 150); end_t = 150; cleared = 2; }
    /* the condor glides back and forth above the summit */
    condor_x += condor_dir * 3.0f * DT;
    if (condor_x > HWID - 1.5f) condor_dir = -1;
    if (condor_x < -HWID + 1.5f) condor_dir = 1;
    if (fabsf(condor_x - px) < 1.3f && CONDOR_ROW * IT > py + 0.5f && CONDOR_ROW * IT < py + P_H + 1.0f) {
        cleared = 1;
        add_score(3000);
        hud_message("You caught the condor! MOUNTAIN CLEAR!", 180);
        jvx = vx = 0;   /* he lets go and drops straight down */
        {   /* the star comes down to where Tux will land */
            int r = rowf(py + 0.05f), c, under = 0;
            while (r > 0 && !under) {
                for (c = colf(px - P_HW + 0.08f); c <= colf(px + P_HW - 0.08f); c++) if (solid(c, r - 1)) under = 1;
                if (!under) r--;
            }
            star_spawn(v3(px, r * IT + 0.3f, 0), g_ice_mountain - 1, 1, v3(condor_x, CONDOR_ROW * IT, 0));
        }
        sfx(SFX_1UP);
    }
}

void ice_update(void)
{
    int i;
    if (!lv.ice_mode) return;
    if (pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE) {
        player_step();
        for (i = 0; i < ntopi; i++) topi_update(&topi[i]);
        birds_update();
        bear_update();
        icicles_update();
        for (i = 0; i < nclouds; i++) clouds[i].x = wrapx(clouds[i].x + clouds[i].vx * DT);
        bonus_update();
        /* below the bottom of the screen means a fall */
        if (py < cam_bot - 1.5f && pl.act != ACT_DEATH) miss();
        if (!score_star_given && g_ice_score >= SCORE_STAR) {
            score_star_given = 1;
            hud_message("8000 POINTS! A Kernel Star appears!", 150);
            star_spawn(v3(px, py + 2.2f, 0), 4, 1, v3(px, py + 5, 0));
        }
        if (end_t > 0 && --end_t == 0) course_exit();
    }
    /* the camera never scrolls down */
    {
        float target = (4 * cam_floor - 1) * IT;
        if (bonus) target = maxf(target, bonus_cam);
        if (target > cam_bot) cam_bot += (target - cam_bot) * minf(1, 3.0f * DT) + 0.01f;
    }
    pl.pos = v3(px, py, 0);
    pl.vel = v3(0, vy, 0);
    pl.floor_y = py;
    pl.yaw = face > 0 ? PI_F / 2 : -PI_F / 2;
    pl.fwd = fabsf(vx);
}

void ice_camera(void)
{
    float asp = g_view_aspect > 0.5f ? g_view_aspect : 1.333f;
    float vh;
    cam_dist = clampf((HWID + 0.4f) / (tanf(25.0f * PI_F / 180) * asp), 12, 26);
    vh = view_h();
    cam.fov = 50;
    cam.at = v3(0, cam_bot + vh * 0.5f, 0);
    cam.pos = v3(0, cam_bot + vh * 0.5f + 0.6f, cam_dist);
    cam.yaw = PI_F;
    if (cam.shake > 0) { cam.pos.y += frands() * cam.shake; cam.shake -= DT; }
}

/* ---------------------------------------------------------------- drawing */
static void cube(float x, float y, float z0, float z1, float s, float r, float g, float b, int top_snow)
{
    float x0 = x - s, x1 = x + s, y0 = y, y1 = y + 2 * s;
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(x0, y0, z1); glVertex3f(x1, y0, z1); glVertex3f(x1, y1, z1); glVertex3f(x0, y1, z1);
    glNormal3f(0, -1, 0);
    glVertex3f(x0, y0, z0); glVertex3f(x1, y0, z0); glVertex3f(x1, y0, z1); glVertex3f(x0, y0, z1);
    glNormal3f(-1, 0, 0);
    glVertex3f(x0, y0, z0); glVertex3f(x0, y0, z1); glVertex3f(x0, y1, z1); glVertex3f(x0, y1, z0);
    glNormal3f(1, 0, 0);
    glVertex3f(x1, y0, z1); glVertex3f(x1, y0, z0); glVertex3f(x1, y1, z0); glVertex3f(x1, y1, z1);
    if (top_snow) glColor3f(0.95f, 0.97f, 1.0f);
    glNormal3f(0, 1, 0);
    glVertex3f(x0, y1, z1); glVertex3f(x1, y1, z1); glVertex3f(x1, y1, z0); glVertex3f(x0, y1, z0);
    glEnd();
}

static void block_color(int t, int r, int c, float *col)
{
    float v = 0.9f + 0.1f * ((c * 7 + r * 3) % 5) / 4.0f;
    int f = r / 4;
    switch (t) {
    case TBRK:
        if (f <= 2) { col[0] = 0.72f * v; col[1] = 0.45f * v; col[2] = 0.28f * v; }        /* earth */
        else if (f <= 5) { col[0] = 0.35f * v; col[1] = 0.7f * v; col[2] = 0.45f * v; }   /* moss */
        else { col[0] = 0.55f * v; col[1] = 0.8f * v; col[2] = 1.0f * v; }                /* ice */
        break;
    case THARD: col[0] = 0.45f * v; col[1] = 0.48f * v; col[2] = 0.58f * v; break;
    case TCONVR: case TCONVL: col[0] = 0.3f; col[1] = 0.3f; col[2] = 0.35f; break;
    case TSLIP: col[0] = 0.75f * v; col[1] = 0.95f * v; col[2] = 1.0f; break;
    case TPUSHED: col[0] = 0.7f; col[1] = 0.9f; col[2] = 1.0f; break;
    default: col[0] = 0.6f * v; col[1] = 0.62f * v; col[2] = 0.75f * v; break;
    }
}

static void draw_hammer(float x, float y, float ang)
{
    /* ang: 0 = hammer raised straight up, +PI/2 = pointing forward (in the facing direction) */
    glPushMatrix();
    glTranslatef(x, y, 0.35f);
    glRotatef(-DEG(ang) * face, 0, 0, 1);
    glColor3f(0.55f, 0.38f, 0.2f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.05f, 0, 0.05f); glVertex3f(0.05f, 0, 0.05f); glVertex3f(0.05f, 0.9f, 0.05f); glVertex3f(-0.05f, 0.9f, 0.05f);
    glEnd();
    glColor3f(0.75f, 0.78f, 0.82f);
    glBegin(GL_QUADS);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.25f, 0.8f, 0.12f); glVertex3f(0.25f, 0.8f, 0.12f); glVertex3f(0.25f, 1.08f, 0.12f); glVertex3f(-0.25f, 1.08f, 0.12f);
    glNormal3f(0, 1, 0);
    glVertex3f(-0.25f, 1.08f, 0.12f); glVertex3f(0.25f, 1.08f, 0.12f); glVertex3f(0.25f, 1.08f, -0.12f); glVertex3f(-0.25f, 1.08f, -0.12f);
    glEnd();
    glPopMatrix();
}

static void draw_bird(float x, float y, float dir, float ph, float s)
{
    float flap = sinf(ph * 14) * 0.6f;
    glPushMatrix();
    glTranslatef(x, y, 0.3f);
    glScalef(dir * s, s, s);
    glColor3f(0.3f, 0.45f, 0.85f);
    glBegin(GL_TRIANGLES);
    glNormal3f(0, 0, 1);
    glVertex3f(-0.45f, -0.1f, 0); glVertex3f(0.45f, 0.05f, 0); glVertex3f(-0.35f, 0.25f, 0);   /* body */
    glColor3f(0.25f, 0.35f, 0.7f);
    glVertex3f(-0.1f, 0.1f, 0.01f); glVertex3f(0.15f, 0.1f, 0.01f); glVertex3f(-0.3f, 0.1f + 0.6f * cosf(flap), 0.01f);
    glColor3f(1.0f, 0.75f, 0.1f);
    glVertex3f(0.45f, 0.05f, 0.02f); glVertex3f(0.65f, 0.0f, 0.02f); glVertex3f(0.45f, 0.13f, 0.02f);   /* beak */
    glEnd();
    glPopMatrix();
}

static void draw_tux(float x)
{
    TuxPose tp;
    float wt = walk_t * 0.35f;
    memset(&tp, 0, sizeof tp);
    tp.flipL_z = tp.flipR_z = 0.2f;
    if (ground && fabsf(vx) > 0.1f && hammer_t == 0) { tp.footL = sinf(wt) * 0.8f; tp.footR = -tp.footL; tp.body_roll = sinf(wt) * 0.08f; }
    if (!ground) { tp.flipR_z = 2.8f; tp.footL = 0.4f; tp.footR = -0.3f; }
    if (hammer_t > 0) tp.flipR_z = 1.2f + 1.5f * clampf((hammer_t - 8) / 12.0f, 0, 1);
    draw_tux_model(v3(x, py, 0), face > 0 ? PI_F / 2 : -PI_F / 2, &tp, 0.92f, TUXV_TUX);
    glDisable(GL_TEXTURE_2D);
    {
        float ang = 0.4f;
        if (!ground) ang = 0;   /* hammer up: it is what breaks the blocks */
        if (hammer_t > 0) ang = lerpf(PI_F * 0.65f, -0.5f, clampf((hammer_t - 8) / 12.0f, 0, 1));
        draw_hammer(x + face * 0.3f, py + 0.85f, ang);
    }
}

void ice_draw(void)
{
    int r, c, r0, r1, i;
    float col[3];
    if (!lv.ice_mode) return;
    r0 = rowf(cam_bot) - 2;
    r1 = rowf(cam_bot + view_h()) + 2;
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_LIGHTING);
    for (r = r0; r <= r1; r++) {
        if (r < -6 || r >= IROWS) continue;
        for (c = -3; c < IW + 3; c++) {
            int t = tile(c, r);
            float x = col_x(c);
            if (t == TE) continue;
            block_color(t, r, c, col);
            cube(x, r * IT, -1.0f, 0.6f, IT * 0.5f - 0.02f, col[0], col[1], col[2], breakable(t) && !solid(c, r + 1));
            if (t == TCONVR || t == TCONVL) {   /* moving chevrons on the conveyor */
                float sh = fmodf(g_frame * 0.03f * (t == TCONVR ? 1 : -1) + c * 0.5f, 1.0f);
                glColor3f(1.0f, 0.8f, 0.2f);
                glBegin(GL_TRIANGLES);
                glNormal3f(0, 0, 1);
                {
                    float cx = x - IT * 0.5f + sh * IT, dirc = t == TCONVR ? 1 : -1;
                    glVertex3f(cx - 0.12f * dirc, r * IT + 0.25f, 0.62f); glVertex3f(cx + 0.12f * dirc, r * IT + 0.4f, 0.62f);
                    glVertex3f(cx - 0.12f * dirc, r * IT + 0.55f, 0.62f);
                }
                glEnd();
            }
        }
    }
    /* clouds */
    for (i = 0; i < nclouds; i++) {
        Cloud *cl = &clouds[i];
        int k;
        float y = (cl->row + 1) * IT;
        if (y < cam_bot - 2 || y > cam_bot + view_h() + 2) continue;
        for (k = -1; k <= 1; k++) {
            float x = cl->x + k * WRAPW;
            if (fabsf(x) > HWID + cl->w) continue;
            cube(x - cl->w * 0.25f, y - 0.4f, -0.4f, 0.4f, cl->w * 0.25f, 0.97f, 0.97f, 1.0f, 0);
            cube(x + cl->w * 0.25f, y - 0.4f, -0.4f, 0.4f, cl->w * 0.25f, 0.97f, 0.97f, 1.0f, 0);
        }
    }
    /* icicles */
    for (i = 0; i < nicicles; i++) {
        Icicle *ic = &icicles[i];
        float x = col_x(ic->col), y = ic->state == 2 ? ic->y : ic->row * IT, len = ic->state == 2 ? 0.8f : 0.8f * ic->grow;
        if (ic->state == 3 || len < 0.05f) continue;
        if (ic->state == 1) x += sinf(g_frame * 1.7f) * 0.04f;
        glColor3f(0.8f, 0.92f, 1.0f);
        glBegin(GL_TRIANGLES);
        glNormal3f(0, 0, 1);
        glVertex3f(x - 0.18f, y, 0.4f); glVertex3f(x + 0.18f, y, 0.4f); glVertex3f(x, y - len, 0.4f);
        glEnd();
    }
    /* seals and their blocks of ice */
    for (i = 0; i < ntopi; i++) {
        Topi *t = &topi[i];
        float y = t->state == 3 ? t->y : (t->row + 1) * IT;
        if (t->state == 4) continue;
        glPushMatrix();
        glTranslatef(t->x, y, 0.1f);
        glRotatef(t->dir > 0 ? 90 : -90, 0, 1, 0);
        if (t->state == 3) glRotatef(180, 0, 0, 1);
        glScalef(0.5f, 0.5f, 0.5f);
        glCallList(mdl[MDL_SEAL]);
        glPopMatrix();
        glDisable(GL_TEXTURE_2D);
        if (t->state == 2) cube(t->bx, (t->row + 1) * IT, -0.3f, 0.5f, IT * 0.45f, 0.7f, 0.9f, 1.0f, 0);
    }
    /* birds */
    glDisable(GL_LIGHTING);
    for (i = 0; i < 3; i++) if (birds[i].alive) draw_bird(birds[i].x, birds[i].y, birds[i].vx > 0 ? 1 : -1, birds[i].ph, 1.0f);
    glEnable(GL_LIGHTING);
    /* the Ice Brute in sunglasses */
    if (bear.active) {
        glPushMatrix();
        glTranslatef(bear.x, bear.y + (bear.state == 1 ? 0.6f : 0), 0);
        glRotatef(90, 0, 1, 0);
        glScalef(0.9f, 0.9f, 0.9f);
        glCallList(mdl[MDL_BRUTE]);
        glPopMatrix();
        glDisable(GL_TEXTURE_2D);
        cube(bear.x + 0.25f, bear.y + 1.55f, 0.6f, 0.75f, 0.12f, 0.05f, 0.05f, 0.05f, 0);
        cube(bear.x + 0.55f, bear.y + 1.55f, 0.6f, 0.75f, 0.12f, 0.05f, 0.05f, 0.05f, 0);
    }
    /* bonus fish and the condor */
    for (i = 0; i < nfish; i++) {
        static const int fm[4] = { MDL_COIN, MDL_COIN_RED, MDL_COIN_BLUE, MDL_FISH_GREEN };
        Fish *f = &fish[i];
        if (f->got || f->y < cam_bot - 2 || f->y > cam_bot + view_h() + 2) continue;
        glPushMatrix();
        glTranslatef(f->x, f->y + 0.4f + sinf(g_frame * 0.08f + i) * 0.1f, 0.2f);
        glRotatef(g_frame * 3.0f, 0, 1, 0);
        glScalef(1.2f, 1.2f, 1.2f);
        glCallList(mdl[fm[f->kind]]);
        glPopMatrix();
    }
    glDisable(GL_TEXTURE_2D);
    if (CONDOR_ROW * IT < cam_bot + view_h() + 4) {
        float y = CONDOR_ROW * IT + sinf(g_frame * 0.06f) * 0.3f;
        glDisable(GL_LIGHTING);
        glPushMatrix();
        glTranslatef(condor_x, y, 0.3f);
        glScalef(condor_dir * 2.4f, 2.4f, 2.4f);
        glColor3f(0.4f, 0.25f, 0.15f);
        glBegin(GL_TRIANGLES);
        glNormal3f(0, 0, 1);
        glVertex3f(-0.6f, 0, 0); glVertex3f(0.5f, 0.1f, 0); glVertex3f(-0.4f, 0.3f, 0);
        glVertex3f(-0.2f, 0.15f, 0.01f); glVertex3f(0.2f, 0.15f, 0.01f); glVertex3f(-0.5f, 0.15f + 0.9f * sinf(g_frame * 0.2f), 0.01f);
        glColor3f(1, 1, 1);
        glVertex3f(0.35f, 0.08f, 0.02f); glVertex3f(0.6f, 0.15f, 0.02f); glVertex3f(0.4f, 0.3f, 0.02f);
        glEnd();
        glPopMatrix();
        glEnable(GL_LIGHTING);
        if (!cleared) {   /* the Kernel Star it carries */
            glPushMatrix();
            glTranslatef(condor_x, y - 0.8f, 0.3f);
            glRotatef(g_frame * 4.0f, 0, 1, 0);
            glScalef(0.6f, 0.6f, 0.6f);
            glCallList(mdl[MDL_STAR]);
            glPopMatrix();
        }
    }
    /* Tux, also drawn on the far side when he wraps around the edge */
    if (pl.act != ACT_DEATH && pl.act != ACT_STAR_DANCE && pl.visible && !(pl.invinc > 0 && (pl.invinc / 3) % 2 == 0)) {
        draw_tux(px);
        if (px > HWID - 1.2f) draw_tux(px - WRAPW);
        if (px < -HWID + 1.2f) draw_tux(px + WRAPW);
    }
    glEnable(GL_TEXTURE_2D);
}

void ice_hud(void)
{
    char buf[64];
    if (!lv.ice_mode) return;
    snprintf(buf, sizeof buf, tr("SCORE %06d"), g_ice_score);
    gfx_text_shadow(g_vw * 0.5f - gfx_text_width(buf, 2) * 0.5f, 60, 2, buf, 1, 1, 1, 1);
    if (bonus) {
        snprintf(buf, sizeof buf, tr("BONUS  %d"), (bonus_t + 59) / 60);
        gfx_text_shadow(g_vw * 0.5f - gfx_text_width(buf, 2) * 0.5f, 92, 2, buf, 1.0f, bonus_t < 600 ? 0.4f : 0.9f, 0.3f, 1);
    } else {
        snprintf(buf, sizeof buf, tr("MOUNTAIN %d  FLOOR %d"), g_ice_mountain, best_floor);
        gfx_text_shadow(g_vw * 0.5f - gfx_text_width(buf, 1.5f) * 0.5f, 92, 1.5f, buf, 0.8f, 0.9f, 1, 1);
    }
}

/* debug: where Tux is on the grid (bot tests) */
int ice_debug_floor(void) { return best_floor; }
int ice_debug_bonus(void) { return bonus; }

/* ---------------------------------------------------------------- the play-test autopilot (bot.c) */
/* where a jump from (x, y) with sideways speed jv lands (the landing height), or -1e9 if it falls away */
static float sim_jump(float x, float y, float jv, float *lx)
{
    float v = JUMPV;
    int i;
    for (i = 0; i < 240; i++) {
        float ny;
        v -= GRAV * DT;
        if (v < -18) v = -18;
        if (!body_hits_wall(wrapx(x + jv * DT), y)) x = wrapx(x + jv * DT);
        ny = y + v * DT;
        if (v > 0) {
            int r = rowf(ny + P_H), c, hit = 0;
            for (c = colf(x - P_HW); c <= colf(x + P_HW); c++) if (solid(c, r)) hit = 1;
            if (hit) { v = 0; ny = r * IT - P_H - 0.001f; }
        } else {
            int r = rowf(ny), c, hit = 0;
            for (c = colf(x - P_HW + 0.08f); c <= colf(x + P_HW - 0.08f); c++) if (solid(c, r)) hit = 1;
            if (hit && y >= (r + 1) * IT - 0.05f) { *lx = x; return (r + 1) * IT; }
        }
        y = ny;
    }
    return -1e9f;
}

/* Climbs like a player would: if the floor above has a hole two tiles wide, walk up under it and jump
 * through while moving; otherwise stand under two breakable blocks and knock them out first.
 * Fills *sx (stick) and *buttons. */
void ice_bot(float *sx, unsigned *buttons)
{
    static int jump_cool;
    int r = rowf(py - 0.05f), up = r + 4, c, best = -1, i;
    float bestd = 1e9f, tx;
    *sx = 0;
    *buttons = 0;
    if (jump_cool > 0) jump_cool--;
    if (!ground || jump_cool > 0) return;
    /* seals and birds close by: turn to face them and swing the hammer */
    for (i = 0; i < ntopi; i++) {
        Topi *t = &topi[i];
        float d = dxw(t->state == 2 ? t->bx : t->x, px);
        if (t->state <= 2 && t->row == r && fabsf(d) < 1.9f && (fabsf(d) < 1.2f || t->dir * d < 0)) {   /* close or coming closer */
            if ((d > 0) != (face > 0)) { *sx = d > 0 ? 0.3f : -0.3f; return; }
            if (hammer_t == 0) *buttons = BT_B;
            return;
        }
    }
    for (i = 0; i < 3; i++)
        if (birds[i].alive && fabsf(dxw(birds[i].x, px)) < 2.2f && fabsf(birds[i].y - py - 0.7f) < 1.0f) {
            float d = dxw(birds[i].x, px);
            if ((d > 0) != (face > 0)) { *sx = d > 0 ? 0.3f : -0.3f; return; }
            if (hammer_t == 0) *buttons = BT_B;
            return;
        }
    if (r < 32) for (i = 0; i < IW; i++) {   /* a hole (two empty tiles) in the floor above, with floor beyond it */
        c = colf(px) + ((i & 1) ? (i + 1) / 2 : -(i / 2));
        if (!solid(c, up) && !solid(c + 1, up) && (solid(c - 1, up) || solid(c + 2, up)) && !solid(c, r + 1)) {
            float hx = col_x(c) + IT * 0.5f, d = fabsf(dxw(hx, px));
            int k, step = c >= colf(px) ? 1 : -1, ok = solid(c, r) && solid(c + 1, r);
            for (k = colf(px); ok && k != c + step; k += step) if (!solid(k, r)) ok = 0;   /* no hole on the way */
            if (d < bestd && ok) { bestd = d; best = c; }
        }
    }
    if (r >= 32) {   /* bonus stage: try out jumps and take the one that lands highest */
        static float plan_x;
        static int plan_dir, plan_ok;
        if (rowf(py) >= CONDOR_ROW - 3) {   /* on the top platform: wait under the condor's path */
            *sx = fabsf(dxw(condor_x, px)) > 3 ? (dxw(condor_x, px) > 0 ? 0.4f : -0.4f) : 0;
            return;
        }
        if (!plan_ok || fabsf(py - plan_ok * 0.001f) > 0.01f) {
            float best_gain = 0.5f;
            int k, d;
            plan_ok = 0;
            for (k = -16; k <= 16; k++) {
                float sxp = wrapx(px + k * IT * 0.5f);
                int kk, step = k >= 0 ? 1 : -1, ok = 1;
                if (floor_under(sxp, py, NULL) < 0) continue;
                for (kk = 0; kk != k && ok; kk += step)
                    if (floor_under(wrapx(px + kk * IT * 0.5f), py, NULL) < 0 || body_hits_wall(wrapx(px + kk * IT * 0.5f), py)) ok = 0;
                if (!ok) continue;
                for (d = -1; d <= 1; d++) {
                    float lx, ly = sim_jump(sxp, py, d * WALK, &lx), gain = ly - py - fabsf((float)k) * 0.04f;
                    if (ly > -1e8f && gain > best_gain) { best_gain = gain; plan_x = sxp; plan_dir = d; plan_ok = (int)(py * 1000); }
                }
            }
            if (!plan_ok) { *sx = 0.5f; return; }
        }
        if (fabsf(dxw(plan_x, px)) > 0.06f) { *sx = dxw(plan_x, px) > 0 ? 0.5f : -0.5f; return; }
        *sx = (float)plan_dir;
        *buttons = BT_A;
        jump_cool = 30;
        plan_ok = 0;
        return;
    }
    if (best >= 0) {
        int dir = solid(best + 2, up) ? 1 : -1;
        tx = col_x(best) + IT * 0.5f - dir * 0.25f;
        if (fabsf(dxw(tx, px)) > 0.08f) { *sx = dxw(tx, px) > 0 ? 0.6f : -0.6f; return; }
        *sx = (float)dir;
        *buttons = BT_A;
        jump_cool = 50;
        return;
    }
    /* no hole: break the breakable block above that is nearest */
    for (i = 0; i < IW; i++) {
        c = colf(px) + ((i & 1) ? (i + 1) / 2 : -(i / 2));
        if (breakable(tile(c, up)) && solid(c, r) && solid(c + 1, r)) { best = c; break; }
    }
    if (best < 0) { *sx = 1; return; }
    tx = col_x(best);
    if (fabsf(dxw(tx, px)) > 0.06f) { *sx = dxw(tx, px) > 0 ? 0.5f : -0.5f; return; }
    *buttons = BT_A;
    jump_cool = 45;
}
