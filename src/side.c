/* side.c - TTY WORLD 1-1: a side-scrolling stage in the style of the classic 2D platformers
 *
 * The camera looks at the course from the side and Tux can only run left and right (the course is
 * a plane at z = 0). ? blocks give fish when bumped from below, bricks shatter, walkers march along
 * the ground, cannons fire bullets, a warp pipe leads to a bonus room underground and the course
 * ends at a flagpole in front of a little castle. Everything sits on a grid of 1.6-unit tiles. */
#include "game.h"
#include <stdio.h>
#include <string.h>

#define PR 0.42f
#define PH 1.6f
#define HALF (SIDE_TILE * 0.5f)

static float cam_x, cam_y;
static int cam_snap;
static GLuint dl_qblock, dl_used, dl_brick, dl_walker, dl_cannon, dl_bullet, dl_flag;

void side_reset(void)
{
    cam_snap = 1;
}

/* ---------------------------------------------------------------- camera: fixed side view */
void side_camera(void)
{
    float tx = pl.pos.x + clampf(pl.vel.x * 0.35f, -4, 4), ty = cam_y;
    float under = pl.pos.y < -20;
    /* vertical: follow when Tux leaves a band around the view centre, settle on the ground */
    if (!player_in_air() || pl.pos.y < cam_y - 3.5f || pl.pos.y > cam_y + 4.5f) ty = pl.pos.y;
    if (cam_snap) { cam_x = pl.pos.x; cam_y = pl.pos.y; cam_snap = 0; }
    cam_x += (tx - cam_x) * minf(1.0f, 5.0f * DT);
    cam_y += (ty - cam_y) * minf(1.0f, (fabsf(ty - cam_y) > 8 ? 12.0f : 3.0f) * DT);
    cam.at = v3(cam_x, cam_y + 3.6f, 0);
    cam.pos = v3(cam_x, cam_y + 4.4f, under ? 17.0f : 19.0f);
    cam.yaw = PI_F;   /* looking toward -z: right on screen is +x */
    cam.fov = 50;
    if (cam.shake > 0) {
        cam.pos = v3add(cam.pos, v3(frands() * cam.shake, frands() * cam.shake, 0));
        cam.shake -= DT;
    }
}

/* ---------------------------------------------------------------- blocks */
/* Tux's head hits the underside of a block this frame */
static int bumped_from_below(Obj *o)
{
    float head = pl.pos.y + PH, bottom = o->pos.y - HALF;
    if (!player_in_air() || pl.vel.y <= 0) return 0;
    if (fabsf(pl.pos.x - o->pos.x) > HALF + PR * 0.4f) return 0;
    return head <= bottom + 0.15f && head + pl.vel.y * DT >= bottom - 0.05f;
}

static void block_bonk(void)
{
    pl.vel.y = minf(pl.vel.y, -1.0f);
    sfx_ex(SFX_THUD, 0.8f, 1.4f);
}

/* ? block: param = 0 fish, 1 ten fish, 2 a 1-UP, 3 a Kernel Star (param2), 4 a red fish;
 * i[0] = hidden (invisible and not solid until bumped) */
static void qblock_update(Obj *o)
{
    if (o->f[0] > 0) o->f[0] = maxf(0, o->f[0] - DT * 6);   /* bump animation */
    if (o->state == 1) return;                               /* used up */
    if (!bumped_from_below(o)) return;
    block_bonk();
    o->f[0] = 1;
    o->i[0] = 0;
    switch (o->param) {
    case 0: game_add_coins(1, o->pos); fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(0, 1.4f, 0)), 0.8f); o->state = 1; break;
    case 1:
        game_add_coins(1, o->pos);
        fx_spawn(OT_FX_SPARKLE, v3add(o->pos, v3(0, 1.4f, 0)), 0.8f);
        if (++o->i[1] >= 10) o->state = 1;
        break;
    /* items pop out underneath, into Tux's flippers (blocks can be too high to climb onto) */
    case 2: obj_spawn(OT_ONEUP, v3add(o->pos, v3(0, -HALF - 2.2f, 0)), 0, 0); sfx(SFX_STAR_APPEAR); o->state = 1; break;
    case 3: star_spawn(v3add(o->pos, v3(0, -HALF - 2.8f, 0)), o->param2, 1, o->pos); o->state = 1; break;
    case 4: obj_spawn(OT_REDCOIN, v3add(o->pos, v3(0, -HALF - 2.2f, 0)), 0, 0); sfx(SFX_STAR_APPEAR); o->state = 1; break;
    }
}

static void brick_break(Obj *o)
{
    int k;
    for (k = 0; k < 4; k++) fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(frands() * 0.6f, frands() * 0.6f, 0)), 0.7f);
    fx_spawn(OT_FX_EXPLODE, o->pos, 0.5f);
    sfx(SFX_BREAK);
    if (o->param > 0) game_add_coins(o->param, o->pos);
    obj_kill(o);
}

static void brick_update(Obj *o)
{
    if (o->f[0] > 0) o->f[0] = maxf(0, o->f[0] - DT * 6);
    if (bumped_from_below(o)) { block_bonk(); brick_break(o); return; }
    /* a ground pound on top breaks it too */
    if (pl.act == ACT_GPOUND_LAND && pl.act_t <= 1 && pl.platform == obj_index(o)) brick_break(o);
}

/* ---------------------------------------------------------------- walkers, cannons, bullets */
static void walker_update(Obj *o)
{
    /* f[0] = direction (+1 / -1); state 90 = squashed */
    float dx = pl.pos.x - o->pos.x;
    o->timer++;
    if (o->state == 90) { if (o->timer > 40) obj_kill(o); return; }
    if (fabsf(dx) > 34) return;   /* asleep until Tux comes near, like in the old games */
    {
        Surf *s;
        vec3 np = v3(o->pos.x + o->f[0] * 1.8f * DT, o->pos.y, 0), wn, ahead;
        float fy;
        if (col_walls(&np, 0.45f, 0.45f, &wn, NULL)) o->f[0] = -o->f[0];
        ahead = v3(np.x + o->f[0] * 0.6f, np.y, 0);
        fy = col_floor(ahead.x, ahead.y + 0.6f, ahead.z, &s);
        if (!s || fy < np.y - 1.0f) o->f[0] = -o->f[0];   /* turn back at a ledge */
        else o->pos = np;
        fy = col_floor(o->pos.x, o->pos.y + 0.6f, 0, &s);
        if (s) o->pos.y = fy;
    }
    o->yaw = o->f[0] > 0 ? PI_F / 2 : -PI_F / 2;
    if (fabsf(dx) < 0.55f + PR && pl.pos.y < o->pos.y + 1.0f && pl.pos.y + PH > o->pos.y) {
        if (player_in_air() && pl.vel.y < 0 && pl.pos.y > o->pos.y + 0.4f) {
            o->state = 90;
            o->timer = 0;
            sfx(SFX_STOMP);
            player_bounce((in.held & BT_A) ? 15 : 10);
            game_add_coins(1, o->pos);
        } else if (pl.act == ACT_BELLY_SLIDE || pl.act == ACT_DIVE || player_is_attacking()) {
            o->state = 90;
            o->timer = 0;
            sfx(SFX_POP);
            game_add_coins(1, o->pos);
        } else player_hurt(1, o->pos);
    }
}

static void cannon_update(Obj *o)
{
    float dx = pl.pos.x - o->pos.x;
    o->timer++;
    if (fabsf(dx) < 26 && fabsf(dx) > 2.5f && o->timer % 170 == (o->param * 60) % 170) {
        Obj *b = obj_spawn(OT_SIDEBULLET, v3add(o->pos, v3(dx < 0 ? -1.0f : 1.0f, 1.25f, 0)), 0, 0);
        if (b) { b->f[0] = dx < 0 ? -1 : 1; b->flags |= OF_NOCULL; }
        fx_spawn(OT_FX_SMOKE, v3add(o->pos, v3(dx < 0 ? -1.2f : 1.2f, 1.25f, 0)), 0.8f);
        sfx_ex(SFX_BLAST, 0.6f, 1.3f);
    }
}

static void bullet_update(Obj *o)
{
    o->timer++;
    if (o->state == 90) {   /* stomped: falls away */
        o->vel.y -= 30 * DT;
        o->pos = v3mad(o->pos, o->vel, DT);
        if (o->timer > 90) obj_kill(o);
        return;
    }
    o->pos.x += o->f[0] * 7.0f * DT;
    o->yaw = o->f[0] > 0 ? PI_F / 2 : -PI_F / 2;
    if (o->timer > 600) { obj_kill(o); return; }
    if (fabsf(pl.pos.x - o->pos.x) < 0.7f + PR && pl.pos.y < o->pos.y + 0.55f && pl.pos.y + PH > o->pos.y - 0.55f) {
        if (player_in_air() && pl.vel.y < 0 && pl.pos.y > o->pos.y + 0.1f) {
            o->state = 90;
            o->timer = 0;
            o->vel = v3(o->f[0] * 2, 0, 0);
            sfx(SFX_STOMP);
            player_bounce(12);
        } else player_hurt(1, o->pos);
    }
}

/* ---------------------------------------------------------------- warp pipes and the flagpole */
static void pipe_update(Obj *o)
{
    /* standing on top of the pipe and pressing Z (crouch) warps to f[0..2] */
    if (!player_in_air() && fabsf(pl.pos.x - o->pos.x) < 0.9f && fabsf(pl.pos.y - o->pos.y) < 0.2f && (in.pressed & BT_Z) &&
        pl.act != ACT_STAR_DANCE && pl.act != ACT_DEATH) {
        pl.pos = v3(o->f[0], o->f[1], o->f[2]);
        pl.vel = v3(0, 0, 0);
        pl.fwd = 0;
        player_set_act(ACT_IDLE);
        sfx(SFX_WARP);
        cam_snap = 1;
    }
}

static void flagpole_update(Obj *o)
{
    /* state 0: waiting, 1: Tux slides down, 2: done; f[0] = flag height */
    float top = o->pos.y + o->height;
    o->timer++;
    if (o->state == 0) {
        o->f[0] = o->height - 1.2f;
        if (fabsf(pl.pos.x - o->pos.x) < 0.3f + PR && pl.pos.y < top && pl.pos.y + PH > o->pos.y && pl.act != ACT_DEATH) {
            float h = pl.pos.y - o->pos.y;
            o->state = 1;
            o->timer = 0;
            sfx(SFX_GRAB);
            if (h > o->height - 4.5f) {
                hud_message("TOP OF THE FLAGPOLE!", 120);
                star_spawn(v3add(o->pos, v3(2.6f, -SIDE_TILE + 0.6f, 0)), 1, 1, pl.pos);   /* waits at the foot of the pole */
            } else {
                static char msg[48];
                int bonus = 1 + (int)(h / o->height * 9);
                snprintf(msg, sizeof msg, tr("Flagpole bonus: %d fish"), bonus);
                hud_message(msg, 120);
                game_add_coins(bonus, pl.pos);
            }
        }
    } else if (o->state == 1) {
        /* slide down the pole while the flag comes down to meet Tux */
        pl.no_ctrl = 1;
        if (pl.act != ACT_FROZEN) player_set_act(ACT_FROZEN);
        pl.pos.x = o->pos.x - 0.6f;
        pl.pos.y = maxf(o->pos.y, pl.pos.y - 9.0f * DT);
        pl.vel = v3(0, 0, 0);
        pl.floor_y = pl.pos.y;
        pl.yaw = PI_F / 2;
        o->f[0] = maxf(0.4f, o->f[0] - 9.0f * DT);
        if (o->timer % 6 == 0) sfx_ex(SFX_SLIDE, 0.5f, 1.4f);
        if (pl.pos.y <= o->pos.y + 0.01f && o->f[0] <= 0.41f) {
            o->state = 2;
            pl.no_ctrl = 0;
            player_set_act(ACT_IDLE);
            pl.pos.x = o->pos.x + 0.8f;
        }
    }
}

void side_obj_update(Obj *o)
{
    switch (o->type) {
    case OT_QBLOCK: qblock_update(o); break;
    case OT_BRICK: brick_update(o); break;
    case OT_WALKER: walker_update(o); break;
    case OT_SIDECANNON: cannon_update(o); break;
    case OT_SIDEBULLET: bullet_update(o); break;
    case OT_PIPEWARP: pipe_update(o); break;
    case OT_FLAGPOLE: flagpole_update(o); break;
    }
}

/* solid boxes of the blocks and the cannons */
int side_obj_solid(Obj *o)
{
    vec3 c;
    int idx = obj_index(o), i;
    float hx = HALF, hy = HALF, hz = HALF;
    vec3 p[8];
    if (o->type == OT_QBLOCK && o->i[0]) return 1;   /* hidden block: not there until bumped */
    if (o->type != OT_QBLOCK && o->type != OT_BRICK && o->type != OT_SIDECANNON) return 0;
    c = o->pos;
    if (o->type == OT_SIDECANNON) { hx = 0.75f; hy = 0.95f; hz = 0.75f; c.y += 0.95f; }
    for (i = 0; i < 8; i++) p[i] = v3add(c, v3((i & 1) ? hx : -hx, (i & 2) ? hy : -hy, (i & 4) ? hz : -hz));
#define Q(a, b, c2, d) col_dyn_add(p[a], p[b], p[c2], SF_NOCAM, idx); col_dyn_add(p[a], p[c2], p[d], SF_NOCAM, idx);
    Q(2, 6, 7, 3) Q(0, 1, 5, 4) Q(1, 3, 7, 5) Q(0, 4, 6, 2) Q(4, 5, 7, 6) Q(0, 2, 3, 1)
#undef Q
    return 1;
}

/* ---------------------------------------------------------------- models */
static Mesh M;
static void mb_start(void) { mesh_init(&M); mb_begin(&M); }
static GLuint mb_finish(void) { GLuint d; mb_end(); d = mesh_compile(&M); mesh_free(&M); return d; }

static void textured_cube(int tex, float r, float g, float b)
{
    mb_tex(tex);
    mb_uvscale(1.0f / SIDE_TILE);
    mb_color(r, g, b);
    mb_push();   /* built from 0 to one tile so that the texture maps once onto each face */
    mb_translate(HALF, HALF, HALF);
    mb_scale(HALF, HALF, HALF);
    mb_box();
    mb_pop();
    mb_tex(TEX_NONE);
}

static void build_models(void)
{
    int k;
    if (dl_qblock) return;
    mb_start(); textured_cube(TEX_QBLOCK, 1, 1, 1); dl_qblock = mb_finish();
    mb_start(); textured_cube(TEX_USEDBLOCK, 1, 1, 1); dl_used = mb_finish();
    mb_start(); textured_cube(TEX_BRICK, 0.85f, 0.45f, 0.3f); dl_brick = mb_finish();
    mb_start();   /* walker: a round grumpy bug on two big feet (front +z) */
    mb_color(0.55f, 0.3f, 0.2f);
    mb_ellipsoid(0, 0.6f, 0, 0.6f, 0.5f, 0.55f, 12);
    mb_color(0.95f, 0.85f, 0.7f);
    mb_ellipsoid(0, 0.35f, 0.05f, 0.45f, 0.25f, 0.45f, 10);
    mb_color(1, 1, 1);
    mb_ellipsoid(0.2f, 0.75f, 0.45f, 0.13f, 0.18f, 0.08f, 8);
    mb_ellipsoid(-0.2f, 0.75f, 0.45f, 0.13f, 0.18f, 0.08f, 8);
    mb_color(0.05f, 0.05f, 0.05f);
    mb_ellipsoid(0.18f, 0.73f, 0.52f, 0.06f, 0.09f, 0.04f, 6);
    mb_ellipsoid(-0.18f, 0.73f, 0.52f, 0.06f, 0.09f, 0.04f, 6);
    mb_cuboid(0.08f, 0.9f, 0.48f, 0.35f, 0.95f, 0.52f);   /* angry brows */
    mb_cuboid(-0.35f, 0.9f, 0.48f, -0.08f, 0.95f, 0.52f);
    mb_color(0.2f, 0.12f, 0.08f);
    mb_ellipsoid(0.28f, 0.1f, 0.05f, 0.2f, 0.12f, 0.3f, 8);
    mb_ellipsoid(-0.28f, 0.1f, 0.05f, 0.2f, 0.12f, 0.3f, 8);
    dl_walker = mb_finish();
    mb_start();   /* cannon: a black tower with a skull plate */
    mb_color(0.15f, 0.15f, 0.17f);
    mb_cuboid(-0.75f, 0, -0.75f, 0.75f, 0.7f, 0.75f);
    mb_push(); mb_translate(0, 1.25f, 0); mb_rotate(90, 0, 0, 1); mb_scale(0.6f, 1.6f, 0.6f); mb_translate(0, -0.5f, 0); mb_cylinder(14, 1); mb_pop();
    mb_color(0.9f, 0.9f, 0.85f);
    mb_cuboid(-0.3f, 0.15f, 0.76f, 0.3f, 0.55f, 0.78f);
    dl_cannon = mb_finish();
    mb_start();   /* bullet with an angry face (front +z) */
    mb_color(0.12f, 0.12f, 0.14f);
    mb_ellipsoid(0, 0, 0.1f, 0.5f, 0.5f, 0.7f, 12);
    mb_cuboid(-0.5f, -0.5f, -0.6f, 0.5f, 0.5f, -0.3f);
    mb_color(1, 1, 1);
    mb_ellipsoid(0.25f, 0.15f, 0.55f, 0.1f, 0.14f, 0.08f, 8);
    mb_ellipsoid(-0.25f, 0.15f, 0.55f, 0.1f, 0.14f, 0.08f, 8);
    for (k = 0; k < 2; k++) {   /* little arms */
        mb_ellipsoid(k ? 0.55f : -0.55f, -0.1f, -0.1f, 0.12f, 0.2f, 0.12f, 6);
    }
    dl_bullet = mb_finish();
    mb_start();   /* the flag: a triangle with a penguin emblem */
    mb_color(1, 1, 1);
    mb_tri(v3(0, 0, 0.02f), v3(0, 1.4f, 0.02f), v3(-2.0f, 0.7f, 0.02f));
    mb_tri(v3(0, 1.4f, -0.02f), v3(0, 0, -0.02f), v3(-2.0f, 0.7f, -0.02f));
    mb_color(0.1f, 0.1f, 0.12f);
    mb_ellipsoid(-0.65f, 0.7f, 0, 0.28f, 0.32f, 0.05f, 8);
    mb_color(1.0f, 0.65f, 0.1f);
    mb_ellipsoid(-0.65f, 0.6f, 0.03f, 0.1f, 0.06f, 0.04f, 6);
    dl_flag = mb_finish();
}

static void draw_at(GLuint dl, vec3 p, float yaw, float sy)
{
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    glRotatef(DEG(yaw), 0, 1, 0);
    if (sy != 1) glScalef(1, sy, 1);
    glCallList(dl);
    glPopMatrix();
}

void side_obj_draw(Obj *o)
{
    build_models();
    switch (o->type) {
    case OT_QBLOCK:
        if (o->i[0]) break;   /* hidden */
        draw_at(o->state ? dl_used : dl_qblock, v3add(o->pos, v3(-HALF, sinf(o->f[0] * PI_F) * 0.35f - HALF, -HALF)), 0, 1);
        break;
    case OT_BRICK: draw_at(dl_brick, v3add(o->pos, v3(-HALF, sinf(o->f[0] * PI_F) * 0.35f - HALF, -HALF)), 0, 1); break;
    case OT_WALKER:
        if (o->state == 90) { draw_at(dl_walker, o->pos, o->yaw, 0.3f); break; }
        draw_at(dl_walker, v3add(o->pos, v3(0, fabsf(sinf(o->timer * 0.25f)) * 0.08f, 0)), o->yaw + sinf(o->timer * 0.25f) * 0.25f, 1);
        break;
    case OT_SIDECANNON: draw_at(dl_cannon, o->pos, 0, 1); break;
    case OT_SIDEBULLET:
        glPushMatrix();
        glTranslatef(o->pos.x, o->pos.y, o->pos.z);
        glRotatef(DEG(o->yaw), 0, 1, 0);
        if (o->state == 90) glRotatef(180, 0, 0, 1);
        glCallList(dl_bullet);
        glPopMatrix();
        break;
    case OT_FLAGPOLE: {
        int k;
        glDisable(GL_TEXTURE_2D);
        glColor3f(0.7f, 0.85f, 0.7f);
        glBegin(GL_QUADS);
        for (k = 0; k < 4; k++) {   /* the pole */
            float a0 = k * PI_F / 2, a1 = (k + 1) * PI_F / 2, r = 0.12f;
            glNormal3f(sinf(a0 + PI_F / 4), 0, cosf(a0 + PI_F / 4));
            glVertex3f(o->pos.x + sinf(a0) * r, o->pos.y, o->pos.z + cosf(a0) * r);
            glVertex3f(o->pos.x + sinf(a1) * r, o->pos.y, o->pos.z + cosf(a1) * r);
            glVertex3f(o->pos.x + sinf(a1) * r, o->pos.y + o->height, o->pos.z + cosf(a1) * r);
            glVertex3f(o->pos.x + sinf(a0) * r, o->pos.y + o->height, o->pos.z + cosf(a0) * r);
        }
        glEnd();
        glPushMatrix();   /* the ball on top */
        glTranslatef(o->pos.x, o->pos.y + o->height + 0.3f, o->pos.z);
        glColor3f(0.3f, 0.8f, 0.3f);
        glBegin(GL_TRIANGLE_FAN);
        glNormal3f(0, 0, 1);
        glVertex3f(0, 0, 0.35f);
        for (k = 0; k <= 10; k++) glVertex3f(sinf(TAU_F * k / 10) * 0.35f, cosf(TAU_F * k / 10) * 0.35f, 0);
        glEnd();
        glPopMatrix();
        draw_at(dl_flag, v3add(o->pos, v3(-0.1f, o->f[0] - 0.7f, 0)), 0, 1);
        break;
    }
    }
}

/* ---------------------------------------------------------------- background: hills and clouds with parallax */
static void flat_disc(float cx, float cy, float z, float r, float cr, float cg, float cb)
{
    int k;
    glColor3f(cr, cg, cb);
    glBegin(GL_TRIANGLE_FAN);
    glVertex3f(cx, cy, z);
    for (k = 0; k <= 20; k++) glVertex3f(cx + sinf(TAU_F * k / 20) * r, cy + cosf(TAU_F * k / 20) * r, z);
    glEnd();
}

/* called from draw_sky (world coordinates, depth test off) */
void side_draw_sky(void)
{
    int k;
    float px;
    glDisable(GL_TEXTURE_2D);
    if (cam.pos.y < -18) {   /* underground: a dark wall of blue bricks behind everything */
        glColor3f(0.02f, 0.03f, 0.08f);
        glBegin(GL_QUADS);
        glVertex3f(cam.pos.x - 200, cam.pos.y - 100, -40); glVertex3f(cam.pos.x + 200, cam.pos.y - 100, -40);
        glVertex3f(cam.pos.x + 200, cam.pos.y + 100, -40); glVertex3f(cam.pos.x - 200, cam.pos.y + 100, -40);
        glEnd();
        glEnable(GL_TEXTURE_2D);
        return;
    }
    /* far snowy mountains (move slowest) */
    px = cam.pos.x * 0.7f;
    for (k = -6; k <= 30; k++) {
        float x = px + k * 26.0f - 40, h = 14 + 8 * fabsf(sinf(k * 1.7f));
        glBegin(GL_TRIANGLES);
        glColor3f(0.55f, 0.65f, 0.85f);
        glVertex3f(x - 18, -8, -90); glVertex3f(x + 18, -8, -90); glVertex3f(x, h, -90);
        glColor3f(0.95f, 0.97f, 1.0f);
        glVertex3f(x - 18 * 0.3f, h - (h + 8) * 0.3f, -90); glVertex3f(x + 18 * 0.3f, h - (h + 8) * 0.3f, -90); glVertex3f(x, h, -90);
        glEnd();
    }
    /* round green hills with snowy caps */
    px = cam.pos.x * 0.45f;
    for (k = -6; k <= 40; k++) {
        float x = px + k * 19.0f - 30, r = 6 + 4 * fabsf(sinf(k * 2.3f));
        flat_disc(x, -6 + r * 0.4f, -60, r, 0.45f, 0.75f, 0.45f);
        flat_disc(x, -6 + r * 1.2f, -59.9f, r * 0.45f, 0.97f, 0.98f, 1.0f);
    }
    /* puffy clouds */
    px = cam.pos.x * 0.25f;
    for (k = -4; k <= 30; k++) {
        float x = px + k * 23.0f - 20, y = 14 + 4 * sinf(k * 1.3f);
        flat_disc(x, y, -50, 2.2f, 1, 1, 1);
        flat_disc(x - 2.2f, y - 0.6f, -50, 1.7f, 1, 1, 1);
        flat_disc(x + 2.3f, y - 0.5f, -50, 1.8f, 1, 1, 1);
    }
    glEnable(GL_TEXTURE_2D);
}
