/* camera.c - "Lakitu"-style follow camera with collision */
#include "game.h"

Camera cam;
static const float zoom_dist[3] = { 6.5f, 9.0f, 13.5f };
static float focus_y;
static int manual_t;

float camera_yaw(void) { return cam.yaw; }

void camera_reset(void)
{
    cam.yaw = pl.yaw;
    cam.zoom = 1;
    cam.dist = cam.dist_target = zoom_dist[1];
    cam.mode = CAM_FOLLOW;
    cam.yaw_target_off = 0;
    cam.shake = 0;
    focus_y = pl.pos.y + 1.2f;
    cam.at = v3(pl.pos.x, focus_y, pl.pos.z);
    cam.pos = v3add(v3mad(cam.at, yaw_dir(cam.yaw), -cam.dist), v3(0, cam.dist * 0.4f, 0));
}

static void collide(vec3 focus, vec3 *p)
{
    float t;
    vec3 n;
    if (col_ray(focus, *p, &t, &n, 1)) {
        float tt = maxf(t - 0.4f / maxf(v3dist(focus, *p), 0.5f), 0.08f);
        *p = v3lerp(focus, *p, tt);
    }
}

/* how far the camera can go from target along (yaw, height ratio) before hitting a wall */
static float free_len(vec3 target, float yaw, float hd, float h)
{
    vec3 n, p = v3add(v3mad(target, yaw_dir(yaw), -hd), v3(0, h, 0));
    float t = 1, len = sqrtf(hd * hd + h * h);
    if (!col_ray(target, p, &t, &n, 1)) t = 1;
    return t * len * (hd / maxf(len, 0.01f));
}

void camera_update(void)
{
    vec3 target, desired, d;
    float follow_yaw, hd, h;
    int in_air = player_in_air() || (pl.act == ACT_SKATE && pl.act_s == 1);

    if (pl.act == ACT_SKYFLY && cam.mode == CAM_FOLLOW) { sky_camera(); return; }
    if (cam.mode == CAM_FIXED) {
        cam.pos = cam.fixed_pos;
        cam.at = v3add(pl.pos, v3(0, 1.0f, 0));
        cam.yaw = yaw_to(cam.pos, pl.pos);
        return;
    }

    /* manual control */
    if (cam.mode == CAM_FOLLOW) {
        if (in.pressed & BT_CL) { cam.yaw_target_off += PI_F / 4; manual_t = 60; sfx_ex(SFX_MENU_MOVE, 0.3f, 1.5f); }
        if (in.pressed & BT_CR) { cam.yaw_target_off -= PI_F / 4; manual_t = 60; sfx_ex(SFX_MENU_MOVE, 0.3f, 1.5f); }
        if (fabsf(in.cx) > 0.2f) { cam.yaw += -in.cx * 2.6f * DT; manual_t = 30; }
        if (in.pressed & BT_CU) { if (cam.zoom > 0) cam.zoom--; sfx_ex(SFX_MENU_MOVE, 0.3f, 1.8f); }
        if (in.pressed & BT_CD) { if (cam.zoom < 2) cam.zoom++; sfx_ex(SFX_MENU_MOVE, 0.3f, 1.2f); }
        if (in.cy > 0.6f && cam.zoom > 0 && (g_frame % 12) == 0) cam.zoom--;
        if (in.cy < -0.6f && cam.zoom < 2 && (g_frame % 12) == 0) cam.zoom++;
        if (in.pressed & BT_R) { cam.yaw_target_off = angle_diff(pl.yaw, cam.yaw); manual_t = 40; }
        cam.dist_target = zoom_dist[cam.zoom];
        if (cur_level == LV_LOBBY) cam.dist_target *= 0.9f;
        if (pl.act == ACT_SKATE) cam.dist_target += minf(pl.fwd * 0.12f, 3.5f);
    } else {
        cam.dist_target = 6.5f;
    }
    {
        float rot = cam.yaw_target_off * minf(1.0f, 9.0f * DT);
        cam.yaw = wrap_angle(cam.yaw + rot);
        cam.yaw_target_off -= rot;
    }
    if (manual_t > 0) manual_t--;
    cam.dist = approachf(cam.dist, cam.dist_target, 12 * DT);

    /* vertical focus smoothing */
    target = v3add(pl.pos, v3(0, 1.2f, 0));
    if (pl.act == ACT_SWIM) target.y += 0.3f;
    {
        float rate = in_air ? 1.5f : 5.0f;
        if (target.y < focus_y - 3.0f || target.y > focus_y + 4.0f) rate = 6.0f;
        if (pl.act == ACT_POLE || pl.act == ACT_LEDGE_HANG) rate = 4.0f;
        focus_y += (target.y - focus_y) * minf(1.0f, rate * DT);
    }
    target.y = focus_y;

    /* string follow */
    if (cam.mode == CAM_FOLLOW) {
        d = v3sub(target, cam.pos);
        if (v3len(v3(d.x, 0, d.z)) > 0.5f) {
            follow_yaw = atan2f(d.x, d.z);
            if (manual_t == 0) cam.yaw = approach_angle(cam.yaw, follow_yaw, 3.0f * DT);
        }
        /* dash boots: stay right behind Tux, like a racing game */
        if (manual_t == 0 && pl.act == ACT_SKATE)
            cam.yaw = approach_angle(cam.yaw, pl.yaw, (1.2f + pl.fwd * 0.12f) * DT);
        /* gentle auto align behind a running player */
        if (manual_t == 0 && pl.fwd > 4.0f && !in_air && pl.act == ACT_WALK) {
            float diff = fabsf(angle_diff(pl.yaw, cam.yaw));
            if (diff < 2.0f) cam.yaw = approach_angle(cam.yaw, pl.yaw, 0.5f * (pl.fwd / 9.0f) * DT);
        }
    }

    hd = cam.dist;
    h = cam.dist * 0.42f;
    if (pl.act == ACT_SWIM) h = cam.dist * 0.2f;
    /* Avoid obstacles like Lakitu: when the spot behind Tux is blocked (narrow corridors, walls),
     * try positions rotated around him and higher up, and drift toward the most open one. */
    {
        static float high;
        static const float yoff[7] = { 0, 0.5f, -0.5f, 1.0f, -1.0f, 1.6f, -1.6f };
        float best_score = -1, best_yaw = 0, best_high = 0, base_free;
        int i, k;
        base_free = free_len(target, cam.yaw, hd, h);
        if (base_free < hd * 0.6f && cam.mode == CAM_FOLLOW && manual_t == 0) {
            for (k = 0; k < 2; k++) {
                float hh = k ? 2.2f : 1.0f, dd = k ? 0.7f : 1.0f;
                for (i = 0; i < 7; i++) {
                    float fl = free_len(target, cam.yaw + yoff[i], hd * dd, h * hh);
                    float score = fl / (hd * dd) - fabsf(yoff[i]) * 0.12f - k * 0.08f;
                    if (score > best_score) { best_score = score; best_yaw = yoff[i]; best_high = (float)k; }
                }
            }
            cam.yaw = approach_angle(cam.yaw, wrap_angle(cam.yaw + best_yaw), (base_free < hd * 0.3f ? 4.0f : 1.8f) * DT);
            high = approachf(high, best_high, 1.5f * DT);
        } else {
            high = approachf(high, base_free < hd * 0.8f ? high : 0.0f, 1.0f * DT);
        }
        hd *= 1.0f - 0.3f * high;
        h *= 1.0f + 1.2f * high;
    }
    desired = v3add(v3mad(target, yaw_dir(cam.yaw), -hd), v3(0, h, 0));
    collide(target, &desired);
    cam.pos = v3lerp(cam.pos, desired, minf(1.0f, 10.0f * DT));
    collide(target, &cam.pos);
    cam.at = target;
    if (cam.shake > 0) {
        cam.pos = v3add(cam.pos, v3(frands() * cam.shake, frands() * cam.shake, frands() * cam.shake));
        cam.shake -= DT;
    }
}
