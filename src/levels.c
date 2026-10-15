/* levels.c - the castle hub, the courses and the boss lair */
#include "game.h"
#include <string.h>
#include <stdio.h>

/* ---------------------------------------------------------------- helpers */
static float sstep(float e0, float e1, float x)
{
    float t = clampf((x - e0) / (e1 - e0), 0, 1);
    return t * t * (3 - 2 * t);
}

/* distance from point to rectangle (0 inside) */
static float rect_dist(float x, float z, float x0, float z0, float x1, float z1)
{
    float dx = maxf(maxf(x0 - x, 0), x - x1), dz = maxf(maxf(z0 - z, 0), z - z1);
    return sqrtf(dx * dx + dz * dz);
}

/* depth inside rectangle (distance to nearest edge, <= 0 outside) */
static float rect_inside(float x, float z, float x0, float z0, float x1, float z1)
{
    return minf(minf(x - x0, x1 - x), minf(z - z0, z1 - z));
}

static float boundary(float x, float z, float start, float slope)
{
    float m = maxf(fabsf(x), fabsf(z));
    return m > start ? (m - start) * slope : 0;
}

static float floor_y(float x, float z, float ytop)
{
    Surf *s;
    float y = col_floor(x, ytop, z, &s);
    return s ? y : 0;
}

static Obj *spawn_at(int type, float x, float z, float ytop, float yaw, int param)
{
    return obj_spawn(type, v3(x, floor_y(x, z, ytop), z), yaw, param);
}

static void coin_line(vec3 a, vec3 b, int n, int snap)
{
    int i;
    for (i = 0; i < n; i++) {
        vec3 p = v3lerp(a, b, n > 1 ? (float)i / (n - 1) : 0);
        if (snap) p.y = floor_y(p.x, p.z, p.y + 2) + 0.15f;
        obj_spawn(OT_COIN, p, 0, 0);
    }
}

static void coin_ring(vec3 c, float r, int n, int snap)
{
    int i;
    for (i = 0; i < n; i++) {
        float a = TAU_F * i / n;
        vec3 p = v3(c.x + sinf(a) * r, c.y, c.z + cosf(a) * r);
        if (snap) p.y = floor_y(p.x, p.z, p.y + 2) + 0.15f;
        obj_spawn(OT_COIN, p, 0, 0);
    }
}

static Obj *sign(float x, float z, float ytop, float yaw, const char *text)
{
    Obj *o = spawn_at(OT_SIGN, x, z, ytop, yaw, 0);
    if (o) o->text = text;
    return o;
}

static Obj *npc(float x, float z, float ytop, float yaw, const char *text)
{
    Obj *o = spawn_at(OT_NPC, x, z, ytop, yaw, 0);
    if (o) o->text = text;
    return o;
}

static void tree(float x, float z, float ytop, int kind)
{
    Obj *o = spawn_at(OT_TREE, x, z, ytop, frand() * TAU_F, kind);
    if (o) o->height = kind == 2 ? 2.4f : 2.0f;
}

static void decor(int model, float x, float z, float ytop, float scale, float radius)
{
    Obj *o = spawn_at(OT_DECOR, x, z, ytop, frand() * TAU_F, model);
    if (o) { o->scale = scale; o->radius = radius; o->height = 3; }
}

static Obj *warp_box(vec3 p, float hx, float hz, float h, int level, int spawn, int select, int ret)
{
    Obj *o = obj_spawn(OT_WARP, p, 0, level);
    if (!o) return NULL;
    o->param2 = spawn;
    o->f[5] = hx; o->f[7] = hz;
    o->height = h;
    o->i[0] = select;
    o->i[2] = ret;
    return o;
}

static void set_sky(float tr, float tg, float tb, float br, float bg, float bb, float fs, float fe)
{
    lv.sky_top[0] = tr; lv.sky_top[1] = tg; lv.sky_top[2] = tb;
    lv.sky_bot[0] = br; lv.sky_bot[1] = bg; lv.sky_bot[2] = bb;
    lv.fog[0] = br; lv.fog[1] = bg; lv.fog[2] = bb;
    lv.fog_start = fs; lv.fog_end = fe;
}

/* invisible floor far below the playfield so that falling off edges is possible (and deadly) */
static void death_floor(float y, float x0, float z0, float x1, float z1)
{
    col_add_tri(v3(x0, y, z0), v3(x0, y, z1), v3(x1, y, z1), SF_DEATH | SF_NOCAM, 0);
    col_add_tri(v3(x0, y, z0), v3(x1, y, z1), v3(x1, y, z0), SF_DEATH | SF_NOCAM, 0);
}

static void finish_geometry(void)
{
    lb_end(&lv.dl);
    lb_end_alpha(&lv.dl_alpha);
    col_build_grid();
}

static void battlements(float x0, float z0, float x1, float z1, float y, int tex, float r, float g, float b)
{
    float x, z;
    for (x = x0; x <= x1 - 1.0f; x += 3) {
        lb_box(x, y, z0, x + 1.5f, y + 1.5f, z0 + 1, tex, tex, r, g, b, 0);
        lb_box(x, y, z1 - 1, x + 1.5f, y + 1.5f, z1, tex, tex, r, g, b, 0);
    }
    for (z = z0 + 3; z <= z1 - 3; z += 3) {
        lb_box(x0, y, z, x0 + 1, y + 1.5f, z + 1.5f, tex, tex, r, g, b, 0);
        lb_box(x1 - 1, y, z, x1, y + 1.5f, z + 1.5f, tex, tex, r, g, b, 0);
    }
}

/* ================================================================
 * CASTLE GROUNDS
 * ================================================================ */
static float grounds_h(float x, float z)
{
    float h = 0.6f * sinf(x * 0.07f) * cosf(z * 0.05f) + 0.35f * sinf(x * 0.13f + z * 0.11f) + 0.3f;
    float dc = rect_dist(x, z, -30, -72, 30, -24);
    float ds = sqrtf(x * x + (z - 42) * (z - 42));
    float dp = sqrtf((x - 45) * (x - 45) + (z - 5) * (z - 5));
    float path = fabsf(x) < 4 && z > -26 && z < 46 ? 0 : 1;
    h *= sstep(0, 10, dc) * sstep(4, 12, ds) * (path ? 1 : 0.2f);
    if (dp < 14) h = minf(h, -3.8f * (1 - (dp / 14) * (dp / 14)));
    h += boundary(x, z, 74, 2.6f);
    return h;
}

static void grounds_style(float x, float y, float z, vec3 n, int *tex, float *col, int *flags)
{
    (void)flags;
    if (n.y < 0.62f) { *tex = TEX_ROCK; return; }
    if (y < -0.4f) { *tex = TEX_SAND; return; }
    if (fabsf(x) < 3.2f && z > -38 && z < 44) { *tex = TEX_DIRT; return; }
    *tex = TEX_GRASS;
    col[0] = col[1] = col[2] = 0.93f + 0.07f * sinf(x * 0.3f + z * 0.2f);
}

static void build_grounds(void)
{
    int i;
    Obj *o;
    lv.name = "KERNEL CASTLE GROUNDS";
    lv.music = MUS_CASTLE;
    lv.course = -1;
    set_sky(0.45f, 0.6f, 0.85f, 0.84f, 0.9f, 0.97f, 60, 180);
    lv.ambient = 0.55f; lv.diffuse = 0.65f;
    lv.death_y = -30;
    lv.clouds = 1;
    lv.snow = 1;
    lb_begin();
    lb_terrain(-100, -100, 100, 100, 80, 80, grounds_h, grounds_style);
    /* the pond is frozen over, except for a fishing hole in the middle */
    lb_box(32, -5.0f, -9, 58, -0.6f, 1, TEX_ICE, TEX_ICE, 0.9f, 0.97f, 1, SF_SLIPPERY);
    lb_box(32, -5.0f, 9, 58, -0.6f, 19, TEX_ICE, TEX_ICE, 0.9f, 0.97f, 1, SF_SLIPPERY);
    lb_box(32, -5.0f, 1, 41, -0.6f, 9, TEX_ICE, TEX_ICE, 0.9f, 0.97f, 1, SF_SLIPPERY);
    lb_box(49, -5.0f, 1, 58, -0.6f, 9, TEX_ICE, TEX_ICE, 0.9f, 0.97f, 1, SF_SLIPPERY);
    lb_water(41, 1, 49, 9, -0.7f, -5);
    /* castle (snow on every roof) */
    lb_box(-22, -0.5f, -62, 22, 16, -38, TEX_SNOW, TEX_BRICK, 0.95f, 0.97f, 1.0f, 0);
    battlements(-22, -62, 22, -38, 16, TEX_BRICK, 0.95f, 0.97f, 1.0f);
    lb_box(-8, 16, -58, 8, 28, -44, TEX_SNOW, TEX_BRICK, 0.95f, 0.97f, 1.0f, 0);
    lb_cone(0, 28, -51, 11.5f, 9, 4, TEX_ROOF, 1, 1, 1, 1);
    {
        static const float tx[4] = { -22, 22, -22, 22 }, tz[4] = { -38, -38, -62, -62 };
        for (i = 0; i < 4; i++) {
            lb_cyl(tx[i], -0.5f, tz[i], 4, 22.5f, 16, TEX_SNOW, TEX_BRICK, 0.92f, 0.95f, 1.0f, 0);
            lb_cone(tx[i], 22, tz[i], 5, 7, 16, TEX_ROOF, 1, 1, 1, 1);
            lb_cone(tx[i], 26.5f, tz[i], 2.1f, 2.9f, 16, TEX_SNOW, 1, 1, 1, 0);
        }
    }
    /* door frame and a big window with a star */
    lb_box(-2.2f, 0, -38.2f, -1.6f, 4.2f, -37.6f, TEX_STONE, TEX_STONE, 0.8f, 0.8f, 0.85f, 0);
    lb_box(1.6f, 0, -38.2f, 2.2f, 4.2f, -37.6f, TEX_STONE, TEX_STONE, 0.8f, 0.8f, 0.85f, 0);
    lb_box(-2.2f, 3.6f, -38.2f, 2.2f, 4.2f, -37.6f, TEX_STONE, TEX_STONE, 0.8f, 0.8f, 0.85f, 0);
    lb_appwindow(v3(0, 10, -37.95f), 0, 6, 4.5f, TEX_CYDATA, 0.1f, 0.65f, 0.95f);   /* a holographic kernel window above the gate */
    /* footbridge over the frozen pond */
    lb_box(30, -0.6f, 12, 60, 0.0f, 16, TEX_PLANK, TEX_WOOD, 1, 1, 1, 0);
    finish_geometry();

    lv.spawn[0] = v3(0, floor_y(0, 45, 10), 45); lv.spawn_yaw[0] = PI_F;
    lv.spawn[1] = v3(0, floor_y(0, -34, 10), -34); lv.spawn_yaw[1] = 0;

    o = obj_spawn(OT_DOOR, v3(0, 0, -37.75f), 0, 0);
    if (o) { o->param2 = LV_LOBBY; o->i[0] = 0; }
    sign(3, 36, 5, PI_F, "KERNEL CASTLE\n\nThe castle has lost its Kernel Stars! Enter\n"
                       "the castle and jump into the glowing app\nwindows to reach the worlds inside.");
    sign(-6, 2, 5, 0.3f, "TUX'S MOVES:\nA (Space): jump. Jump again right after landing\n"
                         "for a double jump, and once more for a triple!\n"
                         "Crouch (Z) + A: backflip.  Run + Z + A: long jump.\n"
                         "Turn around while running + A: side flip.\n"
                         "B while running: dive into a belly slide!\n"
                         "In the air: Z = ground pound, B = kick/dive.\n"
                         "Jump into a wall, then A again: wall kick!");
    npc(9, -28, 5, PI_F * 0.8f, "PENGUIN: Segfault took over the castle! Inside\nit is all cyberspace now: the app windows\nlead to other worlds.\n"
                               "Some doors need Kernel Stars to open...\nCollect as many as you can, Tux!");
    npc(26, 6, 5, -PI_F / 2, "PENGUIN: The pond froze solid - perfect for\nbelly sliding! (Run + B to dive.) There's an\n"
                             "ice-fishing hole in the middle: swim with\nA (up), Z (down) and B (dash).");
    decor(MDL_SNOWMAN, -10, 36, 5, 1.0f, 1.0f);
    decor(MDL_SNOWMAN, 12, -30, 5, 0.8f, 0.8f);
    decor(MDL_CRYSTAL, -26, -34, 5, 1.2f, 0.6f);
    decor(MDL_CRYSTAL, 26, -34, 5, 1.2f, 0.6f);
    for (i = 0; i < 16; i++) {
        float a = TAU_F * i / 16 + 0.2f, r = 40 + (i % 3) * 9;
        float x = sinf(a) * r, z = cosf(a) * r;
        if (rect_dist(x, z, -30, -70, 30, -24) < 4) continue;
        if (fabsf(x) < 6 && z > -30) continue;
        if (sqrtf((x - 45) * (x - 45) + (z - 5) * (z - 5)) < 16) continue;
        tree(x, z, 20, i % 4 == 0 ? 2 : 0);
    }
    for (i = 0; i < 10; i++) decor(MDL_BUSH, -60 + i * 13, 60 - (i % 3) * 7, 20, 1.0f, 0.8f);
    coin_ring(v3(-30, 0, 20), 3, 8, 1);
    coin_ring(v3(45, -2.4f, 5), 2.5f, 6, 0);   /* in the ice-fishing hole */
    o = spawn_at(OT_ONEUP, -60, -60, 20, 0, 0);
    if (o) o->pos.y += 0.5f;
    coin_line(v3(-6, 0, 30), v3(6, 0, 30), 5, 1);
    /* rocket pack + secret roof star */
    obj_spawn(OT_ROCKETBOX, v3(14, floor_y(14, 28, 10) + 0.7f, 28), 0, 0);
    sign(16, 24, 10, PI_F, "ROCKET CRATE\nBreak the crate (punch it or jump on it) to\nget the Rocket Pack. "
                          "Jump, then press A in\nmid-air to ignite it. HOLD A to keep firing!\n"
                          "Stick forward = nose down, back = climb.\nSomething shiny sits on the castle roof...");
    obj_spawn(OT_STAR, v3(14, 16.3f, -50), 0, 0);
    coin_ring(v3(-14, 16.3f, -50), 2.5f, 8, 0);
}

/* ================================================================
 * CASTLE LOBBY
 * ================================================================ */
static void wall(float x0, float y0, float z0, float x1, float y1, float z1)
{
    lb_box(x0, y0, z0, x1, y1, z1, TEX_CYWALL, TEX_CYWALL, 1, 1, 1, 0);   /* castle interior = cyberspace */
}

static void lobby_label(vec3 c, float yaw, float h, const char *text, int course)
{
    if (lv.nlabels >= 16) return;
    lv.label[lv.nlabels].p = v3add(v3mad(c, yaw_dir(yaw), 0.4f), v3(0, h * 0.5f + 1.05f, 0));
    lv.label[lv.nlabels].text = text;
    lv.label[lv.nlabels].course = course;
    lv.nlabels++;
}

/* an app window that is the entrance to a course (with its floating title) */
static void lobby_window(vec3 c, float yaw, float w, float h, int tex, float r, float g, float b, int course)
{
    lb_appwindow(c, yaw, w, h, tex, r, g, b);
    lobby_label(c, yaw, h, courses[course].name, course);
}

static void build_lobby(void)
{
    Obj *o;
    lv.name = "KERNEL CASTLE";
    lv.music = MUS_CASTLE;
    lv.course = -1;
    /* inside the castle is cyberspace */
    set_sky(0.0f, 0.02f, 0.06f, 0.02f, 0.04f, 0.1f, 45, 150);
    lv.ambient = 0.88f; lv.diffuse = 0.25f;
    lv.death_y = -30;
    lv.snow = 3;   /* data rain */
    lb_begin();
    /* main hall */
    lb_box(-30, -1, -40, 30, 0, 10, TEX_CYGRID, TEX_CYWALL, 1, 1, 1, 0);
    lb_box(-4, 0, -20, 4, 0.05f, 10, TEX_CYGRID, TEX_CYGRID, 1, 0.35f, 1, 0);   /* glowing data path */
    lb_box(-31, 16, -41, 31, 17, 11, TEX_CYWALL, TEX_CYWALL, 0.55f, 0.55f, 0.7f, 0);
    /* west wall with the slide hole at balcony level */
    wall(-31, 0, -41, -30, 16, -38);
    wall(-31, 0, -34, -30, 16, 11);
    wall(-31, 0, -38, -30, 6, -34);
    wall(-31, 9.5f, -38, -30, 16, -34);
    /* east wall with the bay door opening (floor) and the manor alcove (balcony) */
    wall(30, 0, -41, 31, 6, -12);
    wall(30, 6, -41, 31, 16, -38);
    wall(30, 6, -34, 31, 16, -12);
    wall(30, 10, -38, 31, 16, -34);
    wall(30, 0, -8, 31, 16, 11);
    wall(30, 4, -12, 31, 16, -8);
    /* south wall */
    wall(-31, 0, 10, 31, 16, 11);
    /* north wall: final door (floor) and fortress door (balcony) */
    wall(-31, 0, -41, -20, 16, -40);
    wall(-20, 4, -41, -16, 16, -40);
    wall(-16, 0, -41, -2, 16, -40);
    wall(-2, 0, -41, 2, 6, -40);
    wall(-2, 10, -41, 2, 16, -40);
    wall(2, 0, -41, 31, 16, -40);
    /* balcony, stairs and neon railings */
    lb_box(-30, 5, -40, 30, 6, -32, TEX_CYGRID, TEX_CYWALL, 0.85f, 0.9f, 1, 0);
    /* door thresholds: floor inside the wall thickness of each doorway */
    lb_box(30, -1, -12, 31, 0, -8, TEX_CYGRID, TEX_CYWALL, 1, 1, 1, 0);     /* bay door */
    lb_box(-2, 5, -41, 2, 6, -40, TEX_CYGRID, TEX_CYWALL, 1, 1, 1, 0);      /* fortress door */
    lb_box(-20, -1, -41, -16, 0, -40, TEX_CYGRID, TEX_CYWALL, 1, 1, 1, 0);  /* final door */
    lb_box(30, 5, -38, 31, 6, -34, TEX_CYGRID, TEX_CYWALL, 1, 1, 1, 0);     /* manor door */
    lb_stairs(-4, -20, 4, -32, 0, 6, 1, 12, TEX_CYGRID, 1, 0.45f, 1);
    lb_box(-30, 6, -32.4f, -4, 6.25f, -32, TEX_NONE, TEX_NONE, 0.25f, 0.9f, 1, 0);
    lb_box(4, 6, -32.4f, 30, 6.25f, -32, TEX_NONE, TEX_NONE, 0.25f, 0.9f, 1, 0);
    lb_box(-30, 6.85f, -32.4f, -4, 7, -32, TEX_NONE, TEX_NONE, 0.25f, 0.9f, 1, 0);
    lb_box(4, 6.85f, -32.4f, 30, 7, -32, TEX_NONE, TEX_NONE, 0.25f, 0.9f, 1, 0);
    lb_box(-30, 6.25f, -32.3f, -4, 6.85f, -32.1f, TEX_CYDATA, TEX_CYDATA, 0.6f, 0.8f, 1, 0);
    lb_box(4, 6.25f, -32.3f, 30, 6.85f, -32.1f, TEX_CYDATA, TEX_CYDATA, 0.6f, 0.8f, 1, 0);
    /* pillars of streaming data */
    lb_cyl(-12, 0, 0, 1.2f, 16, 12, TEX_CYDATA, TEX_CYDATA, 1, 1, 1, 0);
    lb_cyl(12, 0, 0, 1.2f, 16, 12, TEX_CYDATA, TEX_CYDATA, 1, 1, 1, 0);
    lb_cyl(-12, 0, -24, 1.2f, 16, 12, TEX_CYDATA, TEX_CYDATA, 1, 1, 1, 0);
    lb_cyl(12, 0, -24, 1.2f, 16, 12, TEX_CYDATA, TEX_CYDATA, 1, 1, 1, 0);
    /* PEAK window (west wall) */
    lobby_window(v3(-30.0f, 3.3f, -10), PI_F / 2, 6, 4.5f, TEX_PORTAL1, 0.2f, 0.5f, 0.95f, CRS_PEAK);
    /* BAY room (east) */
    lb_box(31, -1, -18, 47, 0, -2, TEX_CYGRID, TEX_CYWALL, 0.8f, 1, 1, 0);
    lb_box(31, 8, -19, 48, 9, -1, TEX_CYWALL, TEX_CYWALL, 0.55f, 0.55f, 0.7f, 0);
    wall(31, 0, -19, 48, 8, -18);
    wall(31, 0, -2, 48, 8, -1);
    wall(47, 0, -19, 48, 8, -1);
    lobby_window(v3(47.0f, 3.3f, -10), -PI_F / 2, 6, 4.5f, TEX_PORTAL2, 0.1f, 0.65f, 0.75f, CRS_BAY);
    /* FORT room (north, balcony level) */
    lb_box(-8, 5, -57, 8, 6, -41, TEX_CYGRID, TEX_CYWALL, 0.9f, 0.95f, 1, 0);
    lb_box(-9, 14, -58, 9, 15, -41, TEX_CYWALL, TEX_CYWALL, 0.55f, 0.55f, 0.7f, 0);
    wall(-9, 6, -58, -8, 14, -41);
    wall(8, 6, -58, 9, 14, -41);
    wall(-9, 6, -58, 9, 14, -57);
    lobby_window(v3(0, 9.8f, -57.0f), 0, 6, 4.5f, TEX_PORTAL3, 0.45f, 0.5f, 0.6f, CRS_FORT);
    /* final room: a corrupted, red-alert part of the system */
    lb_box(-24, -1, -56, -12, 0, -41, TEX_HEX, TEX_DARK, 1, 0.35f, 0.35f, 0);
    lb_box(-25, 6, -57, -11, 7, -41, TEX_DARK, TEX_DARK, 1, 0.4f, 0.4f, 0);
    lb_box(-25, 0, -57, -24, 6, -41, TEX_CYWALL, TEX_CYWALL, 1, 0.35f, 0.35f, 0);
    lb_box(-12, 0, -57, -11, 6, -41, TEX_CYWALL, TEX_CYWALL, 1, 0.35f, 0.35f, 0);
    lb_box(-25, 0, -57, -11, 6, -56, TEX_CYWALL, TEX_CYWALL, 1, 0.35f, 0.35f, 0);
    lobby_window(v3(-18, 2.8f, -56.0f), 0, 5, 3.8f, TEX_PORTAL5, 0.9f, 0.15f, 0.15f, CRS_LAIR);
    /* secret slide alcove */
    lb_box(-37, 5, -38, -31, 6, -34, TEX_CYGRID, TEX_CYWALL, 0.5f, 1, 1, 0);
    /* side walls go all the way up; the ceiling only spans the space between them */
    lb_box(-37, 9.5f, -38, -31, 10.5f, -34, TEX_CYWALL, TEX_CYWALL, 0.5f, 0.6f, 0.7f, 0);
    lb_box(-38, 6, -39, -31, 10.5f, -38, TEX_CYWALL, TEX_CYWALL, 0.8f, 0.9f, 1, 0);
    lb_box(-38, 6, -34, -31, 10.5f, -33, TEX_CYWALL, TEX_CYWALL, 0.8f, 0.9f, 1, 0);
    lb_box(-38, 6, -38, -37, 10.5f, -34, TEX_CYWALL, TEX_CYWALL, 0.8f, 0.9f, 1, 0);
    lb_appwindow(v3(-37.0f, 7.6f, -36), PI_F / 2, 3.0f, 2.3f, TEX_PORTAL4, 0.2f, 0.8f, 0.9f);
    /* manor alcove (east end of the balcony) */
    lb_box(31, 5, -39, 41, 6, -33, TEX_CYGRID, TEX_CYWALL, 0.8f, 0.5f, 1, 0);
    lb_box(31, 10, -39, 41, 11, -33, TEX_CYWALL, TEX_CYWALL, 0.6f, 0.5f, 0.8f, 0);
    lb_box(31, 6, -40, 42, 11, -39, TEX_CYWALL, TEX_CYWALL, 0.8f, 0.6f, 1, 0);
    lb_box(31, 6, -33, 42, 11, -32, TEX_CYWALL, TEX_CYWALL, 0.8f, 0.6f, 1, 0);
    lb_box(41, 6, -39, 42, 11, -33, TEX_CYWALL, TEX_CYWALL, 0.8f, 0.6f, 1, 0);
    lobby_window(v3(41.0f, 7.8f, -36), -PI_F / 2, 3.6f, 2.6f, TEX_PORTAL7, 0.5f, 0.25f, 0.7f, CRS_MANOR);
    /* spire (west wall) and labyrinth (east wall) windows near the entrance */
    lobby_window(v3(-30.0f, 3.3f, 3.5f), PI_F / 2, 5, 4.0f, TEX_PORTAL8, 0.35f, 0.75f, 1.0f, CRS_SPIRE);
    lobby_window(v3(30.0f, 3.3f, 3.5f), -PI_F / 2, 5, 4.0f, TEX_PORTAL9, 0.75f, 0.8f, 0.85f, CRS_MAZE);
    /* drifts window on the south wall */
    lobby_window(v3(18, 3.3f, 10.0f), PI_F, 6, 4.5f, TEX_PORTAL6, 0.95f, 0.55f, 0.2f, CRS_DRIFTS);
    /* aurora sea window on the south wall, west of the entrance */
    lobby_window(v3(-18, 3.3f, 10.0f), PI_F, 6, 4.5f, TEX_PORTAL10, 0.25f, 0.85f, 0.6f, CRS_SEA);
    /* decoration: holographic panels, monitor wall, statue pedestal */
    {
        int k;
        for (k = 0; k < 4; k++) {
            float x = -24 + k * 16;
            lb_quad(TEX_CYDATA, v3(x - 1.5f, 6, 9.95f), v3(x + 1.5f, 6, 9.95f), v3(x + 1.5f, 14, 9.95f), v3(x - 1.5f, 14, 9.95f), 0.7f, 0.9f, 1, 0, 0);
        }
        for (k = 0; k < 3; k++) {
            float z = -2 - k * 12;
            if (k == 1) continue;
            lb_quad(TEX_CYDATA, v3(-29.95f, 7, z + 1.5f), v3(-29.95f, 7, z - 1.5f), v3(-29.95f, 14, z - 1.5f), v3(-29.95f, 14, z + 1.5f), 0.7f, 0.9f, 1, 0, 0);
            lb_quad(TEX_CYDATA, v3(29.95f, 7, z - 1.5f), v3(29.95f, 7, z + 1.5f), v3(29.95f, 14, z + 1.5f), v3(29.95f, 14, z - 1.5f), 0.7f, 0.9f, 1, 0, 0);
        }
        /* FROSTCLOUD SKYWAY window on the balcony, north wall */
        lobby_window(v3(16, 8.2f, -40.0f), 0, 5, 3.5f, TEX_PORTAL11, 0.95f, 0.75f, 0.3f, CRS_SKY);
        /* STARDUST ORBIT window on the balcony, north wall (west side) */
        lobby_window(v3(-16, 8.2f, -40.0f), 0, 5, 3.5f, TEX_PORTAL12, 0.6f, 0.4f, 1.0f, CRS_SPACE);
        for (k = 0; k < 5; k++) {   /* a wall of monitors above the balcony */
            float x = -20 + k * 10;
            lb_appwindow(v3(x, 12.2f, -40.0f), 0, 3.6f, 2.4f, TEX_CYDATA, 0.15f, 0.35f, 0.6f);
        }
        lb_cyl(0, 0, -14, 2.2f, 1.2f, 16, TEX_CYGRID, TEX_CYWALL, 0.5f, 1, 1, 0);
        lb_cyl(0, 1.2f, -14, 2.5f, 0.12f, 20, TEX_NONE, TEX_NONE, 0.3f, 0.95f, 1, 0);
    }
    finish_geometry();

    lv.spawn[0] = v3(0, 0.05f, 6); lv.spawn_yaw[0] = PI_F;
    lv.spawn[1] = v3(-27, 0, -10); lv.spawn_yaw[1] = PI_F / 2;
    lv.spawn[2] = v3(43, 0, -10); lv.spawn_yaw[2] = -PI_F / 2;
    lv.spawn[3] = v3(0, 6, -53); lv.spawn_yaw[3] = 0;
    lv.spawn[4] = v3(-27, 6, -36); lv.spawn_yaw[4] = PI_F / 2;
    lv.spawn[5] = v3(-18, 0, -50); lv.spawn_yaw[5] = 0;
    lv.spawn[6] = v3(38, 6, -36); lv.spawn_yaw[6] = -PI_F / 2;
    lv.spawn[7] = v3(18, 0, 6.5f); lv.spawn_yaw[7] = PI_F;
    lv.spawn[8] = v3(-27, 0, 3.5f); lv.spawn_yaw[8] = PI_F / 2;
    lv.spawn[9] = v3(27, 0, 3.5f); lv.spawn_yaw[9] = -PI_F / 2;
    lv.spawn[10] = v3(-18, 0, 6.5f); lv.spawn_yaw[10] = PI_F;
    lv.spawn[11] = v3(16, 6, -36.5f); lv.spawn_yaw[11] = 0;
    lv.spawn[12] = v3(-16, 6, -36.5f); lv.spawn_yaw[12] = 0;

    /* doors */
    o = obj_spawn(OT_DOOR, v3(0, 0, 9.8f), PI_F, 0);
    if (o) { o->param2 = LV_GROUNDS; o->i[0] = 1; }
    obj_spawn(OT_STARDOOR, v3(30.5f, 0, -10), PI_F / 2, 1);
    obj_spawn(OT_STARDOOR, v3(0, 6, -40.5f), 0, 4);
    obj_spawn(OT_STARDOOR, v3(-18, 0, -40.5f), 0, 16);
    obj_spawn(OT_STARDOOR, v3(30.5f, 6, -36), PI_F / 2, 6);
    /* portals */
    warp_box(v3(-29.4f, 0.0f, -10), 0.6f, 3.0f, 5, LV_PEAK, 0, 1, 1);
    warp_box(v3(46.4f, 0.0f, -10), 0.6f, 3.0f, 5, LV_BAY, 0, 1, 2);
    warp_box(v3(0, 6.0f, -56.4f), 3.0f, 0.6f, 5, LV_FORT, 0, 1, 3);
    {
        Obj *w = warp_box(v3(-36.4f, 6.0f, -36), 0.6f, 1.6f, 3.5f, LV_SLIDE, 0, 0, 4);
        if (w) w->i[3] = 1;   /* glowing portal so it is clear this is a warp */
    }
    warp_box(v3(-18, 0.0f, -55.4f), 2.5f, 0.6f, 5, LV_LAIR, 0, 1, 5);
    warp_box(v3(40.4f, 6.0f, -36), 0.6f, 1.6f, 3.5f, LV_MANOR, 0, 1, 6);
    {
        Obj *w = warp_box(v3(-29.4f, 0.0f, 3.5f), 0.6f, 2.5f, 4.5f, LV_SPIRE, 0, 1, 8);
        if (w) w->i[1] = 5;
        w = warp_box(v3(29.4f, 0.0f, 3.5f), 0.6f, 2.5f, 4.5f, LV_MAZE, 0, 1, 9);
        if (w) w->i[1] = 7;
    }
    {
        Obj *w = warp_box(v3(18, 0.0f, 9.4f), 3.0f, 0.6f, 5, LV_DRIFTS, 0, 1, 7);
        if (w) w->i[1] = 8;
        w = warp_box(v3(-18, 0.0f, 9.4f), 3.0f, 0.6f, 5, LV_SEA, 0, 1, 10);
        if (w) w->i[1] = 10;
        w = warp_box(v3(16, 6.0f, -39.4f), 2.5f, 0.6f, 4, LV_SKY, 0, 1, 11);
        if (w) w->i[1] = 12;
        w = warp_box(v3(-16, 6.0f, -39.4f), 2.5f, 0.6f, 4, LV_SPACE, 0, 1, 12);
        if (w) w->i[1] = 14;
    }

    {
        Obj *st = obj_spawn(OT_DECOR, v3(0, 1.2f, -14), 0, -1);
        if (st) { st->scale = 2.2f; st->radius = 2.3f; st->height = 4; }
    }
    sign(-3, 4, 2, PI_F, "Stars needed:\nBITSTREAM BAY door (east): 1 star\n"
                        "BLIZZARD FORTRESS door (upstairs): 4 stars\n"
                        "ZOMBIE PROCESS MANOR (balcony, east): 6 stars\n"
                        "DAEMON DRIFTS window (by the entrance): 8\n"
                        "ICICLE SPIRE window (west, by the door): 5\n"
                        "WHITEOUT LABYRINTH window (east, by the door): 7\n"
                        "AURORA SEA RALLY window (by the entrance, west): 10\n"
                        "FROSTCLOUD SKYWAY window (balcony, north): 12\n"
                        "STARDUST ORBIT window (balcony, north-west): 14\n"
                        "SEGFAULT's door (under the balcony): 16 stars\n"
                        "Jump into an app window to enter its world.");
    {
        Obj *g = npc(18, 4, 2, PI_F, NULL);
        if (g) g->param = 1;
    }
    npc(8, -12, 2, -PI_F / 2, "PENGUIN: Psst! I heard a cold draft coming\nfrom a hole at the west end of the balcony...\n"
                              "Maybe a secret slide? Penguins LOVE slides!");
    npc(-20, -2, 2, PI_F / 2, "PENGUIN: The window of PENGUIN PEAK is right\nhere. King Daemon sits on the summit, forking\n"
                              "bombs. Pick one up with B and throw it back!");
    coin_line(v3(-20, 0.2f, -28), v3(-20, 0.2f, -36), 4, 0);
}

/* ================================================================
 * PENGUIN PEAK
 * ================================================================ */
static float peak_h(float x, float z)
{
    float h = 0.45f * sinf(x * 0.11f) * sinf(z * 0.09f) + 0.3f * sinf(x * 0.05f + 1.0f) + 0.25f;
    float dm = rect_dist(x, z, -30, -60, 30, 4);
    float pd = rect_inside(x, z, -62.5f, 12.5f, -27.5f, 47.5f);
    float ip = sqrtf((x + 35) * (x + 35) + (z + 30) * (z + 30));
    h *= sstep(0, 6, dm);
    if (ip < 10) h = lerpf(0.05f, h, sstep(7, 10, ip));
    if (pd > 0) h = -18.0f * clampf(pd / 2.5f, 0, 1);
    h += boundary(x, z, 76, 2.8f);
    return h;
}

static void peak_style(float x, float y, float z, vec3 n, int *tex, float *col, int *flags)
{
    float ip = sqrtf((x + 35) * (x + 35) + (z + 30) * (z + 30));
    if (n.y < 0.62f || y < -2) { *tex = TEX_ROCK; col[0] = 0.75f; col[1] = 0.78f; col[2] = 0.85f; return; }
    if (ip < 7) { *tex = TEX_ICE; *flags = SF_SLIPPERY; return; }   /* frozen pond */
    if (y > 8) { *tex = TEX_SNOW; return; }
    *tex = TEX_GRASS;
    col[0] = col[1] = col[2] = 0.92f + 0.08f * sinf(x * 0.2f) * cosf(z * 0.17f);
}

static void build_peak(void)
{
    Obj *o;
    int i;
    lv.name = "PENGUIN PEAK";
    lv.music = MUS_FIELD;
    lv.course = CRS_PEAK;
    set_sky(0.35f, 0.58f, 0.95f, 0.8f, 0.88f, 0.98f, 70, 200);
    lv.ambient = 0.55f; lv.diffuse = 0.68f;
    lv.death_y = -14;
    lv.clouds = 1;
    lv.snow = 1;
    g_coin_star_idx = 5;
    lb_begin();
    lb_terrain(-90, -90, 90, 90, 72, 72, peak_h, peak_style);
    /* the mountain: three tiers + ramps */
    lb_box(-26, -1, -56, 26, 6, -4, TEX_GRASS, TEX_ROCK, 1, 1, 1, 0);
    lb_box(-18, 6, -48, 18, 12, -12, TEX_GRASS, TEX_ROCK, 0.95f, 1, 0.95f, 0);
    lb_box(-11, 12, -41, 11, 18, -19, TEX_SNOW, TEX_ROCK, 1, 1, 1, 0);
    lb_ramp(6, -4, 26, 0, 6, 0, 0, 0.6f, TEX_DIRT, TEX_ROCK, 1, 0.95f, 0.9f, 0);
    lb_ramp(-24, -40, -18, -12, 12, 6, 1, 0.6f, TEX_DIRT, TEX_ROCK, 1, 0.95f, 0.9f, 0);
    lb_ramp(-11, -45, 11, -41, 12, 18, 0, 0.6f, TEX_DIRT, TEX_ROCK, 1, 0.95f, 0.9f, 0);
    /* summit decoration: low rocks at the corners */
    lb_box(-11, 18, -41, -9, 19, -39, TEX_ROCK, TEX_ROCK, 1, 1, 1, 0);
    lb_box(9, 18, -21, 11, 19, -19, TEX_ROCK, TEX_ROCK, 1, 1, 1, 0);
    /* floating island */
    lb_cyl(52, 19, 34, 8, 3, 18, TEX_GRASS, TEX_ROCK, 1, 1, 1, 0);
    lb_cyl(52, 15.5f, 34, 5.5f, 3.6f, 14, TEX_ROCK, TEX_ROCK, 0.9f, 0.9f, 0.9f, 0);
    lb_cyl(52, 12.5f, 34, 2.8f, 3.1f, 10, TEX_ROCK, TEX_ROCK, 0.85f, 0.85f, 0.85f, 0);
    /* posts for red fish */
    lb_box(58, -1, 56, 60, 3.5f, 58, TEX_WOOD, TEX_WOOD, 1, 1, 1, 0);
    lb_box(65, -1, 39, 67, 4.0f, 41, TEX_WOOD, TEX_WOOD, 1, 1, 1, 0);
    /* crusher bridge + pillar in the pit */
    lb_box(-44, -0.5f, 29, -25, 0, 31, TEX_PLANK, TEX_WOOD, 1, 1, 1, 0);
    lb_cyl(-47, -20, 30, 3.5f, 20, 16, TEX_STONE, TEX_ROCK, 1, 1, 1, 0);
    finish_geometry();

    lv.spawn[0] = v3(0, floor_y(0, 62, 10), 62); lv.spawn_yaw[0] = PI_F;

    /* star 0: King Daemon */
    obj_spawn(OT_KING, v3(0, 18, -34), 0, 0);
    /* star 1: race */
    o = spawn_at(OT_RACER, 6, 52, 5, PI_F, 1);
    if (o) o->f[0] = 7.2f;
    obj_spawn(OT_RACEFLAG, v3(8, 18, -22), 0, 1);
    {
        static const float path[][3] = {
            { 20, 0, 12 }, { 28, 0, -2 }, { 7, 6, -2 }, { 4, 6, -8 }, { -21, 6, -12 }, { -21, 12, -38.5f },
            { -16, 12, -38.5f }, { -12, 12, -43 }, { 11, 18, -43 }, { 9, 18, -38 }, { 8, 18, -22 }
        };
        g_race_n = (int)(sizeof path / sizeof path[0]);
        for (i = 0; i < g_race_n; i++) g_race_path[i] = v3(path[i][0], path[i][1], path[i][2]);
    }
    /* star 2: red fish */
    obj_spawn(OT_REDCOIN, v3(59, 3.7f, 57), 0, 0);
    obj_spawn(OT_REDCOIN, v3(66, 4.2f, 40), 0, 0);
    obj_spawn(OT_REDCOIN, v3(23, 6.2f, -53), 0, 0);
    obj_spawn(OT_REDCOIN, v3(0, floor_y(0, -66, 5) + 0.2f, -66), 0, 0);
    obj_spawn(OT_REDCOIN, v3(48, 22.2f, 30), 0, 0);
    obj_spawn(OT_REDCOIN, v3(56, 22.2f, 30), 0, 0);
    obj_spawn(OT_REDCOIN, v3(56, 22.2f, 38), 0, 0);
    obj_spawn(OT_REDCOIN, v3(48, 22.2f, 38), 0, 0);
    obj_spawn(OT_STARMARKER, v3(52, 23.0f, 34), 0, 2);
    spawn_at(OT_TRAMPOLINE, 52, 45.5f, 5, 0, 0);
    obj_spawn(OT_ROCKETBOX, v3(30, floor_y(30, 58, 10) + 0.7f, 58), 0, 0);
    /* star 3: crusher bridge */
    for (i = 0; i < 2; i++) {
        o = obj_spawn(OT_CRUSHER, v3(i == 0 ? -31.0f : -38.5f, 6.7f, 30), PI_F / 2, i * 45);
        if (o) o->f[0] = 1.7f;
    }
    obj_spawn(OT_STAR, v3(-47, 0.3f, 30), 0, 3);
    /* flagpole with a 1-up on top */
    o = spawn_at(OT_POLE, -60, -50, 5, 0, 0);
    if (o) { o->height = 14; obj_spawn(OT_ONEUP, v3(o->pos.x, o->pos.y + 14.2f, o->pos.z), 0, 0); }
    /* star 4: behind the watchdog's gate */
    spawn_at(OT_WATCHDOG, 47, -46, 5, 0, 0);
    o = spawn_at(OT_CAGE, 53, -52, 5, 0, 0);
    if (o) obj_spawn(OT_STAR, v3(o->pos.x, o->pos.y + 0.5f, o->pos.z), 0, 4);

    /* enemies */
    {
        static const float bugs[][2] = { { 20, 30 }, { -15, 20 }, { 34, -12 }, { -40, -20 }, { 45, 10 }, { -20, 55 }, { 30, -60 } };
        for (i = 0; i < 7; i++) spawn_at(OT_BUG, bugs[i][0], bugs[i][1], 5, frand() * TAU_F, 0);
        spawn_at(OT_BUG, 10, -50, 8, 0, 0);
    }
    spawn_at(OT_FORKBOMB, -10, -50, 8, 0, 0);
    spawn_at(OT_FORKBOMB, 15, -20, 8, 1, 0);
    spawn_at(OT_FORKBOMB, 0, -15, 14, 2, 0);
    o = obj_spawn(OT_ROLLERGEN, v3(-21, 12, -38.5f), 0, 260);
    if (o) { o->f[0] = -21; o->f[1] = 6; o->f[2] = -13; o->timer = 200; }

    /* crates */
    o = spawn_at(OT_CRATE, 14, -10, 8, 0, 5);
    o = spawn_at(OT_CRATE, -10, -8, 8, 0.3f, 5);
    o = spawn_at(OT_CRATE, 20, -30, 8, 0.6f, 5);
    o = spawn_at(OT_CRATE, -22, -50, 8, 0, -1);

    /* coins (lots: 100 fish star) */
    coin_line(v3(0, 0, 54), v3(0, 0, 30), 5, 1);
    coin_ring(v3(-20, 0, 50), 3.5f, 8, 1);
    coin_line(v3(24, 0, -2), v3(9, 0, -2), 5, 1);
    coin_line(v3(-21, 6, -16), v3(-21, 6, -36), 5, 1);
    coin_line(v3(-8, 12, -43), v3(8, 12, -43), 5, 1);
    coin_ring(v3(14, 6, -46), 3, 8, 1);
    coin_ring(v3(40, 0, 10), 4, 8, 1);
    coin_line(v3(36, 0, 50), v3(36, 0, 40), 5, 1);
    coin_line(v3(-29, 0.3f, 30), v3(-35, 0.3f, 30), 3, 0);
    coin_ring(v3(52, 22.2f, 34), 5.5f, 6, 0);
    coin_ring(v3(0, 0, -68), 4, 8, 1);
    coin_line(v3(-50, 0, -10), v3(-50, 0, -30), 5, 1);
    coin_ring(v3(-62, 0, -30), 4, 8, 1);
    coin_ring(v3(-70, 0, 60), 3, 8, 1);
    coin_line(v3(60, 0, -10), v3(60, 0, -30), 5, 1);
    obj_spawn(OT_ONEUP, v3(-47, 1.5f, 30), 0, 0)->pos.y = 3.0f;

    /* signs */
    sign(4, 55, 5, PI_F, "WELCOME TO PENGUIN PEAK!\nKing Daemon has made the summit his throne.\n"
                         "Follow the ramps around the mountain to reach\nhim. Watch out for the rolling iron balls!");
    sign(47, 50, 5, PI_F, "TRAMPOLINE: Jump onto it, then press A\njust before you touch the mat to bounce\n"
                          "higher and higher. Three good bounces in a\nrow reach the floating isle above!\nTIP: a ground pound on the mat bounces extra high.");
    sign(-23, 25, 5, -PI_F / 2, "DANGER! CRUSHER BRIDGE\nRun under the crushers when they rise.");
    sign(-56, -46, 5, PI_F, "Climb the pole: jump onto it, then hold up.\nWhat is waiting at the very top?");
    sign(38, -38, 5, PI_F * 0.8f, "BEWARE OF THE WATCHDOG!\nIt guards the gate and lunges at intruders.\n"
                                 "Right after a lunge it is out of breath...\nKICK the watchdog 3 times to reset it!");
    npc(10, 58, 5, PI_F, "PENGUIN: The Quick Gnu over there says nobody\ncan beat him up the mountain. Talk to him!");

    /* trees */
    {
        static const float tr[][2] = { { -30, 60 }, { -40, 45 }, { 30, 35 }, { 60, 20 }, { -70, 10 }, { 70, -10 },
                                       { -60, -70 }, { 40, -65 }, { 65, -60 }, { -35, -5 }, { 20, 65 }, { -10, 70 } };
        for (i = 0; i < 12; i++) tree(tr[i][0], tr[i][1], 10, i % 3 == 0 ? 2 : 0);
    }
    decor(MDL_ROCK, 30, 20, 5, 1.5f, 1.4f);
    decor(MDL_ROCK, -45, 60, 5, 2.0f, 1.8f);
    decor(MDL_BUSH, 15, 45, 5, 1.0f, 0.9f);
    decor(MDL_BUSH, -12, 40, 5, 1.2f, 1.0f);
}

/* ================================================================
 * BITSTREAM BAY
 * ================================================================ */
static float bay_h(float x, float z)
{
    float dz = z + 10, r = sqrtf(x * x + dz * dz), h;
    if (r < 40) h = -24 + r * 0.15f + 0.8f * sinf(x * 0.2f) * sinf(z * 0.17f) * sstep(0, 10, r);
    else if (r < 52) h = lerpf(-18.0f, 1.5f, sstep(40, 52, r));
    else h = 1.5f + (r - 52) * 0.06f + 0.2f * sinf(x * 0.3f) * sinf(z * 0.25f);
    h += boundary(x, z, 78, 3.0f);
    return h;
}

static void bay_style(float x, float y, float z, vec3 n, int *tex, float *col, int *flags)
{
    (void)x; (void)z; (void)flags;
    if (n.y < 0.6f && y > 2) { *tex = TEX_ROCK; return; }
    if (y < -3) { *tex = TEX_ROCK; col[0] = 0.6f; col[1] = 0.7f; col[2] = 0.8f; return; }   /* sea floor */
    *tex = TEX_SAND;
    if (y > 4) { *tex = TEX_GRASS; }
}

static void build_bay(void)
{
    Obj *o;
    int i, clam;
    lv.name = "BITSTREAM BAY";
    lv.music = MUS_WATER;
    lv.course = CRS_BAY;
    set_sky(0.3f, 0.5f, 0.85f, 0.78f, 0.87f, 0.96f, 70, 200);
    lv.ambient = 0.55f; lv.diffuse = 0.66f;
    lv.death_y = -40;
    lv.clouds = 1;
    lv.snow = 1;
    g_coin_star_idx = 5;
    lb_begin();
    lb_terrain(-90, -90, 90, 90, 72, 72, bay_h, bay_style);
    lb_water(-60, -70, 60, 50, 0, -30);
    /* drifting ice floes and icebergs on the polar sea */
    {
        static const float fl[6][3] = { { 20, 25, 3.0f }, { -26, 18, 2.5f }, { 30, -34, 3.5f }, { -40, -10, 2.8f },
                                        { 12, -48, 2.6f }, { 38, 6, 2.2f } };
        int k;
        for (k = 0; k < 6; k++)
            lb_cyl(fl[k][0], -0.4f, fl[k][1], fl[k][2], 0.65f, 12, TEX_SNOW, TEX_ICE, 0.95f, 0.98f, 1, SF_SLIPPERY);
        lb_cone(44, -8, -46, 7, 22, 9, TEX_ICE, 0.8f, 0.92f, 1, 1);
        lb_cone(-46, -8, -50, 6, 18, 8, TEX_ICE, 0.8f, 0.92f, 1, 1);
        lb_cone(50, -8, 22, 5, 16, 8, TEX_ICE, 0.8f, 0.92f, 1, 1);
    }
    /* lighthouse island */
    lb_cyl(-22, -21, -30, 7, 23, 20, TEX_SNOW, TEX_ROCK, 1, 1, 1, 0);
    for (i = 0; i < 6; i++) {
        float c = (i & 1) ? 0.4f : 1.0f;
        lb_cyl(-22, 2 + i * 4, -30, 2.5f, 4, 16, TEX_BRICK, TEX_NONE, 1, c, c, 0);
    }
    lb_cyl(-22, 26, -30, 3.0f, 0.6f, 16, TEX_METAL, TEX_METAL, 0.8f, 0.8f, 0.85f, 0);
    lb_cyl(-22, 26.6f, -30, 1.2f, 2.0f, 12, TEX_NONE, TEX_NONE, 1, 0.95f, 0.5f, 0);
    lb_cone(-22, 28.6f, -30, 1.6f, 1.5f, 12, TEX_ROOF, 1, 1, 1, 1);
    for (i = 0; i < 16; i++) {
        float a = i * 0.7f, yt = 2 + 1.5f * (i + 1);
        lb_obox(v3(-22 + sinf(a) * 3.7f, yt - 0.25f, -30 + cosf(a) * 3.7f), v3(1.0f, 0.25f, 1.0f), a, 0,
                TEX_WOOD, TEX_WOOD, 1, 0.9f, 0.8f, 0);
    }
    /* sunken ship */
    lb_box(4, -23, 2, 20, -21.5f, 8, TEX_PLANK, TEX_WOOD, 0.8f, 0.7f, 0.6f, 0);
    lb_box(4, -21.5f, 1.5f, 20, -18, 2, TEX_WOOD, TEX_PLANK, 0.8f, 0.7f, 0.6f, 0);
    lb_box(4, -21.5f, 8, 20, -18, 8.5f, TEX_WOOD, TEX_PLANK, 0.8f, 0.7f, 0.6f, 0);
    lb_box(3.5f, -21.5f, 1.5f, 4, -17.5f, 8.5f, TEX_WOOD, TEX_PLANK, 0.8f, 0.7f, 0.6f, 0);
    lb_box(20, -21.5f, 1.5f, 20.5f, -17.5f, 8.5f, TEX_WOOD, TEX_PLANK, 0.8f, 0.7f, 0.6f, 0);
    lb_box(4, -18.3f, 2, 14.5f, -18, 8, TEX_PLANK, TEX_WOOD, 0.8f, 0.7f, 0.6f, 0);
    /* reef rocks */
    lb_cyl(-30, -22, 0, 3, 6, 8, TEX_ROCK, TEX_ROCK, 0.8f, 0.6f, 0.6f, 0);
    lb_cyl(25, -22, -20, 2.5f, 8, 8, TEX_ROCK, TEX_ROCK, 0.6f, 0.7f, 0.8f, 0);
    lb_cyl(-10, -22, -40, 2.0f, 9, 8, TEX_ROCK, TEX_ROCK, 0.7f, 0.8f, 0.6f, 0);
    /* wooden pier */
    lb_box(-4, 0.6f, 36, 4, 1.0f, 56, TEX_PLANK, TEX_WOOD, 1, 1, 1, 0);
    for (i = 0; i < 4; i++) {
        lb_box(-4, -6, 38 + i * 5, -3.4f, 0.6f, 38.6f + i * 5, TEX_WOOD, TEX_WOOD, 0.8f, 0.7f, 0.6f, 0);
        lb_box(3.4f, -6, 38 + i * 5, 4, 0.6f, 38.6f + i * 5, TEX_WOOD, TEX_WOOD, 0.8f, 0.7f, 0.6f, 0);
    }
    finish_geometry();

    lv.spawn[0] = v3(0, floor_y(0, 64, 10), 64); lv.spawn_yaw[0] = PI_F;

    /* star 0: ship */
    obj_spawn(OT_STAR, v3(17, -21.4f, 5), 0, 0);
    coin_line(v3(6, -21.2f, 5), v3(13, -21.2f, 5), 5, 0);
    obj_spawn(OT_PUFFER, v3(12, -16, 10), 0, 0);
    obj_spawn(OT_PUFFER, v3(12, -16, 0), 0, 0);
    /* star 1: red fish */
    {
        static const float rc[8][2] = { { -30, 12 }, { 30, -5 }, { 22, -30 }, { -5, -36 }, { 0, 22 }, { -36, -15 }, { 35, 15 }, { 10, -46 } };
        for (i = 0; i < 8; i++) obj_spawn(OT_REDCOIN, v3(rc[i][0], floor_y(rc[i][0], rc[i][1], -1) + 1.2f, rc[i][1]), 0, 0);
        obj_spawn(OT_STARMARKER, v3(0, -8, -10), 0, 1);
    }
    /* star 2: lighthouse */
    obj_spawn(OT_STAR, v3(-19.8f, 26.7f, -30), 0, 2);
    for (i = 0; i < 16; i++) {
        float a = i * 0.7f, yt = 2 + 1.5f * (i + 1);
        obj_spawn(OT_COIN, v3(-22 + sinf(a) * 3.7f, yt + 0.2f, -30 + cosf(a) * 3.7f), 0, 0);
    }
    /* star 3: clam */
    o = spawn_at(OT_CLAM, -12, 22, -1, 0.5f, 0);
    clam = o ? obj_index(o) : -1;
    if (o) {
        Obj *s = obj_spawn(OT_STAR, v3(o->pos.x, o->pos.y + 0.4f, o->pos.z), 0, 3);
        if (s) s->i[0] = clam + 1;
    }
    /* star 4: rings */
    {
        static const float rp[6][3] = { { 0, -2.0f, 36 }, { 8, -5, 26 }, { 14, -9, 14 }, { 6, -13, -2 }, { -8, -10, -12 }, { -15, -6, -19 } };
        for (i = 0; i < 6; i++) {
            int nx = i < 5 ? i + 1 : i;
            int pv = i < 5 ? i : i - 1;
            float yaw = atan2f(rp[nx][0] - rp[pv][0], rp[nx][2] - rp[pv][2]);
            o = obj_spawn(OT_RING, v3(rp[i][0], rp[i][1], rp[i][2]), yaw, i);
            if (o) o->param2 = 4;
        }
    }
    /* enemies */
    spawn_at(OT_CRAB, 20, 52, 6, 0, 0);
    spawn_at(OT_CRAB, -25, 50, 6, 0, 0);
    spawn_at(OT_CRAB, 45, 32, 6, 1, 0);
    spawn_at(OT_CRAB, -48, 22, 6, 2, 0);
    obj_spawn(OT_PUFFER, v3(-20, -14, 5), 0, 0);
    obj_spawn(OT_PUFFER, v3(15, -12, -20), 0, 0);
    obj_spawn(OT_PUFFER, v3(-8, -15, 25), 0, 0);
    /* crates, coins */
    spawn_at(OT_CRATE, 30, 50, 6, 0, 5);
    spawn_at(OT_CRATE, -35, 47, 6, 0.4f, 5);
    spawn_at(OT_CRATE, 50, -40, 6, 0, -1);
    coin_line(v3(-20, 3, 62), v3(-40, 3, 56), 5, 1);
    coin_line(v3(20, 3, 62), v3(40, 3, 56), 5, 1);
    coin_line(v3(-58, 3, 0), v3(-58, 3, -20), 5, 1);
    coin_line(v3(58, 3, 0), v3(58, 3, -20), 5, 1);
    coin_ring(v3(20, -12, -12), 3, 8, 0);
    coin_ring(v3(-28, -10, -2), 3, 8, 0);
    coin_ring(v3(5, -8, 30), 3, 8, 0);
    coin_ring(v3(-6, -16, -26), 3, 8, 0);
    coin_ring(v3(-22, 2.2f, -30), 5.2f, 6, 0);
    coin_line(v3(0, 1.2f, 50), v3(0, 1.2f, 40), 4, 0);
    /* signs / npcs */
    sign(4, 60, 6, PI_F, "BITSTREAM BAY\nA polar sea - and penguins are born swimmers!\nIn water: A = swim up, Z = dive, B = dash.\n"
                         "Your power meter drains while underwater.\nSurface to catch your breath!");
    npc(-5, 60, 6, PI_F, "PENGUIN: An old ship sank in the middle of the\nbay. There's something shiny inside...\n"
                         "And the giant clam only opens now and then.");
    sign(10, 57, 6, PI_F, "Swim through the golden rings in order,\nstarting under the pier!");
    for (i = 0; i < 8; i++) {
        float a = TAU_F * i / 8 + 0.4f;
        decor(MDL_PINE_SNOW, sinf(a) * 68, cosf(a) * 68 - 10, 10, 1.0f, 0.5f);
    }
}

/* ================================================================
 * BLIZZARD FORTRESS
 * ================================================================ */
static float fort_h(float x, float z)
{
    float h = 0.45f * sinf(x * 0.08f) * cosf(z * 0.07f) + 0.25f * sinf(x * 0.21f + z * 0.13f) + 0.3f;
    float df = rect_dist(x, z, -26, -66, 26, -8);
    float dw = rect_dist(x, z, -60, -56, -20, -34);
    float dp = rect_dist(x, z, -46, 8, -30, 16);
    float pd = rect_inside(x, z, 30, -20, 70, 30);
    float ip = sqrtf((x + 25) * (x + 25) + (z - 45) * (z - 45));
    h *= sstep(0, 6, df) * sstep(0, 6, dw) * sstep(0, 4, dp);
    if (ip < 9) h = lerpf(0.02f, h, sstep(6, 9, ip));
    if (pd > 0) h = lerpf(h, -1.0f, clampf(pd / 2.5f, 0, 1));
    h += boundary(x, z, 76, 2.8f);
    return h;
}

static void fort_style(float x, float y, float z, vec3 n, int *tex, float *col, int *flags)
{
    float ip = sqrtf((x + 25) * (x + 25) + (z - 45) * (z - 45));
    if (rect_inside(x, z, 30, -20, 70, 30) > 0.5f && y < -0.6f) {
        *tex = TEX_WATER; col[0] = 0.5f; col[1] = 0.75f; col[2] = 1.0f; *flags = SF_BURN; return;
    }
    if (n.y < 0.62f) { *tex = TEX_ROCK; col[0] = 0.8f; col[1] = 0.85f; col[2] = 0.95f; return; }
    if (ip < 7) { *tex = TEX_ICE; *flags = SF_SLIPPERY; return; }
    *tex = TEX_SNOW;
    *flags = SF_SNOW;
}

static void build_fort(void)
{
    Obj *o;
    int i;
    lv.name = "BLIZZARD FORTRESS";
    lv.music = MUS_SNOW;
    lv.course = CRS_FORT;
    set_sky(0.55f, 0.6f, 0.68f, 0.8f, 0.83f, 0.88f, 18, 95);
    lv.ambient = 0.58f; lv.diffuse = 0.45f;
    lv.death_y = -20;
    lv.snow = 2;            /* blizzard: dense snow, fog and gusts of wind */
    lv.wind_yaw = -PI_F / 2; /* the wind blows toward -x (from the east) */
    g_coin_star_idx = 5;
    lb_begin();
    lb_terrain(-90, -90, 90, 90, 72, 72, fort_h, fort_style);
    /* fortress base, ramp, tower */
    lb_box(-20, -1, -60, 20, 8, -30, TEX_SNOW, TEX_STONE, 0.95f, 0.97f, 1, 0);
    battlements(-20, -60, 20, -30, 8, TEX_STONE, 0.85f, 0.88f, 0.95f);
    lb_ramp(-3, -30, 3, -14, 8, 0, 1, 0.6f, TEX_STONE, TEX_STONE, 0.9f, 0.9f, 0.95f, 0);
    lb_box(-6, 8, -54, 6, 20, -42, TEX_SNOW, TEX_STONE, 0.9f, 0.92f, 1, 0);
    lb_cone(-6, 20, -54, 1.2f, 2.5f, 6, TEX_ICE, 1, 1, 1, 0);
    lb_cone(6, 20, -54, 1.2f, 2.5f, 6, TEX_ICE, 1, 1, 1, 0);
    lb_cone(-6, 20, -42, 1.2f, 2.5f, 6, TEX_ICE, 1, 1, 1, 0);
    lb_cone(6, 20, -42, 1.2f, 2.5f, 6, TEX_ICE, 1, 1, 1, 0);
    /* west pillar for the sliding platforms */
    lb_box(-56, -1, -50, -46, 8, -40, TEX_SNOW, TEX_STONE, 0.9f, 0.92f, 1, 0);
    /* frozen pillars (wall kicks) */
    lb_box(-44, -1, 10, -40, 14, 14, TEX_SNOW, TEX_ICE, 0.85f, 0.95f, 1, 0);
    lb_box(-36.5f, -1, 10, -32.5f, 12, 14, TEX_SNOW, TEX_ICE, 0.85f, 0.95f, 1, 0);
    /* brute's ice floe and bridge */
    lb_cyl(50, -2, 5, 7, 3, 20, TEX_ICE, TEX_ICE, 0.85f, 0.95f, 1, SF_SLIPPERY);
    lb_box(28, 0.4f, 4, 43.5f, 1.0f, 6, TEX_SNOW, TEX_WOOD, 1, 1, 1, 0);
    finish_geometry();

    lv.spawn[0] = v3(0, floor_y(0, 62, 10), 62); lv.spawn_yaw[0] = PI_F;

    /* star 0: tower top via elevator */
    o = obj_spawn(OT_ELEVATOR, v3(0, 8.25f, -40.45f), 0, 0);
    if (o) { obj_set_box(o, 1.5f, 0.25f, 1.5f, TEX_METAL, TEX_METAL, 0.9f, 0.85f, 0.4f); o->f[0] = 12.0f; }
    obj_spawn(OT_STAR, v3(0, 20.3f, -48), 0, 0);
    /* star 1: carry the orphaned child process back to init */
    obj_spawn(OT_BABY, v3(12, 8, -52), 0, 0);
    o = spawn_at(OT_MOTHER, 12, 28, 6, PI_F, 1);
    if (o) o->text = "INIT (PID 1): Oh dear! One of my child processes\nwas orphaned up on top of the fortress and\n"
                     "can't get down! Could you carry it back to me\nso I can adopt it? (Pick up with B, put down with Z)";
    /* crushers on the fortress */
    for (i = 0; i < 2; i++) {
        o = obj_spawn(OT_CRUSHER, v3(i ? 12.0f : -12.0f, 15, -36), 0, i * 60);
        if (o) o->f[0] = 9.7f;
    }
    /* star 2: sliding platforms + red fish */
    {
        static const float ph[3] = { 0, 3, 0 }, off[3] = { -6, -6, -4 }, hx[3] = { -24, -32, -40 };
        for (i = 0; i < 3; i++) {
            o = obj_spawn(OT_PLATFORM, v3(hx[i], 7.75f, -45), 0, 0);
            if (o) {
                obj_set_box(o, 1.5f, 0.25f, 1.5f, TEX_ICE, TEX_METAL, 0.8f, 0.9f, 1);
                o->f[0] = off[i]; o->f[3] = 6; o->f[4] = ph[i];
            }
        }
        obj_spawn(OT_REDCOIN, v3(-27, 9.0f, -45), 0, 0);
        obj_spawn(OT_REDCOIN, v3(-35, 9.0f, -45), 0, 0);
        obj_spawn(OT_REDCOIN, v3(-42, 9.0f, -45), 0, 0);
        obj_spawn(OT_REDCOIN, v3(-48, 8.2f, -42), 0, 0);
        obj_spawn(OT_REDCOIN, v3(-54, 8.2f, -42), 0, 0);
        obj_spawn(OT_REDCOIN, v3(-54, 8.2f, -48), 0, 0);
        obj_spawn(OT_REDCOIN, v3(-48, 8.2f, -48), 0, 0);
        obj_spawn(OT_REDCOIN, v3(-18, 8.2f, -57), 0, 0);
        obj_spawn(OT_STARMARKER, v3(-51, 9.5f, -45), 0, 2);
    }
    /* star 3: ice brute */
    obj_spawn(OT_BRUTE, v3(52, 1.0f, 5), PI_F * 1.5f, 3);
    /* star 4: frozen pillar */
    obj_spawn(OT_STAR, v3(-42, 14.3f, 12), 0, 4);
    /* blue switch + blue fish */
    spawn_at(OT_BLUESWITCH, -10, 20, 5, 0, 0);
    for (i = 0; i < 6; i++) {
        o = obj_spawn(OT_BLUECOIN, v3(-22 + i * 5, floor_y(-22 + i * 5, 32, 5) + 0.2f, 32), 0, 1);
    }
    /* enemies */
    spawn_at(OT_BUG, 20, 30, 5, 0, 0);
    spawn_at(OT_BUG, -20, 0, 5, 1, 0);
    spawn_at(OT_BUG, 25, -10, 5, 2, 0);
    spawn_at(OT_BUG, -50, 30, 5, 3, 0);
    spawn_at(OT_FORKBOMB, 15, -20, 5, 0, 0);
    spawn_at(OT_FORKBOMB, -15, -20, 5, 0, 0);
    spawn_at(OT_FORKBOMB, 8, -55, 10, 0, 0);
    /* crates / coins */
    spawn_at(OT_CRATE, -14, -50, 10, 0, 5);
    spawn_at(OT_CRATE, 16, -34, 10, 0, 5);
    spawn_at(OT_CRATE, 25, 45, 5, 0, 5);
    spawn_at(OT_CRATE, -60, 60, 5, 0, -1);
    coin_line(v3(0, 0, -16), v3(0, 7, -28), 5, 1);
    coin_ring(v3(25, 0, 10), 3.5f, 8, 1);
    coin_ring(v3(-25, 0, 45), 4, 8, 1);
    coin_ring(v3(-55, 0, -10), 3.5f, 8, 1);
    coin_ring(v3(0, 8, -36), 2.5f, 8, 1);
    coin_line(v3(30, 1.2f, 5), v3(42, 1.2f, 5), 5, 0);
    coin_line(v3(-30, 0, -70), v3(30, 0, -70), 5, 1);
    coin_line(v3(60, 0, 50), v3(60, 0, 35), 5, 1);
    sign(4, 56, 5, PI_F, "BLIZZARD FORTRESS\nA howling blizzard sweeps the fortress! Strong\n"
                         "gusts can push you off ledges - shelter behind\nwalls or crouch (Z) to brace yourself. The lake\nto the "
                         "east is freezing cold: don't touch the water!\nThe Ice Brute lives on the floe there.");
    sign(-30, 18, 5, PI_F, "Two frozen pillars... Jump at a wall, then\npress A again just as you touch it for a\n"
                           "WALL KICK. Bounce between the pillars!");
    sign(-6, -26, 5, PI_F, "The elevator goes to the top of the tower.\nStand on it to ride up.");
    for (i = 0; i < 14; i++) {
        float a = TAU_F * i / 14 + 0.3f, r = 55 + (i % 3) * 7;
        float x = sinf(a) * r, z = cosf(a) * r;
        if (rect_inside(x, z, 26, -24, 74, 34) > -2) continue;
        tree(x, z, 10, 1);
    }
    decor(MDL_SNOWMAN, 15, 50, 5, 1.0f, 1.0f);
    decor(MDL_SNOWMAN, -35, 30, 5, 1.2f, 1.2f);
}

/* ================================================================
 * SECRET ICE SLIDE
 * ================================================================ */
static void build_slide(void)
{
    static const float P[][3] = {
        { 0, 100, -4 }, { 0, 96, -20 }, { 8, 92, -34 }, { 24, 88, -40 }, { 40, 84, -32 }, { 46, 80, -16 },
        { 38, 76, -2 }, { 22, 72, 4 }, { 8, 68, 14 }, { 6, 64, 30 }, { 18, 60, 42 }, { 36, 56, 42 },
        { 48, 52, 30 }, { 48, 48, 12 }, { 36, 44, 0 }, { 18, 40, -6 }, { 2, 36, -2 }, { -10, 32, 8 },
        { -14, 28, 24 }, { -26, 22, 30 }, { -40, 16, 30 }, { -52, 10, 24 }, { -58, 5, 12 }, { -56, 1, -2 }
    };
    vec3 pts[32];
    int i, n = (int)(sizeof P / sizeof P[0]);
    Obj *o;
    lv.name = "SECRET ICE SLIDE";
    lv.music = MUS_SLIDE;
    lv.course = CRS_SLIDE;
    set_sky(0.05f, 0.08f, 0.25f, 0.25f, 0.35f, 0.6f, 50, 160);
    lv.ambient = 0.6f; lv.diffuse = 0.55f;
    lv.death_y = -15;
    lv.stars_sky = 1;
    lv.snow = 1;
    for (i = 0; i < n; i++) pts[i] = v3(P[i][0], P[i][1], P[i][2]);
    lb_begin();
    lb_box(-5, 98, -4, 5, 100, 8, TEX_ICE, TEX_ICE, 0.9f, 0.95f, 1, 0);
    lb_box(-5, 100, 7.5f, 5, 103, 8, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    lb_box(-5.5f, 100, -4, -5, 103, 8, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    lb_box(5, 100, -4, 5.5f, 103, 8, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    lb_track(pts, n, 6.0f, 2.6f, 6, TEX_SLIDE, 1, 1, 1, SF_SLIDE);
    lb_box(-68, -1, -44, -44, 0, -1, TEX_SNOW, TEX_ICE, 1, 1, 1, 0);
    lb_box(-69, -1, -45, -68, 2, -1, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    lb_box(-44, -1, -45, -43, 2, -1, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    lb_box(-69, -1, -45, -43, 2, -44, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    lb_box(-58, 0, -44, -54, 4.5f, -43.5f, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    lb_appwindow(v3(-56, 2.25f, -43.45f), 0, 3.6f, 3.4f, TEX_PORTAL4, 0.2f, 0.75f, 0.9f);
    death_floor(-25, -120, -120, 120, 120);
    finish_geometry();
    lv.spawn[0] = v3(0, 100, 4); lv.spawn_yaw[0] = PI_F;
    /* portal back to the castle at the back of the start platform */
    o = warp_box(v3(0, 100, 7.0f), 1.6f, 0.5f, 3, LV_LOBBY, 4, 0, 0);
    if (o) o->i[3] = 1;
    o = obj_spawn(OT_TIMERSTART, v3(0, 99, -5.5f), 0, 0);
    if (o) { o->f[5] = 3; o->f[7] = 0.6f; o->height = 4; }
    o = obj_spawn(OT_TIMERGOAL, v3(-56, 0, -6), 0, 0);
    if (o) o->param2 = 21;
    warp_box(v3(-56, 0, -42.9f), 1.8f, 0.5f, 4, LV_LOBBY, 4, 0, 0);
    for (i = 2; i < n - 2; i += 2) obj_spawn(OT_COIN, v3(pts[i].x, pts[i].y + 0.4f, pts[i].z), 0, 0);
    sign(3, 6, 102, PI_F, "SECRET ICE SLIDE\nBelly-slide all the way down! Steer with the\n"
                          "stick. Reach the bottom in under 21 seconds\nfor a bonus Kernel Star!\n"
                          "The portal behind you leads back to the castle.");
}

/* ================================================================
 * SEGFAULT'S LAIR
 * ================================================================ */
static void build_lair(void)
{
    Obj *o;
    int i;
    lv.name = "SEGFAULT'S LAIR";
    lv.music = MUS_FINAL;
    lv.course = CRS_LAIR;
    set_sky(0.02f, 0.03f, 0.1f, 0.08f, 0.14f, 0.28f, 40, 150);
    lv.snow = 1;
    lv.ambient = 0.55f; lv.diffuse = 0.6f;
    lv.death_y = -12;
    lv.stars_sky = 1;
    lb_begin();
    /* lava sea (visual only) */
    lb_quad(TEX_LAVA, v3(-200, -14, -250), v3(-200, -14, 150), v3(200, -14, 150), v3(200, -14, -250), 1, 1, 1, 0, 0);
    lb_cyl(0, -3, 60, 6, 3, 18, TEX_HEX, TEX_DARK, 1, 1, 1, 0);
    lb_box(-1.5f, -0.5f, 40, 1.5f, 0, 54.5f, TEX_HEX, TEX_DARK, 1, 1, 1, 0);
    lb_cyl(0, -3, 34, 6, 3, 18, TEX_HEX, TEX_DARK, 1, 1, 1, 0);
    lb_cyl(0, -1, 4, 5, 3, 18, TEX_HEX, TEX_DARK, 1, 1, 1, 0);
    lb_cyl(0, -1, -26, 5, 3, 18, TEX_HEX, TEX_DARK, 1, 1, 1, 0);
    lb_cyl(0, -1, -56, 5, 5, 18, TEX_HEX, TEX_DARK, 1, 1, 1, 0);
    /* boss arena */
    lb_cyl(0, -3, -150, 20, 3, 36, TEX_HEX, TEX_DARK, 1, 1, 1, 0);
    lb_cyl(0, -9, -150, 14, 6, 24, TEX_DARK, TEX_DARK, 0.7f, 0.7f, 0.7f, 0);
    death_floor(-20, -100, -200, 100, 100);
    finish_geometry();

    lv.spawn[0] = v3(0, 0, 62); lv.spawn_yaw[0] = PI_F;
    lv.spawn[1] = v3(0, 0, -134); lv.spawn_yaw[1] = PI_F;

    o = obj_spawn(OT_FLAMEBAR, v3(0, 0.7f, 34), 0, 5);
    if (o) o->f[0] = 1.5f;
    o = obj_spawn(OT_PLATFORM, v3(0, -0.25f, 26), 0, 0);
    if (o) { obj_set_box(o, 2, 0.25f, 2, TEX_HEX, TEX_METAL, 1, 0.6f, 0.6f); o->f[2] = -13; o->f[3] = 7; }
    for (i = 0; i < 4; i++) {
        o = obj_spawn(OT_FALLPLAT, v3(0, 1.75f, -3.9f - i * 4.4f), 0, 0);
        if (o) obj_set_box(o, 1.8f, 0.25f, 1.7f, TEX_HEX, TEX_DARK, 0.9f, 0.5f, 0.5f);
    }
    o = obj_spawn(OT_ROTPLAT, v3(0, 1.75f, -40), PI_F / 2, 0);
    if (o) { obj_set_box(o, 7, 0.25f, 1.5f, TEX_HEX, TEX_METAL, 0.8f, 0.6f, 1); o->f[3] = 0.6f; }
    o = obj_spawn(OT_FLAMEBAR, v3(0, 2.7f, -26), 0, 4);
    if (o) o->f[0] = -1.8f;
    o = warp_box(v3(0, 4, -58), 1.2f, 1.2f, 3, LV_LAIR, 1, 0, 0);
    if (o) o->i[3] = 1;
    /* arena */
    obj_spawn(OT_SEGFAULT, v3(0, 0, -150), 0, 0);
    for (i = 0; i < 8; i++) {
        float a = TAU_F * i / 8 + PI_F / 8;
        obj_spawn(OT_SPIKEBOMB, v3(sinf(a) * 13.0f, 0, -150 + cosf(a) * 13.0f), 0, 0);
    }
    /* coins and help */
    coin_ring(v3(0, 0.2f, 60), 3, 6, 0);
    coin_line(v3(0, 0.2f, 52), v3(0, 0.2f, 42), 4, 0);
    coin_ring(v3(0, 2.2f, 4), 3, 6, 0);
    coin_ring(v3(0, 2.2f, -26), 3.5f, 6, 0);
    obj_spawn(OT_ONEUP, v3(3, 4.5f, -56), 0, 0);
    coin_ring(v3(0, 0.2f, -150), 12, 10, 0);
    sign(3, 58, 2, PI_F, "SEGFAULT'S LAIR\nA frozen void. Beyond the frost bars and the\ncrumbling ice "
                        "platforms waits Segfault himself. He can't be\nhurt directly... Stand behind one of the spike\n"
                        "mines and jump aside when he charges, so he\nrams the mine instead of you!");
}


/* ================================================================
 * DAEMON DRIFTS (snow dunes, powder snow, ice pyramid, hot spring)
 * ================================================================ */
static float drifts_h(float x, float z)
{
    float h = 1.3f * sinf(x * 0.08f) * sinf(z * 0.06f) + 0.9f * sinf((x + z) * 0.05f) + 1.3f;
    float dp = rect_dist(x, z, -28, -58, 28, -2);
    float dq = sqrtf((x + 45) * (x + 45) + (z - 45) * (z - 45));
    float doa = sqrtf((x - 45) * (x - 45) + (z - 40) * (z - 40));
    h *= sstep(0, 8, dp);
    if (dq < 18) h = lerpf(0.3f, h, sstep(14, 18, dq));
    if (doa < 16) h = lerpf(-3.0f + doa * 0.12f, h, sstep(10, 16, doa));
    h += boundary(x, z, 76, 2.8f);
    return h;
}

static void drifts_style(float x, float y, float z, vec3 n, int *tex, float *col, int *flags)
{
    float dq = sqrtf((x + 45) * (x + 45) + (z - 45) * (z - 45));
    if (n.y < 0.62f) { *tex = TEX_ROCK; col[0] = 0.72f; col[1] = 0.76f; col[2] = 0.86f; return; }
    if (dq < 13.5f) { *tex = TEX_SNOW; col[0] = 0.72f; col[1] = 0.84f; col[2] = 1.0f; *flags = SF_POWDER; return; }
    *tex = TEX_SAND;
    if (y < -0.8f) { *tex = TEX_ROCK; col[0] = 0.55f; col[1] = 0.5f; col[2] = 0.5f; }   /* warm spring bed */
}

static void build_drifts(void)
{
    Obj *o;
    int i;
    static const float ob[4][2] = { { -36, 20 }, { 36, 20 }, { -52, -36 }, { 52, -36 } };
    lv.name = "DAEMON DRIFTS";
    lv.music = MUS_DRIFTS;
    lv.course = CRS_DRIFTS;
    set_sky(0.4f, 0.62f, 0.95f, 0.88f, 0.92f, 1.0f, 70, 200);
    lv.ambient = 0.58f; lv.diffuse = 0.68f;
    lv.snow = 1;
    lv.death_y = -20;
    g_coin_star_idx = 4;
    lb_begin();
    lb_terrain(-90, -90, 90, 90, 72, 72, drifts_h, drifts_style);
    lb_water(32, 27, 58, 53, -0.5f, -6);
    lv.water[lv.nwater - 1].warm = 1;   /* hot spring */
    /* stepped ice pyramid */
    for (i = 0; i < 7; i++) {
        float hs = 22 - i * 3;
        lb_box(-hs, i * 2.5f - (i == 0 ? 1.0f : 0), -30 - hs, hs, (i + 1) * 2.5f, -30 + hs, TEX_SNOW, TEX_ICE,
               0.85f + i * 0.02f, 0.93f, 1.0f, 0);
    }
    lb_cone(0, 17.5f, -30, 2.0f, 2.5f, 4, TEX_ICE, 0.7f, 0.9f, 1, 1);
    /* ice obelisks */
    for (i = 0; i < 4; i++) {
        float x = ob[i][0], z = ob[i][1], b = drifts_h(x, z);
        lb_box(x - 1.5f, b - 1.0f, z - 1.5f, x + 1.5f, b + 4.0f, z + 1.5f, TEX_SNOW, TEX_ICE, 0.75f, 0.9f, 1.0f, 0);
    }
    /* stepping stones across the powder snow, and the island */
    lb_cyl(-34, -0.5f, 45, 1.3f, 1.3f, 10, TEX_ROCK, TEX_ROCK, 0.9f, 0.85f, 0.8f, 0);
    lb_cyl(-37.6f, -0.5f, 44, 1.3f, 1.3f, 10, TEX_ROCK, TEX_ROCK, 0.9f, 0.85f, 0.8f, 0);
    lb_cyl(-41.2f, -0.5f, 46, 1.3f, 1.3f, 10, TEX_ROCK, TEX_ROCK, 0.9f, 0.85f, 0.8f, 0);
    lb_cyl(-45, -0.5f, 45, 2.6f, 1.5f, 14, TEX_SNOW, TEX_ROCK, 1, 1, 1, 0);
    lb_cyl(-49, -0.5f, 49, 1.3f, 1.3f, 10, TEX_ROCK, TEX_ROCK, 0.9f, 0.85f, 0.8f, 0);
    lb_cyl(-52, -0.5f, 53, 1.3f, 1.3f, 10, TEX_ROCK, TEX_ROCK, 0.9f, 0.85f, 0.8f, 0);
    finish_geometry();

    lv.spawn[0] = v3(0, floor_y(0, 62, 10), 62); lv.spawn_yaw[0] = PI_F;
    /* star 0: pyramid top */
    obj_spawn(OT_STAR, v3(0, 20.4f, -30), 0, 0);
    /* star 1: red fish of the hot spring */
    {
        static const float rc[8][3] = { { 45, -1.5f, 40 }, { 40, -1.2f, 44 }, { 50, -1.2f, 36 }, { 30, 0, 34 },
                                        { 58, 0, 46 }, { 45, 0, 58 }, { 36, 0, 54 }, { 55, 0, 26 } };
        for (i = 0; i < 8; i++) {
            float y = rc[i][1];
            if (y == 0) y = floor_y(rc[i][0], rc[i][2], 10) + 0.3f;
            obj_spawn(OT_REDCOIN, v3(rc[i][0], y, rc[i][2]), 0, 0);
        }
        obj_spawn(OT_STARMARKER, v3(45, 1.5f, 40), 0, 1);
    }
    /* star 2: obelisks */
    for (i = 0; i < 4; i++) {
        float x = ob[i][0], z = ob[i][1];
        obj_spawn(OT_TOPSWITCH, v3(x, drifts_h(x, z) + 4.0f, z), 0, 2);
    }
    obj_spawn(OT_STARMARKER, v3(0, floor_y(0, 30, 10) + 1.0f, 30), 0, 2);
    /* star 3: powder snow island */
    o = obj_spawn(OT_STEAM, v3(45, -0.4f, 40), 0, 12);
    obj_spawn(OT_STAR, v3(-45, 1.2f, 45), 0, 3);
    /* enemies */
    spawn_at(OT_CRAB, 20, 40, 10, 0, 0);
    spawn_at(OT_CRAB, -20, 30, 10, 1, 0);
    spawn_at(OT_CRAB, 30, -10, 10, 2, 0);
    spawn_at(OT_CRAB, -30, -10, 10, 3, 0);
    spawn_at(OT_BUG, 0, 10, 10, 0, 0);
    spawn_at(OT_BUG, -55, 0, 10, 0, 0);
    spawn_at(OT_BUG, 55, 0, 10, 0, 0);
    spawn_at(OT_BUG, 10, -66, 10, 0, 0);
    spawn_at(OT_FORKBOMB, 12, 0, 10, 0, 0);
    spawn_at(OT_FORKBOMB, -12, 0, 10, 0, 0);
    /* pyramid coins: a line on each tier (south face) */
    for (i = 0; i < 6; i++) {
        float hs = 22 - i * 3 - 1.5f, y = (i + 1) * 2.5f;
        coin_line(v3(-6, y + 0.2f, -30 + hs), v3(6, y + 0.2f, -30 + hs), 4, 0);
    }
    coin_ring(v3(0, 0, 45), 4, 8, 1);
    coin_ring(v3(-30, 0, 0), 4, 8, 1);
    coin_ring(v3(30, 0, 0), 4, 8, 1);
    coin_ring(v3(-55, 0, -60), 4, 8, 1);
    coin_ring(v3(55, 0, -60), 4, 8, 1);
    coin_ring(v3(0, 0, -68), 4, 8, 1);
    coin_line(v3(-20, 0, 55), v3(-28, 0, 45), 5, 1);
    coin_line(v3(20, 0, 55), v3(28, 0, 47), 5, 1);
    coin_line(v3(-60, 0, 20), v3(-60, 0, 0), 5, 1);
    coin_line(v3(60, 0, 20), v3(60, 0, 0), 5, 1);
    spawn_at(OT_CRATE, 8, 50, 10, 0, 5);
    spawn_at(OT_CRATE, -8, 50, 10, 0.5f, 5);
    spawn_at(OT_CRATE, 0, -62, 10, 0, -1);
    o = spawn_at(OT_HEART, 6, 40, 10, 0, 0);
    if (o) { o->pos.y += 0.5f; o->home = o->pos; }
    sign(4, 56, 10, PI_F, "DAEMON DRIFTS\nBeware the bluish POWDER SNOW: it slows you\n"
                          "down and swallows you if you stand still. Keep\njumping! The ice pyramid's steps lead up top.");
    sign(-30, 40, 10, -PI_F / 2, "Hop across the stepping stones to reach\nthe little island in the powder snow.");
    sign(-6, 30, 10, PI_F, "Four ice obelisks surround the pyramid.\nStand on top of each one to make it shine!");
    sign(30, 28, 10, PI_F * 0.75f, "HOT SPRING\nA warm soak heals you and you never run out\nof breath in here. Ahhh...");
    for (i = 0; i < 8; i++) {
        float a = TAU_F * i / 8 + 0.3f;
        decor(MDL_PINE_SNOW, 45 + sinf(a) * 15, 40 + cosf(a) * 15, 10, 1.0f, 0.4f);
    }
    for (i = 0; i < 10; i++) {
        float x = -70 + i * 15, z = (i % 2) ? -15 : 10;
        if (fabsf(x) < 30) continue;
        decor(i % 2 ? MDL_CRYSTAL : MDL_ICESPIRE, x, z, 10, 1.0f, 0.6f);
    }
    decor(MDL_CRYSTAL, 22, 62, 10, 1.2f, 0.7f);
    decor(MDL_CRYSTAL, -26, 64, 10, 0.9f, 0.6f);
    decor(MDL_ROCK, 15, 20, 10, 1.5f, 1.4f);
    decor(MDL_ROCK, -18, -66, 10, 2.0f, 1.8f);
}

/* ================================================================
 * ZOMBIE PROCESS MANOR
 * ================================================================ */
static float manor_h(float x, float z)
{
    float h = 0.4f * sinf(x * 0.09f) * cosf(z * 0.07f) + 0.2f * sinf(x * 0.23f + z * 0.19f) + 0.2f;
    float dm = rect_dist(x, z, -24, -54, 30, -14);
    h *= sstep(0, 6, dm);
    h += boundary(x, z, 66, 2.8f);
    return h;
}

static void manor_style(float x, float y, float z, vec3 n, int *tex, float *col, int *flags)
{
    (void)y; (void)flags;
    if (n.y < 0.62f) { *tex = TEX_ROCK; col[0] = 0.5f; col[1] = 0.45f; col[2] = 0.55f; return; }
    *tex = TEX_GRASS;   /* moonlit snow */
    col[0] = 0.62f; col[1] = 0.66f + 0.04f * sinf(x * 0.2f + z * 0.3f); col[2] = 0.82f;
    if (fabsf(x) < 2.5f && z > -20 && z < 45) { *tex = TEX_DIRT; col[0] = 0.6f; col[1] = 0.62f; col[2] = 0.75f; }
}

static void mwall(float x0, float y0, float z0, float x1, float y1, float z1)
{
    lb_box(x0, y0, z0, x1, y1, z1, TEX_BRICK, TEX_BRICK, 0.55f, 0.45f, 0.55f, 0);
}

static void build_manor(void)
{
    Obj *o;
    int i;
    lv.name = "ZOMBIE PROCESS MANOR";
    lv.music = MUS_HAUNT;
    lv.course = CRS_MANOR;
    set_sky(0.04f, 0.04f, 0.14f, 0.2f, 0.2f, 0.32f, 40, 150);
    lv.snow = 1;
    lv.ambient = 0.42f; lv.diffuse = 0.55f;
    lv.death_y = -20;
    lv.stars_sky = 1;
    g_coin_star_idx = 4;
    lb_begin();
    lb_terrain(-80, -80, 80, 80, 64, 64, manor_h, manor_style);
    /* floors */
    lb_box(-20, -0.5f, -50, 20, 0.05f, -20, TEX_PLANK, TEX_WOOD, 0.75f, 0.6f, 0.55f, 0);
    lb_box(-15, 8, -49, 19, 8.5f, -21, TEX_CARPET, TEX_WOOD, 0.6f, 0.4f, 0.6f, 0);
    lb_box(-19, 8, -49, -15, 8.5f, -40, TEX_CARPET, TEX_WOOD, 0.6f, 0.4f, 0.6f, 0);
    lb_box(-19, 8, -24, -15, 8.5f, -21, TEX_CARPET, TEX_WOOD, 0.6f, 0.4f, 0.6f, 0);
    lb_stairs(-19, -24, -15, -40, 0, 8, 1, 16, TEX_PLANK, 0.8f, 0.65f, 0.55f);
    /* outer walls */
    mwall(-20, 0, -21, -2, 16, -20);
    mwall(2, 0, -21, 20, 16, -20);
    mwall(-2, 4, -21, 2, 8.5f, -20);
    mwall(-2, 12, -21, 2, 16, -20);
    mwall(-20, 0, -50, 20, 16, -49);
    mwall(-20, 0, -50, -19, 16, -20);
    mwall(19, 0, -50, 20, 16, -20);
    /* roof */
    lb_box(-20.5f, 16, -50.5f, 20.5f, 16.5f, -19.5f, TEX_SNOW, TEX_DARK, 0.75f, 0.78f, 0.9f, 0);
    battlements(-20.5f, -50.5f, 20.5f, -19.5f, 16.5f, TEX_BRICK, 0.5f, 0.42f, 0.5f);
    lb_cyl(-20, 16.5f, -50, 2.5f, 4, 12, TEX_BRICK, TEX_BRICK, 0.5f, 0.42f, 0.5f, 0);
    lb_cone(-20, 20.5f, -50, 3.2f, 5, 12, TEX_ROOF, 0.6f, 0.4f, 0.6f, 1);
    lb_cyl(20, 16.5f, -50, 2.5f, 4, 12, TEX_BRICK, TEX_BRICK, 0.5f, 0.42f, 0.5f, 0);
    lb_cone(20, 20.5f, -50, 3.2f, 5, 12, TEX_ROOF, 0.6f, 0.4f, 0.6f, 1);
    /* balcony */
    lb_box(-6, 8, -20, 6, 8.5f, -15, TEX_PLANK, TEX_WOOD, 0.7f, 0.55f, 0.5f, 0);
    lb_box(-6, 8.5f, -15.3f, 6, 9.5f, -15, TEX_WOOD, TEX_WOOD, 0.5f, 0.35f, 0.3f, 0);
    lb_box(-6.3f, 8.5f, -20, -6, 9.5f, -15, TEX_WOOD, TEX_WOOD, 0.5f, 0.35f, 0.3f, 0);
    lb_box(6, 8.5f, -20, 6.3f, 9.5f, -15, TEX_WOOD, TEX_WOOD, 0.5f, 0.35f, 0.3f, 0);
    /* library shelves (upstairs, north wall) */
    lb_box(-12, 8.5f, -49, -8, 10.0f, -46, TEX_WOOD, TEX_PLANK, 0.6f, 0.4f, 0.3f, 0);
    lb_box(-6, 8.5f, -49, -2, 11.5f, -46, TEX_WOOD, TEX_PLANK, 0.6f, 0.4f, 0.3f, 0);
    lb_box(0, 8.5f, -49, 4, 13.0f, -46, TEX_WOOD, TEX_PLANK, 0.6f, 0.4f, 0.3f, 0);
    lb_box(6, 8.5f, -49, 10, 14.2f, -46, TEX_WOOD, TEX_PLANK, 0.6f, 0.4f, 0.3f, 0);
    lb_box(12, 8.5f, -40, 18, 10.5f, -36, TEX_WOOD, TEX_PLANK, 0.6f, 0.4f, 0.3f, 0);
    /* hall pillars + graveyard */
    lb_cyl(-8, 0, -30, 0.8f, 8, 10, TEX_STONE, TEX_STONE, 0.6f, 0.55f, 0.6f, 0);
    lb_cyl(8, 0, -30, 0.8f, 8, 10, TEX_STONE, TEX_STONE, 0.6f, 0.55f, 0.6f, 0);
    for (i = 0; i < 12; i++) {
        float x = 25 + (i % 4) * 6, z = 5 + (i / 4) * 7, b = manor_h(x, z);
        lb_box(x - 0.8f, b - 0.5f, z - 0.25f, x + 0.8f, b + 1.6f, z + 0.25f, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0);
    }
    /* iron fence along the path */
    for (i = 0; i < 8; i++) {
        float z = -12 + i * 6;
        lb_box(-4.2f, -0.5f, z, -3.9f, 1.8f, z + 0.3f, TEX_METAL, TEX_METAL, 0.3f, 0.3f, 0.35f, 0);
        lb_box(3.9f, -0.5f, z, 4.2f, 1.8f, z + 0.3f, TEX_METAL, TEX_METAL, 0.3f, 0.3f, 0.35f, 0);
    }
    finish_geometry();

    lv.spawn[0] = v3(0, floor_y(0, 42, 10), 42); lv.spawn_yaw[0] = PI_F;
    /* star 0: the Zombie King in the great hall */
    obj_spawn(OT_GHOSTKING, v3(0, 0.3f, -38), 0, 0);
    /* star 1: red fish in the library */
    obj_spawn(OT_REDCOIN, v3(-10, 10.3f, -47.5f), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-4, 11.8f, -47.5f), 0, 0);
    obj_spawn(OT_REDCOIN, v3(2, 13.3f, -47.5f), 0, 0);
    obj_spawn(OT_REDCOIN, v3(8, 14.5f, -47.5f), 0, 0);
    obj_spawn(OT_REDCOIN, v3(15, 10.8f, -38), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-17, 8.8f, -45), 0, 0);
    obj_spawn(OT_REDCOIN, v3(0, 8.8f, -17), 0, 0);
    obj_spawn(OT_REDCOIN, v3(17, 8.8f, -23), 0, 0);
    obj_spawn(OT_STARMARKER, v3(8, 9.6f, -30), 0, 1);
    /* star 2: rooftop (reach it by climbing the dead tree pole) */
    o = spawn_at(OT_POLE, 23.5f, -24, 10, 0, 0);
    if (o) o->height = 18.5f;
    obj_spawn(OT_STAR, v3(0, 16.9f, -35), 0, 2);
    /* star 3: yard zombies */
    {
        static const float gp[5][2] = { { -25, 10 }, { 28, 20 }, { -30, -30 }, { 35, -35 }, { 15, 40 } };
        for (i = 0; i < 5; i++) {
            o = obj_spawn(OT_GHOST, v3(gp[i][0], floor_y(gp[i][0], gp[i][1], 10) + 0.5f, gp[i][1]), 0, 1);
            if (o) o->param2 = 3;
        }
        obj_spawn(OT_STARMARKER, v3(-12, floor_y(-12, 20, 10) + 1.2f, 20), 0, 3);
    }
    /* ghosts inside */
    obj_spawn(OT_GHOST, v3(10, 9.0f, -40), 0, 0);
    obj_spawn(OT_GHOST, v3(-8, 9.0f, -30), 0, 0);
    /* coins */
    coin_ring(v3(-10, 0.3f, -26), 2.5f, 8, 0);
    coin_ring(v3(10, 0.3f, -26), 2.5f, 8, 0);
    coin_line(v3(-17, 1.0f, -26), v3(-17, 7.5f, -38), 8, 0);
    coin_ring(v3(-30, 0, 30), 4, 8, 1);
    coin_ring(v3(30, 0, -5), 4, 8, 1);
    coin_ring(v3(-45, 0, -10), 4, 8, 1);
    coin_ring(v3(45, 0, 35), 4, 8, 1);
    coin_ring(v3(-40, 0, -45), 4, 8, 1);
    coin_ring(v3(40, 0, -48), 4, 8, 1);
    coin_line(v3(25, 0, 0), v3(43, 0, 0), 5, 1);
    coin_line(v3(25, 0, 26), v3(43, 0, 26), 5, 1);
    coin_line(v3(0, 0, 30), v3(0, 0, -12), 5, 1);
    coin_ring(v3(0, 8.8f, -30), 3, 6, 0);
    spawn_at(OT_CRATE, -10, -44, 2, 0, 5);
    spawn_at(OT_CRATE, 12, -44, 2, 0, 5);
    spawn_at(OT_CRATE, 0, 9.5f, 10.5f, 0, -1);
    o = obj_spawn(OT_HEART, v3(-12, floor_y(-12, 34, 10) + 0.8f, 34), 0, 0);
    obj_spawn(OT_ONEUP, v3(3, 9.2f, -17), 0, 0);
    sign(3, 36, 10, PI_F, "ZOMBIE PROCESS MANOR\nZombie processes are out of phase: attacks go\n"
                          "right through them. GROUND POUND next to one\nto shake it into the real world, then hit it!");
    sign(20, -18, 10, PI_F, "The dead tree is as tall as the manor.\nClimb it and jump onto the roof!");
    for (i = 0; i < 12; i++) {
        float a = TAU_F * i / 12 + 0.2f, r = 50 + (i % 2) * 6;
        decor(MDL_DEADTREE, sinf(a) * r, cosf(a) * r - 5, 10, 1.0f + (i % 3) * 0.2f, 0.4f);
    }
}


/* ================================================================
 * ICICLE SPIRE - climb a tower of ice chimneys with wall kicks
 * ================================================================ */
#define SPIRE_SEGS 7
#define SPIRE_SEG_H 13.0f

static void build_spire(void)
{
    Obj *o;
    int i;
    lv.name = "ICICLE SPIRE";
    lv.music = MUS_SPIRE;
    lv.course = CRS_SPIRE;
    set_sky(0.3f, 0.5f, 0.9f, 0.75f, 0.85f, 0.98f, 80, 260);
    lv.ambient = 0.58f; lv.diffuse = 0.62f;
    lv.death_y = -25;
    lv.clouds = 1;
    lv.snow = 1;
    lb_begin();
    /* soft snowy base (no fall damage anywhere on this course) */
    lb_cyl(0, -2, 0, 16, 2, 28, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
    lb_cyl(0, -6, 0, 11, 4, 20, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
    for (i = 0; i < SPIRE_SEGS; i++) {
        float cx = (i % 2) ? 5.5f : 0.0f, y = i * SPIRE_SEG_H;
        /* two facing ice walls, 3.5 apart: kick from one to the other */
        lb_box(cx - 2.75f, y, -2.5f, cx - 1.75f, y + SPIRE_SEG_H, 2.5f, TEX_SNOW, TEX_ICE, 0.75f, 0.9f, 1.0f, 0);
        lb_box(cx + 1.75f, y, -2.5f, cx + 2.75f, y + SPIRE_SEG_H, 2.5f, TEX_SNOW, TEX_ICE, 0.75f, 0.9f, 1.0f, 0);
        /* rest ledges on both sides of the chimney top (whichever way the last kick goes);
         * the next chimney stands on one of them */
        if (i < SPIRE_SEGS - 1) {
            lb_box(cx + 2.75f, y + SPIRE_SEG_H - 1, -3.5f, cx + 11.75f, y + SPIRE_SEG_H, 3.5f, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
            lb_box(cx - 11.75f, y + SPIRE_SEG_H - 1, -3.5f, cx - 2.75f, y + SPIRE_SEG_H, 3.5f, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
            /* narrow bridges in front of / behind the chimney join the two ledges */
            lb_box(cx - 2.75f, y + SPIRE_SEG_H - 1, 2.5f, cx + 2.75f, y + SPIRE_SEG_H, 3.5f, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
            lb_box(cx - 2.75f, y + SPIRE_SEG_H - 1, -3.5f, cx + 2.75f, y + SPIRE_SEG_H, -2.5f, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
        }
    }
    /* summit: platforms on both sides of the last chimney */
    lb_box(2.75f, SPIRE_SEGS * SPIRE_SEG_H - 1, -6, 14, SPIRE_SEGS * SPIRE_SEG_H, 6, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
    lb_box(-14, SPIRE_SEGS * SPIRE_SEG_H - 1, -6, -2.75f, SPIRE_SEGS * SPIRE_SEG_H, 6, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
    lb_box(-2.75f, SPIRE_SEGS * SPIRE_SEG_H - 1, 2.5f, 2.75f, SPIRE_SEGS * SPIRE_SEG_H, 6, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
    lb_box(-2.75f, SPIRE_SEGS * SPIRE_SEG_H - 1, -6, 2.75f, SPIRE_SEGS * SPIRE_SEG_H, -2.5f, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SNOW);
    lb_cone(12, SPIRE_SEGS * SPIRE_SEG_H, 4, 1.2f, 4, 8, TEX_ICE, 0.7f, 0.9f, 1, 1);
    lb_cone(12, SPIRE_SEGS * SPIRE_SEG_H, -4, 1.2f, 4, 8, TEX_ICE, 0.7f, 0.9f, 1, 1);
    death_floor(-30, -120, -120, 120, 120);
    finish_geometry();

    lv.spawn[0] = v3(0, 0, 9); lv.spawn_yaw[0] = PI_F;
    /* star 0: halfway ledge (top of the 3rd chimney) */
    obj_spawn(OT_STAR, v3(10.4f, 3 * SPIRE_SEG_H + 0.6f, 0), 0, 0);
    /* stars 1 and 2: reach the summit (2 = under the time limit) */
    o = obj_spawn(OT_TIMERSTART, v3(0, 0, 0), 0, 0);
    if (o) { o->f[5] = 1.6f; o->f[7] = 2.4f; o->height = 3; }
    o = obj_spawn(OT_TIMERGOAL, v3(0, SPIRE_SEGS * SPIRE_SEG_H, 0), 0, 1);
    if (o) { o->param2 = 60; o->radius = 12.0f; o->height = 3; }
    /* star 3: red fish floating inside the chimneys */
    for (i = 0; i < SPIRE_SEGS; i++)
        obj_spawn(OT_REDCOIN, v3((i % 2) ? 5.5f : 0.0f, i * SPIRE_SEG_H + 7.0f, 0), 0, 0);
    obj_spawn(OT_REDCOIN, v3(0, 0.3f, 13), 0, 0);
    obj_spawn(OT_STARMARKER, v3(-8, 1.5f, 8), 0, 3);
    /* coins on the ledges, a 1-up on the summit */
    for (i = 1; i < SPIRE_SEGS; i++) {
        float y = i * SPIRE_SEG_H + 0.3f, x = (i % 2) ? 10.5f : -5.0f;
        coin_line(v3(x, y, -2.5f), v3(x, y, 2.5f), 3, 0);
    }
    coin_ring(v3(0, 0.3f, 0), 9, 10, 0);
    obj_spawn(OT_ONEUP, v3(12, SPIRE_SEGS * SPIRE_SEG_H + 0.6f, 0), 0, 0);
    obj_spawn(OT_HEART, v3(6, 0.8f, 8), 0, 0);
    sign(3, 10, 2, PI_F, "ICICLE SPIRE\nClimb the tower of ice chimneys with WALL\n"
                        "KICKS: jump at a wall, then press A again the\nmoment you touch it to bounce to the other wall.\n"
                        "Keep kicking to climb! The snowy ledges are soft,\nso falling never hurts here.");
    for (i = 0; i < 10; i++) {
        float a = TAU_F * i / 10 + 0.3f;
        decor(i % 2 ? MDL_ICESPIRE : MDL_PINE_SNOW, sinf(a) * 13.5f, cosf(a) * 13.5f, 2, 1.0f, 0.5f);
    }
}

/* ================================================================
 * WHITEOUT LABYRINTH - a random ice maze haunted by the OOM Killer
 * ================================================================ */
Maze g_maze;
int g_maze_spotted;
unsigned g_maze_seed;
static unsigned maze_rng;

static unsigned mrand(void)
{
    maze_rng ^= maze_rng << 13; maze_rng ^= maze_rng >> 17; maze_rng ^= maze_rng << 5;
    return maze_rng;
}

int maze_cell(vec3 p, int *cx, int *cz)
{
    int x = (int)floorf((p.x - g_maze.x0) / g_maze.cell), z = (int)floorf((p.z - g_maze.z0) / g_maze.cell);
    if (x < 0 || z < 0 || x >= g_maze.w || z >= g_maze.h) return 0;
    *cx = x; *cz = z;
    return 1;
}

vec3 maze_center(int cx, int cz)
{
    return v3(g_maze.x0 + (cx + 0.5f) * g_maze.cell, 0, g_maze.z0 + (cz + 0.5f) * g_maze.cell);
}

/* breadth-first search from the target so that every cell knows its next step */
int maze_next_step(int fx, int fz, int tx, int tz, int *nx, int *nz)
{
    static const int dx[4] = { 0, 0, -1, 1 }, dz[4] = { -1, 1, 0, 0 };
    static signed char dist[MAZE_MAX][MAZE_MAX];
    static int qx[MAZE_MAX * MAZE_MAX], qz[MAZE_MAX * MAZE_MAX];
    int head = 0, tail = 0, k, best = 127;
    memset(dist, -1, sizeof dist);
    dist[tz][tx] = 0;
    qx[tail] = tx; qz[tail++] = tz;
    while (head < tail) {
        int x = qx[head], z = qz[head++];
        for (k = 0; k < 4; k++) {
            int ax = x + dx[k], az = z + dz[k];
            if (g_maze.wall[z][x] & (1 << k)) continue;
            if (ax < 0 || az < 0 || ax >= g_maze.w || az >= g_maze.h || dist[az][ax] >= 0) continue;
            dist[az][ax] = (signed char)(dist[z][x] + 1);
            qx[tail] = ax; qz[tail++] = az;
        }
    }
    if (dist[fz][fx] < 0) return 0;
    for (k = 0; k < 4; k++) {
        int ax = fx + dx[k], az = fz + dz[k];
        if (g_maze.wall[fz][fx] & (1 << k)) continue;
        if (ax < 0 || az < 0 || ax >= g_maze.w || az >= g_maze.h || dist[az][ax] < 0) continue;
        if (dist[az][ax] < best) { best = dist[az][ax]; *nx = ax; *nz = az; }
    }
    return best < 127;
}

static void maze_open(int x, int z, int k)
{
    static const int dx[4] = { 0, 0, -1, 1 }, dz[4] = { -1, 1, 0, 0 }, opp[4] = { 1, 0, 3, 2 };
    g_maze.wall[z][x] &= (unsigned char)~(1 << k);
    g_maze.wall[z + dz[k]][x + dx[k]] &= (unsigned char)~(1 << opp[k]);
}

static void maze_generate(int w, int h)
{
    static const int dx[4] = { 0, 0, -1, 1 }, dz[4] = { -1, 1, 0, 0 };
    static int sx[MAZE_MAX * MAZE_MAX], sz[MAZE_MAX * MAZE_MAX];
    static unsigned char seen[MAZE_MAX][MAZE_MAX];
    int sp = 0, x, z, i;
    g_maze.w = w; g_maze.h = h;
    memset(seen, 0, sizeof seen);
    for (z = 0; z < h; z++) for (x = 0; x < w; x++) g_maze.wall[z][x] = 15;
    /* recursive backtracker */
    sx[sp] = 0; sz[sp++] = 0; seen[0][0] = 1;
    while (sp > 0) {
        int cx = sx[sp - 1], cz = sz[sp - 1], opts[4], n = 0, k;
        for (k = 0; k < 4; k++) {
            int ax = cx + dx[k], az = cz + dz[k];
            if (ax >= 0 && az >= 0 && ax < w && az < h && !seen[az][ax]) opts[n++] = k;
        }
        if (!n) { sp--; continue; }
        k = opts[mrand() % n];
        maze_open(cx, cz, k);
        seen[cz + dz[k]][cx + dx[k]] = 1;
        sx[sp] = cx + dx[k]; sz[sp++] = cz + dz[k];
    }
    /* knock out some extra walls so there are loops to escape through */
    for (i = 0; i < w * h / 6; i++) {
        int cx = 1 + (int)(mrand() % (unsigned)(w - 2)), cz = 1 + (int)(mrand() % (unsigned)(h - 2)), k = (int)(mrand() % 4);
        if (g_maze.wall[cz][cx] & (1 << k)) maze_open(cx, cz, k);
    }
}

static int maze_walls_count(int x, int z)
{
    int k, n = 0;
    for (k = 0; k < 4; k++) if (g_maze.wall[z][x] & (1 << k)) n++;
    return n;
}

static void build_maze(void)
{
    Obj *o;
    int x, z, i, n;
    const float C = 7.0f, WH = 7.0f, T = 0.5f;   /* cell size, wall height, half thickness */
    const int W = 9, H = 9;
    lv.name = "WHITEOUT LABYRINTH";
    lv.music = MUS_MAZE;
    lv.course = CRS_MAZE;
    set_sky(0.7f, 0.75f, 0.82f, 0.86f, 0.89f, 0.93f, 6, 38);   /* whiteout: you can't see far */
    lv.ambient = 0.62f; lv.diffuse = 0.45f;
    lv.death_y = -20;
    lv.snow = 1;
    maze_rng = g_maze_seed ? g_maze_seed : ((unsigned)(plat_time() * 1000.0) ^ 0x9e3779b9u) | 1u;
    maze_generate(W, H);
    g_maze.cell = C;
    g_maze.x0 = -W * C * 0.5f;
    g_maze.z0 = -H * C * 0.5f;
    g_maze_spotted = 0;
    {
        extern int g_trace;
        if (g_trace) {   /* debug: print the maze */
            for (z = 0; z < H; z++) {
                for (x = 0; x < W; x++) printf("+%s", (g_maze.wall[z][x] & 1) ? "--" : "  ");
                printf("+\n");
                for (x = 0; x < W; x++) printf("%s  ", (g_maze.wall[z][x] & 4) ? "|" : " ");
                printf("|\n");
            }
            for (x = 0; x < W; x++) printf("+--");
            printf("+\n");
        }
    }
    lb_begin();
    lb_box(g_maze.x0 - 12, -1, g_maze.z0 - 12, -g_maze.x0 + 12, 0, -g_maze.z0 + 12, TEX_SNOW, TEX_ICE, 1, 1, 1, 0);
    /* posts at every grid corner, wall slabs between them */
    for (z = 0; z <= H; z++)
        for (x = 0; x <= W; x++) {
            float px = g_maze.x0 + x * C, pz = g_maze.z0 + z * C;
            lb_box(px - T, 0, pz - T, px + T, WH + 0.3f, pz + T, TEX_SNOW, TEX_ICE, 0.7f, 0.85f, 1.0f, 0);
            if (x < W && (z == H ? (g_maze.wall[H - 1][x] & 2) : (g_maze.wall[z][x] & 1)))
                lb_box(px + T, 0, pz - T, px + C - T, WH, pz + T, TEX_SNOW, TEX_ICE, 0.82f, 0.92f, 1.0f, 0);
            if (z < H && (x == W ? (g_maze.wall[z][W - 1] & 8) : (g_maze.wall[z][x] & 4)))
                lb_box(px - T, 0, pz + T, px + T, WH, pz + C - T, TEX_SNOW, TEX_ICE, 0.82f, 0.92f, 1.0f, 0);
        }
    /* an invisible lid at wall height: no climbing over the walls */
    {
        float x0 = g_maze.x0 - 1, z0 = g_maze.z0 - 1, x1 = -g_maze.x0 + 1, z1 = -g_maze.z0 + 1;
        col_add_tri(v3(x0, WH, z0), v3(x1, WH, z0), v3(x1, WH, z1), SF_NOCAM, 0);
        col_add_tri(v3(x0, WH, z0), v3(x1, WH, z1), v3(x0, WH, z1), SF_NOCAM, 0);
    }
    finish_geometry();

    lv.spawn[0] = maze_center(0, 0); lv.spawn_yaw[0] = 0;
    /* star 0: the heart of the labyrinth */
    obj_spawn(OT_STAR, v3add(maze_center(W / 2, H / 2), v3(0, 0.6f, 0)), 0, 0);
    /* star 1: red fish hidden in dead ends */
    n = 0;
    for (i = 0; i < W * H * 3 && n < 8; i++) {
        int cx = (int)(mrand() % W), cz = (int)(mrand() % H);
        int deep = (cx + cz) > 3 && !(cx == W - 1 && cz == H - 1) && !(cx == W / 2 && cz == H / 2);
        int k, dup = 0;
        if (!deep) continue;
        if (i < W * H * 2 && maze_walls_count(cx, cz) < 3) continue;   /* prefer dead ends */
        for (k = 0; k < nobj; k++)
            if (objs[k].active && objs[k].type == OT_REDCOIN && v3distxz(objs[k].pos, maze_center(cx, cz)) < 1) dup = 1;
        if (dup) continue;
        obj_spawn(OT_REDCOIN, v3add(maze_center(cx, cz), v3(0, 0.4f, 0)), 0, 0);
        n++;
    }
    obj_spawn(OT_STARMARKER, v3add(maze_center(0, 0), v3(1.5f, 1.5f, 1.5f)), 0, 1);
    /* star 2: reach the far exit without ever being seen */
    obj_spawn(OT_MAZEEXIT, maze_center(W - 1, H - 1), 0, 2);
    /* the OOM Killer starts far from Tux */
    o = obj_spawn(OT_OOM, maze_center(W - 1, 0), PI_F, 0);
    if (o) { o->i[0] = W / 2; o->i[1] = H / 2; }
    /* fish scattered through the corridors */
    for (i = 0; i < 40; i++) {
        int cx = (int)(mrand() % W), cz = (int)(mrand() % H);
        obj_spawn(OT_COIN, v3add(maze_center(cx, cz), v3(((int)(mrand() % 3) - 1) * 1.5f, 0.3f, ((int)(mrand() % 3) - 1) * 1.5f)), 0, 0);
    }
    obj_spawn(OT_HEART, v3add(maze_center(0, 1), v3(0, 0.8f, 0)), 0, 0);
    sign(1.8f + g_maze.x0 + C * 0.5f, g_maze.z0 + C * 0.5f + 1.8f, 2, PI_F,
         "WHITEOUT LABYRINTH\nThe maze changes every time you enter. Deep\n"
         "inside roams the OOM KILLER - it cannot be\ndefeated! It sees what is in front of it, but\n"
         "not through walls. Break its line of sight and\nhide. The exit is in the far corner.");
}

/* ================================================================
 * AURORA SEA RALLY - dash boots, buoy gates, ramps and whales
 * ================================================================ */
static float sea_hash(int x, int y, int seed)
{
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + (unsigned)seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float)((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}

static const float SEA_GATES[][2] = {
    { 0, 105 }, { -55, 112 }, { -105, 80 }, { -122, 25 }, { -110, -35 }, { -75, -90 },
    { -20, -118 }, { 40, -112 }, { 95, -85 }, { 122, -30 }, { 118, 35 }, { 75, 95 }
};
#define SEA_NG ((int)(sizeof SEA_GATES / sizeof SEA_GATES[0]))

static vec3 sea_pt(int k) { k = (k % SEA_NG + SEA_NG) % SEA_NG; return v3(SEA_GATES[k][0], 0, SEA_GATES[k][1]); }
static float sea_dir(int k) { vec3 d = v3sub(sea_pt(k + 1), sea_pt(k - 1)); return atan2f(d.x, d.z); }
static float seg_yaw(int k) { vec3 d = v3sub(sea_pt(k + 1), sea_pt(k)); return atan2f(d.x, d.z); }
/* point on the segment from gate k to k+1 at fraction t, shifted sideways by lat */
static vec3 seg_at(int k, float t, float lat)
{
    vec3 a = sea_pt(k), b = sea_pt(k + 1), p = v3lerp(a, b, t), f = yaw_dir(seg_yaw(k));
    return v3mad(p, v3(f.z, 0, -f.x), lat);
}

/* an ice ramp: low end under the water, launching toward yaw; returns the lip position */
static vec3 sea_ramp(vec3 c, float yaw, float len, float rise, float width)
{
    float pitch = atanf(rise / len), hl = 0.5f * sqrtf(len * len + rise * rise);
    vec3 f = yaw_dir(yaw);
    /* top surface: from y = -0.4 (start) to rise - 0.4 (lip) */
    float cy = -0.4f + rise * 0.5f - 0.5f * cosf(pitch);
    lb_obox(v3(c.x, cy, c.z), v3(width, 0.5f, hl), yaw, pitch, TEX_ICE, TEX_ICE, 0.7f, 0.95f, 1.0f, 0);
    /* glowing guide stripes */
    {
        int i;
        for (i = 0; i < 4; i++) {
            float t = -0.8f + i * 0.5f;
            vec3 p = v3add(v3mad(c, f, t * hl), v3(0, cy + 0.5f / cosf(pitch) + t * hl * sinf(pitch) + 0.03f, 0));
            lb_obox(p, v3(width * 0.15f, 0.04f, 0.6f), yaw, pitch, TEX_NONE, TEX_NONE, 1.0f, 0.55f, 0.15f, 0);
        }
    }
    return v3add(v3mad(c, f, len * 0.5f), v3(0, rise - 0.4f, 0));
}

/* a point on the flight path off a ramp lip (slope rise/len) at a typical speed, d units past the lip */
static vec3 ramp_arc(vec3 lip, float yaw, float rise, float len, float d)
{
    float sp = 26.0f, vy = sp * rise / len * 1.15f, t = d / sp;
    return v3add(v3mad(lip, yaw_dir(yaw), d), v3(0, vy * t - 0.5f * 32.3f * t * t + 0.2f, 0));
}

static void sea_chunk(vec3 p, int mode, float amp, float speed, float phase)
{
    Obj *o = obj_spawn(OT_ICECHUNK, p, phase, mode);
    if (!o) return;
    o->f[0] = phase; o->f[1] = amp; o->f[2] = speed;
}

static Obj *sea_whale(float cx, float cz, float r, float w, float phase)
{
    Obj *o = obj_spawn(OT_WHALE, v3(cx, 0, cz), 0, 0);
    if (!o) return NULL;
    o->f[0] = cx; o->f[1] = cz; o->f[2] = r; o->f[3] = w; o->f[4] = phase;
    o->home = v3(cx, 0, cz);
    return o;
}

static void build_sea(void)
{
    Obj *o;
    int i, k;
    vec3 lip;
    lv.name = "AURORA SEA RALLY";
    lv.music = MUS_SEA;
    lv.course = CRS_SEA;
    set_sky(0.01f, 0.03f, 0.1f, 0.12f, 0.3f, 0.4f, 70, 280);
    lv.ambient = 0.6f; lv.diffuse = 0.55f;
    lv.death_y = -40;
    lv.stars_sky = 1;
    lv.aurora = 1;
    lv.snow = 1;
    g_coin_star_idx = 4;
    lb_begin();
    /* the sea and its floor */
    lb_box(-170, -15, -170, 170, -14, 170, TEX_ROCK, TEX_ROCK, 0.25f, 0.35f, 0.5f, 0);
    lb_water(-168, -168, 168, 168, 0, -14);
    /* frozen cliffs all around */
    lb_box(-172, -14, -172, 172, 12, -166, TEX_ICE, TEX_ICE, 0.7f, 0.85f, 1, 0);
    lb_box(-172, -14, 166, 172, 12, 172, TEX_ICE, TEX_ICE, 0.7f, 0.85f, 1, 0);
    lb_box(-172, -14, -166, -166, 12, 166, TEX_ICE, TEX_ICE, 0.7f, 0.85f, 1, 0);
    lb_box(166, -14, -166, 172, 12, 166, TEX_ICE, TEX_ICE, 0.7f, 0.85f, 1, 0);
    for (i = 0; i < 28; i++) {
        float a = TAU_F * i / 28, r = 178 + 8 * sinf(i * 2.3f);
        lb_cone(sinf(a) * r, -2, cosf(a) * r, 14 + 6 * sea_hash(i, 1, 4), 26 + 22 * sea_hash(i, 2, 4), 7, TEX_ICE, 0.75f, 0.9f, 1, 0);
    }
    /* glacier island in the middle (the course goes around it) */
    lb_cone(0, -3, 0, 62, 28, 28, TEX_SNOW, 1, 1, 1, 1);
    lb_cone(-14, 10, 10, 9, 16, 9, TEX_ICE, 0.75f, 0.9f, 1, 1);
    lb_cone(12, 8, -8, 7, 13, 8, TEX_ICE, 0.75f, 0.9f, 1, 1);
    /* the whale's spout peak */
    lb_cyl(19, -14, -80, 4, 28, 16, TEX_SNOW, TEX_ICE, 0.85f, 0.95f, 1, 0);
    /* start / finish arch at gate 0 */
    {
        vec3 g = sea_pt(0), f = yaw_dir(sea_dir(0)), r = v3(f.z, 0, -f.x);
        vec3 pa = v3mad(g, r, 9.5f), pb = v3mad(g, r, -9.5f);
        lb_cyl(pa.x, -3, pa.z, 1.0f, 10, 10, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
        lb_cyl(pb.x, -3, pb.z, 1.0f, 10, 10, TEX_ICE, TEX_ICE, 0.8f, 0.9f, 1, 0);
        for (k = 0; k < 10; k++) {
            vec3 c = v3add(v3lerp(pa, pb, (k + 0.5f) / 10), v3(0, 6.3f, 0));
            float cc = (k & 1) ? 0.1f : 0.95f;
            lb_obox(c, v3(0.95f, 0.6f, 0.15f), sea_dir(0) + PI_F / 2, 0, TEX_NONE, TEX_NONE, cc, cc, cc, 0);
            lb_obox(v3add(c, v3(0, 1.2f, 0)), v3(0.95f, 0.6f, 0.15f), sea_dir(0) + PI_F / 2, 0, TEX_NONE, TEX_NONE, 1 - cc, 1 - cc, 1 - cc, 0);
        }
    }
    /* start dock */
    lb_box(10, -3, 99, 22, 0.3f, 111, TEX_SNOW, TEX_ICE, 1, 1, 1, SF_SLIPPERY);
    /* ramps */
    lip = sea_ramp(seg_at(2, 0.5f, 0), seg_yaw(2), 12, 4.5f, 3.5f);
    obj_spawn(OT_REDCOIN, ramp_arc(lip, seg_yaw(2), 4.5f, 12, 9), 0, 0);
    lip = sea_ramp(seg_at(4, 0.55f, -3), seg_yaw(4), 10, 3.5f, 3.0f);
    obj_spawn(OT_REDCOIN, ramp_arc(lip, seg_yaw(4), 3.5f, 10, 8), 0, 0);
    lip = sea_ramp(seg_at(9, 0.45f, 0), seg_yaw(9), 16, 7.0f, 4.0f);   /* the big one */
    obj_spawn(OT_REDCOIN, ramp_arc(lip, seg_yaw(9), 7.0f, 16, 10.5f), 0, 0);
    for (k = 0; k < 5; k++) obj_spawn(OT_COIN, ramp_arc(lip, seg_yaw(9), 7.0f, 16, 3.0f + k * 3.5f + (k >= 2) * 1.5f), 0, 0);
    /* a small ramp with a wall of ice behind it on the north straight: jump it or go around */
    lip = sea_ramp(seg_at(11, 0.45f, 3), seg_yaw(11), 8, 3.0f, 3.0f);
    {
        vec3 w = seg_at(11, 0.62f, 3);
        lb_obox(v3(w.x, 0.2f, w.z), v3(4.5f, 1.4f, 1.2f), seg_yaw(11), 0, TEX_SNOW, TEX_ICE, 0.8f, 0.92f, 1, 0);
    }
    /* icebergs and floes along the course */
    for (k = 0; k < SEA_NG; k++) {
        vec3 a = seg_at(k, 0.3f, (k & 1) ? 17 : -17), b = seg_at(k, 0.75f, (k & 1) ? -19 : 19);
        lb_cone(a.x, -6, a.z, 5 + 2 * sea_hash(k, 3, 3), 14 + 8 * sea_hash(k, 4, 3), 8, TEX_ICE, 0.75f, 0.9f, 1, 1);
        lb_cyl(b.x, -0.6f, b.z, 3.5f + 2 * sea_hash(k, 5, 3), 0.85f, 12, TEX_SNOW, TEX_ICE, 0.95f, 0.98f, 1, SF_SLIPPERY);
    }
    /* icebergs in the lane on the west side: weave between them */
    {
        vec3 a = seg_at(3, 0.35f, 4), b = seg_at(3, 0.7f, -4);
        lb_cone(a.x, -6, a.z, 3.2f, 12, 8, TEX_ICE, 0.75f, 0.9f, 1, 1);
        lb_cone(b.x, -6, b.z, 3.2f, 11, 8, TEX_ICE, 0.75f, 0.9f, 1, 1);
    }
    /* far floes with red fish */
    lb_cyl(-135, -0.6f, -135, 6, 0.85f, 14, TEX_SNOW, TEX_ICE, 0.95f, 0.98f, 1, SF_SLIPPERY);
    lb_cyl(-140, -0.6f, 140, 6, 0.85f, 14, TEX_SNOW, TEX_ICE, 0.95f, 0.98f, 1, SF_SLIPPERY);
    lb_cyl(140, -0.6f, 138, 5, 0.85f, 14, TEX_SNOW, TEX_ICE, 0.95f, 0.98f, 1, SF_SLIPPERY);
    death_floor(-40, -200, -200, 200, 200);
    finish_geometry();

    lv.spawn[0] = v3(16, 0.3f, 105); lv.spawn_yaw[0] = sea_dir(0);
    /* buoy gates (gate 0 = start / finish line) */
    for (k = 0; k < SEA_NG; k++) {
        o = obj_spawn(OT_GATE, sea_pt(k), sea_dir(k), k);
        if (o && k == 0) o->f[0] = 9.0f;
    }
    /* the rival waits on the start dock */
    o = obj_spawn(OT_SEAL, v3(12.5f, 0.3f, 101.5f), sea_dir(0), 0);
    if (o) o->f[3] = sea_dir(0);
    /* boost pads */
    {
        static const float bp[][3] = { { 0, 0.5f, 0 }, { 5, 0.5f, 3 }, { 7, 0.5f, -3 }, { 10, 0.55f, 0 }, { 1, 0.6f, -4 } };
        for (k = 0; k < (int)(sizeof bp / sizeof bp[0]); k++)
            obj_spawn(OT_BOOSTPAD, seg_at((int)bp[k][0], bp[k][1], bp[k][2]), seg_yaw((int)bp[k][0]), 0);
    }
    /* drifting ice: a slalom on the south-east stretch, and some strays */
    for (i = 0; i < 6; i++) {
        vec3 p = seg_at(8, 0.15f + i * 0.13f, (i & 1) ? 4.5f : -4.5f);
        sea_chunk(v3(p.x, 0, p.z), 1 + (i % 2), 2.5f, 0.8f + 0.1f * i, i * 1.3f);
    }
    for (i = 0; i < 6; i++) {
        int sg = (i * 5 + 1) % SEA_NG;
        vec3 p = seg_at(sg, 0.6f, (i & 1) ? 8 : -8);
        if (sg == 8 || sg == 9 || sg == 2 || sg == 4 || sg == 11) continue;
        sea_chunk(v3(p.x, 0, p.z), 1, 6.0f, 0.5f, i * 2.0f);
    }
    /* the great whales: one circles the spout peak, one cruises across the southern straight */
    sea_whale(0, -80, 12, 0.42f, 0);
    sea_whale(10, -122, 13, -0.38f, 1.5f);
    /* star 3 on the spout peak */
    obj_spawn(OT_STAR, v3(19, 14.6f, -80), 0, 3);
    /* star 2: red fish */
    {
        vec3 p = seg_at(8, 0.5f, 0);
        obj_spawn(OT_REDCOIN, v3(p.x, 0.5f, p.z), 0, 0);
    }
    obj_spawn(OT_REDCOIN, v3(-135, 0.8f, -135), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-140, 0.8f, 140), 0, 0);
    obj_spawn(OT_REDCOIN, v3(140, 0.8f, 138), 0, 0);
    obj_spawn(OT_REDCOIN, v3(0, 25.6f, 0), 0, 0);
    obj_spawn(OT_STARMARKER, v3(16, 4.0f, 110), 0, 2);
    /* fish along the course */
    for (k = 0; k < SEA_NG; k++) {
        if (k == 9) continue;
        coin_line(v3add(seg_at(k, 0.2f, (k & 1) ? 3 : -3), v3(0, 0.5f, 0)), v3add(seg_at(k, 0.4f, (k & 1) ? 3 : -3), v3(0, 0.5f, 0)), 5, 0);
        coin_line(v3add(seg_at(k, 0.7f, (k & 1) ? -5 : 5), v3(0, 0.5f, 0)), v3add(seg_at(k, 0.85f, (k & 1) ? -5 : 5), v3(0, 0.5f, 0)), 3, 0);
    }
    coin_ring(v3(0, 0.5f, -80), 6, 8, 0);
    for (i = 0; i < 3; i++) coin_ring(v3(0, 6.6f + i * 7.0f, 0), 46 - i * 12.0f, 10 - i * 2, 1);
    obj_spawn(OT_ONEUP, v3(0, 26.0f, 3), 0, 0);
    obj_spawn(OT_HEART, v3(16, 1.0f, 101), 0, 0);
    for (i = 0; i < 12; i++) {
        float a = TAU_F * i / 12 + 0.2f, r = 30 + 12 * sea_hash(i, 9, 2);
        float x = sinf(a) * r, z = cosf(a) * r;
        decor(i % 3 ? MDL_PINE_SNOW : MDL_ICESPIRE, x, z, 30, 1.0f, 0.5f);
    }
    sign(13, 108, 2, sea_dir(0) + PI_F, "AURORA SEA RALLY\nYour DASH BOOTS run on water - as long as you\n"
                                          "keep moving! Steer with left/right, pull back\nto brake, Z for a sharp carve, A to jump.\n"
                                          "Press B in mid-air for a spin trick: land it for\na speed boost. Skate into the seal on this\ndock to start the race!");
}

/* ================================================================
 * FROSTCLOUD SKYWAY - rocket flight along a loop in the sky (sky.c)
 * ================================================================ */
static void sky_coin(float s, float y)
{
    Obj *o = obj_spawn(OT_SKYCOIN, sky_world(s, y), 0, 0);
    if (o) { o->f[6] = s; o->f[7] = y; o->home = o->pos; }
}

static void build_sky(void)
{
    int i, k, n;
    float L = sky_len(), s;
    lv.name = "FROSTCLOUD SKYWAY";
    lv.music = MUS_SKY;
    lv.course = CRS_SKY;
    set_sky(0.25f, 0.45f, 0.9f, 0.8f, 0.88f, 1.0f, 90, 320);
    lv.ambient = 0.62f; lv.diffuse = 0.6f;
    lv.death_y = -60;
    lv.clouds = 1;
    lv.snow = 1;
    lb_begin();
    /* a sea of clouds far below */
    lb_alpha_mode = 1;
    for (i = -4; i < 4; i++)
        for (k = -4; k < 4; k++) {
            float x0 = i * 60.0f, z0 = k * 60.0f, y = 4 + 2 * sea_hash(i, k, 7);
            lb_quad(TEX_CLOUD, v3(x0, y, z0), v3(x0, y, z0 + 60), v3(x0 + 60, y, z0 + 60), v3(x0 + 60, y, z0), 1, 1, 1, 0, 0);
            lb_quad(TEX_CLOUD, v3(x0 + 20, y - 3, z0 + 10), v3(x0 + 20, y - 3, z0 + 70), v3(x0 + 80, y - 3, z0 + 70), v3(x0 + 80, y - 3, z0 + 10), 0.9f, 0.93f, 1, 0, 0);
        }
    lb_alpha_mode = 0;
    lb_box(-260, -8, -260, 260, -6, 260, TEX_SNOW, TEX_SNOW, 0.85f, 0.9f, 1, 0);
    /* the great frost crystal in the middle of the loop */
    lb_cone(0, -6, 0, 14, 90, 6, TEX_ICE, 0.6f, 0.85f, 1, 0);
    lb_cone(0, 84, 0, 7, -22, 6, TEX_ICE, 0.7f, 0.9f, 1, 0);
    for (i = 0; i < 5; i++) {
        float a = TAU_F * i / 5;
        lb_cone(sinf(a) * 12, -6, cosf(a) * 12, 6, 40 + 12 * sea_hash(i, 2, 2), 5, TEX_ICE, 0.65f, 0.88f, 1, 0);
    }
    /* floating ice islands inside the loop (never between the camera and Tux) */
    for (i = 0; i < 14; i++) {
        float ss = L * (i + 0.5f) / 14;
        vec3 p = v3mad(sky_point(ss), sky_normal(ss), -(16 + 10 * sea_hash(i, 3, 3)));
        float r = 4 + 3 * sea_hash(i, 4, 3), y = p.y - 16 + 26 * sea_hash(i, 5, 3);
        lb_cyl(p.x, y, p.z, r, 1.2f, 10, TEX_SNOW, TEX_ICE, 1, 1, 1, 0);
        lb_cone(p.x, y, p.z, r, -r * 1.6f, 10, TEX_ICE, 0.7f, 0.88f, 1, 0);
        if (i % 2) lb_cone(p.x + r * 0.3f, y + 1.2f, p.z, 1.2f, 3.5f, 6, TEX_ICE, 0.75f, 0.92f, 1, 0);
    }
    finish_geometry();

    lv.spawn[0] = sky_world(0, 0); lv.spawn_yaw[0] = atan2f(sky_tangent(0).x, sky_tangent(0).z);
    /* fish patterns along the whole loop */
    n = (int)(L / 13.0f);
    for (k = 0; k < n; k++) {
        float s0 = 8 + k * 13.0f;
        float yc = (k % 3 == 0) ? 0 : ((k % 3 == 1) ? 8 : -8);
        switch (k % 5) {
        case 0:   /* a wave */
            for (i = 0; i < 7; i++) sky_coin(s0 + i * 1.8f, 7 * sinf((s0 + i * 1.8f) * 0.15f));
            break;
        case 1:   /* a ring to paraloop */
            for (i = 0; i < 10; i++) { float a = TAU_F * i / 10; sky_coin(s0 + 6 + cosf(a) * 3.6f, yc + sinf(a) * 3.6f); }
            break;
        case 2:   /* a column: sweep up or down through it */
            for (i = 0; i < 7; i++) sky_coin(s0 + 6, -11 + i * 3.6f);
            break;
        case 3:   /* a cluster */
            for (i = 0; i < 9; i++) sky_coin(s0 + 4 + (i % 3) * 2.2f, yc + (i / 3 - 1) * 2.2f);
            break;
        default:  /* a diagonal */
            for (i = 0; i < 6; i++) sky_coin(s0 + i * 2.0f, (k & 1 ? -1 : 1) * (-9 + i * 3.6f));
            break;
        }
    }
    /* dash rings */
    for (s = 30; s < L - 10; s += 55) {
        Obj *o = obj_spawn(OT_SKYRING, sky_world(s, (int)s % 2 ? 11.0f : -10.0f), 0, 0);
        if (o) { vec3 t = sky_tangent(s); o->yaw = atan2f(t.x, t.z); }
    }
}


/* ================================================================
 * STARDUST ORBIT - free rocket flight around an ice planet (space.c)
 * ================================================================ */
/* one triangle of a closed body around c, wound so that it faces outward */
static void out_tri(int tex, vec3 c, vec3 a, vec3 b, vec3 d, float r, float g, float bb, int collide)
{
    vec3 n = v3cross(v3sub(b, a), v3sub(d, a)), m = v3scale(v3add(v3add(a, b), d), 1.0f / 3);
    if (v3dot(n, v3sub(m, c)) < 0) { vec3 t = b; b = d; d = t; }
    lb_tri(tex, a, b, d, r, g, bb, 0, collide);
}

static vec3 ball_v(vec3 c, float R, int i, int j, int sl, int st, int seed, float rough)
{
    float th = TAU_F * (i % sl) / sl, ph = PI_F * j / st - PI_F / 2;
    float k = (j == 0 || j == st) ? 1.0f : 1.0f + rough * (sea_hash(i % sl, j, seed) - 0.5f);
    return v3add(c, v3(cosf(ph) * sinf(th) * R * k, sinf(ph) * R * k, cosf(ph) * cosf(th) * R * k));
}

/* a (lumpy) ball: style 0 = rock, 1 = the ice planet (snowy poles), 2 = grey moon */
static void lb_ball(vec3 c, float R, int sl, int st, int seed, float rough, int style)
{
    int i, j;
    for (j = 0; j < st; j++)
        for (i = 0; i < sl; i++) {
            vec3 a = ball_v(c, R, i, j, sl, st, seed, rough), b = ball_v(c, R, i + 1, j, sl, st, seed, rough);
            vec3 e = ball_v(c, R, i + 1, j + 1, sl, st, seed, rough), d = ball_v(c, R, i, j + 1, sl, st, seed, rough);
            float lat = fabsf((j + 0.5f) / st - 0.5f) * 2, h = sea_hash(i, j, seed + 7), cr, cg, cb;
            int tex;
            if (style == 1) {
                if (lat > 0.62f + 0.12f * h) { tex = TEX_SNOW; cr = cg = cb = 0.95f + 0.05f * h; }
                else { tex = TEX_ICE; cr = 0.45f + 0.2f * h; cg = 0.7f + 0.15f * h; cb = 1.0f; }
            } else if (style == 2) { tex = TEX_ROCK; cr = cg = 0.6f + 0.2f * h; cb = 0.68f + 0.2f * h; }
            else { tex = TEX_ROCK; cr = 0.72f + 0.15f * h; cg = 0.64f + 0.12f * h; cb = 0.6f + 0.12f * h; }
            if (j > 0) out_tri(tex, c, a, b, e, cr, cg, cb, 1);
            if (j < st - 1) out_tri(tex, c, a, e, d, cr, cg, cb, 1);
        }
}

static void space_boost_ring(vec3 p, vec3 dir)
{
    Obj *o = obj_spawn(OT_SPACERING, p, atan2f(dir.x, dir.z), -1);
    if (o) { dir = v3norm(dir); o->pitch = asinf(clampf(dir.y, -1, 1)); }
}

static void build_space(void)
{
    static const vec3 avoid[] = { { 172, 4, 40 }, { -120, 12, -150 } };   /* meteors in the belt */
    vec3 st = v3(0, 40, -240), moon = v3(230, 40, 140), rf[3];
    int i, k;
    Obj *o;
    lv.name = "STARDUST ORBIT";
    lv.music = MUS_SPACE;
    lv.course = CRS_SPACE;
    set_sky(0.0f, 0.0f, 0.03f, 0.0f, 0.0f, 0.02f, 320, 900);
    lv.ambient = 0.55f; lv.diffuse = 0.8f;
    lv.death_y = -5000;
    lv.space = 1;
    g_coin_star_idx = 5;
    lb_begin();
    /* the ice planet and its ring */
    lb_ball(v3(0, 0, 0), 38, 28, 18, 1, 0.06f, 1);
    lb_alpha_mode = 1;
    for (i = 0; i < 64; i++) {
        float a0 = TAU_F * i / 64, a1 = TAU_F * (i + 1) / 64;
        for (k = 0; k < 3; k++) {
            float r0 = 52 + k * 7, r1 = r0 + 6, c = k == 1 ? 0.75f : 0.95f;
            lb_quad(TEX_NONE, v3(sinf(a0) * r0, 0, cosf(a0) * r0), v3(sinf(a1) * r0, 0, cosf(a1) * r0),
                    v3(sinf(a1) * r1, 0, cosf(a1) * r1), v3(sinf(a0) * r1, 0, cosf(a0) * r1), c * 0.7f, c * 0.85f, c, 0, 0);
        }
    }
    lb_alpha_mode = 0;
    /* the little grey moon */
    lb_ball(moon, 16, 16, 10, 5, 0.12f, 2);
    /* KERNEL STATION: a hub tube to fly through, solar wings, modules and an antenna */
    lb_box(st.x - 5.5f, st.y - 5.5f, st.z - 25, st.x + 5.5f, st.y - 4.5f, st.z + 25, TEX_METAL, TEX_METAL, 0.8f, 0.82f, 0.9f, 0);
    lb_box(st.x - 5.5f, st.y + 4.5f, st.z - 25, st.x + 5.5f, st.y + 5.5f, st.z + 25, TEX_METAL, TEX_METAL, 0.8f, 0.82f, 0.9f, 0);
    lb_box(st.x - 5.5f, st.y - 4.5f, st.z - 25, st.x - 4.5f, st.y + 4.5f, st.z + 25, TEX_METAL, TEX_METAL, 0.75f, 0.78f, 0.88f, 0);
    lb_box(st.x + 4.5f, st.y - 4.5f, st.z - 25, st.x + 5.5f, st.y + 4.5f, st.z + 25, TEX_METAL, TEX_METAL, 0.75f, 0.78f, 0.88f, 0);
    for (k = 0; k < 2; k++) {   /* glowing frames around both mouths */
        float z = st.z + (k ? 25.5f : -25.5f);
        lb_box(st.x - 6.5f, st.y - 6.5f, z - 0.5f, st.x + 6.5f, st.y - 5.5f, z + 0.5f, TEX_NONE, TEX_NONE, 0.3f, 1, 0.6f, 0);
        lb_box(st.x - 6.5f, st.y + 5.5f, z - 0.5f, st.x + 6.5f, st.y + 6.5f, z + 0.5f, TEX_NONE, TEX_NONE, 0.3f, 1, 0.6f, 0);
        lb_box(st.x - 6.5f, st.y - 5.5f, z - 0.5f, st.x - 5.5f, st.y + 5.5f, z + 0.5f, TEX_NONE, TEX_NONE, 0.3f, 1, 0.6f, 0);
        lb_box(st.x + 5.5f, st.y - 5.5f, z - 0.5f, st.x + 6.5f, st.y + 5.5f, z + 0.5f, TEX_NONE, TEX_NONE, 0.3f, 1, 0.6f, 0);
    }
    /* baffles inside: over, under, right, left */
    lb_box(st.x - 4.5f, st.y, st.z + 14.6f, st.x + 4.5f, st.y + 4.5f, st.z + 15.4f, TEX_CIRCUIT, TEX_METAL, 1, 0.5f, 0.4f, 0);
    lb_box(st.x - 4.5f, st.y - 4.5f, st.z + 4.6f, st.x + 4.5f, st.y, st.z + 5.4f, TEX_CIRCUIT, TEX_METAL, 1, 0.5f, 0.4f, 0);
    lb_box(st.x - 4.5f, st.y - 4.5f, st.z - 5.4f, st.x, st.y + 4.5f, st.z - 4.6f, TEX_CIRCUIT, TEX_METAL, 1, 0.5f, 0.4f, 0);
    lb_box(st.x, st.y - 4.5f, st.z - 13.4f, st.x + 4.5f, st.y + 4.5f, st.z - 12.6f, TEX_CIRCUIT, TEX_METAL, 1, 0.5f, 0.4f, 0);
    for (k = -1; k <= 1; k += 2) {
        lb_box(st.x + k * 5.5f, st.y - 1, st.z - 1, st.x + k * 30, st.y + 1, st.z + 1, TEX_METAL, TEX_METAL, 0.7f, 0.7f, 0.8f, 0);
        lb_box(st.x + k * 30, st.y - 0.4f, st.z - 14, st.x + k * 62, st.y + 0.4f, st.z - 1.5f, TEX_CIRCUIT, TEX_METAL, 0.35f, 0.55f, 1, 0);
        lb_box(st.x + k * 30, st.y - 0.4f, st.z + 1.5f, st.x + k * 62, st.y + 0.4f, st.z + 14, TEX_CIRCUIT, TEX_METAL, 0.35f, 0.55f, 1, 0);
    }
    lb_box(st.x - 4, st.y + 5.5f, st.z + 6, st.x + 4, st.y + 11, st.z + 18, TEX_METAL, TEX_METAL, 0.95f, 0.95f, 1, 0);
    lb_box(st.x - 4, st.y + 5.5f, st.z - 18, st.x + 4, st.y + 11, st.z - 6, TEX_METAL, TEX_METAL, 0.95f, 0.95f, 1, 0);
    lb_cyl(st.x, st.y + 5.5f, st.z, 0.6f, 18, 6, TEX_METAL, TEX_METAL, 0.9f, 0.9f, 0.95f, 0);
    lb_box(st.x - 3, st.y - 9, st.z - 3, st.x + 3, st.y - 5.5f, st.z + 3, TEX_METAL, TEX_METAL, 0.6f, 0.6f, 0.7f, 0);
    /* the asteroid belt (rocks keep clear of the red fish and the meteors) */
    for (k = 0; k < 3; k++) {
        float a = 0.8f + k * 1.95f;
        rf[k] = v3(sinf(a) * 170, (k - 1) * 6.0f, cosf(a) * 170);
    }
    for (i = 0; i < 46; i++) {
        float a = TAU_F * (i + 0.6f * sea_hash(i, 1, 30)) / 46, r = 150 + 40 * sea_hash(i, 2, 30);
        float y = -15 + 30 * sea_hash(i, 3, 30), sz = 2.5f + 4.0f * sea_hash(i, 4, 30);
        vec3 c = v3(sinf(a) * r, y, cosf(a) * r);
        int ok = 1;
        for (k = 0; k < 3; k++) if (v3dist(c, rf[k]) < sz + 6) ok = 0;
        for (k = 0; k < 2; k++) if (v3dist(c, avoid[k]) < sz + 8) ok = 0;
        if (ok) lb_ball(c, sz, 7, 5, 40 + i, 0.5f, 0);
    }
    finish_geometry();

    /* the ring course: Tux starts lined up with the first (green) ring */
    for (k = 0; k < SPACE_RINGS; k++) {
        vec3 p = space_ring_pos(k), t = v3norm(v3sub(space_ring_pos((k + 1) % SPACE_RINGS), space_ring_pos((k + SPACE_RINGS - 1) % SPACE_RINGS)));
        o = obj_spawn(OT_SPACERING, p, atan2f(t.x, t.z), k);
        if (o) o->pitch = asinf(clampf(t.y, -1, 1));
        coin_line(v3lerp(p, space_ring_pos((k + 1) % SPACE_RINGS), 0.3f), v3lerp(p, space_ring_pos((k + 1) % SPACE_RINGS), 0.7f), 3, 0);
        if (k == 0) {
            vec3 th = v3norm(v3(t.x, 0, t.z));
            lv.spawn[0] = v3sub(v3mad(p, th, -45), v3(0, 0.8f, 0));
            lv.spawn_yaw[0] = atan2f(t.x, t.z);
        }
    }
    /* star 0: in the middle of the station's tube */
    obj_spawn(OT_STAR, st, 0, 0);
    obj_spawn(OT_COIN, v3(st.x, st.y - 2.2f, st.z + 20), 0, 0);
    obj_spawn(OT_COIN, v3(st.x, st.y - 2.2f, st.z + 17), 0, 0);
    obj_spawn(OT_COIN, v3(st.x, st.y + 2.2f, st.z + 10), 0, 0);
    obj_spawn(OT_COIN, v3(st.x, st.y + 2.2f, st.z + 7), 0, 0);
    obj_spawn(OT_COIN, v3(st.x + 2.2f, st.y, st.z - 8), 0, 0);
    obj_spawn(OT_COIN, v3(st.x - 2.2f, st.y, st.z - 17), 0, 0);
    obj_spawn(OT_COIN, v3(st.x - 2.2f, st.y, st.z - 21), 0, 0);
    /* star 2: the comet */
    o = obj_spawn(OT_COMET, v3(0, 0, 0), 0, 0);
    if (o) o->f[0] = 5.0f;
    /* star 3: red fish */
    for (k = 0; k < 3; k++) obj_spawn(OT_REDCOIN, rf[k], 0, 0);
    obj_spawn(OT_REDCOIN, v3(st.x, st.y + 25.0f, st.z), 0, 0);              /* antenna tip */
    obj_spawn(OT_REDCOIN, v3(st.x + 46, st.y + 2.5f, st.z), 0, 0);          /* between the solar wings */
    obj_spawn(OT_REDCOIN, v3add(moon, v3scale(v3norm(moon), 19.5f)), 0, 0);  /* far side of the moon */
    obj_spawn(OT_REDCOIN, v3(0, -46, 0), 0, 0);                             /* under the south pole */
    obj_spawn(OT_REDCOIN, v3(-29.2f, 0, 38.1f), 0, 0);                      /* between planet and ring */
    obj_spawn(OT_STARMARKER, v3(0, 88, 0), 0, 3);
    /* star 4: ice meteors */
    obj_spawn(OT_METEOR, v3(0, 60, 0), 0, 0);
    obj_spawn(OT_METEOR, v3(0, -62, 0), 0, 0);
    obj_spawn(OT_METEOR, avoid[0], 0, 0);
    obj_spawn(OT_METEOR, avoid[1], 0, 0);
    obj_spawn(OT_METEOR, v3(st.x, st.y, st.z - 44), 0, 0);
    obj_spawn(OT_METEOR, v3(205, 50, 128), 0, 0);
    /* drifting asteroids: smash them at full boost for fish */
    for (i = 0; i < 18; i++) {
        o = obj_spawn(OT_ASTEROID, v3(0, 0, 0), 0, i);
        if (!o) break;
        o->f[0] = TAU_F * i / 18;
        o->f[1] = 156 + 28 * sea_hash(i, 5, 31);
        o->f[2] = -10 + 20 * sea_hash(i, 6, 31);
        o->f[3] = (i & 1 ? 1 : -1) * (0.02f + 0.03f * sea_hash(i, 7, 31));
        o->f[5] = 0.3f + sea_hash(i, 8, 31);
        o->radius = 1.6f + 1.1f * sea_hash(i, 9, 31);
    }
    /* fish: around the planet under its ring, and around the moon */
    coin_ring(v3(0, 0, 0), 46, 24, 0);
    coin_ring(moon, 22, 12, 0);
    obj_spawn(OT_ONEUP, v3add(moon, v3(0, 19, 0)), 0, 0);
    /* boost rings on the way to the station, the moon and the north pole */
    space_boost_ring(v3(0, 22, -130), v3(0, 0.12f, -1));
    space_boost_ring(v3(0, 32, -175), v3(0, 0.1f, -1));
    space_boost_ring(v3(115, 18, 70), v3norm(moon));
    space_boost_ring(v3(165, 28, 100), v3norm(moon));
    space_boost_ring(v3(-60, 30, 30), v3(0.5f, 0.5f, -0.4f));
    space_boost_ring(v3(-150, 20, 120), v3(1, 0, 0.2f));
}

void build_level(int level)
{
    switch (level) {
    case LV_GROUNDS: build_grounds(); break;
    case LV_LOBBY: build_lobby(); break;
    case LV_PEAK: build_peak(); break;
    case LV_BAY: build_bay(); break;
    case LV_FORT: build_fort(); break;
    case LV_SLIDE: build_slide(); break;
    case LV_LAIR: build_lair(); break;
    case LV_DRIFTS: build_drifts(); break;
    case LV_MANOR: build_manor(); break;
    case LV_SPIRE: build_spire(); break;
    case LV_MAZE: build_maze(); break;
    case LV_SEA: build_sea(); break;
    case LV_SKY: build_sky(); break;
    case LV_SPACE: build_space(); break;
    }
}

float terrain_h(float x, float z) { return peak_h(x, z); }
