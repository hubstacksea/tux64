/* models.c - procedurally built character / object models */
#include "game.h"
#include <string.h>

GLuint mdl[MDL_COUNT];

/* Tux parts per variant */
static GLuint tux_torso[TUXV_COUNT], tux_head[TUXV_COUNT], tux_flipper[TUXV_COUNT], tux_foot, tux_cap;

static Mesh M;
static void begin(void) { mesh_init(&M); mb_begin(&M); }
static GLuint end(void) { GLuint d; mb_end(); d = mesh_compile(&M); mesh_free(&M); return d; }

static void build_tux(void)
{
    int v;
    for (v = 0; v < TUXV_COUNT; v++) {
        float br = 0.1f, bg = 0.1f, bb = 0.13f;   /* body */
        float wr = 0.97f, wg = 0.97f, wb = 0.95f; /* belly */
        if (v == TUXV_BABY) { br = 0.5f; bg = 0.5f; bb = 0.56f; wr = 0.85f; wg = 0.85f; wb = 0.88f; }
        if (v == TUXV_NPC) { br = 0.12f; bg = 0.14f; bb = 0.25f; }
        /* torso */
        begin();
        mb_color(br, bg, bb);
        mb_ellipsoid(0, 0.68f, 0, 0.48f, 0.6f, 0.42f, 18);
        mb_color(wr, wg, wb);
        mb_ellipsoid(0, 0.62f, 0.1f, 0.41f, 0.5f, 0.36f, 18);
        if (v == TUXV_NPC) {           /* red scarf */
            mb_color(0.85f, 0.15f, 0.15f);
            mb_push(); mb_translate(0, 1.08f, 0); mb_torus(0.33f, 0.08f, 14, 6); mb_pop();
        }
        if (v == TUXV_MOTHER) {        /* pink bow */
            mb_color(1.0f, 0.45f, 0.65f);
            mb_ellipsoid(0.12f, 1.12f, 0.33f, 0.1f, 0.07f, 0.05f, 8);
            mb_ellipsoid(-0.12f, 1.12f, 0.33f, 0.1f, 0.07f, 0.05f, 8);
        }
        tux_torso[v] = end();
        /* head (origin at neck) */
        begin();
        mb_color(br, bg, bb);
        mb_ellipsoid(0, 0.22f, 0.02f, 0.34f, 0.33f, 0.33f, 16);
        if (v == TUXV_BABY) {          /* fluffy tuft */
            mb_ellipsoid(0, 0.55f, 0, 0.08f, 0.1f, 0.08f, 8);
        }
        mb_color(1, 1, 1);
        mb_ellipsoid(0.12f, 0.27f, 0.27f, 0.1f, 0.13f, 0.06f, 10);
        mb_ellipsoid(-0.12f, 0.27f, 0.27f, 0.1f, 0.13f, 0.06f, 10);
        mb_color(0.05f, 0.05f, 0.07f);
        mb_ellipsoid(0.105f, 0.255f, 0.325f, 0.045f, 0.06f, 0.025f, 8);
        mb_ellipsoid(-0.105f, 0.255f, 0.325f, 0.045f, 0.06f, 0.025f, 8);
        mb_color(1.0f, 0.68f, 0.08f);
        mb_ellipsoid(0, 0.12f, 0.31f, 0.15f, 0.065f, 0.17f, 12);
        mb_color(0.9f, 0.5f, 0.05f);
        mb_ellipsoid(0, 0.065f, 0.28f, 0.12f, 0.045f, 0.13f, 10);
        if (v == TUXV_MOTHER) {
            mb_color(1.0f, 0.45f, 0.65f);
            mb_ellipsoid(0, 0.56f, 0, 0.12f, 0.06f, 0.12f, 8);
        }
        tux_head[v] = end();
        /* flipper (origin at shoulder, hanging down, left side +x) */
        begin();
        mb_color(br, bg, bb);
        mb_ellipsoid(0.04f, -0.3f, 0, 0.08f, 0.36f, 0.2f, 12);
        tux_flipper[v] = end();
    }
    begin();
    mb_color(1.0f, 0.65f, 0.08f);
    mb_ellipsoid(0, -0.11f, 0.14f, 0.17f, 0.07f, 0.27f, 12);
    tux_foot = end();
    /* feather cap: red cap with white wings (origin at neck like the head) */
    begin();
    mb_color(0.9f, 0.12f, 0.12f);
    mb_push(); mb_translate(0, 0.36f, 0.0f); mb_scale(0.31f, 0.2f, 0.31f); mb_hemisphere(14, 5); mb_pop();
    mb_ellipsoid(0, 0.37f, 0.22f, 0.2f, 0.03f, 0.14f, 10);
    mb_color(1, 1, 1);
    {
        int side;
        for (side = -1; side <= 1; side += 2) {
            float x = side * 0.3f;
            vec3 a = v3(x, 0.42f, 0.05f), b = v3(x + side * 0.32f, 0.72f, -0.15f), c = v3(x + side * 0.12f, 0.45f, -0.25f);
            vec3 d = v3(x + side * 0.3f, 0.58f, -0.3f);
            mb_tri(a, b, c); mb_tri(a, c, b);
            mb_tri(c, b, d); mb_tri(c, d, b);
        }
    }
    tux_cap = end();
}

void draw_tux_model(vec3 pos, float yaw, const TuxPose *tp, float s, int v)
{
    glPushMatrix();
    glTranslatef(pos.x, pos.y + tp->lift, pos.z);
    glRotatef(DEG(yaw + tp->spin), 0, 1, 0);
    glScalef(s, s, s);
    if (tp->flip != 0) {
        glTranslatef(0, 0.8f, 0);
        glRotatef(DEG(tp->flip), 1, 0, 0);
        glTranslatef(0, -0.8f, 0);
    }
    glTranslatef(0, 0.7f, 0);
    glRotatef(DEG(tp->body_pitch), 1, 0, 0);
    glRotatef(DEG(tp->body_roll), 0, 0, 1);
    glTranslatef(0, -0.7f, 0);
    if (tp->body_squash != 0) glScalef(1 - tp->body_squash * 0.5f, 1 + tp->body_squash, 1 - tp->body_squash * 0.5f);
    glCallList(tux_torso[v]);
    /* head */
    glPushMatrix();
    glTranslatef(0, 1.1f, 0);
    glRotatef(DEG(tp->head_yaw), 0, 1, 0);
    glRotatef(DEG(tp->head_pitch), 1, 0, 0);
    glCallList(tux_head[v]);
    if (tp->cap) glCallList(tux_cap);
    glPopMatrix();
    /* flippers */
    glPushMatrix();
    glTranslatef(0.42f, 1.0f, 0);
    glRotatef(DEG(tp->flipL_z), 0, 0, 1);
    glRotatef(DEG(tp->flipL_x), 1, 0, 0);
    glCallList(tux_flipper[v]);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-0.42f, 1.0f, 0);
    glRotatef(-DEG(tp->flipR_z), 0, 0, 1);
    glRotatef(DEG(tp->flipR_x), 1, 0, 0);
    glScalef(-1, 1, 1);
    glFrontFace(GL_CW);
    glCallList(tux_flipper[v]);
    glFrontFace(GL_CCW);
    glPopMatrix();
    /* feet */
    glPushMatrix();
    glTranslatef(0.2f, 0.17f + 0.1f * fabsf(tp->footL), 0.02f + tp->footL * 0.3f);
    glRotatef(-DEG(tp->footL) * 0.5f, 1, 0, 0);
    glCallList(tux_foot);
    glPopMatrix();
    glPushMatrix();
    glTranslatef(-0.2f, 0.17f + 0.1f * fabsf(tp->footR), 0.02f + tp->footR * 0.3f);
    glRotatef(-DEG(tp->footR) * 0.5f, 1, 0, 0);
    glCallList(tux_foot);
    glPopMatrix();
    glPopMatrix();
}

/* ---------------- generic helpers for objects with procedural sizes ---------------- */
GLuint model_box(float sx, float sy, float sz, int tt, int ts, float r, float g, float b)
{
    vec3 p[8];
    int i;
    begin();
    for (i = 0; i < 8; i++) p[i] = v3((i & 1) ? sx : -sx, (i & 2) ? sy : -sy, (i & 4) ? sz : -sz);
    mb_uvscale(g_tex_scale[tt]);
    mb_tex(tt); mb_color(r, g, b);
    mb_quad(p[2], p[6], p[7], p[3]);
    mb_tex(ts); mb_uvscale(g_tex_scale[ts]); mb_color(r * 0.85f, g * 0.85f, b * 0.85f);
    mb_quad(p[0], p[1], p[5], p[4]);
    mb_quad(p[1], p[3], p[7], p[5]);
    mb_quad(p[0], p[4], p[6], p[2]);
    mb_quad(p[4], p[5], p[7], p[6]);
    mb_quad(p[0], p[2], p[3], p[1]);
    return end();
}

GLuint model_cyl(float r, float h, int tt, int ts, float cr, float cg, float cb)
{
    int i, seg = 20;
    begin();
    for (i = 0; i < seg; i++) {
        float a0 = TAU_F * i / seg, a1 = TAU_F * (i + 1) / seg;
        vec3 p0 = v3(sinf(a0) * r, 0, cosf(a0) * r), p1 = v3(sinf(a1) * r, 0, cosf(a1) * r);
        vec3 q0 = v3(p0.x, h, p0.z), q1 = v3(p1.x, h, p1.z);
        mb_tex(ts); mb_uvscale(g_tex_scale[ts]); mb_color(cr * 0.85f, cg * 0.85f, cb * 0.85f);
        mb_quad(p0, p1, q1, q0);
        mb_tri(v3(0, 0, 0), p1, p0);
        mb_tex(tt); mb_uvscale(g_tex_scale[tt]); mb_color(cr, cg, cb);
        mb_tri(v3(0, h, 0), q0, q1);
    }
    return end();
}

static void eye_pair(float x, float y, float z, float s, int angry)
{
    mb_color(1, 1, 1);
    mb_ellipsoid(x, y, z, 0.09f * s, 0.13f * s, 0.05f * s, 10);
    mb_ellipsoid(-x, y, z, 0.09f * s, 0.13f * s, 0.05f * s, 10);
    mb_color(0.05f, 0.05f, 0.05f);
    mb_ellipsoid(x * 0.9f, y - 0.02f * s, z + 0.04f * s, 0.045f * s, 0.07f * s, 0.03f * s, 8);
    mb_ellipsoid(-x * 0.9f, y - 0.02f * s, z + 0.04f * s, 0.045f * s, 0.07f * s, 0.03f * s, 8);
    if (angry) {
        mb_push(); mb_translate(x, y + 0.15f * s, z + 0.02f * s); mb_rotate(-25, 0, 0, 1); mb_scale(0.12f * s, 0.03f * s, 0.03f * s); mb_box(); mb_pop();
        mb_push(); mb_translate(-x, y + 0.15f * s, z + 0.02f * s); mb_rotate(25, 0, 0, 1); mb_scale(0.12f * s, 0.03f * s, 0.03f * s); mb_box(); mb_pop();
    }
}

static void spikes(float r, int n, float len, float cr, float cg, float cb)
{
    int i, j;
    mb_color(cr, cg, cb);
    for (j = 0; j < n; j++) {
        float ph = -PI_F / 2 + PI_F * (j + 0.5f) / n;
        int cnt = (int)(n * 2 * cosf(ph)) + 1;
        for (i = 0; i < cnt; i++) {
            float th = TAU_F * i / cnt + j * 0.5f;
            vec3 d = v3(cosf(ph) * sinf(th), sinf(ph), cosf(ph) * cosf(th));
            vec3 up = v3(0, 1, 0);
            vec3 ax = v3cross(up, d);
            float ang = DEG(acosf(clampf(d.y, -1, 1)));
            mb_push();
            mb_translate(d.x * r * 0.9f, d.y * r * 0.9f, d.z * r * 0.9f);
            if (v3len(ax) > 1e-4f) mb_rotate(ang, ax.x, ax.y, ax.z);
            else if (d.y < 0) mb_rotate(180, 1, 0, 0);
            mb_scale(r * 0.15f, len, r * 0.15f);
            mb_cone(6);
            mb_pop();
        }
    }
}

static void fish(float r, float g, float b, float alpha)
{
    mb_color4(r, g, b, alpha);
    mb_ellipsoid(0, 0, 0, 0.45f, 0.3f, 0.09f, 14);
    mb_tri(v3(0.35f, 0, 0.0f), v3(0.75f, 0.3f, 0), v3(0.75f, -0.3f, 0));
    mb_tri(v3(0.35f, 0, 0.0f), v3(0.75f, -0.3f, 0), v3(0.75f, 0.3f, 0));
    mb_color4(r * 0.8f, g * 0.8f, b * 0.8f, alpha);
    mb_tri(v3(-0.05f, 0.25f, 0), v3(0.2f, 0.42f, 0), v3(0.25f, 0.22f, 0));
    mb_tri(v3(-0.05f, 0.25f, 0), v3(0.25f, 0.22f, 0), v3(0.2f, 0.42f, 0));
    mb_color4(0.05f, 0.05f, 0.05f, alpha);
    mb_ellipsoid(-0.28f, 0.06f, 0.07f, 0.05f, 0.05f, 0.03f, 6);
    mb_ellipsoid(-0.28f, 0.06f, -0.07f, 0.05f, 0.05f, 0.03f, 6);
}

void models_init(void)
{
    int i;
    build_tux();

    begin(); fish(1.0f, 0.82f, 0.15f, 1); mdl[MDL_COIN] = end();
    begin(); fish(0.95f, 0.15f, 0.12f, 1); mdl[MDL_COIN_RED] = end();
    begin(); fish(0.25f, 0.45f, 1.0f, 1); mdl[MDL_COIN_BLUE] = end();
    begin(); fish(0.2f, 0.9f, 0.3f, 1); mdl[MDL_FISH_GREEN] = end();
    begin(); fish(0.6f, 0.75f, 0.9f, 1); mdl[MDL_FISH] = end();

    /* star with eyes */
    begin();
    mb_color(1.0f, 0.85f, 0.1f);
    mb_star(0.45f, 0.28f);
    mb_color(0.1f, 0.05f, 0.0f);
    mb_cuboid(-0.14f, 0.0f, 0.2f, -0.08f, 0.22f, 0.27f);
    mb_cuboid(0.08f, 0.0f, 0.2f, 0.14f, 0.22f, 0.27f);
    mdl[MDL_STAR] = end();
    begin();
    mb_color4(0.5f, 0.7f, 1.0f, 0.55f);
    mb_star(0.45f, 0.28f);
    mdl[MDL_STAR_GHOST] = end();

    /* bug (walker enemy) - faces +z */
    begin();
    mb_color(0.55f, 0.2f, 0.12f);
    mb_ellipsoid(0, 0.55f, -0.1f, 0.6f, 0.42f, 0.7f, 16);
    mb_color(0.35f, 0.1f, 0.06f);
    mb_cuboid(-0.02f, 0.55f, -0.75f, 0.02f, 0.98f, 0.5f);
    mb_color(0.2f, 0.15f, 0.12f);
    mb_ellipsoid(0, 0.5f, 0.55f, 0.38f, 0.32f, 0.3f, 12);
    eye_pair(0.14f, 0.6f, 0.78f, 1.0f, 1);
    mb_color(0.85f, 0.8f, 0.6f);
    mb_push(); mb_translate(0.12f, 0.35f, 0.82f); mb_rotate(70, 1, 0, 0); mb_scale(0.05f, 0.18f, 0.05f); mb_cone(6); mb_pop();
    mb_push(); mb_translate(-0.12f, 0.35f, 0.82f); mb_rotate(70, 1, 0, 0); mb_scale(0.05f, 0.18f, 0.05f); mb_cone(6); mb_pop();
    mb_color(0.15f, 0.1f, 0.08f);
    for (i = 0; i < 3; i++) {
        float z = -0.4f + i * 0.4f;
        mb_push(); mb_translate(0.5f, 0.3f, z); mb_rotate(-50, 0, 0, 1); mb_scale(0.05f, 0.4f, 0.05f); mb_translate(0, -1, 0); mb_cylinder(6, 0); mb_pop();
        mb_push(); mb_translate(-0.5f, 0.3f, z); mb_rotate(50, 0, 0, 1); mb_scale(0.05f, 0.4f, 0.05f); mb_translate(0, -1, 0); mb_cylinder(6, 0); mb_pop();
    }
    mb_push(); mb_translate(0.1f, 0.75f, 0.75f); mb_rotate(-30, 1, 0, 0); mb_rotate(-20, 0, 0, 1); mb_scale(0.025f, 0.4f, 0.025f); mb_cylinder(5, 0); mb_pop();
    mb_push(); mb_translate(-0.1f, 0.75f, 0.75f); mb_rotate(-30, 1, 0, 0); mb_rotate(20, 0, 0, 1); mb_scale(0.025f, 0.4f, 0.025f); mb_cylinder(5, 0); mb_pop();
    mdl[MDL_BUG] = end();

    /* forkbomb */
    for (i = 0; i < 2; i++) {
        begin();
        if (i == 0) mb_color(0.12f, 0.12f, 0.16f); else mb_color(0.95f, 0.2f, 0.15f);
        mb_ellipsoid(0, 0.7f, 0, 0.6f, 0.6f, 0.6f, 18);
        eye_pair(0.17f, 0.8f, 0.52f, 1.2f, 0);
        mb_color(0.8f, 0.8f, 0.75f);
        mb_push(); mb_translate(0, 1.25f, 0); mb_scale(0.12f, 0.2f, 0.12f); mb_cylinder(8, 1); mb_pop();
        mb_color(0.95f, 0.8f, 0.4f);
        mb_push(); mb_translate(0, 1.42f, 0); mb_scale(0.03f, 0.15f, 0.03f); mb_cylinder(5, 1); mb_pop();
        mb_color(0.75f, 0.75f, 0.8f);  /* wind-up key */
        mb_push(); mb_translate(0, 0.75f, -0.62f); mb_scale(0.05f, 0.05f, 0.2f); mb_translate(0, 0, -1); mb_box(); mb_pop();
        mb_push(); mb_translate(0, 0.75f, -0.9f); mb_scale(0.04f, 0.18f, 0.1f); mb_box(); mb_pop();
        mb_color(0.95f, 0.75f, 0.1f);
        mb_ellipsoid(0.25f, 0.1f, 0.08f, 0.17f, 0.1f, 0.25f, 10);
        mb_ellipsoid(-0.25f, 0.1f, 0.08f, 0.17f, 0.1f, 0.25f, 10);
        mdl[i == 0 ? MDL_BOMB : MDL_BOMB_LIT] = end();
    }

    /* King Daemon (used scaled up) */
    begin();
    mb_color(0.14f, 0.12f, 0.2f);
    mb_ellipsoid(0, 0.7f, 0, 0.62f, 0.6f, 0.6f, 20);
    eye_pair(0.18f, 0.82f, 0.53f, 1.2f, 1);
    mb_color(0.95f, 0.95f, 0.9f); /* mustache */
    mb_ellipsoid(0.12f, 0.6f, 0.56f, 0.14f, 0.05f, 0.05f, 8);
    mb_ellipsoid(-0.12f, 0.6f, 0.56f, 0.14f, 0.05f, 0.05f, 8);
    mb_color(0.85f, 0.15f, 0.12f); /* horns */
    mb_push(); mb_translate(0.35f, 1.15f, 0); mb_rotate(-30, 0, 0, 1); mb_scale(0.09f, 0.35f, 0.09f); mb_cone(8); mb_pop();
    mb_push(); mb_translate(-0.35f, 1.15f, 0); mb_rotate(30, 0, 0, 1); mb_scale(0.09f, 0.35f, 0.09f); mb_cone(8); mb_pop();
    mb_color(1.0f, 0.8f, 0.15f); /* crown */
    mb_push(); mb_translate(0, 1.22f, 0); mb_scale(0.28f, 0.12f, 0.28f); mb_cylinder(12, 0); mb_pop();
    for (i = 0; i < 6; i++) {
        float a = TAU_F * i / 6;
        mb_push(); mb_translate(sinf(a) * 0.26f, 1.34f, cosf(a) * 0.26f); mb_scale(0.06f, 0.12f, 0.06f); mb_cone(5); mb_pop();
    }
    mb_color(0.95f, 0.75f, 0.1f);
    mb_ellipsoid(0.27f, 0.1f, 0.08f, 0.18f, 0.1f, 0.26f, 10);
    mb_ellipsoid(-0.27f, 0.1f, 0.08f, 0.18f, 0.1f, 0.26f, 10);
    mb_color(0.14f, 0.12f, 0.2f);
    mb_ellipsoid(0.62f, 0.6f, 0.1f, 0.12f, 0.2f, 0.12f, 8);
    mb_ellipsoid(-0.62f, 0.6f, 0.1f, 0.12f, 0.2f, 0.12f, 8);
    mdl[MDL_KING] = end();

    /* crusher block: half size 1.5 x 1.7 x 1.5 centred at origin, face on +z */
    begin();
    mb_tex(TEX_STONE); mb_uvscale(0.4f);
    mb_color(0.65f, 0.68f, 0.75f);
    mb_cuboid(-1.5f, -1.7f, -1.5f, 1.5f, 1.7f, 1.5f);
    mb_tex(TEX_NONE);
    mb_color(1, 1, 1);
    mb_cuboid(-0.9f, 0.2f, 1.5f, -0.35f, 0.75f, 1.56f);
    mb_cuboid(0.35f, 0.2f, 1.5f, 0.9f, 0.75f, 1.56f);
    mb_color(0.9f, 0.1f, 0.1f);
    mb_cuboid(-0.75f, 0.3f, 1.55f, -0.5f, 0.55f, 1.6f);
    mb_cuboid(0.5f, 0.3f, 1.55f, 0.75f, 0.55f, 1.6f);
    mb_color(0.2f, 0.2f, 0.25f);
    mb_push(); mb_translate(-0.62f, 0.95f, 1.55f); mb_rotate(-15, 0, 0, 1); mb_scale(0.4f, 0.08f, 0.05f); mb_box(); mb_pop();
    mb_push(); mb_translate(0.62f, 0.95f, 1.55f); mb_rotate(15, 0, 0, 1); mb_scale(0.4f, 0.08f, 0.05f); mb_box(); mb_pop();
    mb_cuboid(-0.8f, -0.9f, 1.5f, 0.8f, -0.55f, 1.57f);
    mb_color(1, 1, 1);
    for (i = 0; i < 5; i++) mb_cuboid(-0.75f + i * 0.32f, -0.88f, 1.56f, -0.55f + i * 0.32f, -0.6f, 1.6f);
    mb_color(0.55f, 0.58f, 0.65f);
    for (i = 0; i < 4; i++) {
        mb_push(); mb_translate(i < 2 ? -1.5f : 1.5f, (i & 1) ? 0.9f : -0.9f, 0); mb_rotate(i < 2 ? 90 : -90, 0, 0, 1); mb_scale(0.25f, 0.4f, 0.25f); mb_cone(6); mb_pop();
    }
    mdl[MDL_CRUSHER] = end();

    /* roller (iron ball), r = 1 */
    begin();
    mb_tex(TEX_METAL); mb_uvscale(0.5f);
    mb_color(0.45f, 0.45f, 0.5f);
    mb_sphere(18, 12);
    mdl[MDL_ROLLER] = end();

    /* puffer fish */
    begin();
    mb_color(0.95f, 0.8f, 0.2f);
    mb_sphere(14, 10);
    eye_pair(0.3f, 0.25f, 0.88f, 2.2f, 1);
    spikes(1.0f, 4, 0.45f, 0.95f, 0.95f, 0.85f);
    mb_color(0.9f, 0.5f, 0.15f);
    mb_tri(v3(0, 0, -0.9f), v3(0, 0.5f, -1.5f), v3(0, -0.5f, -1.5f));
    mb_tri(v3(0, 0, -0.9f), v3(0, -0.5f, -1.5f), v3(0, 0.5f, -1.5f));
    mdl[MDL_PUFFER] = end();

    /* crab */
    begin();
    mb_color(0.9f, 0.25f, 0.15f);
    mb_ellipsoid(0, 0.45f, 0, 0.7f, 0.3f, 0.5f, 14);
    mb_color(0.95f, 0.35f, 0.2f);
    mb_ellipsoid(0.85f, 0.6f, 0.35f, 0.25f, 0.18f, 0.3f, 10);
    mb_ellipsoid(-0.85f, 0.6f, 0.35f, 0.25f, 0.18f, 0.3f, 10);
    mb_color(0.9f, 0.25f, 0.15f);
    mb_cuboid(0.12f, 0.65f, 0.25f, 0.18f, 0.95f, 0.31f);
    mb_cuboid(-0.18f, 0.65f, 0.25f, -0.12f, 0.95f, 0.31f);
    mb_color(1, 1, 1);
    mb_ellipsoid(0.15f, 1.0f, 0.28f, 0.09f, 0.09f, 0.09f, 8);
    mb_ellipsoid(-0.15f, 1.0f, 0.28f, 0.09f, 0.09f, 0.09f, 8);
    mb_color(0, 0, 0);
    mb_ellipsoid(0.15f, 1.0f, 0.36f, 0.04f, 0.05f, 0.03f, 6);
    mb_ellipsoid(-0.15f, 1.0f, 0.36f, 0.04f, 0.05f, 0.03f, 6);
    mb_color(0.8f, 0.2f, 0.12f);
    for (i = 0; i < 3; i++) {
        float z = -0.25f + i * 0.25f;
        mb_push(); mb_translate(0.6f, 0.35f, z); mb_rotate(-60, 0, 0, 1); mb_scale(0.05f, 0.4f, 0.05f); mb_translate(0, -1, 0); mb_cylinder(5, 0); mb_pop();
        mb_push(); mb_translate(-0.6f, 0.35f, z); mb_rotate(60, 0, 0, 1); mb_scale(0.05f, 0.4f, 0.05f); mb_translate(0, -1, 0); mb_cylinder(5, 0); mb_pop();
    }
    mdl[MDL_CRAB] = end();

    /* brute (ice bully) */
    begin();
    mb_tex(TEX_ICE); mb_uvscale(0.6f);
    mb_color(0.45f, 0.65f, 0.95f);
    mb_ellipsoid(0, 1.0f, 0, 0.95f, 0.95f, 0.95f, 18);
    mb_tex(TEX_NONE);
    eye_pair(0.28f, 1.25f, 0.82f, 1.8f, 1);
    mb_color(0.95f, 0.95f, 1.0f);
    mb_push(); mb_translate(0.55f, 1.7f, 0.1f); mb_rotate(-40, 0, 0, 1); mb_scale(0.15f, 0.55f, 0.15f); mb_cone(8); mb_pop();
    mb_push(); mb_translate(-0.55f, 1.7f, 0.1f); mb_rotate(40, 0, 0, 1); mb_scale(0.15f, 0.55f, 0.15f); mb_cone(8); mb_pop();
    mb_color(0.2f, 0.25f, 0.4f);
    mb_ellipsoid(0.4f, 0.12f, 0.15f, 0.28f, 0.14f, 0.4f, 10);
    mb_ellipsoid(-0.4f, 0.12f, 0.15f, 0.28f, 0.14f, 0.4f, 10);
    mdl[MDL_BRUTE] = end();

    /* Segfault: body centred at origin (feet at 0) */
    begin();
    mb_color(0.45f, 0.1f, 0.3f);
    mb_ellipsoid(0, 2.2f, -0.2f, 2.0f, 1.8f, 2.2f, 20);
    mb_color(0.95f, 0.85f, 0.55f);
    mb_ellipsoid(0, 2.0f, 0.9f, 1.4f, 1.4f, 1.3f, 16);
    mb_color(0.8f, 0.8f, 0.8f);
    for (i = 0; i < 7; i++) {
        float a = -1.2f + i * 0.4f;
        mb_push(); mb_translate(sinf(a) * 1.6f, 3.4f + cosf(a) * 0.5f, -0.6f - cosf(a) * 0.9f);
        mb_rotate(DEG(a) * 0.6f, 0, 0, -1); mb_rotate(-25, 1, 0, 0); mb_scale(0.25f, 0.8f, 0.25f); mb_cone(6); mb_pop();
    }
    mb_color(0.35f, 0.08f, 0.22f);
    mb_ellipsoid(1.1f, 0.4f, 0.3f, 0.6f, 0.45f, 0.8f, 10);
    mb_ellipsoid(-1.1f, 0.4f, 0.3f, 0.6f, 0.45f, 0.8f, 10);
    mb_ellipsoid(1.8f, 2.2f, 0.9f, 0.4f, 0.8f, 0.4f, 10);
    mb_ellipsoid(-1.8f, 2.2f, 0.9f, 0.4f, 0.8f, 0.4f, 10);
    mb_color(0.95f, 0.95f, 0.9f);
    for (i = 0; i < 3; i++) {
        mb_push(); mb_translate(1.8f + (i - 1) * 0.15f, 1.35f, 1.15f); mb_rotate(160, 1, 0, 0); mb_scale(0.08f, 0.3f, 0.08f); mb_cone(5); mb_pop();
        mb_push(); mb_translate(-1.8f + (i - 1) * 0.15f, 1.35f, 1.15f); mb_rotate(160, 1, 0, 0); mb_scale(0.08f, 0.3f, 0.08f); mb_cone(5); mb_pop();
    }
    mdl[MDL_SEG_BODY] = end();
    begin(); /* head, origin at neck, faces +z */
    mb_color(0.5f, 0.12f, 0.32f);
    mb_ellipsoid(0, 0.5f, 0.5f, 0.95f, 0.8f, 1.0f, 16);
    mb_ellipsoid(0, 0.2f, 1.3f, 0.7f, 0.45f, 0.6f, 12);
    mb_color(0.95f, 0.85f, 0.55f);
    mb_ellipsoid(0, -0.05f, 1.2f, 0.6f, 0.25f, 0.55f, 12);
    mb_color(1, 1, 0.3f);
    mb_ellipsoid(0.42f, 0.8f, 1.15f, 0.2f, 0.14f, 0.1f, 8);
    mb_ellipsoid(-0.42f, 0.8f, 1.15f, 0.2f, 0.14f, 0.1f, 8);
    mb_color(0.9f, 0, 0);
    mb_ellipsoid(0.42f, 0.8f, 1.24f, 0.07f, 0.12f, 0.04f, 6);
    mb_ellipsoid(-0.42f, 0.8f, 1.24f, 0.07f, 0.12f, 0.04f, 6);
    mb_color(0.95f, 0.9f, 0.7f);
    mb_push(); mb_translate(0.6f, 1.1f, 0.2f); mb_rotate(-35, 0, 0, 1); mb_rotate(-30, 1, 0, 0); mb_scale(0.18f, 0.9f, 0.18f); mb_cone(8); mb_pop();
    mb_push(); mb_translate(-0.6f, 1.1f, 0.2f); mb_rotate(35, 0, 0, 1); mb_rotate(-30, 1, 0, 0); mb_scale(0.18f, 0.9f, 0.18f); mb_cone(8); mb_pop();
    mb_color(1, 1, 1);
    for (i = 0; i < 4; i++) {
        mb_push(); mb_translate(-0.36f + i * 0.24f, 0.0f, 1.75f); mb_rotate(180, 1, 0, 0); mb_scale(0.07f, 0.2f, 0.07f); mb_cone(5); mb_pop();
    }
    mb_color(1.0f, 0.5f, 0.1f); /* glitchy hair */
    for (i = 0; i < 5; i++) {
        mb_push(); mb_translate(-0.4f + i * 0.2f, 1.2f, 0.2f - (i % 2) * 0.2f); mb_rotate(-20, 1, 0, 0); mb_scale(0.12f, 0.5f, 0.12f); mb_cone(5); mb_pop();
    }
    mdl[MDL_SEG_HEAD] = end();
    begin(); /* tail: extends along -z from origin */
    mb_color(0.45f, 0.1f, 0.3f);
    for (i = 0; i < 4; i++) mb_ellipsoid(0, 0, -0.5f - i * 0.65f, 0.45f - i * 0.08f, 0.4f - i * 0.07f, 0.45f, 10);
    mb_color(0.85f, 0.85f, 0.85f);
    mb_push(); mb_translate(0, 0, -2.9f); mb_rotate(-90, 1, 0, 0); mb_scale(0.25f, 0.6f, 0.25f); mb_cone(6); mb_pop();
    mdl[MDL_SEG_TAIL] = end();

    /* spike mine */
    begin();
    mb_color(0.18f, 0.18f, 0.22f);
    mb_sphere(14, 10);
    spikes(1.0f, 4, 0.5f, 0.7f, 0.7f, 0.75f);
    mb_color(1, 0.2f, 0.1f);
    mb_ellipsoid(0, 0.98f, 0, 0.15f, 0.08f, 0.15f, 8);
    mdl[MDL_SPIKEBOMB] = end();

    /* sign */
    begin();
    mb_tex(TEX_WOOD); mb_uvscale(1.0f);
    mb_color(0.7f, 0.5f, 0.3f);
    mb_cuboid(-0.08f, 0, -0.08f, 0.08f, 1.0f, 0.08f);
    mb_cuboid(-0.7f, 0.9f, -0.06f, 0.7f, 1.7f, 0.06f);
    mb_tex(TEX_NONE);
    mb_color(0.3f, 0.2f, 0.1f);
    for (i = 0; i < 3; i++) mb_cuboid(-0.5f, 1.05f + i * 0.2f, 0.06f, 0.5f - (i == 2) * 0.3f, 1.12f + i * 0.2f, 0.08f);
    mdl[MDL_SIGN] = end();

    /* gnu racer */
    begin();
    mb_color(0.45f, 0.32f, 0.2f);
    mb_ellipsoid(0, 1.2f, 0, 0.45f, 0.45f, 0.85f, 14);
    mb_ellipsoid(0, 1.65f, 0.85f, 0.28f, 0.32f, 0.4f, 12);
    mb_color(0.3f, 0.2f, 0.12f);
    mb_ellipsoid(0, 1.35f, 1.05f, 0.18f, 0.3f, 0.12f, 8); /* beard */
    mb_ellipsoid(0, 1.6f, 0.4f, 0.12f, 0.35f, 0.3f, 8);   /* mane */
    mb_color(0.9f, 0.88f, 0.8f);
    mb_push(); mb_translate(0.25f, 1.9f, 0.8f); mb_rotate(-70, 0, 0, 1); mb_scale(0.06f, 0.35f, 0.06f); mb_cone(6); mb_pop();
    mb_push(); mb_translate(-0.25f, 1.9f, 0.8f); mb_rotate(70, 0, 0, 1); mb_scale(0.06f, 0.35f, 0.06f); mb_cone(6); mb_pop();
    mb_color(1, 1, 1);
    mb_ellipsoid(0.13f, 1.75f, 1.15f, 0.06f, 0.07f, 0.04f, 6);
    mb_ellipsoid(-0.13f, 1.75f, 1.15f, 0.06f, 0.07f, 0.04f, 6);
    mb_color(0.3f, 0.22f, 0.15f);
    mb_cuboid(0.18f, 0, 0.45f, 0.32f, 0.9f, 0.6f);
    mb_cuboid(-0.32f, 0, 0.45f, -0.18f, 0.9f, 0.6f);
    mb_cuboid(0.18f, 0, -0.6f, 0.32f, 0.9f, -0.45f);
    mb_cuboid(-0.32f, 0, -0.6f, -0.18f, 0.9f, -0.45f);
    mb_color(0.95f, 0.2f, 0.2f); /* red running shoes */
    mb_cuboid(0.16f, 0, 0.42f, 0.34f, 0.12f, 0.68f);
    mb_cuboid(-0.34f, 0, 0.42f, -0.16f, 0.12f, 0.68f);
    mb_cuboid(0.16f, 0, -0.62f, 0.34f, 0.12f, -0.38f);
    mb_cuboid(-0.34f, 0, -0.62f, -0.16f, 0.12f, -0.38f);
    mdl[MDL_GNU] = end();

    /* cannon (in-ground hole with rim) */
    begin();
    mb_tex(TEX_METAL); mb_uvscale(0.5f);
    mb_color(0.35f, 0.35f, 0.4f);
    mb_push(); mb_translate(0, 0.1f, 0); mb_torus(1.0f, 0.22f, 20, 8); mb_pop();
    mb_tex(TEX_NONE);
    mb_color(0.02f, 0.02f, 0.03f);
    mb_push(); mb_translate(0, 0.04f, 0); mb_disc(20); mb_pop();
    mdl[MDL_CANNON] = end();

    /* pine trees */
    for (i = 0; i < 2; i++) {
        int k;
        begin();
        mb_color(0.45f, 0.3f, 0.18f);
        mb_push(); mb_scale(0.3f, 1.6f, 0.3f); mb_cylinder(8, 0); mb_pop();
        for (k = 0; k < 3; k++) {
            mb_color(0.12f, 0.45f + k * 0.05f, 0.2f);
            mb_push(); mb_translate(0, 1.2f + k * 1.1f, 0); mb_scale(1.6f - k * 0.4f, 1.8f, 1.6f - k * 0.4f); mb_cone(10); mb_pop();
            mb_color(0.95f, 0.97f, 1.0f);
            if (i == 1) {
                mb_push(); mb_translate(0, 2.0f + k * 1.1f, 0); mb_scale((1.6f - k * 0.4f) * 0.55f, 1.0f, (1.6f - k * 0.4f) * 0.55f); mb_cone(10); mb_pop();
            } else {
                mb_push(); mb_translate(0, 2.45f + k * 1.1f, 0); mb_scale((1.6f - k * 0.4f) * 0.32f, 0.55f, (1.6f - k * 0.4f) * 0.32f); mb_cone(10); mb_pop();
            }
        }
        mdl[i == 0 ? MDL_PINE : MDL_PINE_SNOW] = end();
    }
    begin();
    mb_color(0.45f, 0.3f, 0.18f);
    mb_push(); mb_scale(0.3f, 2.2f, 0.3f); mb_cylinder(8, 0); mb_pop();
    mb_color(0.18f, 0.42f, 0.3f);   /* evergreen ball tree buried in snow */
    mb_ellipsoid(0, 3.0f, 0, 1.6f, 1.3f, 1.6f, 14);
    mb_color(0.96f, 0.98f, 1.0f);
    mb_ellipsoid(0, 3.55f, 0, 1.45f, 0.85f, 1.45f, 14);
    mb_ellipsoid(0.6f, 3.9f, 0.3f, 0.9f, 0.7f, 0.9f, 10);
    mdl[MDL_TREE] = end();

    /* doors: 2.4 wide, 3.4 tall, origin at bottom centre, slab in xy plane */
    for (i = 0; i < 2; i++) {
        begin();
        mb_tex(TEX_WOOD); mb_uvscale(0.8f);
        mb_color(0.65f, 0.42f, 0.25f);
        mb_cuboid(-1.2f, 0, -0.12f, 1.2f, 3.4f, 0.12f);
        mb_tex(TEX_NONE);
        mb_color(0.95f, 0.8f, 0.3f);
        mb_ellipsoid(0.85f, 1.6f, 0.15f, 0.09f, 0.09f, 0.06f, 8);
        mb_ellipsoid(0.85f, 1.6f, -0.15f, 0.09f, 0.09f, 0.06f, 8);
        if (i == 1) {
            mb_color(1.0f, 0.85f, 0.15f);
            mb_push(); mb_translate(0, 2.3f, 0.13f); mb_scale(0.55f, 0.55f, 0.15f); mb_star(0.45f, 0.3f); mb_pop();
            mb_push(); mb_translate(0, 2.3f, -0.13f); mb_scale(0.55f, 0.55f, 0.15f); mb_star(0.45f, 0.3f); mb_pop();
        }
        mdl[i == 0 ? MDL_DOOR : MDL_STARDOOR] = end();
    }

    /* clam */
    begin();
    mb_color(0.95f, 0.6f, 0.7f);
    mb_push(); mb_scale(1.8f, -0.7f, 1.8f); mb_hemisphere(14, 5); mb_pop();
    mb_color(0.98f, 0.95f, 0.9f);
    mb_ellipsoid(0, -0.1f, 0, 0.5f, 0.3f, 0.5f, 10);
    mdl[MDL_CLAM_BOT] = end();
    begin();   /* pivot at back edge (z = -1.8) */
    mb_color(0.95f, 0.6f, 0.7f);
    mb_push(); mb_translate(0, 0, 1.8f); mb_scale(1.8f, 0.7f, 1.8f); mb_hemisphere(14, 5); mb_pop();
    mdl[MDL_CLAM_TOP] = end();

    begin();
    mb_color(1.0f, 0.85f, 0.3f);
    mb_push(); mb_rotate(90, 1, 0, 0); mb_torus(1.4f, 0.14f, 24, 6); mb_pop();
    mdl[MDL_RING] = end();

    begin();
    mb_tex(TEX_PLANK); mb_uvscale(1.0f);
    mb_color(0.85f, 0.65f, 0.4f);
    mb_cuboid(-0.7f, 0, -0.7f, 0.7f, 1.4f, 0.7f);
    mb_tex(TEX_NONE);
    mb_color(0.5f, 0.35f, 0.2f);
    mb_cuboid(-0.72f, 0.0f, -0.72f, 0.72f, 0.12f, 0.72f);
    mb_cuboid(-0.72f, 1.28f, -0.72f, 0.72f, 1.42f, 0.72f);
    mdl[MDL_CRATE] = end();

    begin();
    mb_color(0.3f, 0.3f, 0.35f);
    mb_push(); mb_scale(1.0f, 0.15f, 1.0f); mb_cylinder(16, 1); mb_pop();
    mb_color(0.2f, 0.4f, 1.0f);
    mb_push(); mb_translate(0, 0.15f, 0); mb_scale(0.7f, 0.25f, 0.7f); mb_cylinder(16, 1); mb_pop();
    mdl[MDL_SWITCH] = end();

    begin();
    mb_tex(TEX_METAL); mb_uvscale(1.0f);
    mb_color(0.7f, 0.7f, 0.75f);
    mb_push(); mb_scale(0.12f, 1.0f, 0.12f); mb_cylinder(8, 1); mb_pop();
    mdl[MDL_POLE] = end();

    begin();
    mb_color(0.8f, 0.8f, 0.85f);
    mb_push(); mb_scale(0.1f, 4.0f, 0.1f); mb_cylinder(8, 1); mb_pop();
    mb_color(1.0f, 0.85f, 0.1f);
    mb_ellipsoid(0, 4.05f, 0, 0.18f, 0.18f, 0.18f, 8);
    mb_color(0.2f, 0.8f, 0.3f);
    mb_tri(v3(0, 3.9f, 0), v3(0, 2.9f, 0), v3(1.6f, 3.4f, 0));
    mb_tri(v3(0, 3.9f, 0), v3(1.6f, 3.4f, 0), v3(0, 2.9f, 0));
    mdl[MDL_FLAG] = end();

    begin(); /* crown for king's head when grabbed etc (unused visual) */
    mb_color(1.0f, 0.8f, 0.15f);
    mb_push(); mb_scale(0.3f, 0.15f, 0.3f); mb_cylinder(12, 0); mb_pop();
    mdl[MDL_CROWN] = end();

    begin();
    mb_color(0.97f, 0.97f, 1.0f);
    mb_ellipsoid(0, 1.0f, 0, 1.1f, 1.0f, 1.1f, 14);
    mb_ellipsoid(0, 2.5f, 0, 0.75f, 0.7f, 0.75f, 12);
    mb_color(0.05f, 0.05f, 0.05f);
    mb_ellipsoid(0.25f, 2.7f, 0.65f, 0.08f, 0.08f, 0.05f, 6);
    mb_ellipsoid(-0.25f, 2.7f, 0.65f, 0.08f, 0.08f, 0.05f, 6);
    mb_color(1.0f, 0.5f, 0.1f);
    mb_push(); mb_translate(0, 2.5f, 0.7f); mb_rotate(90, 1, 0, 0); mb_scale(0.1f, 0.4f, 0.1f); mb_cone(6); mb_pop();
    mb_color(0.85f, 0.15f, 0.2f);
    mb_push(); mb_translate(0, 1.95f, 0); mb_torus(0.6f, 0.12f, 12, 6); mb_pop();
    mdl[MDL_SNOWMAN] = end();

    begin();
    mb_tex(TEX_ROCK); mb_uvscale(0.6f);
    mb_color(0.7f, 0.68f, 0.65f);
    mb_ellipsoid(0, 0.4f, 0, 1.0f, 0.7f, 0.9f, 8);
    mdl[MDL_ROCK] = end();

    begin();
    mb_color(0.95f, 0.97f, 1.0f);   /* snow mound with winter berries */
    mb_ellipsoid(0, 0.5f, 0, 0.9f, 0.7f, 0.9f, 10);
    mb_ellipsoid(0.6f, 0.4f, 0.2f, 0.6f, 0.5f, 0.6f, 8);
    mb_color(0.15f, 0.4f, 0.25f);
    mb_ellipsoid(-0.2f, 0.75f, 0.55f, 0.35f, 0.15f, 0.2f, 6);
    mb_color(0.95f, 0.15f, 0.2f);
    mb_ellipsoid(0.3f, 0.95f, 0.5f, 0.12f, 0.12f, 0.12f, 6);
    mb_ellipsoid(-0.4f, 0.8f, 0.55f, 0.12f, 0.12f, 0.12f, 6);
    mdl[MDL_BUSH] = end();

    /* feather cap box: red block with wing emblem, centred at origin (half size 0.7) */
    begin();
    mb_tex(TEX_METAL); mb_uvscale(0.7f);
    mb_color(0.95f, 0.25f, 0.2f);
    mb_cuboid(-0.7f, -0.7f, -0.7f, 0.7f, 0.7f, 0.7f);
    mb_tex(TEX_NONE);
    mb_color(1, 1, 1);
    for (i = 0; i < 4; i++) {
        mb_push();
        mb_rotate(i * 90.0f, 0, 1, 0);
        mb_tri(v3(0, -0.2f, 0.72f), v3(0.45f, 0.35f, 0.72f), v3(0.15f, 0.0f, 0.72f));
        mb_tri(v3(0, -0.2f, 0.72f), v3(-0.15f, 0.0f, 0.72f), v3(-0.45f, 0.35f, 0.72f));
        mb_pop();
    }
    mdl[MDL_CAPBOX] = end();

    /* ghosts ("zombie processes"): two alpha levels */
    for (i = 0; i < 2; i++) {
        float al = i == 0 ? 0.88f : 0.28f;
        int k;
        begin();
        mb_color4(0.92f, 0.95f, 1.0f, al);
        mb_ellipsoid(0, 1.0f, 0, 0.62f, 0.6f, 0.6f, 16);
        mb_push(); mb_translate(0, 0.95f, 0); mb_rotate(180, 1, 0, 0); mb_scale(0.6f, 0.85f, 0.6f); mb_cone(14); mb_pop();
        mb_ellipsoid(0.62f, 0.95f, 0.15f, 0.2f, 0.12f, 0.12f, 8);
        mb_ellipsoid(-0.62f, 0.95f, 0.15f, 0.2f, 0.12f, 0.12f, 8);
        mb_color4(0.1f, 0.1f, 0.2f, al);
        mb_ellipsoid(0.2f, 1.18f, 0.5f, 0.1f, 0.15f, 0.06f, 8);
        mb_ellipsoid(-0.2f, 1.18f, 0.5f, 0.1f, 0.15f, 0.06f, 8);
        mb_color4(0.5f, 0.05f, 0.15f, al);
        mb_ellipsoid(0, 0.85f, 0.53f, 0.2f, 0.14f, 0.07f, 10);
        mb_color4(1.0f, 0.4f, 0.5f, al);
        mb_ellipsoid(0, 0.78f, 0.56f, 0.09f, 0.06f, 0.05f, 6);
        for (k = 0; k < 3; k++) {   /* little "zzz" tufts on the head */
            mb_color4(0.7f, 0.9f, 0.7f, al);
            mb_ellipsoid(-0.15f + k * 0.15f, 1.6f + (k % 2) * 0.05f, -0.1f, 0.05f, 0.08f, 0.05f, 6);
        }
        mdl[i == 0 ? MDL_GHOST : MDL_GHOST_FADE] = end();
    }
    begin();
    mb_color(1.0f, 0.15f, 0.25f);
    mb_ellipsoid(0.22f, 0.18f, 0, 0.3f, 0.3f, 0.2f, 12);
    mb_ellipsoid(-0.22f, 0.18f, 0, 0.3f, 0.3f, 0.2f, 12);
    mb_push(); mb_translate(0, 0.12f, 0); mb_rotate(180, 1, 0, 0); mb_scale(0.48f, 0.55f, 0.2f); mb_cone(12); mb_pop();
    mdl[MDL_HEART] = end();
    /* ice spire (tall crystal) */
    begin();
    {
        int k;
        mb_tex(TEX_ICE); mb_uvscale(0.5f);
        for (k = 0; k < 5; k++) {
            float a = TAU_F * k / 5, r = k ? 0.7f : 0;
            float hh = k ? 3.0f + (k % 2) * 1.2f : 6.0f;
            mb_color4(0.65f, 0.88f, 1.0f, 1);
            mb_push(); mb_translate(sinf(a) * r, 0, cosf(a) * r); mb_rotate(k ? 12.0f : 0, cosf(a), 0, -sinf(a));
            mb_scale(k ? 0.45f : 0.75f, hh * 0.75f, k ? 0.45f : 0.75f); mb_cylinder(6, 0);
            mb_translate(0, 1, 0); mb_scale(1, 0.45f, 1); mb_cone(6); mb_pop();
        }
    }
    mdl[MDL_ICESPIRE] = end();
    begin();   /* icicle crystal cluster */
    mb_tex(TEX_ICE); mb_uvscale(0.6f);
    for (i = 0; i < 7; i++) {
        float a = TAU_F * i / 7 + 0.3f, r = i ? 0.45f : 0, hh = i ? 1.2f + (i % 3) * 0.5f : 2.6f;
        mb_color(0.75f, 0.92f, 1.0f);
        mb_push(); mb_translate(sinf(a) * r, 0, cosf(a) * r); mb_rotate(i ? 25.0f : 0, cosf(a), 0, -sinf(a));
        mb_scale(0.28f, hh, 0.28f); mb_cone(6); mb_pop();
    }
    mdl[MDL_CRYSTAL] = end();
    begin();
    {
        int k;
        mb_color(0.3f, 0.22f, 0.18f);
        mb_push(); mb_scale(0.35f, 3.5f, 0.35f); mb_cylinder(8, 0); mb_pop();
        for (k = 0; k < 5; k++) {
            mb_push(); mb_translate(0, 2.0f + k * 0.4f, 0); mb_rotate(k * 77.0f, 0, 1, 0); mb_rotate(55, 0, 0, 1);
            mb_scale(0.09f, 1.4f - k * 0.15f, 0.09f); mb_cylinder(6, 0); mb_pop();
        }
    }
    mb_color(0.95f, 0.97f, 1.0f);
    mb_ellipsoid(0, 3.5f, 0, 0.42f, 0.18f, 0.42f, 8);
    mdl[MDL_DEADTREE] = end();
    /* watchdog (chain chomp): iron ball r=1 facing +z with a big mouth */
    begin();
    mb_tex(TEX_METAL); mb_uvscale(0.6f);
    mb_color(0.15f, 0.15f, 0.2f);
    mb_sphere(18, 12);
    mb_tex(TEX_NONE);
    mb_color(0.6f, 0.05f, 0.08f);
    mb_ellipsoid(0, -0.15f, 0.72f, 0.62f, 0.38f, 0.32f, 14);
    mb_color(1, 1, 1);
    for (i = 0; i < 7; i++) {
        float a = -1.0f + i * 0.33f;
        mb_push(); mb_translate(sinf(a) * 0.6f, 0.15f, 0.75f + cosf(a) * 0.12f - 0.1f); mb_rotate(180, 1, 0, 0); mb_scale(0.07f, 0.18f, 0.07f); mb_cone(5); mb_pop();
        mb_push(); mb_translate(sinf(a) * 0.55f, -0.45f, 0.72f + cosf(a) * 0.1f - 0.1f); mb_scale(0.06f, 0.15f, 0.06f); mb_cone(5); mb_pop();
    }
    mb_ellipsoid(0.38f, 0.45f, 0.72f, 0.18f, 0.2f, 0.1f, 10);
    mb_ellipsoid(-0.38f, 0.45f, 0.72f, 0.18f, 0.2f, 0.1f, 10);
    mb_color(0, 0, 0);
    mb_ellipsoid(0.36f, 0.43f, 0.8f, 0.07f, 0.09f, 0.04f, 8);
    mb_ellipsoid(-0.36f, 0.43f, 0.8f, 0.07f, 0.09f, 0.04f, 8);
    mdl[MDL_CHOMP] = end();
    begin();
    mb_color(0.35f, 0.35f, 0.4f);
    mb_push(); mb_rotate(90, 0, 0, 1); mb_torus(0.22f, 0.07f, 10, 5); mb_pop();
    mdl[MDL_LINK] = end();
    begin();
    mb_tex(TEX_WOOD); mb_uvscale(1.0f);
    mb_color(0.65f, 0.45f, 0.28f);
    mb_push(); mb_scale(0.4f, 1.5f, 0.4f); mb_cylinder(10, 1); mb_pop();
    mdl[MDL_POST] = end();
    begin();
    mb_color(0.3f, 0.3f, 0.35f);
    for (i = 0; i < 16; i++) {
        float a = TAU_F * i / 16;
        mb_push(); mb_translate(sinf(a) * 1.5f, 0, cosf(a) * 1.5f); mb_scale(0.07f, 2.8f, 0.07f); mb_cylinder(6, 0); mb_pop();
    }
    mb_push(); mb_translate(0, 2.8f, 0); mb_torus(1.5f, 0.1f, 18, 5); mb_pop();
    mb_push(); mb_translate(0, 0.1f, 0); mb_torus(1.5f, 0.1f, 18, 5); mb_pop();
    mb_push(); mb_translate(0, 2.8f, 0); mb_scale(1.55f, 0.15f, 1.55f); mb_disc(16); mb_pop();
    mdl[MDL_CAGE] = end();

    for (i = 0; i < 2; i++) {
        begin();
        mb_color(0.5f, 0.45f, 0.4f);
        mb_push(); mb_scale(1.2f, 0.12f, 1.2f); mb_cylinder(16, 1); mb_pop();
        if (i) mb_color(1.0f, 0.9f, 0.3f); else mb_color(0.35f, 0.3f, 0.5f);
        mb_push(); mb_translate(0, 0.12f, 0); mb_scale(0.8f, 0.08f, 0.8f); mb_cylinder(16, 1); mb_pop();
        mdl[i ? MDL_TOPSWITCH_ON : MDL_TOPSWITCH] = end();
    }
    begin();
    mb_tex(TEX_SNOW); mb_uvscale(0.8f);
    mb_color(1, 1, 1);
    mb_ellipsoid(0, 0, 0, 1, 1, 1, 20);
    mdl[MDL_MOUND] = end();
    /* the OOM Killer: hulking frost giant with a dark visor (faces +z, ~3.4 tall) */
    begin();
    mb_tex(TEX_SNOW); mb_uvscale(0.7f);
    mb_color(0.82f, 0.84f, 0.92f);
    mb_ellipsoid(0, 1.75f, 0, 0.95f, 1.1f, 0.78f, 18);
    mb_ellipsoid(0, 2.95f, 0.1f, 0.58f, 0.55f, 0.55f, 16);
    mb_ellipsoid(1.05f, 1.75f, 0.15f, 0.27f, 0.9f, 0.27f, 10);
    mb_ellipsoid(-1.05f, 1.75f, 0.15f, 0.27f, 0.9f, 0.27f, 10);
    mb_ellipsoid(0.45f, 0.45f, 0, 0.32f, 0.5f, 0.32f, 10);
    mb_ellipsoid(-0.45f, 0.45f, 0, 0.32f, 0.5f, 0.32f, 10);
    mb_tex(TEX_NONE);
    mb_color(0.08f, 0.08f, 0.12f);
    mb_ellipsoid(0, 2.98f, 0.5f, 0.42f, 0.2f, 0.14f, 12);
    mb_color(1.0f, 0.15f, 0.1f);
    mb_ellipsoid(0.17f, 2.98f, 0.62f, 0.09f, 0.06f, 0.04f, 8);
    mb_ellipsoid(-0.17f, 2.98f, 0.62f, 0.09f, 0.06f, 0.04f, 8);
    mb_color(0.7f, 0.85f, 1.0f);
    for (i = 0; i < 4; i++) {
        float x = (i < 2 ? 1 : -1) * (0.55f + (i & 1) * 0.3f);
        mb_push(); mb_translate(x, 2.55f, -0.1f); mb_rotate(i < 2 ? -20.0f : 20.0f, 0, 0, 1); mb_scale(0.14f, 0.6f, 0.14f); mb_cone(6); mb_pop();
    }
    mb_color(0.95f, 0.95f, 1.0f);
    for (i = 0; i < 3; i++) {   /* claws */
        mb_push(); mb_translate(1.05f + (i - 1) * 0.1f, 0.85f, 0.35f); mb_rotate(180, 1, 0, 0); mb_scale(0.05f, 0.25f, 0.05f); mb_cone(5); mb_pop();
        mb_push(); mb_translate(-1.05f + (i - 1) * 0.1f, 0.85f, 0.35f); mb_rotate(180, 1, 0, 0); mb_scale(0.05f, 0.25f, 0.05f); mb_cone(5); mb_pop();
    }
    mdl[MDL_OOM] = end();
    /* cyberspace gates used inside the castle (same size as the wooden doors) */
    for (i = 0; i < 2; i++) {
        int k;
        begin();
        mb_tex(TEX_CYWALL); mb_uvscale(0.5f);
        mb_color(0.8f, 0.9f, 1.0f);
        mb_cuboid(-1.2f, 0, -0.12f, 1.2f, 3.4f, 0.12f);
        mb_tex(TEX_NONE);
        mb_color(0.3f, 0.95f, 1.0f);
        mb_cuboid(-1.2f, 3.25f, -0.15f, 1.2f, 3.4f, 0.15f);
        mb_cuboid(-1.2f, 0, -0.15f, 1.2f, 0.12f, 0.15f);
        mb_cuboid(-1.2f, 0, -0.15f, -1.08f, 3.4f, 0.15f);
        mb_cuboid(1.08f, 0, -0.15f, 1.2f, 3.4f, 0.15f);
        for (k = 0; k < 6; k++) mb_cuboid(-0.8f, 0.5f + k * 0.18f, -0.14f, 0.8f, 0.55f + k * 0.18f, 0.14f);   /* data slots */
        if (i == 1) {
            mb_color(1.0f, 0.85f, 0.15f);
            mb_push(); mb_translate(0, 2.3f, 0.13f); mb_scale(0.55f, 0.55f, 0.15f); mb_star(0.45f, 0.3f); mb_pop();
            mb_push(); mb_translate(0, 2.3f, -0.13f); mb_scale(0.55f, 0.55f, 0.15f); mb_star(0.45f, 0.3f); mb_pop();
        } else {
            mb_color(0.3f, 0.95f, 1.0f);
            mb_push(); mb_translate(0, 2.3f, 0.14f); mb_rotate(45, 0, 0, 1); mb_scale(0.35f, 0.35f, 0.03f); mb_box(); mb_pop();
            mb_push(); mb_translate(0, 2.3f, -0.14f); mb_rotate(45, 0, 0, 1); mb_scale(0.35f, 0.35f, 0.03f); mb_box(); mb_pop();
        }
        mdl[i ? MDL_CYSTARDOOR : MDL_CYDOOR] = end();
    }
    mdl[MDL_KEY] = mdl[MDL_STAR];
}
