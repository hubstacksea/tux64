/* mathx.h - small vector math helpers */
#ifndef MATHX_H
#define MATHX_H
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define PI_F 3.14159265f
#define TAU_F 6.28318531f
#define DEG(x) ((x) * 57.2957795f)
#define RAD(x) ((x) * 0.0174532925f)

typedef struct { float x, y, z; } vec3;

static inline vec3 v3(float x, float y, float z) { vec3 r; r.x = x; r.y = y; r.z = z; return r; }
static inline vec3 v3add(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline vec3 v3sub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline vec3 v3scale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline vec3 v3mad(vec3 a, vec3 b, float s) { return v3(a.x + b.x * s, a.y + b.y * s, a.z + b.z * s); }
static inline float v3dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline vec3 v3cross(vec3 a, vec3 b) { return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
static inline float v3len(vec3 a) { return sqrtf(v3dot(a, a)); }
static inline vec3 v3norm(vec3 a) { float l = v3len(a); return l > 1e-8f ? v3scale(a, 1.0f / l) : v3(0, 1, 0); }
static inline vec3 v3lerp(vec3 a, vec3 b, float t) { return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t); }
static inline float v3distxz(vec3 a, vec3 b) { float dx = a.x - b.x, dz = a.z - b.z; return sqrtf(dx * dx + dz * dz); }
static inline float v3dist(vec3 a, vec3 b) { return v3len(v3sub(a, b)); }

static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float approachf(float v, float target, float step)
{
    if (v < target) { v += step; if (v > target) v = target; }
    else if (v > target) { v -= step; if (v < target) v = target; }
    return v;
}
/* wrap angle to -PI..PI */
static inline float wrap_angle(float a)
{
    while (a > PI_F) a -= TAU_F;
    while (a < -PI_F) a += TAU_F;
    return a;
}
static inline float angle_diff(float to, float from) { return wrap_angle(to - from); }
static inline float approach_angle(float a, float target, float step)
{
    float d = angle_diff(target, a);
    if (d > step) d = step;
    if (d < -step) d = -step;
    return wrap_angle(a + d);
}
static inline float yaw_to(vec3 from, vec3 to) { return atan2f(to.x - from.x, to.z - from.z); }
static inline vec3 yaw_dir(float yaw) { return v3(sinf(yaw), 0, cosf(yaw)); }

/* deterministic random */
extern unsigned int rng_state;
static inline unsigned int rng_u32(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}
static inline float frand(void) { return (rng_u32() & 0xFFFFFF) / 16777216.0f; }
static inline float frands(void) { return frand() * 2.0f - 1.0f; }

#endif
