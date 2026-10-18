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
    if (lv.nlabels >= MAX_LABELS) return;
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
        /* NANO VILLAGE (west wall) and HUGEPAGE HOUSE (east wall), deep in the hall */
        lobby_window(v3(-30.0f, 3.3f, -24), PI_F / 2, 5, 4.0f, TEX_PORTAL13, 0.4f, 0.85f, 0.35f, CRS_NANO);
        lobby_window(v3(30.0f, 3.3f, -24), -PI_F / 2, 5, 4.0f, TEX_PORTAL14, 0.85f, 0.55f, 0.3f, CRS_HUGE);
        /* UPTIME HEIGHTS and CORE DUMP CAVERNS on the balcony, north wall, at both ends */
        lobby_window(v3(25, 8.2f, -40.0f), 0, 5, 3.5f, TEX_PORTAL15, 0.95f, 0.5f, 0.35f, CRS_CITY);
        lobby_window(v3(-25, 8.2f, -40.0f), 0, 5, 3.5f, TEX_PORTAL16, 0.55f, 0.35f, 0.25f, CRS_CAVE);
        /* TTY WORLD 1-1 on the south wall, east of the entrance */
        lobby_window(v3(9, 3.3f, 10.0f), PI_F, 5, 4.0f, TEX_PORTAL17, 0.95f, 0.3f, 0.25f, CRS_SIDE);
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
    lv.spawn[13] = v3(-27, 0, -24); lv.spawn_yaw[13] = PI_F / 2;
    lv.spawn[14] = v3(27, 0, -24); lv.spawn_yaw[14] = -PI_F / 2;
    lv.spawn[15] = v3(25, 6, -36.5f); lv.spawn_yaw[15] = 0;
    lv.spawn[16] = v3(-25, 6, -36.5f); lv.spawn_yaw[16] = 0;
    lv.spawn[17] = v3(9, 0, 6.5f); lv.spawn_yaw[17] = PI_F;

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
        w = warp_box(v3(-29.4f, 0.0f, -24), 0.6f, 2.5f, 4.5f, LV_NANO, 0, 1, 13);
        if (w) w->i[1] = 18;
        w = warp_box(v3(29.4f, 0.0f, -24), 0.6f, 2.5f, 4.5f, LV_HUGE, 0, 1, 14);
        if (w) w->i[1] = 20;
        w = warp_box(v3(25, 6.0f, -39.4f), 2.5f, 0.6f, 4, LV_CITY, 0, 1, 15);
        if (w) w->i[1] = 22;
        w = warp_box(v3(-25, 6.0f, -39.4f), 2.5f, 0.6f, 4, LV_CAVE, 0, 1, 16);
        if (w) w->i[1] = 24;
        w = warp_box(v3(9, 0.0f, 9.4f), 2.5f, 0.6f, 4.5f, LV_SIDE, 0, 1, 17);
        if (w) w->i[1] = 11;
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
                        "NANO VILLAGE window (west, deep in the hall): 18\n"
                        "HUGEPAGE HOUSE window (east, deep in the hall): 20\n"
                        "UPTIME HEIGHTS window (balcony, north-east end): 22\n"
                        "CORE DUMP CAVERNS window (balcony, north-west end): 24\n"
                        "TTY WORLD 1-1 window (east of the entrance): 11\n"
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

/* ================================================================
 * NANO VILLAGE - a kingdom of tiny penguins; Tux is a giant here
 * ================================================================ */
#define NANO_BAY_Z 25.0f
static vec3 nano_track_pts[160];

static float nano_road_dist(float x, float z)
{
    static const float seg[][4] = {
        { 16, -18, 16, -6 }, { 16, -6, 4, 7 }, { 2, 10, -10, 18 }, { -10, 18, -22, 26 },
        { 2, 10, -12, 8 }, { -12, 8, -27, 14 }, { 2, 10, 22, 4 }, { 22, 4, 34, -6 }
    };
    float best = 1e9f;
    int i;
    for (i = 0; i < (int)(sizeof seg / sizeof seg[0]); i++) {
        float ax = seg[i][0], az = seg[i][1], bx = seg[i][2], bz = seg[i][3];
        float dx = bx - ax, dz = bz - az, t = clampf(((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz), 0, 1);
        float px = ax + dx * t - x, pz = az + dz * t - z;
        best = minf(best, sqrtf(px * px + pz * pz));
    }
    return best;
}

static float nano_track_dist(float x, float z)
{
    float best = 1e9f;
    int i;
    for (i = 0; i < 160; i++) best = minf(best, v3distxz(nano_track_pts[i], v3(x, 0, z)));
    return best;
}

static float nano_h(float x, float z)
{
    float h = 0.3f + 0.18f * sinf(x * 0.17f) * sinf(z * 0.13f) + 0.1f * sinf(x * 0.07f + z * 0.05f);
    float d, t;
    /* snowy mountains in the north-west */
    d = sqrtf((x + 44) * (x + 44) + (z + 40) * (z + 40));
    if (d < 26) { t = 1 - d / 26; h += 8.0f * t * t; }
    d = sqrtf((x + 24) * (x + 24) + (z + 54) * (z + 54));
    if (d < 14) { t = 1 - d / 14; h += 5.0f * t * t; }
    /* the windmill hill */
    d = sqrtf((x + 30) * (x + 30) + (z - 16) * (z - 16));
    h += 1.6f * sstep(0, 1, 1 - d / 7);
    /* the royal castle on a plateau, ringed by its moat */
    if (rect_inside(x, z, 8, -34, 24, -18) > 0) h = 0.6f;
    else if (rect_inside(x, z, 5.5f, -36.5f, 26.5f, -15.5f) > 0) h = -0.35f;
    /* the river from the mountains down to the bay */
    if (z > -42 && z < NANO_BAY_Z + 2) {
        float rd = fabsf(x + 12);
        if (rd < 1.4f) h = -0.3f;
        else if (rd < 2.6f) h = lerpf(-0.3f, h, sstep(1.4f, 2.6f, rd));
    }
    /* the shallow bay in the south, Blefuscu island and a rock */
    h = lerpf(h, -0.8f, sstep(NANO_BAY_Z, NANO_BAY_Z + 6, z));
    d = sqrtf((x - 28) * (x - 28) + (z - 56) * (z - 56));
    h = maxf(h, 0.45f - maxf(0, d - 5) * 0.5f);
    d = sqrtf((x - 5) * (x - 5) + (z - 40) * (z - 40));
    h = maxf(h, 0.25f - maxf(0, d - 0.8f) * 1.2f);
    h += boundary(x, z, 58, 1.2f);
    return h;
}

static void nano_style(float x, float y, float z, vec3 n, int *tex, float *col, int *flags)
{
    float v = 0.92f + 0.08f * sinf(x * 0.9f) * cosf(z * 0.7f);
    if (y < 0.08f) { *tex = TEX_SAND; col[0] = 0.95f; col[1] = 0.88f; col[2] = 0.7f; return; }
    if (n.y < 0.72f) { *tex = TEX_ROCK; col[0] = 0.7f; col[1] = 0.7f; col[2] = 0.75f; return; }
    if (y > 4.2f) { *tex = TEX_SNOW; return; }
    if (rect_inside(x, z, 8, -34, 24, -18) > 0) { *tex = TEX_STONE; col[0] = 0.85f; col[1] = 0.85f; col[2] = 0.8f; return; }
    if (nano_road_dist(x, z) < 0.55f) { *tex = TEX_DIRT; col[0] = 0.85f; col[1] = 0.72f; col[2] = 0.55f; return; }
    *tex = TEX_GRASS;
    if (x > 18 && x < 56 && z > -12 && z < 22) {   /* a patchwork of fields */
        int cx = (int)floorf(x / 4.5f), cz = (int)floorf(z / 3.5f);
        unsigned hsh = (unsigned)(cx * 73856093) ^ (unsigned)(cz * 19349663);
        switch ((hsh >> 3) % 4) {
        case 0: col[0] = 1.0f; col[1] = 0.9f; col[2] = 0.45f; *tex = TEX_DIRT; break;   /* wheat */
        case 1: col[0] = 0.62f; col[1] = 0.88f; col[2] = 0.42f; break;
        case 2: col[0] = 0.62f; col[1] = 0.45f; col[2] = 0.3f; *tex = TEX_DIRT; break;  /* ploughed */
        default: *tex = TEX_SNOW; break;                                                 /* snowy */
        }
        return;
    }
    col[0] = 0.7f * v; col[1] = 0.92f * v; col[2] = 0.55f * v;
}

/* trees too small to bump into: Tux wades through the forest */
static void nano_tree(float x, float z, float s)
{
    float y = nano_h(x, z) - 0.02f;
    float g = 0.8f + 0.2f * frand();
    lb_cone(x, y, z, 0.2f * s, 0.55f * s, 6, TEX_NONE, 0.15f * g, 0.45f * g, 0.2f * g, 0);
    if (y > 2.5f) lb_cone(x, y + 0.3f * s, z, 0.12f * s, 0.3f * s, 6, TEX_NONE, 0.95f, 0.97f, 1.0f, 0);
    else lb_cone(x, y + 0.25f * s, z, 0.14f * s, 0.38f * s, 6, TEX_NONE, 0.2f * g, 0.55f * g, 0.25f * g, 0);
}

/* a tiny house, low enough for the giant to step over */
static void nano_house(float x, float z, float w, float d, int blue)
{
    static const float wallc[4][3] = { { 1, 0.96f, 0.88f }, { 0.95f, 0.88f, 0.78f }, { 0.9f, 0.93f, 1 }, { 1, 0.9f, 0.85f } };
    static const float roofc[4][3] = { { 0.85f, 0.25f, 0.2f }, { 0.55f, 0.32f, 0.2f }, { 0.3f, 0.5f, 0.3f }, { 0.75f, 0.4f, 0.2f } };
    int k = (int)(frand() * 4) & 3, rk = blue ? 0 : (int)(frand() * 4) & 3;
    float y0 = nano_h(x, z) - 0.12f, yw = y0 + 0.12f + 0.22f + frand() * 0.06f, yp = yw + 0.12f + frand() * 0.04f;
    float x0 = x - w / 2, x1 = x + w / 2, z0 = z - d / 2, z1 = z + d / 2, ov = 0.05f;
    float rr = blue ? 0.25f : roofc[rk][0], rg = blue ? 0.4f : roofc[rk][1], rb = blue ? 0.85f : roofc[rk][2];
    int front = frand() < 0.5f;
    float fz = front ? z0 - 0.004f : z1 + 0.004f;
    lb_box(x0, y0, z0, x1, yw, z1, TEX_NONE, TEX_NONE, wallc[k][0], wallc[k][1], wallc[k][2], 0);
    lb_ramp(x0 - ov, z0 - ov, x1 + ov, z, yw, yp, 1, 0.03f, TEX_ROOF, TEX_ROOF, rr, rg, rb, 0);
    lb_ramp(x0 - ov, z, x1 + ov, z1 + ov, yp, yw, 1, 0.03f, TEX_ROOF, TEX_ROOF, rr, rg, rb, 0);
    /* gable ends */
    lb_tri(TEX_NONE, v3(x0, yw, z0), v3(x0, yw, z1), v3(x0, yp, z), wallc[k][0] * 0.85f, wallc[k][1] * 0.85f, wallc[k][2] * 0.85f, 0, 0);
    lb_tri(TEX_NONE, v3(x1, yw, z1), v3(x1, yw, z0), v3(x1, yp, z), wallc[k][0] * 0.85f, wallc[k][1] * 0.85f, wallc[k][2] * 0.85f, 0, 0);
    /* door and windows on the front */
    {
        float dy = y0 + 0.12f, s = front ? 1.0f : -1.0f;
        vec3 a = v3(x - 0.04f * s, dy, fz), b = v3(x + 0.04f * s, dy, fz);
        lb_quad(TEX_NONE, a, b, v3(b.x, dy + 0.13f, fz), v3(a.x, dy + 0.13f, fz), 0.35f, 0.22f, 0.12f, 0, 0);
        if (w > 0.55f) {
            float wx;
            for (wx = -1; wx <= 1; wx += 2) {
                float cx = x + wx * w * 0.3f;
                vec3 c0 = v3(cx - 0.035f * s, dy + 0.08f, fz), c1 = v3(cx + 0.035f * s, dy + 0.08f, fz);
                lb_quad(TEX_NONE, c0, c1, v3(c1.x, dy + 0.15f, fz), v3(c0.x, dy + 0.15f, fz), 1.0f, 0.85f, 0.4f, 0, 0);
            }
        }
    }
    if (frand() < 0.4f) lb_box(x1 - 0.12f, yw, z - 0.05f, x1 - 0.05f, yp + 0.06f, z + 0.03f, TEX_BRICK, TEX_BRICK, 0.7f, 0.4f, 0.35f, 0);
}

static int nano_house_ok(float x, float z, const vec3 *placed, int n, float near_track)
{
    int i;
    float h = nano_h(x, z);
    if (h < 0.15f || h > 3.5f) return 0;
    if (fabsf(nano_h(x + 0.5f, z) - h) > 0.25f || fabsf(nano_h(x, z + 0.5f) - h) > 0.25f) return 0;
    if (nano_road_dist(x, z) < 1.0f) return 0;
    if (fabsf(x + 12) < 3.2f && z < NANO_BAY_Z + 2) return 0;
    if (nano_track_dist(x, z) < near_track) return 0;
    if (rect_inside(x, z, 4, -38, 28, -13) > 0) return 0;
    if (sqrtf((x - 2) * (x - 2) + (z - 10) * (z - 10)) < 3.8f) return 0;
    if (sqrtf((x + 30) * (x + 30) + (z - 16) * (z - 16)) < 2.5f) return 0;
    if (sqrtf((x - 8) * (x - 8) + (z - 15) * (z - 15)) < 1.8f) return 0;
    if (x > 33 && z < -10) return 0;
    for (i = 0; i < n; i++) if (v3distxz(placed[i], v3(x, 0, z)) < 1.25f) return 0;
    return 1;
}

static Obj *nano_folk(float x, float z, float wander, const char *text)
{
    Obj *o = obj_spawn(OT_NANOFOLK, v3(x, nano_h(x, z), z), frand() * TAU_F, (int)(frand() * 1000));
    if (o) { o->f[0] = wander; o->text = text; o->f[1] = o->yaw; }
    return o;
}

static void build_nano(void)
{
    static vec3 placed[140];
    int np = 0, i, k;
    Obj *o;
    lv.name = "NANO VILLAGE";
    lv.music = MUS_NANO;
    lv.course = CRS_NANO;
    lv.scale_world = 1;
    set_sky(0.32f, 0.58f, 0.95f, 0.78f, 0.88f, 1.0f, 45, 140);
    lv.ambient = 0.58f; lv.diffuse = 0.66f;
    lv.death_y = -8;
    lv.clouds = 1;
    g_coin_star_idx = 4;
    for (i = 0; i < 160; i++) nano_track_pts[i] = nano_track_point(i / 160.0f);
    lb_begin();
    lb_uv_mul = 2.5f;   /* everything is finely detailed, as seen by a giant */
    lb_terrain(-64, -64, 64, 64, 128, 128, nano_h, nano_style);
    lb_uv_mul = 1.0f;
    lb_water(-64, NANO_BAY_Z, 64, 64, -0.05f, -1.2f);
    lb_water(-13.4f, -42, -10.6f, NANO_BAY_Z + 0.5f, 0.08f, -0.6f);
    lb_water(5.5f, -36.5f, 26.5f, -34, 0.1f, -0.6f);
    lb_water(5.5f, -18, 26.5f, -15.5f, 0.1f, -0.6f);
    lb_water(5.5f, -34, 8, -18, 0.1f, -0.6f);
    lb_water(24, -34, 26.5f, -18, 0.1f, -0.6f);

    /* the royal castle: walls the giant can jump over, towers and the keep */
    {
        float y = 0.6f, top = y + 1.1f;
        lb_box(9, y, -33, 23, top, -32.4f, TEX_STONE, TEX_STONE, 0.92f, 0.9f, 0.85f, 0);
        lb_box(9, y, -19.6f, 15, top, -19, TEX_STONE, TEX_STONE, 0.92f, 0.9f, 0.85f, 0);
        lb_box(17, y, -19.6f, 23, top, -19, TEX_STONE, TEX_STONE, 0.92f, 0.9f, 0.85f, 0);
        lb_box(9, y, -32.4f, 9.6f, top, -19.6f, TEX_STONE, TEX_STONE, 0.92f, 0.9f, 0.85f, 0);
        lb_box(22.4f, y, -32.4f, 23, top, -19.6f, TEX_STONE, TEX_STONE, 0.92f, 0.9f, 0.85f, 0);
        for (k = 0; k < 20; k++) {   /* battlements */
            float t = 9.3f + k * 0.7f;
            lb_box(t, top, -33, t + 0.3f, top + 0.22f, -32.75f, TEX_STONE, TEX_STONE, 0.9f, 0.88f, 0.82f, 0);
            if (t < 14.6f || t > 17.2f) lb_box(t, top, -19.25f, t + 0.3f, top + 0.22f, -19, TEX_STONE, TEX_STONE, 0.9f, 0.88f, 0.82f, 0);
            lb_box(9, top, -32.6f + k * 0.67f, 9.25f, top + 0.22f, -32.3f + k * 0.67f, TEX_STONE, TEX_STONE, 0.9f, 0.88f, 0.82f, 0);
            lb_box(22.75f, top, -32.6f + k * 0.67f, 23, top + 0.22f, -32.3f + k * 0.67f, TEX_STONE, TEX_STONE, 0.9f, 0.88f, 0.82f, 0);
        }
        for (k = 0; k < 4; k++) {   /* corner towers */
            float tx = (k & 1) ? 23 : 9, tz = (k & 2) ? -19 : -33;
            lb_cyl(tx, y, tz, 1.0f, 2.0f, 14, TEX_STONE, TEX_STONE, 0.95f, 0.93f, 0.88f, 0);
            lb_cone(tx, y + 2.0f, tz, 1.2f, 1.1f, 14, TEX_ROOF, 0.3f, 0.4f, 0.85f, 1);
        }
        /* gatehouse and drawbridge */
        lb_cyl(14.5f, y, -19.3f, 0.5f, 1.6f, 10, TEX_STONE, TEX_STONE, 0.95f, 0.93f, 0.88f, 0);
        lb_cyl(17.5f, y, -19.3f, 0.5f, 1.6f, 10, TEX_STONE, TEX_STONE, 0.95f, 0.93f, 0.88f, 0);
        lb_box(15, 0.45f, -19, 17, 0.62f, -15.2f, TEX_PLANK, TEX_WOOD, 0.9f, 0.75f, 0.55f, 0);
        /* the keep and its tower */
        lb_box(13, y, -29, 19, y + 1.8f, -23, TEX_STONE, TEX_STONE, 0.98f, 0.96f, 0.9f, 0);
        lb_cyl(16, y + 1.8f, -26, 1.1f, 2.0f, 16, TEX_STONE, TEX_STONE, 0.98f, 0.96f, 0.9f, 0);
        lb_box(15.95f, y + 3.8f, -26.05f, 16.05f, y + 4.8f, -25.95f, TEX_NONE, TEX_NONE, 0.4f, 0.3f, 0.2f, 0);
        lb_quad(TEX_NONE, v3(16.05f, y + 4.4f, -26), v3(16.6f, y + 4.5f, -26), v3(16.6f, y + 4.8f, -26), v3(16.05f, y + 4.8f, -26), 0.9f, 0.2f, 0.2f, 0, 0);
        lb_quad(TEX_NONE, v3(16.6f, y + 4.5f, -26), v3(16.05f, y + 4.4f, -26), v3(16.05f, y + 4.8f, -26), v3(16.6f, y + 4.8f, -26), 0.9f, 0.2f, 0.2f, 0, 0);
    }
    /* village plaza: paving, fountain and market stalls */
    {
        float y = nano_h(2, 10);
        lb_cyl(2, y - 0.1f, 10, 3.0f, 0.14f, 24, TEX_STONE, TEX_STONE, 0.9f, 0.88f, 0.85f, 0);
        lb_cyl(2, y + 0.04f, 10, 0.7f, 0.16f, 16, TEX_STONE, TEX_STONE, 0.85f, 0.85f, 0.9f, 0);
        lb_cyl(2, y + 0.05f, 10, 0.6f, 0.16f, 16, TEX_WATER, TEX_WATER, 0.5f, 0.75f, 1, 0);
        lb_cyl(2, y + 0.2f, 10, 0.1f, 0.35f, 8, TEX_STONE, TEX_STONE, 0.9f, 0.9f, 0.95f, 0);
        for (k = 0; k < 6; k++) {
            float a = TAU_F * k / 6 + 0.3f, sx = 2 + sinf(a) * 2.2f, sz = 10 + cosf(a) * 2.2f;
            static const float cc[3][3] = { { 0.9f, 0.3f, 0.3f }, { 0.3f, 0.6f, 0.9f }, { 0.95f, 0.8f, 0.3f } };
            lb_box(sx - 0.2f, y + 0.04f, sz - 0.15f, sx + 0.2f, y + 0.16f, sz + 0.15f, TEX_WOOD, TEX_WOOD, 0.8f, 0.6f, 0.4f, 0);
            lb_box(sx - 0.24f, y + 0.3f, sz - 0.19f, sx + 0.24f, y + 0.33f, sz + 0.19f, TEX_NONE, TEX_NONE, cc[k % 3][0], cc[k % 3][1], cc[k % 3][2], 0);
        }
        /* the church with its steeple */
        y = nano_h(8, 15);
        lb_box(7.4f, y - 0.1f, 14.2f, 8.6f, y + 0.6f, 16.2f, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.9f, 0);
        lb_ramp(7.3f, 14.1f, 8.0f, 16.3f, y + 0.6f, y + 0.95f, 0, 0.05f, TEX_ROOF, TEX_ROOF, 0.45f, 0.3f, 0.25f, 0);
        lb_ramp(8.0f, 14.1f, 8.7f, 16.3f, y + 0.95f, y + 0.6f, 0, 0.05f, TEX_ROOF, TEX_ROOF, 0.45f, 0.3f, 0.25f, 0);
        lb_cyl(8, y + 0.5f, 14.1f, 0.35f, 1.6f, 8, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.9f, 0);
        lb_cone(8, y + 2.1f, 14.1f, 0.42f, 1.1f, 8, TEX_ROOF, 0.45f, 0.3f, 0.25f, 1);
        obj_spawn(OT_REDCOIN, v3(8, y + 3.5f, 14.1f), 0, 0);
    }
    /* the windmill on the hill */
    {
        float y = nano_h(-30, 16);
        lb_cyl(-30, y - 0.1f, 16, 0.6f, 2.1f, 10, TEX_NONE, TEX_NONE, 0.96f, 0.94f, 0.88f, 0);
        lb_cone(-30, y + 2.0f, 16, 0.75f, 0.8f, 10, TEX_ROOF, 0.6f, 0.35f, 0.25f, 1);
        lb_box(-30.1f, y + 1.9f, 16.5f, -29.9f, y + 2.1f, 16.65f, TEX_NONE, TEX_NONE, 0.4f, 0.3f, 0.2f, 0);
        g_nano_windmill = v3(-30, y + 2.0f, 16.7f);
        obj_spawn(OT_REDCOIN, v3(-30, y + 3.3f, 16), 0, 0);
        coin_ring(v3(-30, y - 0.4f, 16), 3.5f, 8, 0);
    }
    /* the railway: rails, sleepers and bridges over the river */
    for (i = 0; i < 160; i++) {
        vec3 a = nano_track_pts[i], b = nano_track_pts[(i + 1) % 160], d = v3sub(b, a), s;
        float ya, yb, len = v3len(d);
        int river_a = fabsf(a.x + 12) < 2.8f, river_b = fabsf(b.x + 12) < 2.8f;
        d = v3scale(d, 1.0f / len);
        s = v3(d.z * 0.09f, 0, -d.x * 0.09f);
        ya = river_a ? 0.42f : nano_h(a.x, a.z) + 0.03f;
        yb = river_b ? 0.42f : nano_h(b.x, b.z) + 0.03f;
        for (k = -1; k <= 1; k += 2) {
            vec3 o2 = v3scale(s, (float)k), w = v3(d.z * 0.015f, 0, -d.x * 0.015f);
            vec3 p0 = v3add(a, o2), p1 = v3add(b, o2);
            p0.y = ya + 0.02f; p1.y = yb + 0.02f;
            lb_quad(TEX_NONE, v3sub(p0, w), v3add(p0, w), v3add(p1, w), v3sub(p1, w), 0.45f, 0.45f, 0.5f, 0, 0);
        }
        {
            vec3 m = v3lerp(a, b, 0.5f), w = v3scale(d, 0.035f);
            float ym = (ya + yb) * 0.5f + 0.01f;
            vec3 q0 = v3sub(v3sub(m, w), v3scale(s, 1.6f)), q1 = v3sub(v3add(m, w), v3scale(s, 1.6f));
            vec3 q2 = v3add(v3add(m, w), v3scale(s, 1.6f)), q3 = v3add(v3sub(m, w), v3scale(s, 1.6f));
            q0.y = q1.y = q2.y = q3.y = ym;
            lb_quad(TEX_NONE, q0, q3, q2, q1, 0.45f, 0.3f, 0.2f, 0, 0);
        }
        if (river_a && river_b) lb_box(minf(a.x, b.x) - 0.15f, 0.3f, minf(a.z, b.z) - 0.25f, maxf(a.x, b.x) + 0.15f, 0.42f,
                                       maxf(a.z, b.z) + 0.25f, TEX_PLANK, TEX_WOOD, 0.8f, 0.6f, 0.4f, 0);
    }
    /* road bridges over the river */
    lb_box(-13.6f, 0.3f, 16.6f, -10.4f, 0.42f, 18.4f, TEX_STONE, TEX_STONE, 0.85f, 0.85f, 0.8f, 0);
    lb_box(-13.6f, 0.3f, 7.3f, -10.4f, 0.42f, 9.1f, TEX_STONE, TEX_STONE, 0.85f, 0.85f, 0.8f, 0);
    /* Lilliput harbour: breakwaters and piers */
    g_nano_harbor = v3(-25, -0.05f, 31);
    lb_box(-33, -0.9f, 27, -32.4f, 0.25f, 34, TEX_STONE, TEX_STONE, 0.8f, 0.8f, 0.78f, 0);
    lb_box(-17.6f, -0.9f, 27, -17, 0.25f, 34, TEX_STONE, TEX_STONE, 0.8f, 0.8f, 0.78f, 0);
    for (k = 0; k < 3; k++) lb_box(-29.2f + k * 4, -0.9f, 26, -28.8f + k * 4, 0.12f, 30.6f, TEX_PLANK, TEX_WOOD, 0.85f, 0.65f, 0.45f, 0);
    /* Blefuscu island: a lighthouse and blue-roofed houses */
    {
        float y = nano_h(33, 58);
        lb_cyl(33, y - 0.1f, 58, 0.45f, 1.2f, 10, TEX_NONE, TEX_NONE, 0.95f, 0.95f, 0.95f, 0);
        lb_cyl(33, y + 1.1f, 58, 0.42f, 1.0f, 10, TEX_NONE, TEX_NONE, 0.9f, 0.2f, 0.2f, 0);
        lb_cyl(33, y + 2.1f, 58, 0.5f, 0.15f, 10, TEX_METAL, TEX_METAL, 0.6f, 0.6f, 0.65f, 0);
        obj_spawn(OT_REDCOIN, v3(33, y + 2.9f, 58), 0, 0);
    }
    /* the invaders' camp in the east */
    {
        static const float tents[6][2] = { { 44, -32 }, { 47, -33 }, { 50, -31 }, { 53, -28 }, { 42, -29 }, { 54, -24 } };
        for (k = 0; k < 6; k++) {
            float tx = tents[k][0], tz = tents[k][1];
            lb_cone(tx, nano_h(tx, tz) - 0.05f, tz, 0.55f, 0.6f, 8, TEX_NONE, 0.3f, 0.4f, 0.85f, 1);
        }
        lb_box(48, nano_h(50, -25) - 0.1f, -26, 52, nano_h(50, -25) + 0.65f, -24, TEX_WOOD, TEX_WOOD, 0.6f, 0.45f, 0.3f, 0);
        obj_spawn(OT_REDCOIN, v3(50, nano_h(50, -25) + 1.2f, -25), 0, 0);
    }
    /* houses: the village, the harbour hamlet, farms and Blefuscu */
    {
        static const float area[4][5] = {   /* cx cz rmin rmax tries */
            { 2, 10, 3.8f, 13, 90 }, { -22, 20, 1, 6, 30 }, { 30, 8, 2, 16, 18 }, { 28, 56, 0.5f, 4, 16 }
        };
        int a;
        for (a = 0; a < 4; a++)
            for (k = 0; k < (int)area[a][4] && np < 140; k++) {
                float ang = frand() * TAU_F, r = lerpf(area[a][2], area[a][3], sqrtf(frand()));
                float x = area[a][0] + sinf(ang) * r, z = area[a][1] + cosf(ang) * r;
                if (!nano_house_ok(x, z, placed, np, 1.4f)) continue;
                placed[np++] = v3(x, 0, z);
                nano_house(x, z, 0.5f + frand() * 0.4f, 0.4f + frand() * 0.3f, a == 3);
            }
    }
    /* forests: on the mountain slopes, around the fires and dotted about */
    for (k = 0; k < 900; k++) {
        float x = frands() * 62, z = frands() * 62, h = nano_h(x, z);
        float mtn = sqrtf((x + 40) * (x + 40) + (z + 40) * (z + 40));
        if (h < 0.15f || h > 7.0f || nano_road_dist(x, z) < 0.8f || nano_track_dist(x, z) < 0.7f) continue;
        if (rect_inside(x, z, 4, -38, 28, -13) > 0 || (x > 17 && x < 57 && z > -13 && z < 23)) continue;
        if (fabsf(x + 12) < 2.2f && z < NANO_BAY_Z + 2) continue;
        if (mtn > 30 && frand() > 0.12f) continue;
        for (i = 0; i < np; i++) if (v3distxz(placed[i], v3(x, 0, z)) < 0.8f) break;
        if (i < np) continue;
        if (sqrtf((x - 2) * (x - 2) + (z - 10) * (z - 10)) < 4 || (x > 34 && z < -12)) continue;
        nano_tree(x, z, 0.8f + frand() * 0.5f);
    }
    finish_geometry();

    lv.spawn[0] = v3(-6, nano_h(-6, 3), 3); lv.spawn_yaw[0] = 0.9f;

    /* star 0: tow the Blefuscu fleet into the harbour */
    {
        static const float ships[5][2] = { { 14, 46 }, { 19, 49 }, { 25, 45 }, { 31, 48 }, { 37, 44 } };
        for (k = 0; k < 5; k++) {
            o = obj_spawn(OT_NANOSHIP, v3(ships[k][0], -0.07f, ships[k][1]), PI_F + frands() * 0.4f, k);
            if (o) o->home = o->pos;
        }
        nano_folk(-24, 26.5f, 2, "HARBOUR MASTER: Tie up the enemy ships by\nwading right up to them, then bring them\n"
                                 "here, between the breakwaters of our harbour!");
    }
    /* star 1: forest fires on the mountain and the rain clouds */
    {
        static const float fires[4][2] = { { -38, -22 }, { -30, -34 }, { -50, -22 }, { -20, -45 } };
        for (k = 0; k < 4; k++) {
            float fx = fires[k][0], fz = fires[k][1];
            obj_spawn(OT_NANOFIRE, v3(fx, nano_h(fx, fz), fz), 0, k);
            for (i = 0; i < 9; i++) {
                float a = TAU_F * i / 9, r = 0.8f + frand() * 1.4f;
                nano_tree(fx + sinf(a) * r, fz + cosf(a) * r, 1.1f);
            }
        }
        o = obj_spawn(OT_NANOCLOUD, v3(-33, nano_h(-33, -14) + 1.0f, -14), 0, 1);
        if (o) o->home = o->pos;
        o = obj_spawn(OT_NANOCLOUD, v3(-22, nano_h(-22, -32) + 1.0f, -32), 0, 1);
        if (o) o->home = o->pos;
        nano_folk(-28, -12, 1.5f, "TINY FORESTER: Help! The mountain forest is on\nfire and our buckets are much too small!\n"
                                  "Grab one of those grey rain clouds with B\nand carry it over each fire until it's out.");
    }
    /* star 2: the invading army and its catapults */
    {
        static const float cata[3][2] = { { 40, -18 }, { 46, -26 }, { 53, -19 } };
        for (k = 0; k < 3; k++) obj_spawn(OT_NANOCATAPULT, v3(cata[k][0], nano_h(cata[k][0], cata[k][1]), cata[k][1]), -PI_F / 2, k);
        for (k = 0; k < 15; k++) {
            float sx = 36.5f + (k / 5) * 1.0f, sz = -25 + (k % 5) * 1.6f;
            obj_spawn(OT_NANOSOLDIER, v3(sx, nano_h(sx, sz), sz), -PI_F / 2, k);
        }
        nano_folk(18, -15, 1.0f, "TINY GUARD: Blefuscu's army has landed in the\neast with catapults! Their arrows sting even\n"
                                 "a giant. Smash all the catapults - a ground\npound (Z in mid-air) scatters the soldiers!");
    }
    /* star 3: red fish (6 placed with the buildings above) */
    obj_spawn(OT_REDCOIN, v3(16, 5.0f, -26.6f), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-44, nano_h(-44, -40) + 0.8f, -40), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-24, nano_h(-24, -54) + 0.8f, -54), 0, 0);
    obj_spawn(OT_REDCOIN, v3(5, 0.85f, 40), 0, 0);
    obj_spawn(OT_STARMARKER, v3(2, nano_h(2, 10) + 3.0f, 10), 0, 3);
    nano_folk(4, 6, 1.0f, "TINY PENGUIN: Wow, you're SO big! Red fish are\nhidden up high all over our kingdom: on\n"
                          "towers, the mill, mountain tops, an island...");
    nano_folk(0, 12.5f, 1.5f, "TINY MAYOR: Welcome, Great Giant! Please mind\nour roofs. The fleet of Blefuscu waits across\n"
                              "the bay, a fire burns on the mountain and an\narmy marches in the east. Will you help us?");
    /* villagers */
    for (k = 0; k < 22; k++) {
        float a = frand() * TAU_F, r = 3.4f + frand() * 7;
        float x = 2 + sinf(a) * r, z = 10 + cosf(a) * r;
        if (nano_h(x, z) < 0.15f || fabsf(x + 12) < 2.6f) continue;
        nano_folk(x, z, 3.5f, NULL);
    }
    for (k = 0; k < 6; k++) nano_folk(-25 + frands() * 4, 24 + frand() * 1.5f, 2, NULL);
    for (k = 0; k < 6; k++) nano_folk(12 + frand() * 8, -31 + frand() * 10, 2.5f, NULL);
    for (k = 0; k < 5; k++) nano_folk(26 + frands() * 2, 54 + frands() * 2, 1.5f, NULL);
    /* the little train */
    for (k = 0; k < 4; k++) obj_spawn(OT_NANOTRAIN, nano_track_pts[0], 0, k);
    /* clouds drifting at knee height */
    for (k = 0; k < 12; k++) {
        float x = frands() * 50, z = frands() * 40 - 5;
        o = obj_spawn(OT_NANOCLOUD, v3(x, maxf(nano_h(x, z), 0) + 1.4f + frand() * 0.9f, z), 0, 0);
        if (o) o->home = o->pos;
    }
    /* fish (100 for the coin star) */
    coin_line(v3(16, 0.8f, -14), v3(16, 0.8f, -7), 5, 1);
    coin_line(v3(14, 0.8f, -4), v3(6, 0.8f, 5), 5, 1);
    coin_ring(v3(2, 0.6f, 10), 4.5f, 10, 1);
    coin_line(v3(-8, 0.6f, 16.5f), v3(-20, 0.6f, 24.5f), 6, 1);
    coin_line(v3(-5, 0.6f, 9), v3(-25, 0.6f, 13.5f), 7, 1);
    coin_line(v3(8, 0.3f, 33), v3(30, 0.3f, 40), 9, 0);
    coin_ring(v3(28, 0.9f, 56), 3.0f, 8, 1);
    coin_line(v3(-26, 1.5f, -18), v3(-40, 6.5f, -36), 9, 1);
    coin_ring(v3(16, 1.0f, -26), 4.5f, 10, 1);
    coin_line(v3(24, 0.8f, 2), v3(48, 0.8f, 2), 9, 1);
    coin_line(v3(38, 0.8f, -12), v3(50, 0.8f, -12), 6, 1);
    for (k = 0; k < 14; k++) {
        vec3 p = nano_track_pts[k * 11 + 5];
        p.y = nano_h(p.x, p.z) + 0.6f;
        if (fabsf(p.x + 12) < 2.8f) p.y = 1.0f;
        obj_spawn(OT_COIN, p, 0, 0);
    }
    coin_line(v3(-32, 0.3f, 29), v3(-32, 0.3f, 37), 4, 0);
}

/* ================================================================
 * HUGEPAGE HOUSE - a giant's living room; Tux is tiny here
 * ================================================================ */
static void huge_book(float x0, float y0, float z0, float x1, float y1, float z1)
{
    static const float bc[6][3] = { { 0.75f, 0.2f, 0.2f }, { 0.2f, 0.35f, 0.7f }, { 0.2f, 0.55f, 0.3f },
                                    { 0.85f, 0.7f, 0.3f }, { 0.45f, 0.25f, 0.5f }, { 0.9f, 0.9f, 0.85f } };
    int k = (int)(frand() * 6) % 6;
    lb_box(x0, y0, z0, x1, y1, z1, TEX_NONE, TEX_NONE, bc[k][0], bc[k][1], bc[k][2], 0);
}

/* a book lying flat: cover, page block and spine */
static void huge_flat_book(float x0, float z0, float x1, float z1, float y0, float th)
{
    huge_book(x0, y0, z0, x1, y0 + th, z1);
    lb_box(x0 + 0.3f, y0 + 0.15f, z0 - 0.02f, x1 - 0.3f, y0 + th - 0.15f, z0 + 0.4f, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.85f, 0);
}

/* a row of upright books on a shelf (x from the back wall to the front lip); a few short ones are steps */
static void huge_shelf_books(float y, float z0, float z1)
{
    float z = z0;
    while (z < z1 - 1.0f) {
        float w = 1.0f + frand() * 1.0f, h = 5.0f + frand() * 1.6f;
        if (frand() < 0.15f) { z += 1.5f; continue; }
        if (z + w > z1) break;
        huge_book(31.2f, y, z, 35.8f, y + h, z + w);
        z += w + 0.05f;
    }
}

static void build_huge(void)
{
    Obj *o;
    int k;
    lv.name = "HUGEPAGE HOUSE";
    lv.music = MUS_HUGE;
    lv.course = CRS_HUGE;
    lv.scale_world = 2;
    set_sky(0.18f, 0.14f, 0.12f, 0.36f, 0.3f, 0.26f, 80, 220);
    lv.ambient = 0.62f; lv.diffuse = 0.5f;
    lv.death_y = -20;
    g_coin_star_idx = 5;
    lb_begin();
    lb_uv_mul = 0.15f;   /* floorboards and fabrics the size of a giant's */
    /* floor, walls, baseboards */
    lb_box(-37, -2, -37, 37, 0, 37, TEX_PLANK, TEX_WOOD, 0.95f, 0.78f, 0.58f, 0);
    lb_box(-37, 0, -37, 37, 44, -36, TEX_NONE, TEX_NONE, 0.88f, 0.82f, 0.68f, 0);
    lb_box(-37, 0, -36, -36, 44, 36, TEX_NONE, TEX_NONE, 0.88f, 0.82f, 0.68f, 0);
    lb_box(36, 0, -36, 37, 44, 36, TEX_NONE, TEX_NONE, 0.88f, 0.82f, 0.68f, 0);
    lb_box(-37, 0, 36, 28, 44, 37, TEX_NONE, TEX_NONE, 0.88f, 0.82f, 0.68f, 0);
    lb_box(32, 0, 36, 37, 44, 37, TEX_NONE, TEX_NONE, 0.88f, 0.82f, 0.68f, 0);
    lb_box(28, 3.6f, 36, 32, 44, 37, TEX_NONE, TEX_NONE, 0.88f, 0.82f, 0.68f, 0);
    for (k = 0; k < 18; k++) {   /* wallpaper stripes */
        float a = -36 + k * 4 + 1.2f;
        lb_quad(TEX_NONE, v3(a, 2.5f, -35.98f), v3(a + 1.2f, 2.5f, -35.98f), v3(a + 1.2f, 44, -35.98f), v3(a, 44, -35.98f), 0.78f, 0.7f, 0.55f, 0, 0);
        lb_quad(TEX_NONE, v3(a + 1.2f, 2.5f, 35.98f), v3(a, 2.5f, 35.98f), v3(a, 44, 35.98f), v3(a + 1.2f, 44, 35.98f), 0.78f, 0.7f, 0.55f, 0, 0);
        lb_quad(TEX_NONE, v3(-35.98f, 2.5f, a + 1.2f), v3(-35.98f, 2.5f, a), v3(-35.98f, 44, a), v3(-35.98f, 44, a + 1.2f), 0.78f, 0.7f, 0.55f, 0, 0);
        lb_quad(TEX_NONE, v3(35.98f, 2.5f, a), v3(35.98f, 2.5f, a + 1.2f), v3(35.98f, 44, a + 1.2f), v3(35.98f, 44, a), 0.78f, 0.7f, 0.55f, 0, 0);
    }
    lb_box(-36, 0, -36, 36, 2.5f, -35.4f, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.9f, 0);
    lb_box(-36, 0, -35.4f, -35.4f, 2.5f, 35.4f, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.9f, 0);
    lb_box(35.4f, 0, -35.4f, 36, 2.5f, 35.4f, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.9f, 0);
    lb_box(-36, 0, 35.4f, 28, 2.5f, 36, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.9f, 0);
    lb_box(32, 0, 35.4f, 36, 2.5f, 36, TEX_NONE, TEX_NONE, 0.97f, 0.95f, 0.9f, 0);
    /* the mouse hole in the south wall, with a cosy little room behind it */
    lb_box(27, -2, 36, 33, 0, 42, TEX_PLANK, TEX_WOOD, 0.6f, 0.45f, 0.3f, 0);
    lb_box(27, 0, 41, 33, 4, 42, TEX_WOOD, TEX_WOOD, 0.35f, 0.25f, 0.18f, 0);
    lb_box(27, 0, 37, 28, 4, 41, TEX_WOOD, TEX_WOOD, 0.35f, 0.25f, 0.18f, 0);
    lb_box(32, 0, 37, 33, 4, 41, TEX_WOOD, TEX_WOOD, 0.35f, 0.25f, 0.18f, 0);
    lb_box(27, 3.6f, 37, 33, 4.4f, 41, TEX_WOOD, TEX_WOOD, 0.35f, 0.25f, 0.18f, 0);
    /* the window over the counter */
    lb_quad(TEX_NONE, v3(10, 15, -35.9f), v3(24, 15, -35.9f), v3(24, 26, -35.9f), v3(10, 26, -35.9f), 0.85f, 0.95f, 1.0f, 0, 0);
    lb_box(9.4f, 26, -36, 24.6f, 26.8f, -35.4f, TEX_WOOD, TEX_WOOD, 0.97f, 0.97f, 0.95f, 0);
    lb_box(9.4f, 15, -36, 10, 26, -35.4f, TEX_WOOD, TEX_WOOD, 0.97f, 0.97f, 0.95f, 0);
    lb_box(24, 15, -36, 24.6f, 26, -35.4f, TEX_WOOD, TEX_WOOD, 0.97f, 0.97f, 0.95f, 0);
    lb_box(16.7f, 15, -36, 17.3f, 26, -35.6f, TEX_WOOD, TEX_WOOD, 0.97f, 0.97f, 0.95f, 0);
    lb_box(9, 14.3f, -36, 25, 15, -33.4f, TEX_WOOD, TEX_WOOD, 0.97f, 0.97f, 0.95f, 0);   /* windowsill */
    /* the kitchen counter, its drawers and the high shelf */
    lb_box(-36, 0, -36, 8, 10.4f, -26, TEX_WOOD, TEX_WOOD, 0.75f, 0.58f, 0.4f, 0);
    lb_box(-36, 10.4f, -36, 8.5f, 11, -25.5f, TEX_TILE, TEX_TILE, 0.95f, 0.95f, 0.97f, 0);
    lb_box(-34, 10.98f, -34.5f, -26, 11.02f, -28, TEX_METAL, TEX_METAL, 0.7f, 0.72f, 0.75f, 0);   /* sink */
    lb_cyl(-31, 11, -31.5f, 2.2f, 6.0f, 16, TEX_NONE, TEX_NONE, 0.75f, 0.9f, 0.95f, 0);          /* a jar */
    lb_cyl(-31, 17, -31.5f, 2.35f, 0.8f, 16, TEX_METAL, TEX_METAL, 0.85f, 0.3f, 0.25f, 0);
    lb_box(-20, 23.2f, -36, 4, 24, -32, TEX_WOOD, TEX_WOOD, 0.8f, 0.6f, 0.42f, 0);
    lb_box(-19, 20, -36, -18.4f, 23.2f, -35, TEX_METAL, TEX_METAL, 0.6f, 0.6f, 0.62f, 0);   /* brackets */
    lb_box(2.4f, 20, -36, 3, 23.2f, -35, TEX_METAL, TEX_METAL, 0.6f, 0.6f, 0.62f, 0);
    lb_cyl(0.6f, 24, -34.4f, 1.2f, 3.0f, 12, TEX_NONE, TEX_NONE, 0.9f, 0.75f, 0.3f, 0);           /* spice tins */
    lb_cyl(2.8f, 24, -34.6f, 0.9f, 4.0f, 12, TEX_NONE, TEX_NONE, 0.3f, 0.55f, 0.3f, 0);
    for (k = 0; k < 4; k++)   /* drawer fronts drawn on the counter (the drawers themselves move) */
        lb_quad(TEX_NONE, v3(-32.5f + k * 6, 1.0f + k * 2.2f, -25.98f), v3(-27.5f + k * 6, 1.0f + k * 2.2f, -25.98f),
                v3(-27.5f + k * 6, 2.2f + k * 2.2f, -25.98f), v3(-32.5f + k * 6, 2.2f + k * 2.2f, -25.98f), 0.25f, 0.18f, 0.12f, 0, 0);
    /* the sofa and the wall clock above it */
    lb_box(-36, 0, -12, -24, 6, 14, TEX_CARPET, TEX_CARPET, 0.4f, 0.6f, 0.45f, 0);
    lb_box(-36, 6, -12, -32, 13, 14, TEX_CARPET, TEX_CARPET, 0.35f, 0.55f, 0.4f, 0);
    lb_box(-32, 6, -12, -24, 9, -9, TEX_CARPET, TEX_CARPET, 0.35f, 0.55f, 0.4f, 0);
    lb_box(-32, 6, 11, -24, 9, 14, TEX_CARPET, TEX_CARPET, 0.35f, 0.55f, 0.4f, 0);
    lb_box(-31.5f, 6, -8.5f, -25, 7, 0.5f, TEX_CARPET, TEX_CARPET, 0.45f, 0.68f, 0.5f, 0);
    lb_box(-31.5f, 6, 1.5f, -25, 7, 10.5f, TEX_CARPET, TEX_CARPET, 0.45f, 0.68f, 0.5f, 0);
    for (k = 0; k < 24; k++) {
        float a0 = TAU_F * k / 24, a1 = TAU_F * (k + 1) / 24;
        lb_tri(TEX_NONE, v3(-35.85f, 22, -6), v3(-35.85f, 22 + cosf(a1) * 5.5f, -6 + sinf(a1) * 5.5f),
               v3(-35.85f, 22 + cosf(a0) * 5.5f, -6 + sinf(a0) * 5.5f), 0.98f, 0.96f, 0.9f, 0, 0);
        lb_quad(TEX_NONE, v3(-35.8f, 22 + cosf(a0) * 5.5f, -6 + sinf(a0) * 5.5f), v3(-35.8f, 22 + cosf(a1) * 5.5f, -6 + sinf(a1) * 5.5f),
                v3(-35.8f, 22 + cosf(a1) * 6.2f, -6 + sinf(a1) * 6.2f), v3(-35.8f, 22 + cosf(a0) * 6.2f, -6 + sinf(a0) * 6.2f), 0.55f, 0.35f, 0.15f, 0, 0);
    }
    for (k = 0; k < 12; k++) {
        float a = TAU_F * k / 12, c = cosf(a), s = sinf(a), l = k % 3 == 0 ? 1.0f : 0.5f;
        lb_quad(TEX_NONE, v3(-35.75f, 22 + c * 4.9f - s * 0.15f, -6 + s * 4.9f + c * 0.15f), v3(-35.75f, 22 + c * 4.9f + s * 0.15f, -6 + s * 4.9f - c * 0.15f),
                v3(-35.75f, 22 + c * (4.9f - l) + s * 0.15f, -6 + s * (4.9f - l) - c * 0.15f), v3(-35.75f, 22 + c * (4.9f - l) - s * 0.15f, -6 + s * (4.9f - l) + c * 0.15f),
                0.1f, 0.1f, 0.12f, 0, 0);
    }
    /* the dining table: plate, cake, cup and salt; a chair with sugar cubes; a stack of books as stairs */
    lb_box(-22, 8.2f, 4, 2, 9, 20, TEX_WOOD, TEX_WOOD, 0.7f, 0.48f, 0.3f, 0);
    lb_cyl(-21, 0, 5, 0.9f, 8.2f, 10, TEX_WOOD, TEX_WOOD, 0.65f, 0.45f, 0.28f, 0);
    lb_cyl(1, 0, 5, 0.9f, 8.2f, 10, TEX_WOOD, TEX_WOOD, 0.65f, 0.45f, 0.28f, 0);
    lb_cyl(-21, 0, 19, 0.9f, 8.2f, 10, TEX_WOOD, TEX_WOOD, 0.65f, 0.45f, 0.28f, 0);
    lb_cyl(1, 0, 19, 0.9f, 8.2f, 10, TEX_WOOD, TEX_WOOD, 0.65f, 0.45f, 0.28f, 0);
    lb_cyl(-8, 9, 12, 3.6f, 0.3f, 24, TEX_NONE, TEX_NONE, 0.98f, 0.98f, 0.97f, 0);
    lb_box(-10, 9.3f, 10.5f, -6, 11.5f, 14, TEX_NONE, TEX_NONE, 0.95f, 0.8f, 0.55f, 0);
    lb_box(-10, 10.3f, 10.48f, -6, 10.6f, 14.02f, TEX_NONE, TEX_NONE, 1.0f, 0.55f, 0.65f, 0);
    lb_box(-10.1f, 11.5f, 10.4f, -5.9f, 12.0f, 14.1f, TEX_NONE, TEX_NONE, 1.0f, 0.98f, 0.95f, 0);
    lb_cyl(-7, 12, 11.5f, 0.55f, 0.7f, 8, TEX_NONE, TEX_NONE, 0.9f, 0.1f, 0.15f, 0);
    lb_cyl(-17, 9, 8, 1.8f, 3.8f, 16, TEX_NONE, TEX_NONE, 0.98f, 0.98f, 1.0f, 0);
    lb_cyl(-17, 12.75f, 8, 1.6f, 0.1f, 16, TEX_NONE, TEX_NONE, 0.35f, 0.2f, 0.1f, 0);
    lb_box(-15.4f, 10.2f, 7.7f, -14.4f, 12.0f, 8.3f, TEX_NONE, TEX_NONE, 0.98f, 0.98f, 1.0f, 0);
    lb_cyl(-3, 9, 17, 0.8f, 3.0f, 10, TEX_NONE, TEX_NONE, 0.95f, 0.95f, 0.95f, 0);
    lb_cyl(-3, 12, 17, 0.8f, 0.4f, 10, TEX_METAL, TEX_METAL, 0.75f, 0.75f, 0.8f, 0);
    lb_box(-20, 9, 16, -13, 9.15f, 16.5f, TEX_METAL, TEX_METAL, 0.8f, 0.8f, 0.85f, 0);   /* fork and knife */
    lb_box(-20, 9, 17.3f, -13, 9.15f, 17.7f, TEX_METAL, TEX_METAL, 0.8f, 0.8f, 0.85f, 0);
    lb_box(3, 4.4f, 8, 9, 5, 16, TEX_WOOD, TEX_WOOD, 0.75f, 0.52f, 0.32f, 0);            /* chair */
    lb_box(8.2f, 5, 8, 9, 12, 16, TEX_WOOD, TEX_WOOD, 0.75f, 0.52f, 0.32f, 0);
    for (k = 0; k < 4; k++) lb_cyl((k & 1) ? 8.4f : 3.6f, 0, (k & 2) ? 15.4f : 8.6f, 0.45f, 4.4f, 8, TEX_WOOD, TEX_WOOD, 0.7f, 0.48f, 0.3f, 0);
    lb_box(3.2f, 5, 10, 5.2f, 7, 12, TEX_NONE, TEX_NONE, 1, 1, 1, 0);                   /* sugar cubes */
    lb_box(5.6f, 5, 12.5f, 7.2f, 6.6f, 14.1f, TEX_NONE, TEX_NONE, 1, 1, 1, 0);
    huge_flat_book(16, 9, 21, 15, 0, 1.7f);
    huge_flat_book(12.5f, 9.5f, 16.5f, 14.5f, 0, 3.4f);
    huge_flat_book(9.6f, 9.8f, 12.6f, 14.2f, 0, 5.0f);
    /* the bookshelf on the east wall and the pile of books that climbs to it */
    lb_box(30, 0, -26, 36, 32, -25, TEX_WOOD, TEX_WOOD, 0.6f, 0.4f, 0.25f, 0);
    lb_box(30, 0, 5, 36, 32, 6, TEX_WOOD, TEX_WOOD, 0.6f, 0.4f, 0.25f, 0);
    lb_box(30, 31.2f, -26, 36, 32, 6, TEX_WOOD, TEX_WOOD, 0.6f, 0.4f, 0.25f, 0);
    lb_box(30, 0, -25, 36, 1.5f, 5, TEX_WOOD, TEX_WOOD, 0.6f, 0.4f, 0.25f, 0);
    for (k = 0; k < 3; k++) {
        float y = 8 + k * 8;
        lb_box(30, y, -25, 36, y + 0.8f, 5, TEX_WOOD, TEX_WOOD, 0.62f, 0.42f, 0.26f, 0);
        /* steps up to the next shelf: a short and a medium book at the north end */
        huge_book(31.2f, y + 0.8f, -24.9f, 35.8f, y + 0.8f + 2.3f, -22.6f);
        huge_book(31.2f, y + 0.8f, -22.5f, 35.8f, y + 0.8f + 4.6f, -20.2f);
        huge_book(31.2f, y + 0.8f, -20.1f, 35.8f, y + 0.8f + 6.8f, -17.8f);
        huge_shelf_books(y + 0.8f, -15.5f, 4.8f);
    }
    huge_shelf_books(1.5f, -24.8f, 4.8f);
    huge_flat_book(24, -12, 30, -4, 0, 1.6f);
    huge_flat_book(25, -11, 30, -5, 1.6f, 1.6f);
    huge_flat_book(26, -10.5f, 30, -5.5f, 3.2f, 1.6f);
    huge_flat_book(27, -10, 30, -6, 4.8f, 1.6f);
    huge_flat_book(28, -9.5f, 30, -6.5f, 6.4f, 1.5f);
    /* the rug, the cat's cushion and a floor lamp */
    lb_box(4, 0, -22, 28, 0.2f, 2, TEX_CARPET, TEX_CARPET, 0.65f, 0.2f, 0.22f, 0);
    lb_box(5, 0.2f, -21, 27, 0.22f, 1, TEX_CARPET, TEX_CARPET, 0.8f, 0.55f, 0.3f, 0);
    lb_box(6, 0.22f, -20, 26, 0.24f, 0, TEX_CARPET, TEX_CARPET, 0.6f, 0.18f, 0.2f, 0);
    lb_cyl(16, 0.24f, -10, 5.2f, 0.4f, 24, TEX_CARPET, TEX_CARPET, 0.5f, 0.35f, 0.65f, 0);
    lb_cyl(-30, 0, 30, 3.0f, 0.6f, 16, TEX_METAL, TEX_METAL, 0.3f, 0.3f, 0.32f, 0);
    lb_cyl(-30, 0.6f, 30, 0.35f, 23.0f, 8, TEX_METAL, TEX_METAL, 0.5f, 0.5f, 0.52f, 0);
    lb_cyl(-30, 22, 30, 4.2f, 5.5f, 16, TEX_NONE, TEX_NONE, 1.0f, 0.95f, 0.8f, 0);
    lb_uv_mul = 1.0f;
    finish_geometry();

    lv.spawn[0] = v3(-16, 0, -14); lv.spawn_yaw[0] = 1.9f;

    /* star 0: on the cake on the table */
    obj_spawn(OT_STAR, v3(-8, 12.4f, 12.2f), 0, 0);
    /* star 1: the toaster launches Tux to the high shelf; the drawers are the stairs up to the counter */
    o = obj_spawn(OT_TOASTER, v3(-2, 13.3f, -30.2f), 0, 0);
    obj_spawn(OT_STAR, v3(-15, 24.4f, -34), 0, 1);
    for (k = 0; k < 4; k++) {
        o = obj_spawn(OT_PLATFORM, v3(-30 + k * 6, 1.6f + k * 2.2f, -28.2f), 0, 0);
        if (o) {
            obj_set_box(o, 2.6f, 0.6f, 2.0f, TEX_WOOD, TEX_WOOD, 0.85f, 0.65f, 0.45f);
            o->f[2] = 4.4f; o->f[3] = 5.0f; o->f[4] = -k * 1.25f;   /* each drawer opens a little after the one below */
        }
    }
    /* star 2: the sleeping cat (the star is under it) and the ball of yarn */
    o = obj_spawn(OT_GIANTCAT, v3(16, 0.64f, -10), 0.6f, 0);
    if (o) { o->home = o->pos; o->f[0] = -21; o->f[1] = -21; o->f[2] = 23; o->f[3] = 2; o->f[5] = 0.6f; }
    o = obj_spawn(OT_YARN, v3(-27, 0, -18), 0, 0);
    if (o) o->home = o->pos;
    /* star 3: the mouse hole, past the giant's stomping slippers */
    for (k = 0; k < 2; k++) {
        o = obj_spawn(OT_GIANTFOOT, v3(10 + k * 10, 60, 30), 0, k);
        if (o) {
            o->home = v3(o->pos.x, 0, o->pos.z);
            o->f[0] = 4; o->f[1] = 29.5f; o->f[2] = 26; o->f[3] = 30.5f;
            o->i[0] = 60 + k * 90;
        }
    }
    obj_spawn(OT_STAR, v3(30, 1.0f, 39), 0, 3);
    /* star 4: red fish */
    obj_spawn(OT_REDCOIN, v3(-17, 13.6f, 8), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-34, 13.8f, 1), 0, 0);
    obj_spawn(OT_REDCOIN, v3(30.6f, 25.4f, -8), 0, 0);
    obj_spawn(OT_REDCOIN, v3(24, 1.2f, 33), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-34, 11.8f, -27), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-10, 0.8f, 12), 0, 0);
    obj_spawn(OT_REDCOIN, v3(20, 15.8f, -34.5f), 0, 0);
    obj_spawn(OT_REDCOIN, v3(30.6f, 2.3f, -2), 0, 0);
    obj_spawn(OT_STARMARKER, v3(4, 4.0f, -6), 0, 4);
    /* fish */
    coin_line(v3(-16, 0.6f, -8), v3(-4, 0.6f, -8), 6, 0);
    coin_ring(v3(16, 0.8f, -10), 8.0f, 12, 0);
    coin_line(v3(-20, 9.6f, 14), v3(-12, 9.6f, 19), 5, 0);
    coin_line(v3(-1, 9.6f, 6), v3(-1, 9.6f, 13), 4, 0);
    coin_line(v3(-22, 11.6f, -28), v3(4, 11.6f, -28), 8, 0);
    coin_line(v3(-30, 6.6f, -6), v3(-30, 6.6f, 9), 6, 0);
    coin_line(v3(10, 0.6f, 32), v3(26, 0.6f, 32), 6, 0);
    coin_line(v3(-18, 0.6f, 10), v3(-4, 0.6f, 10), 5, 0);
    coin_line(v3(12, 15.6f, -34.5f), v3(22, 15.6f, -34.5f), 4, 0);
    for (k = 0; k < 3; k++) coin_line(v3(30.6f, 9.4f + k * 8, -12), v3(30.6f, 9.4f + k * 8, 3), 5, 0);
    coin_line(v3(18, 2.3f, 12), v3(10.5f, 5.7f, 12), 4, 0);
    coin_ring(v3(-30, 1.2f, 30), 4.2f, 8, 0);
    coin_line(v3(-26, 0.6f, -30), v3(-26, 0.6f, -20), 5, 0);
    coin_line(v3(-14, 24.6f, -34), v3(2, 24.6f, -34), 4, 0);
    coin_line(v3(24, 0.6f, -20), v3(24, 0.6f, -14), 4, 0);
    coin_ring(v3(-8, 0.6f, -22), 4.0f, 8, 0);
    sign(-12, -14, 2, -PI_F / 2, "A NOTE FROM A MOUSE: The giant keeps a star\nup on the high shelf. Hop into the toaster and\n"
                                  "wait for the pop! The drawers of the counter\nslide in and out - ride them up to the top.");
    sign(0, 30, 2, -PI_F / 2, "A NOTE FROM A MOUSE: Our door is at the far end\nof the hall. Watch out for the giant's slippers -\n"
                              "when a shadow grows on the floor, RUN!");
    sign(-20, -2, 2, PI_F / 2, "A NOTE FROM A MOUSE: The cat sleeps on something\nshiny. Don't run or jump near it! Cats can't\n"
                           "resist a ball of yarn... throw one with B.");
}

/* ================================================================
 * UPTIME HEIGHTS - a skyscraper city at dusk, all about height
 * ================================================================ */
/* a tower: office facade on the sides, a flat roof with a parapet (gaps where bridges arrive) */
static void city_tower(float x0, float z0, float x1, float z1, float h, float r, float g, float b)
{
    lb_box(x0, -2, z0, x1, h, z1, TEX_STONE, TEX_OFFICE, r, g, b, 0);
}

/* a parapet along one side from (ax,az) to (bx,bz), leaving out the parameter range g0..g1 (0..1) */
static void city_wall_run(float ax, float az, float bx, float bz, float y, float g0, float g1)
{
    float t = 0.5f, h = y + 1.1f;
    vec3 a = v3(ax, y, az), b = v3(bx, y, bz);
    float segs[2][2] = { { 0, g0 }, { g1, 1 } };
    int k;
    for (k = 0; k < 2; k++) {
        vec3 p = v3lerp(a, b, segs[k][0]), q = v3lerp(a, b, segs[k][1]);
        if (segs[k][1] - segs[k][0] < 0.001f) continue;
        lb_box(minf(p.x, q.x) - (ax == bx ? t / 2 : 0), y, minf(p.z, q.z) - (az == bz ? t / 2 : 0),
               maxf(p.x, q.x) + (ax == bx ? t / 2 : 0), h, maxf(p.z, q.z) + (az == bz ? t / 2 : 0), TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0);
    }
}

static void city_parapet(float x0, float z0, float x1, float z1, float y, int gap_side, float g0, float g1)
{
    /* gap_side: 0 none, 1 -x, 2 +x, 3 -z, 4 +z; the gap spans g0..g1 along that side */
    float t = 0.5f, h = y + 1.1f;
    if (gap_side == 3) { lb_box(x0, y, z0, g0, h, z0 + t, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); lb_box(g1, y, z0, x1, h, z0 + t, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); }
    else lb_box(x0, y, z0, x1, h, z0 + t, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0);
    if (gap_side == 4) { lb_box(x0, y, z1 - t, g0, h, z1, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); lb_box(g1, y, z1 - t, x1, h, z1, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); }
    else lb_box(x0, y, z1 - t, x1, h, z1, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0);
    if (gap_side == 1) { lb_box(x0, y, z0 + t, x0 + t, h, g0, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); lb_box(x0, y, g1, x0 + t, h, z1 - t, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); }
    else lb_box(x0, y, z0 + t, x0 + t, h, z1 - t, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0);
    if (gap_side == 2) { lb_box(x1 - t, y, z0 + t, x1, h, g0, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); lb_box(x1 - t, y, g1, x1, h, z1 - t, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0); }
    else lb_box(x1 - t, y, z0 + t, x1, h, z1 - t, TEX_STONE, TEX_STONE, 0.6f, 0.6f, 0.65f, 0);
}

/* a steel girder (I-beam) between two points: narrow walkway, flanges and web */
static void city_girder(vec3 a, vec3 b, float w)
{
    vec3 d = v3sub(b, a), c = v3lerp(a, b, 0.5f);
    float len = v3len(d), yaw = atan2f(d.x, d.z), pitch = asinf(d.y / len);
    lb_obox(v3add(c, v3(0, -0.15f, 0)), v3(w / 2, 0.15f, len / 2), yaw, pitch, TEX_METAL, TEX_METAL, 0.75f, 0.35f, 0.25f, 0);
    lb_obox(v3add(c, v3(0, -1.0f, 0)), v3(0.12f, 0.7f, len / 2), yaw, pitch, TEX_METAL, TEX_METAL, 0.65f, 0.3f, 0.2f, 0);
    lb_obox(v3add(c, v3(0, -1.75f, 0)), v3(w / 2, 0.12f, len / 2), yaw, pitch, TEX_METAL, TEX_METAL, 0.65f, 0.3f, 0.2f, 0);
}

/* an enclosed skybridge: floor, glass sides and roof (the glass is see-through) */
static void city_skybridge(vec3 a, vec3 b, float w)
{
    vec3 d = v3sub(b, a), c = v3lerp(a, b, 0.5f), side;
    float len = v3len(d), yaw = atan2f(d.x, d.z), pitch = asinf(d.y / len);
    lb_obox(v3add(c, v3(0, -0.3f, 0)), v3(w / 2, 0.3f, len / 2), yaw, pitch, TEX_TILE, TEX_METAL, 0.85f, 0.85f, 0.9f, 0);
    lb_obox(v3add(c, v3(0, 3.6f, 0)), v3(w / 2 + 0.2f, 0.2f, len / 2), yaw, pitch, TEX_METAL, TEX_METAL, 0.6f, 0.62f, 0.7f, 0);
    side = v3(cosf(yaw), 0, -sinf(yaw));
    lb_obox(v3add(c, v3add(v3scale(side, w / 2), v3(0, 0.25f, 0))), v3(0.1f, 0.25f, len / 2), yaw, pitch, TEX_METAL, TEX_METAL, 0.6f, 0.62f, 0.7f, 0);
    lb_obox(v3add(c, v3add(v3scale(side, -w / 2), v3(0, 0.25f, 0))), v3(0.1f, 0.25f, len / 2), yaw, pitch, TEX_METAL, TEX_METAL, 0.6f, 0.62f, 0.7f, 0);
    lb_alpha_mode = 1;
    {
        int k;
        for (k = -1; k <= 1; k += 2) {
            vec3 o = v3scale(side, k * w / 2);
            vec3 p0 = v3add(a, o), p1 = v3add(b, o);
            lb_quad(TEX_NONE, p0, p1, v3add(p1, v3(0, 3.5f, 0)), v3add(p0, v3(0, 3.5f, 0)), 0.6f, 0.85f, 1.0f, 0, 0);
            lb_quad(TEX_NONE, p1, p0, v3add(p0, v3(0, 3.5f, 0)), v3add(p1, v3(0, 3.5f, 0)), 0.6f, 0.85f, 1.0f, 0, 0);
        }
    }
    lb_alpha_mode = 0;
    /* invisible side walls so Tux cannot jump out through the glass */
    {
        int k;
        for (k = -1; k <= 1; k += 2) {
            vec3 o = v3scale(side, k * w / 2);
            vec3 p0 = v3add(a, o), p1 = v3add(b, o), q0 = v3add(p0, v3(0, 3.5f, 0)), q1 = v3add(p1, v3(0, 3.5f, 0));
            col_add_tri(p0, p1, q1, SF_NOCAM, 0); col_add_tri(p0, q1, q0, SF_NOCAM, 0);
            col_add_tri(p0, q1, p1, SF_NOCAM, 0); col_add_tri(p0, q0, q1, SF_NOCAM, 0);
        }
    }
}

static void build_city(void)
{
    Obj *o;
    int k;
    lv.name = "UPTIME HEIGHTS";
    lv.music = MUS_CITY;
    lv.course = CRS_CITY;
    lv.tall_world = 1;
    lv.zfar = 900;
    set_sky(0.08f, 0.1f, 0.3f, 0.95f, 0.55f, 0.42f, 160, 650);
    lv.ambient = 0.55f; lv.diffuse = 0.55f;
    lv.death_y = 25;
    lv.stars_sky = 1;
    lv.snow = 1;
    g_coin_star_idx = 5;
    lb_begin();
    /* the street far below and the skyline all around */
    lb_box(-420, -2, -420, 420, 0, 420, TEX_STONE, TEX_STONE, 0.25f, 0.25f, 0.3f, 0);
    for (k = -6; k <= 6; k++) {
        lb_box(k * 60.0f + 4, 0, -420, k * 60.0f + 12, 0.05f, 420, TEX_NONE, TEX_NONE, 0.12f, 0.12f, 0.15f, 0);
        lb_box(-420, 0, k * 60.0f + 4, 420, 0.05f, k * 60.0f + 12, TEX_NONE, TEX_NONE, 0.12f, 0.12f, 0.15f, 0);
    }
    lb_uv_mul = 1.0f;
    for (k = 0; k < 170; k++) {
        float a = frand() * TAU_F, r = 110 + frand() * 300;
        float x = cosf(a) * r, z = sinf(a) * r, s = 10 + frand() * 14, h = 40 + frand() * frand() * 220;
        float sh = 0.6f + 0.35f * frand();
        if (fmodf(fabsf(x) + 2, 60) < 18 || fmodf(fabsf(z) + 2, 60) < 18) continue;   /* keep the streets clear */
        city_tower(x - s, z - s, x + s, z + s, h, sh * 0.85f, sh * 0.9f, sh);
        if (frand() < 0.3f) city_tower(x - s * 0.5f, z - s * 0.5f, x + s * 0.5f, z + s * 0.5f, h + 10 + frand() * 25, sh * 0.8f, sh * 0.85f, sh);
    }
    /* A: the starting tower (roof 120) */
    city_tower(-15, -15, 15, 15, 120, 0.75f, 0.8f, 0.9f);
    /* A's parapet has openings for the girder (east), the skybridges (west, north) and the girder from E (north-west corner) */
    city_wall_run(14.75f, -14.75f, 14.75f, 14.75f, 120, 0.45f, 0.55f);     /* east: girder */
    city_wall_run(-14.75f, 14.75f, -14.75f, 0, 120, 0.32f, 0.6f);         /* west: skybridge to B */
    city_wall_run(-14.75f, 0, -14.75f, -12, 120, 0, 0);
    city_wall_run(-12, -14.75f, -2.6f, -14.75f, 120, 0, 0);               /* north: skybridge to D */
    city_wall_run(2.6f, -14.75f, 14.75f, -14.75f, 120, 0, 0);
    city_wall_run(14.75f, 14.75f, -14.75f, 14.75f, 120, 0, 0);
    lb_cyl(-6, 120, 6, 4.5f, 0.06f, 20, TEX_NONE, TEX_NONE, 0.25f, 0.6f, 0.35f, 0);              /* helipad */
    lb_box(-7, 120.06f, 4.2f, -6.5f, 120.1f, 7.8f, TEX_NONE, TEX_NONE, 1, 1, 1, 0);
    lb_box(-5.5f, 120.06f, 4.2f, -5.0f, 120.1f, 7.8f, TEX_NONE, TEX_NONE, 1, 1, 1, 0);
    lb_box(-6.5f, 120.06f, 5.8f, -5.5f, 120.1f, 6.2f, TEX_NONE, TEX_NONE, 1, 1, 1, 0);
    lb_box(6, 120, 5, 11, 122.2f, 10, TEX_METAL, TEX_METAL, 0.7f, 0.72f, 0.75f, 0);              /* air conditioners */
    lb_box(6, 120, -10, 10, 121.6f, -6, TEX_METAL, TEX_METAL, 0.7f, 0.72f, 0.75f, 0);
    for (k = 0; k < 4; k++) lb_cyl(-11 + (k & 1) * 3.0f, 120, -11 + (k / 2) * 3.0f, 0.25f, 3.0f, 6, TEX_METAL, TEX_METAL, 0.4f, 0.35f, 0.3f, 0);
    lb_cyl(-9.5f, 123, -9.5f, 2.5f, 3.5f, 14, TEX_WOOD, TEX_WOOD, 0.55f, 0.4f, 0.3f, 0);          /* water tank */
    lb_cone(-9.5f, 126.5f, -9.5f, 2.8f, 1.2f, 14, TEX_ROOF, 0.4f, 0.3f, 0.25f, 1);
    obj_spawn(OT_REDCOIN, v3(-9.5f, 120.6f, -9.5f), 0, 0);   /* under the water tank */
    obj_spawn(OT_REDCOIN, v3(8.5f, 122.8f, 7.5f), 0, 0);    /* on the air conditioner */
    /* star 2: the windy girder east to the small tower F (roof 118) */
    city_tower(54, -6, 66, 6, 118, 0.7f, 0.7f, 0.78f);
    city_parapet(54, -6, 66, 6, 118, 1, -1.5f, 1.5f);
    city_girder(v3(15, 120, 0), v3(33, 119.2f, 0), 2.0f);
    lb_box(32, 116.9f, -2, 36, 119.2f, 2, TEX_METAL, TEX_METAL, 0.6f, 0.3f, 0.2f, 0);   /* junction */
    city_girder(v3(36, 119.2f, 0), v3(54, 118, 0), 2.0f);
    obj_spawn(OT_REDCOIN, v3(34, 119.8f, 0), 0, 0);
    obj_spawn(OT_STAR, v3(60, 118.8f, 0), 0, 2);
    coin_line(v3(18, 120.6f, 0), v3(30, 119.8f, 0), 4, 0);
    coin_line(v3(38, 119.6f, 0), v3(51, 118.7f, 0), 4, 0);
    /* B: the stepped tower to the west (terrace 128, top 175), reached through a skybridge */
    city_tower(-78, -5, -48, 25, 128, 0.65f, 0.75f, 0.85f);
    city_parapet(-78, -5, -48, 25, 128, 2, 4, 12);
    city_tower(-78, -5, -60, 13, 175, 0.7f, 0.78f, 0.88f);
    city_parapet(-78, -5, -60, 13, 175, 0, 0, 0);
    lb_box(-74, 175, -3, -68, 176.3f, 3, TEX_METAL, TEX_METAL, 0.55f, 0.58f, 0.62f, 0);   /* machine room: a step up to the crane's jib */    city_skybridge(v3(-15, 120, 8), v3(-48, 128, 8), 4.0f);
    obj_spawn(OT_REDCOIN, v3(-31.5f, 128.4f, 8), 0, 0);   /* on the skybridge roof */
    coin_line(v3(-19, 121.5f, 8), v3(-44, 127.5f, 8), 6, 0);
    /* the window washer's gondola climbs B's east face to the top */
    o = obj_spawn(OT_PLATFORM, v3(-57.8f, 128.3f, 4), 0, 0);
    if (o) { obj_set_box(o, 1.6f, 0.3f, 2.6f, TEX_METAL, TEX_METAL, 0.85f, 0.85f, 0.3f); o->f[1] = 47.2f; o->f[3] = 12; }
    lb_cyl(-58.6f, 175, 1.2f, 0.12f, 3, 6, TEX_METAL, TEX_METAL, 0.5f, 0.5f, 0.5f, 0);
    lb_cyl(-58.6f, 175, 6.8f, 0.12f, 3, 6, TEX_METAL, TEX_METAL, 0.5f, 0.5f, 0.5f, 0);
    lb_box(-59.5f, 177.6f, 0.8f, -55.6f, 178, 7.2f, TEX_METAL, TEX_METAL, 0.5f, 0.5f, 0.5f, 0);
    obj_spawn(OT_REDCOIN, v3(-76, 175.8f, 10), 0, 0);
    obj_spawn(OT_REDCOIN, v3(-57.5f, 178.6f, 4), 0, 0);   /* on the gondola's beam */
    coin_ring(v3(-69, 175.6f, 4), 5, 8, 0);
    coin_line(v3(-76, 175.6f, 10), v3(-64, 175.6f, 10), 4, 0);
    /* star 1: the tower crane on the unfinished tower C; its jib swings over B's roof */
    {
        float cx = -66, cz = -30, top = 160;
        for (k = 0; k < 4; k++) {   /* the bare steel frame of the unfinished tower */
            float px = cx + ((k & 1) ? 9 : -9), pz = cz + ((k & 2) ? 9 : -9);
            lb_box(px - 0.6f, -2, pz - 0.6f, px + 0.6f, top, pz + 0.6f, TEX_METAL, TEX_METAL, 0.65f, 0.35f, 0.2f, 0);
        }
        for (k = 0; k < 16; k++) {
            float y = 20 + k * 9;
            lb_box(cx - 9, y, cz - 9.4f, cx + 9, y + 0.6f, cz - 8.6f, TEX_METAL, TEX_METAL, 0.65f, 0.35f, 0.2f, 0);
            lb_box(cx - 9, y, cz + 8.6f, cx + 9, y + 0.6f, cz + 9.4f, TEX_METAL, TEX_METAL, 0.65f, 0.35f, 0.2f, 0);
            lb_box(cx - 9.4f, y, cz - 9, cx - 8.6f, y + 0.6f, cz + 9, TEX_METAL, TEX_METAL, 0.65f, 0.35f, 0.2f, 0);
            lb_box(cx + 8.6f, y, cz - 9, cx + 9.4f, y + 0.6f, cz + 9, TEX_METAL, TEX_METAL, 0.65f, 0.35f, 0.2f, 0);
        }
        lb_box(cx - 9.5f, top, cz - 9.5f, cx + 9.5f, top + 0.8f, cz + 9.5f, TEX_PLANK, TEX_METAL, 0.75f, 0.65f, 0.5f, 0);
        /* the crane mast and the operator's cab */
        lb_box(cx - 1.2f, top + 0.8f, cz - 1.2f, cx + 1.2f, 176, cz + 1.2f, TEX_METAL, TEX_METAL, 0.95f, 0.8f, 0.15f, 0);
        lb_box(cx - 1.6f, 178.2f, cz - 1.6f, cx + 1.6f, 180.6f, cz + 1.6f, TEX_METAL, TEX_METAL, 0.95f, 0.8f, 0.15f, 0);
        lb_box(cx - 0.4f, 180.6f, cz - 0.4f, cx + 0.4f, 190, cz + 0.4f, TEX_METAL, TEX_METAL, 0.95f, 0.8f, 0.15f, 0);
        o = obj_spawn(OT_ROTPLAT, v3(cx, 177.2f, cz), 0, 0);
        if (o) { obj_set_box(o, 0.9f, 0.5f, 36.0f, TEX_METAL, TEX_METAL, 0.95f, 0.8f, 0.15f); o->f[3] = 0.22f; o->flags |= OF_NOCULL; }
        obj_spawn(OT_STAR, v3(cx, 181.2f, cz), 0, 1);
    }
    /* D: the spire tower (roof 230) with an express elevator and an antenna mast */
    city_tower(-12, -87, 12, -63, 230, 0.6f, 0.68f, 0.85f);
    city_tower(-8, -83, 8, -67, 234, 0.62f, 0.7f, 0.88f);
    city_parapet(-12, -87, 12, -63, 230, 4, -3, 3);
    city_skybridge(v3(0, 120, -15), v3(0, 120, -58), 5.0f);
    lb_stairs(-2.5f, -63.2f, 2.5f, -67.2f, 230, 234, 1, 8, TEX_STONE, 0.6f, 0.65f, 0.75f);
    /* the glass shaft of the express elevator */
    lb_alpha_mode = 1;
    lb_quad(TEX_NONE, v3(-3, 120, -57.6f), v3(3, 120, -57.6f), v3(3, 232, -57.6f), v3(-3, 232, -57.6f), 0.6f, 0.85f, 1.0f, 0, 0);
    lb_quad(TEX_NONE, v3(3, 120, -57.6f), v3(3, 120, -63), v3(3, 232, -63), v3(3, 232, -57.6f), 0.6f, 0.85f, 1.0f, 0, 0);
    lb_quad(TEX_NONE, v3(-3, 120, -63), v3(-3, 120, -57.6f), v3(-3, 232, -57.6f), v3(-3, 232, -63), 0.6f, 0.85f, 1.0f, 0, 0);
    lb_alpha_mode = 0;    o = obj_spawn(OT_ELEVATOR, v3(0, 119.75f, -60.5f), 0, 0);
    if (o) { obj_set_box(o, 2.6f, 0.25f, 2.6f, TEX_METAL, TEX_METAL, 0.9f, 0.9f, 0.95f); o->f[0] = 110.0f; o->f[1] = 15.0f; o->home = o->pos; o->flags |= OF_NOCULL; }
    o = obj_spawn(OT_POLE, v3(0, 234, -75), 0, 0);
    if (o) { o->height = 34; o->radius = 0.45f; }
    lb_cyl(0, 234, -75, 0.4f, 34, 8, TEX_METAL, TEX_METAL, 0.85f, 0.85f, 0.9f, 0);
    obj_spawn(OT_STAR, v3(0, 265.5f, -75), 0, 0);   /* at the top of the mast: grab it while climbing */
    obj_spawn(OT_REDCOIN, v3(9, 231, -84), 0, 0);
    coin_line(v3(0, 121, -20), v3(0, 121, -54), 6, 0);
    /* star 3: skydive from D's roof through the rings into the rooftop pool on tower E */
    city_tower(-48, -50, -28, -30, 44, 0.7f, 0.65f, 0.6f);
    lb_box(-48, 44, -50, -28, 50, -47, TEX_TILE, TEX_TILE, 0.95f, 0.95f, 0.97f, 0);
    lb_box(-48, 44, -33, -28, 50, -30, TEX_TILE, TEX_TILE, 0.95f, 0.95f, 0.97f, 0);
    lb_box(-48, 44, -47, -45, 50, -33, TEX_TILE, TEX_TILE, 0.95f, 0.95f, 0.97f, 0);
    lb_box(-31, 44, -47, -28, 50, -33, TEX_TILE, TEX_TILE, 0.95f, 0.95f, 0.97f, 0);
    lb_box(-45, 44, -47, -31, 47, -33, TEX_TILE, TEX_TILE, 0.4f, 0.75f, 0.95f, 0);
    lb_water(-45, -47, -31, -33, 49.6f, 44);
    obj_spawn(OT_REDCOIN, v3(-38, 47.8f, -40), 0, 0);
    {
        static const float rings[6][3] = { { -18, 205, -76 }, { -22, 180, -70 }, { -26, 152, -63 }, { -30, 124, -56 }, { -34, 96, -48 }, { -37, 70, -42 } };
        for (k = 0; k < 6; k++) {
            o = obj_spawn(OT_RING, v3(rings[k][0], rings[k][1], rings[k][2]), 0, k);
            if (o) { o->param2 = 3; o->f[0] = 4.5f; o->f[1] = -38; o->f[2] = 46.5f; o->f[3] = -40; o->flags |= OF_NOCULL; }
            coin_line(v3(rings[k][0], rings[k][1] + 7, rings[k][2] - 2), v3(rings[k][0], rings[k][1] + 3, rings[k][2] - 1), 2, 0);
        }
    }
    /* the way back up from the pool: an elevator and a girder to A */
    o = obj_spawn(OT_ELEVATOR, v3(-29.5f, 49.75f, -28.5f), 0, 0);
    if (o) { obj_set_box(o, 1.5f, 0.25f, 1.5f, TEX_METAL, TEX_METAL, 0.9f, 0.9f, 0.95f); o->f[0] = 70.25f; o->f[1] = 12.0f; o->home = o->pos; o->flags |= OF_NOCULL; }
    city_girder(v3(-29.5f, 120, -27), v3(-14.6f, 120, -14.6f), 1.6f);
    /* fish */
    coin_ring(v3(-6, 120.6f, 6), 6.0f, 10, 0);
    coin_line(v3(-12, 120.6f, 12), v3(12, 120.6f, 12), 6, 0);
    coin_line(v3(-12, 120.6f, -2), v3(-12, 120.6f, -12), 4, 0);
    coin_line(v3(-74, 128.6f, 20), v3(-52, 128.6f, 20), 6, 0);
    coin_ring(v3(0, 230.6f, -75), 7.0f, 10, 0);
    coin_line(v3(-9, 230.6f, -66), v3(9, 230.6f, -66), 5, 0);
    for (k = 0; k < 6; k++) obj_spawn(OT_COIN, v3(0, 238 + k * 5, -74.2f), 0, 0);
    coin_line(v3(-27, 120.8f, -25.5f), v3(-17, 120.8f, -16.5f), 4, 0);
    coin_ring(v3(-38, 50.6f, -40), 9.0f, 10, 0);
    coin_line(v3(57, 118.6f, -4), v3(63, 118.6f, 4), 4, 0);
    finish_geometry();

    lv.spawn[0] = v3(0, 120, 10); lv.spawn_yaw[0] = PI_F;
    obj_spawn(OT_STARMARKER, v3(0, 124, 0), 0, 4);
    sign(3, 12, 122, PI_F, "UPTIME HEIGHTS - ROOFTOP NOTICE\nGusts sweep the girder to the east: crouch (Z)\n"
                           "when the wind blows! The window washer's lift\nclimbs the west tower. An express elevator\n"
                           "waits at the end of the north skybridge.");
    sign(-4, -57, 232, 0, "SKYDIVE CLUB: Run off the west edge and fall\nthrough all six rings into the pool on the\n"
                          "roof far below. Steer while you fall!");
}

/* ================================================================
 * CORE DUMP CAVERNS - a giant shaft down to a lake of magma, all about depth
 * ================================================================ */
#define CAVE_R 40.0f
#define CAVE_BOTTOM -290.0f

static float cave_wall_r(float a, float y)
{
    float n = 0.5f + 0.5f * sinf(a * 7 + y * 0.13f) * cosf(a * 3 - y * 0.07f);
    return CAVE_R + 0.4f + n * 2.6f;
}

/* place a box in the cave's polar frame: centre at angle a, radius r, height y; half sizes along the
 * tangent (ht), up (hy) and the radius (hr) */
static void cave_box(float a, float r, float y, float ht, float hy, float hr, int tt, int ts, float cr, float cg, float cb)
{
    lb_obox(cave_polar(a, r, y), v3(ht, hy, hr), a, 0, tt, ts, cr, cg, cb, 0);
}

static float cave_ground(float x, float z)
{
    float d = sqrtf(x * x + z * z);
    float h = 0.2f * sinf(x * 0.1f) * sinf(z * 0.13f);
    if (d < CAVE_R + 1) return -40;   /* the hole itself (the surface mesh is cut away there) */
    if (d < 48) h = lerpf(-0.3f, h, (d - 41) / 7);
    h += maxf(0, d - 80) * 0.6f;
    return h;
}

static void build_cave(void)
{
    Obj *o;
    int i, k;
    const float turn = CAVE_WALK_TURNS * TAU_F;
    lv.name = "CORE DUMP CAVERNS";
    lv.music = MUS_CAVE;
    lv.course = CRS_CAVE;
    lv.tall_world = 2;
    lv.zfar = 600;
    set_sky(0.55f, 0.7f, 0.95f, 0.75f, 0.85f, 0.95f, 80, 320);
    lv.ambient = 0.6f; lv.diffuse = 0.5f;
    lv.death_y = -320;
    g_coin_star_idx = 5;
    lb_begin();
    /* the snowy surface around the opening (a ring of quads, the middle left open) */
    for (k = 0; k < 48; k++) {
        float a0 = TAU_F * k / 48, a1 = TAU_F * (k + 1) / 48;
        static const float rr[6] = { CAVE_R + 0.4f, 44, 50, 62, 80, 110 };
        for (i = 0; i < 5; i++) {
            vec3 p00 = cave_polar(a0, rr[i], 0), p01 = cave_polar(a1, rr[i], 0), p10 = cave_polar(a0, rr[i + 1], 0), p11 = cave_polar(a1, rr[i + 1], 0);
            int tex = i < 1 ? TEX_ROCK : TEX_SNOW;
            if (i == 0) { p00.x = sinf(a0) * cave_wall_r(a0, 0); p00.z = cosf(a0) * cave_wall_r(a0, 0); p01.x = sinf(a1) * cave_wall_r(a1, 0); p01.z = cosf(a1) * cave_wall_r(a1, 0); }
            p00.y = i ? cave_ground(p00.x, p00.z) : 0; p01.y = i ? cave_ground(p01.x, p01.z) : 0;
            p10.y = cave_ground(p10.x, p10.z); p11.y = cave_ground(p11.x, p11.z);
            lb_quad(tex, p00, p10, p11, p01, i ? 1 : 0.7f, i ? 1 : 0.68f, i ? 1 : 0.66f, 0, 1);
        }
    }
    /* the shaft wall, facing inward */
    for (k = 0; k < 48; k++)
        for (i = 0; i < 50; i++) {
            float a0 = TAU_F * k / 48, a1 = TAU_F * (k + 1) / 48, y0 = -6.0f * i, y1 = -6.0f * (i + 1);
            vec3 p00 = cave_polar(a0, cave_wall_r(a0, y0), y0), p01 = cave_polar(a1, cave_wall_r(a1, y0), y0);
            vec3 p10 = cave_polar(a0, cave_wall_r(a0, y1), y1), p11 = cave_polar(a1, cave_wall_r(a1, y1), y1);
            float depth = clampf(-y0 / 290, 0, 1), sh = 0.85f + 0.15f * sinf(k * 1.7f + i * 0.9f);
            float cr = lerpf(0.75f, 0.7f, depth) * sh, cg = lerpf(0.75f, 0.42f, depth) * sh, cb = lerpf(0.8f, 0.35f, depth) * sh;
            if (y0 > -40) { cr = 0.8f * sh; cg = 0.88f * sh; cb = 0.95f * sh; }   /* frost near the opening */
            lb_quad(y0 > -40 ? TEX_ICE : TEX_ROCK, p00, p01, p11, p10, cr, cg, cb, 0, 1);
        }
    /* the spiral ledge: two turns down to the bottom, with a low rim on the open side */
    for (k = 0; k < 160; k++) {
        float a0 = turn * k / 160, a1 = turn * (k + 1) / 160, y0 = cave_walk_y(a0), y1 = cave_walk_y(a1);
        float ri = 33.0f, ro = CAVE_R + 0.6f;
        vec3 i0 = cave_polar(a0, ri, y0), i1 = cave_polar(a1, ri, y1), o0 = cave_polar(a0, ro, y0), o1 = cave_polar(a1, ro, y1);
        vec3 i0b = v3add(i0, v3(0, -1.6f, 0)), i1b = v3add(i1, v3(0, -1.6f, 0)), o0b = v3add(o0, v3(0, -1.6f, 0)), o1b = v3add(o1, v3(0, -1.6f, 0));
        float sh = (k & 1) ? 0.92f : 1.0f, depth = clampf(-y0 / 290, 0, 1);
        int gap = k == 37 || k == 38 || k == 101 || k == 102;   /* two places where the ledge has crumbled away */
        if (gap) continue;
        lb_quad(TEX_STONE, i0, o0, o1, i1, 0.72f * sh, lerpf(0.7f, 0.5f, depth) * sh, lerpf(0.68f, 0.45f, depth) * sh, 0, 1);
        lb_quad(TEX_ROCK, i0b, i1b, o1b, o0b, 0.4f, 0.38f, 0.36f, 0, 1);
        lb_quad(TEX_ROCK, i0, i1, i1b, i0b, 0.55f, 0.5f, 0.48f, 0, 1);
        if (k % 4 != 3) {   /* a low stone rim, broken here and there */
            vec3 r0 = cave_polar(a0, ri + 0.3f, y0), r1 = cave_polar(a1, ri + 0.3f, y1);
            lb_quad(TEX_ROCK, r0, r1, v3add(r1, v3(0, 0.5f, 0)), v3add(r0, v3(0, 0.5f, 0)), 0.5f, 0.48f, 0.45f, 0, 1);
            lb_quad(TEX_ROCK, r1, r0, v3add(r0, v3(0, 0.5f, 0)), v3add(r1, v3(0, 0.5f, 0)), 0.5f, 0.48f, 0.45f, 0, 1);
        }
    }
    /* the bottom: a rocky rim around a lake of magma, stepping stones and a pillar in the middle */
    for (k = 0; k < 48; k++) {
        float a0 = TAU_F * k / 48, a1 = TAU_F * (k + 1) / 48;
        lb_quad(TEX_ROCK, cave_polar(a0, 30, CAVE_BOTTOM), cave_polar(a0, CAVE_R + 3, CAVE_BOTTOM), cave_polar(a1, CAVE_R + 3, CAVE_BOTTOM), cave_polar(a1, 30, CAVE_BOTTOM), 0.4f, 0.3f, 0.28f, 0, 1);
        lb_quad(TEX_ROCK, cave_polar(a0, 30, CAVE_BOTTOM - 3), cave_polar(a0, 30, CAVE_BOTTOM), cave_polar(a1, 30, CAVE_BOTTOM), cave_polar(a1, 30, CAVE_BOTTOM - 3), 0.4f, 0.3f, 0.28f, 0, 1);
        lb_tri(TEX_LAVA, v3(0, CAVE_BOTTOM - 2.5f, 0), cave_polar(a0, 30.5f, CAVE_BOTTOM - 2.5f), cave_polar(a1, 30.5f, CAVE_BOTTOM - 2.5f), 1, 1, 1, SF_BURN, 1);
    }
    {
        static const float stones[5][2] = { { 0.3f, 26 }, { 0.55f, 21.5f }, { 0.8f, 17 }, { 1.1f, 13 }, { 1.45f, 9.5f } };
        for (k = 0; k < 5; k++) lb_cyl(sinf(stones[k][0]) * stones[k][1], CAVE_BOTTOM - 3, cosf(stones[k][0]) * stones[k][1], 2.2f, 3.4f, 10, TEX_ROCK, TEX_ROCK, 0.45f, 0.35f, 0.3f, 0);
        lb_cyl(sinf(1.9f) * 6, CAVE_BOTTOM - 3, cosf(1.9f) * 6, 2.6f, 5.0f, 12, TEX_ROCK, TEX_ROCK, 0.5f, 0.38f, 0.32f, 0);
        obj_spawn(OT_STAR, v3(sinf(1.9f) * 6, CAVE_BOTTOM + 2.6f, cosf(1.9f) * 6), 0, 0);
    }
    /* the central spire of rock rising out of the magma, with two rope bridges to it */
    lb_cyl(0, CAVE_BOTTOM - 3, 0, 3.0f, 196, 14, TEX_ROCK, TEX_ROCK, 0.5f, 0.45f, 0.42f, 0);
    lb_cyl(0, -100.8f, 0, 5.5f, 0.8f, 16, TEX_STONE, TEX_ROCK, 0.6f, 0.55f, 0.5f, 0);
    lb_cyl(0, -200.8f, 0, 5.5f, 0.8f, 16, TEX_STONE, TEX_ROCK, 0.6f, 0.55f, 0.5f, 0);
    for (k = 0; k < 2; k++) {
        float y = k ? -200 : -100, a = (-y) / 290.0f * turn;
        vec3 p0 = cave_polar(a, 33.2f, y), p1 = cave_polar(a, 5.3f, y), d = v3sub(p1, p0), side;
        float len = v3len(d);
        int n = (int)(len / 1.1f);
        d = v3scale(d, 1.0f / len);
        side = v3(d.z, 0, -d.x);
        for (i = 0; i < n; i++) {   /* planks with little gaps, sagging in the middle */
            float t = (i + 0.5f) / n, sag = sinf(t * PI_F) * 1.2f;
            vec3 c = v3add(v3lerp(p0, p1, t), v3(0, -sag - 0.1f, 0));
            lb_obox(c, v3(1.2f, 0.1f, 0.45f), atan2f(d.x, d.z), 0, TEX_PLANK, TEX_WOOD, 0.75f, 0.55f, 0.35f, 0);
        }
        obj_spawn(OT_REDCOIN, v3add(v3lerp(p0, p1, 0.5f), v3(0, -0.6f, 0)), 0, 0);
        coin_line(v3add(v3lerp(p0, p1, 0.2f), v3(0, 0.1f, 0)), v3add(v3lerp(p0, p1, 0.8f), v3(0, 0.1f, 0)), 4, 0);
        (void)side;
    }
    obj_spawn(OT_REDCOIN, v3(0, -96.4f, 0), 0, 0);
    /* the waterfall pouring down the wall into the dark */
    lb_alpha_mode = 1;
    {
        float a = 2.2f, w = 0.06f;
        for (i = 0; i < 40; i++) {
            float y0 = -30 - i * 6.5f, y1 = y0 - 6.5f;
            vec3 p0 = cave_polar(a - w, 39.2f, y0), p1 = cave_polar(a + w, 39.2f, y0), p2 = cave_polar(a + w, 39.2f, y1), p3 = cave_polar(a - w, 39.2f, y1);
            lb_quad(TEX_WATER, p0, p1, p2, p3, 0.75f, 0.9f, 1.0f, 0, 0);
            lb_quad(TEX_WATER, p1, p0, p3, p2, 0.75f, 0.9f, 1.0f, 0, 0);
        }
    }
    lb_alpha_mode = 0;
    /* star 2: the mushroom chimney, hanging in the shaft beside the ledge deep down (below the mine track) */
    {
        float a = 11.0f, y = cave_walk_y(11.0f);
        cave_box(a, 26, y - 0.5f, 4.5f, 0.5f, 7.0f, TEX_STONE, TEX_ROCK, 0.6f, 0.55f, 0.5f);          /* floor, from the ledge inward */
        cave_box(a, 22.0f, y + 21, 4.5f, 21, 0.5f, TEX_ROCK, TEX_ROCK, 0.5f, 0.45f, 0.42f);          /* inner wall */
        cave_box(a, 30.5f, y + 23, 4.5f, 19, 0.5f, TEX_ROCK, TEX_ROCK, 0.5f, 0.45f, 0.42f);          /* outer wall, door below */
        lb_obox(v3add(cave_polar(a, 26.25f, y + 21), v3scale(v3(cosf(a), 0, -sinf(a)), 4.5f)), v3(0.5f, 21, 4.75f), a, 0, TEX_ROCK, TEX_ROCK, 0.5f, 0.45f, 0.42f, 0);
        lb_obox(v3add(cave_polar(a, 26.25f, y + 21), v3scale(v3(cosf(a), 0, -sinf(a)), -4.5f)), v3(0.5f, 21, 4.75f), a, 0, TEX_ROCK, TEX_ROCK, 0.5f, 0.45f, 0.42f, 0);
        o = obj_spawn(OT_MUSHROOM, cave_polar(a, 27.4f, y), 0, 0);   /* under the outer shelves, clear of the first one */
        for (k = 1; k <= 5; k++) {
            float r = (k & 1) ? 25.0f : 27.4f, sy = y + k * 7;
            cave_box(a, r + ((k & 1) ? -0.6f : 0.6f), sy - 0.4f, 3.8f, 0.4f, 1.8f, TEX_STONE, TEX_ROCK, 0.6f, 0.55f, 0.5f);
            if (k < 5) obj_spawn(OT_MUSHROOM, cave_polar(a, r, sy), 0, 0);
        }
        obj_spawn(OT_STAR, cave_polar(a, 25.0f, y + 35.5f), 0, 2);
        obj_spawn(OT_REDCOIN, cave_polar(a, 26, y + 18), 0, 0);
        (void)o;
    }
    /* star 1: the mine cart track (its own steeper spiral) with a station at each end */
    for (k = 0; k < 300; k++) {
        vec3 a = cave_track_point(k / 300.0f), b = cave_track_point((k + 1) / 300.0f), d = v3sub(b, a);
        float len = v3len(d), yaw = atan2f(d.x, d.z), pitch = asinf(d.y / len);
        vec3 side = v3(cosf(yaw), 0, -sinf(yaw)), c = v3lerp(a, b, 0.5f);
        lb_obox(v3add(c, v3(0, -0.15f, 0)), v3(1.1f, 0.08f, 0.18f), yaw, 0, TEX_WOOD, TEX_WOOD, 0.45f, 0.32f, 0.2f, 0);
        lb_obox(v3add(c, v3scale(side, 0.62f)), v3(0.06f, 0.06f, len / 2 + 0.02f), yaw, pitch, TEX_METAL, TEX_METAL, 0.6f, 0.6f, 0.62f, 0);
        lb_obox(v3add(c, v3scale(side, -0.62f)), v3(0.06f, 0.06f, len / 2 + 0.02f), yaw, pitch, TEX_METAL, TEX_METAL, 0.6f, 0.6f, 0.62f, 0);
        if (k % 12 == 0 && k > 6 && k < 294) lb_box(c.x - 0.15f, c.y - 9, c.z - 0.15f, c.x + 0.15f, c.y - 0.2f, c.z + 0.15f, TEX_WOOD, TEX_WOOD, 0.4f, 0.3f, 0.2f, 0);
        if (k % 10 == 5) obj_spawn(OT_COIN, v3add(c, v3(0, 1.0f, 0)), 0, 0);
    }
    cave_box(0.15f, 30, -4.6f, 3.0f, 0.5f, 7, TEX_PLANK, TEX_WOOD, 0.75f, 0.55f, 0.35f);                    /* top station */
    cave_box(CAVE_TRACK_ANGLE - 0.15f, 30.5f, -200.6f, 4.5f, 0.5f, 10, TEX_PLANK, TEX_WOOD, 0.75f, 0.55f, 0.35f);   /* bottom station */
    o = obj_spawn(OT_MINECART, cave_track_point(0), 0, 0);
    obj_spawn(OT_STAR, cave_polar(CAVE_TRACK_ANGLE - 0.1f, 33, -198.6f), 0, 1);
    /* star 3: five dark crystals on the ledge; punch them or stand on them to light them up */
    {
        static const float ca[5] = { 1.2f, 3.4f, 5.6f, 8.6f, 11.2f };
        for (k = 0; k < 5; k++) {
            o = obj_spawn(OT_CAVECRYSTAL, cave_polar(ca[k], 38.3f, cave_walk_y(ca[k])), ca[k], k);
            if (o) o->yaw = ca[k];
        }
    }
    /* falling stalactites above the ledge, each under a rock overhang */
    {
        static const float sa[7] = { 0.8f, 2.6f, 4.4f, 6.0f, 7.2f, 9.4f, 10.6f };
        for (k = 0; k < 7; k++) {
            float y = cave_walk_y(sa[k]);
            cave_box(sa[k], 38.5f, y + 12.5f, 2.5f, 0.8f, 2.6f, TEX_ROCK, TEX_ROCK, 0.5f, 0.45f, 0.42f);
            o = obj_spawn(OT_STALACTITE, cave_polar(sa[k], 36.6f, y + 11.7f), 0, k);
            if (o) o->home = o->pos;
        }
    }
    /* fish along the ledge, and red fish */
    for (k = 0; k < 64; k++) {
        float a = turn * (k + 0.5f) / 64;
        if (fabsf(a - 37.5f * turn / 160) < 0.15f || fabsf(a - 101.5f * turn / 160) < 0.15f) continue;
        obj_spawn(OT_COIN, cave_polar(a, 36.5f, cave_walk_y(a) + 0.5f), 0, 0);
    }
    obj_spawn(OT_REDCOIN, cave_polar(2.2f, 38.8f, cave_walk_y(2.2f) + 0.6f), 0, 0);   /* behind the waterfall */
    obj_spawn(OT_REDCOIN, cave_polar(38.0f * turn / 160, 36.5f, cave_walk_y(38.0f * turn / 160) + 1.0f), 0, 0);   /* over a gap */
    obj_spawn(OT_REDCOIN, cave_polar(102.0f * turn / 160, 36.5f, cave_walk_y(102.0f * turn / 160) + 1.0f), 0, 0);
    obj_spawn(OT_REDCOIN, v3(sinf(1.1f) * 13, CAVE_BOTTOM + 1.2f, cosf(1.1f) * 13), 0, 0);
    obj_spawn(OT_STARMARKER, v3(0, -93, 0), 0, 4);
    /* a little miners' hut and a warning sign at the surface */
    lb_box(-18, -0.2f, 52, -10, 4, 58, TEX_WOOD, TEX_WOOD, 0.6f, 0.45f, 0.3f, 0);
    lb_ramp(-18.5f, 51.5f, -9.5f, 55, 4, 5.5f, 1, 0.2f, TEX_ROOF, TEX_ROOF, 0.85f, 0.9f, 1.0f, 0);
    lb_ramp(-18.5f, 55, -9.5f, 58.5f, 5.5f, 4, 1, 0.2f, TEX_ROOF, TEX_ROOF, 0.85f, 0.9f, 1.0f, 0);
    finish_geometry();

    lv.spawn[0] = v3(-6, 0, 54); lv.spawn_yaw[0] = PI_F;
    coin_ring(v3(0, 0.6f, 0), 52, 12, 1);
    sign(sinf(0.32f) * 34, cosf(0.32f) * 34, -2, 0.32f + PI_F / 2, "MINE CART: Hop in for a ride down to the old\nstation at 200 m. Keep your flippers inside!");
    sign(-3, 46, 2, PI_F, "CORE DUMP CAVERNS - 290 m DEEP\nThe ledge spirals down to the magma at the\n"
                          "bottom. Mind the loose rock overhead and the\ngaps where the ledge has crumbled away.");
    tree(30, 60, 3, 1); tree(-34, 58, 3, 1); tree(55, 10, 3, 1); tree(-58, -20, 3, 1); tree(20, -62, 3, 1);
}

/* ================================================================
 * TTY WORLD 1-1 - a side-scrolling course seen from a fixed side camera
 * ================================================================ */
#define ST SIDE_TILE

/* ground: chunky tiles with a layer of snow on top */
static void side_ground(float x0, float x1, float y)
{
    lb_box(x0, y - 8, -3.2f, x1, y, 3.2f, TEX_SNOW, TEX_GROUNDTILE, 1, 1, 1, 0);
    lb_box(x0, y - 0.3f, -3.25f, x1, y + 0.02f, 3.25f, TEX_SNOW, TEX_SNOW, 1, 1, 1, 0);
}

/* a solid block of tiles (stairs, platforms) */
static void side_solid(float x0, float y0, float x1, float y1, float r, float g, float b)
{
    lb_box(x0, y0, -ST / 2, x1, y1, ST / 2, TEX_GROUNDTILE, TEX_GROUNDTILE, r, g, b, 0);
}

/* a green pipe; returns the height of its top */
static float side_pipe(float x, float y, float h)
{
    lb_cyl(x, y, 0, 1.3f, h - 0.7f, 16, TEX_NONE, TEX_NONE, 0.2f, 0.7f, 0.25f, 0);
    lb_cyl(x, y + h - 0.7f, 0, 1.6f, 0.7f, 16, TEX_NONE, TEX_NONE, 0.25f, 0.82f, 0.3f, 0);
    lb_cyl(x, y + h - 0.02f, 0, 1.2f, 0.04f, 16, TEX_NONE, TEX_NONE, 0.05f, 0.15f, 0.05f, 0);
    lb_box(x - 1.1f, y + 0.3f, 1.25f, x - 0.7f, y + h - 0.9f, 1.32f, TEX_NONE, TEX_NONE, 0.6f, 0.95f, 0.6f, 0);   /* highlight */
    return y + h;
}

static Obj *side_block(int type, float tx, float ty, int param)
{
    /* blocks sit on the tile grid: (tx, ty) = tile column / row, row 0 rests on the ground */
    return obj_spawn(type, v3(tx * ST + ST / 2, ty * ST + ST / 2, 0), 0, param);
}

static void side_walker(float x, float y, float dir)
{
    Obj *o = obj_spawn(OT_WALKER, v3(x, y, 0), 0, 0);
    if (o) o->f[0] = dir;
}

static void build_side(void)
{
    Obj *o;
    int k;
    lv.name = "TTY WORLD 1-1";
    lv.music = MUS_SIDE;
    lv.course = CRS_SIDE;
    lv.side_view = 1;
    set_sky(0.35f, 0.6f, 1.0f, 0.65f, 0.82f, 1.0f, 120, 380);
    lv.ambient = 0.62f; lv.diffuse = 0.6f;
    lv.death_y = -60;
    g_coin_star_idx = 5;
    lb_begin();
    /* the ground, in pieces with pits between them */
    side_ground(-16, 72, 0);
    side_ground(78, 104.8f, 0);
    side_ground(108, 150, 0);
    side_ground(162, 238, 0);
    side_ground(244, 368, 0);
    side_solid(-16, 0, -12.8f, 30, 0.8f, 0.8f, 0.85f);    /* walls at both ends of the course */
    side_solid(364.8f, 0, 368, 30, 0.8f, 0.8f, 0.85f);
    death_floor(-10, 72, -4, 78, 4);
    death_floor(-10, 104.8f, -4, 108, 4);
    death_floor(-10, 150, -4, 162, 4);
    death_floor(-10, 238, -4, 244, 4);
    /* pipes (the third one is a warp pipe down to the bonus room) */
    side_pipe(36, 0, 2.4f);
    side_pipe(46, 0, 3.6f);
    side_pipe(56, 0, 4.8f);
    side_pipe(66, 0, 4.8f);
    /* the twin staircases with a pit between them */
    for (k = 0; k < 4; k++) {
        side_solid(98.4f + k * ST, 0, 104.8f, (k + 1) * ST, 0.9f, 0.85f, 0.85f);
        side_solid(108, 0, 114.4f - k * ST, (k + 1) * ST, 0.9f, 0.85f, 0.85f);
    }
    /* the fire bar's block */
    side_solid(120 - ST / 2, 3.6f, 120 + ST / 2, 3.6f + ST, 0.6f, 0.4f, 0.25f);
    /* floating platforms climbing up over the cannon field */
    side_solid(196, 1.6f, 200.8f, 2.4f, 0.95f, 0.9f, 0.9f);
    side_solid(203.2f, 4.0f, 208, 4.8f, 0.95f, 0.9f, 0.9f);
    side_solid(210.4f, 6.4f, 216.8f, 7.2f, 0.95f, 0.9f, 0.9f);
    side_solid(220, 4.0f, 224.8f, 4.8f, 0.95f, 0.9f, 0.9f);
    /* the exit pipe from the bonus room */
    side_pipe(260, 0, 4.8f);
    /* the big final staircase */
    for (k = 0; k < 8; k++) side_solid(300 + k * ST, 0, 316, (k + 1) * ST, 0.9f, 0.85f, 0.85f);   /* the top is a 3-tile run-up */
    side_solid(317.8f, 0, 319.4f, ST, 0.6f, 0.4f, 0.25f);   /* the flagpole's base block */
    /* the little castle at the end */
    lb_box(332, 0, -3, 348, 6.4f, -0.8f, TEX_BRICK, TEX_BRICK, 0.85f, 0.6f, 0.5f, 0);
    lb_box(336, 6.4f, -3, 344, 10.4f, -0.8f, TEX_BRICK, TEX_BRICK, 0.85f, 0.6f, 0.5f, 0);
    for (k = 0; k < 5; k++) lb_box(332 + k * 3.6f, 6.4f, -3, 333.6f + k * 3.6f, 7.6f, -0.8f, TEX_BRICK, TEX_BRICK, 0.85f, 0.6f, 0.5f, 0);
    for (k = 0; k < 3; k++) lb_box(336 + k * 3.2f, 10.4f, -3, 337.6f + k * 3.2f, 11.6f, -0.8f, TEX_BRICK, TEX_BRICK, 0.85f, 0.6f, 0.5f, 0);
    lb_quad(TEX_NONE, v3(338.4f, 0, -0.78f), v3(341.6f, 0, -0.78f), v3(341.6f, 3.6f, -0.78f), v3(338.4f, 3.6f, -0.78f), 0.05f, 0.05f, 0.08f, 0, 0);
    lb_quad(TEX_NONE, v3(339, 7.4f, -0.78f), v3(341, 7.4f, -0.78f), v3(341, 9.2f, -0.78f), v3(339, 9.2f, -0.78f), 0.05f, 0.05f, 0.08f, 0, 0);
    /* the bonus room underground (entered through the warp pipe) */
    lb_box(248, -46, -3.2f, 302, -40, 3.2f, TEX_GROUNDTILE, TEX_GROUNDTILE, 0.45f, 0.55f, 0.95f, 0);
    lb_box(248, -28, -3.2f, 302, -24, 3.2f, TEX_BRICK, TEX_BRICK, 0.4f, 0.5f, 0.9f, 0);
    lb_box(244.8f, -46, -3.2f, 248, -24, 3.2f, TEX_BRICK, TEX_BRICK, 0.4f, 0.5f, 0.9f, 0);
    lb_box(302, -46, -3.2f, 305.2f, -24, 3.2f, TEX_BRICK, TEX_BRICK, 0.4f, 0.5f, 0.9f, 0);
    for (k = 0; k < 3; k++) side_solid(275.2f + k * ST, -40, 281.6f, -40 + (k + 1) * ST, 0.5f, 0.6f, 1.0f);   /* a little stair-tower */
    side_pipe(296, -40, 3.2f);
    finish_geometry();

    lv.spawn[0] = v3(-6, 0, 0); lv.spawn_yaw[0] = PI_F / 2;

    /* ? blocks and bricks: (column, row) on the tile grid; ? block row 2 has its bottom 3.2 above the ground */
    side_block(OT_QBLOCK, 7.5f, 2.25f, 0);
    side_block(OT_BRICK, 13, 2.25f, 0);
    side_block(OT_QBLOCK, 14, 2.25f, 0);
    side_block(OT_BRICK, 15, 2.25f, 0);
    side_block(OT_QBLOCK, 16, 2.25f, 2);   /* 1-UP */
    side_block(OT_BRICK, 17, 2.25f, 0);
    side_block(OT_QBLOCK, 15, 5.25f, 4);   /* red fish */
    o = side_block(OT_QBLOCK, 52.5f, 2.25f, 2);   /* a hidden 1-UP block */
    if (o) o->i[0] = 1;
    side_block(OT_QBLOCK, 82.5f, 2.25f, 1);   /* ten fish */
    for (k = 0; k < 8; k++) side_block(OT_BRICK, 79.5f + k, 5.25f, k == 4 ? 3 : 0);
    side_block(OT_QBLOCK, 155, 2.25f, 0);
    side_block(OT_QBLOCK, 156, 2.25f, 0);
    side_block(OT_QBLOCK, 157, 2.25f, 0);
    for (k = 0; k < 4; k++) side_block(OT_BRICK, 165 + k, 2.25f, 0);
    o = side_block(OT_QBLOCK, 175, 2.25f, 3);   /* star 3: hidden in an invisible block */
    if (o) { o->i[0] = 1; o->param2 = 3; }
    side_block(OT_QBLOCK, 173, 2.25f, 0);
    side_block(OT_QBLOCK, 177, 2.25f, 0);
    /* walkers */
    side_walker(30, 0, -1); side_walker(51, 0, -1); side_walker(61, 0, -1); side_walker(62.5f, 0, -1);
    side_walker(90, 0, -1); side_walker(94, 0, 1); side_walker(128, 0, -1); side_walker(132, 0, -1);
    side_walker(178, 0, -1); side_walker(205, 0, -1); side_walker(230, 0, -1); side_walker(270, 0, -1);
    side_walker(276, 0, -1); side_walker(290, 0, -1);
    /* the fire bar */
    o = obj_spawn(OT_FLAMEBAR, v3(120, 3.6f + ST / 2, 0), 0, 6);
    if (o) { o->f[0] = 2.0f; o->f[1] = 1; }
    /* the moving platform over the wide pit */
    o = obj_spawn(OT_PLATFORM, v3(152.4f, -0.3f, 0), 0, 0);
    if (o) { obj_set_box(o, 2.4f, 0.3f, 1.2f, TEX_METAL, TEX_METAL, 0.9f, 0.75f, 0.3f); o->f[0] = 7.2f; o->f[3] = 5; }
    /* cannons */
    obj_spawn(OT_SIDECANNON, v3(172, 0, 0), 0, 0);
    obj_spawn(OT_SIDECANNON, v3(186, 0, 0), 0, 1);
    /* warp pipes: down into the bonus room and back up */
    o = obj_spawn(OT_PIPEWARP, v3(56, 4.8f, 0), 0, 0);
    if (o) { o->f[0] = 252; o->f[1] = -39.95f; o->f[2] = 0; }
    o = obj_spawn(OT_PIPEWARP, v3(296, -36.8f, 0), 0, 0);
    if (o) { o->f[0] = 260; o->f[1] = 4.85f; o->f[2] = 0; }
    /* star 2: on the brick tower in the bonus room */
    obj_spawn(OT_STAR, v3(280, -34.4f, 0), 0, 2);
    /* the flagpole (stars 1 at its top) and the goal star at the castle (star 0) */
    o = obj_spawn(OT_FLAGPOLE, v3(318.6f, ST, 0), 0, 0);
    if (o) o->height = 15.2f;
    obj_spawn(OT_STAR, v3(340, 0.6f, 0), 0, 0);
    /* red fish (one more comes out of a ? block) */
    obj_spawn(OT_REDCOIN, v3(75, 3.5f, 0), 0, 0);
    obj_spawn(OT_REDCOIN, v3(106.4f, 7.6f, 0), 0, 0);
    obj_spawn(OT_REDCOIN, v3(130.4f, 7.2f, 0), 0, 0);
    obj_spawn(OT_REDCOIN, v3(156, 2.6f, 0), 0, 0);
    obj_spawn(OT_REDCOIN, v3(213.6f, 8.0f, 0), 0, 0);
    obj_spawn(OT_REDCOIN, v3(186, 2.7f, 0), 0, 0);
    obj_spawn(OT_REDCOIN, v3(266, -39.3f, 0), 0, 0);
    obj_spawn(OT_STARMARKER, v3(46, 9.5f, 0), 0, 4);
    /* fish */
    coin_line(v3(2, 0.6f, 0), v3(9, 0.6f, 0), 4, 0);
    coin_line(v3(20.8f, 6.4f, 0), v3(28, 6.4f, 0), 4, 0);
    coin_line(v3(40, 4.2f, 0), v3(42, 4.2f, 0), 2, 0);
    for (k = 0; k < 5; k++) obj_spawn(OT_COIN, v3(73 + k * 1.2f, 2.5f + sinf(k / 4.0f * PI_F) * 2.0f, 0), 0, 0);
    coin_line(v3(127.2f, 6.8f, 0), v3(138.4f, 6.8f, 0), 6, 0);   /* under the high bricks: jump from the ? block */
    coin_line(v3(84, 0.6f, 0), v3(96, 0.6f, 0), 6, 0);
    for (k = 0; k < 4; k++) obj_spawn(OT_COIN, v3(99.2f + k * ST, (k + 1) * ST + 0.6f, 0), 0, 0);
    coin_line(v3(122, 0.6f, 0), v3(146, 0.6f, 0), 8, 0);
    coin_line(v3(152, 2.0f, 0), v3(160, 2.0f, 0), 5, 0);
    coin_line(v3(165, 0.6f, 0), v3(195, 0.6f, 0), 10, 0);
    coin_line(v3(197, 3.0f, 0), v3(200, 3.0f, 0), 2, 0);
    coin_line(v3(204, 5.4f, 0), v3(207, 5.4f, 0), 2, 0);
    coin_line(v3(220.8f, 5.4f, 0), v3(224, 5.4f, 0), 2, 0);
    for (k = 0; k < 5; k++) obj_spawn(OT_COIN, v3(239 + k * 1.2f, 2.5f + sinf(k / 4.0f * PI_F) * 2.0f, 0), 0, 0);
    coin_line(v3(248, 0.6f, 0), v3(256, 0.6f, 0), 4, 0);
    coin_line(v3(264, 0.6f, 0), v3(296, 0.6f, 0), 10, 0);
    for (k = 0; k < 3; k++) coin_line(v3(254, -38.8f + k * 2.0f, 0), v3(274, -38.8f + k * 2.0f, 0), 8, 0);
    coin_line(v3(284, -39.4f, 0), v3(292, -39.4f, 0), 4, 0);
    sign(2, -1.2f, 2, 0, "TTY WORLD 1-1\nRun to the flagpole at the end of the course!\n"
                            "Left / right to move, A to jump. Bump the ? blocks\nfrom below. Crouch (Z) on a pipe to go down it.");
    sign(170, -1.2f, 2, 0, "Psst... not every block can be seen. Some only\nshow up when you bump into them from below!");
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
    case LV_NANO: build_nano(); break;
    case LV_HUGE: build_huge(); break;
    case LV_CITY: build_city(); break;
    case LV_CAVE: build_cave(); break;
    case LV_SIDE: build_side(); break;
    }
}

float terrain_h(float x, float z) { return peak_h(x, z); }
