#include "perlin.h"

// 6t5-15t4+10t3
static real32 fade(real32 t)
{
    return t * t * t * (t * (t * 6.f - 15.f) + 10.f);
    // return t * t * (3 - 2 * t);
}

static real32 rand_noise(V2 p) 
{
    real32 v = sinf(v2_dot(p, {12.9898f, 78.233f})) * 43758.5453f;
    return v - (u32)v;
}

static V2 grad(V2 p)
{
    real32 a = rand_noise(p) * 2.f * M_PI;
    return { cosf(a), sinf(a) };
}

real32 perlin(V2 p)
{
    V2 p0, p1, p2, p3;

    p0.x = (u32)p.x;
    p0.y = (u32)p.y;

    p1 = {p0.x + 1, p0.y};
    p2 = {p0.x, p0.y + 1};
    p3 = {p0.x + 1, p0.y + 1};

    V2 g0 = grad(p0);
    V2 g1 = grad(p1);
    V2 g2 = grad(p2);
    V2 g3 = grad(p3);

    real32 t0 = p.x - p0.x;
    real32 fade_t0 = fade(t0);
    real32 t1 = p.y - p0.y;
    real32 fade_t1 = fade(t1);

    real32 p0p1 = (1.f - fade_t0) * v2_dot(g0, (p - p0)) + fade_t0 * v2_dot(g1, (p - p1));
    real32 p2p3 = (1.f - fade_t0) * v2_dot(g2, (p - p2)) + fade_t0 * v2_dot(g3, (p - p3));

    real32 value = ((1.f - fade_t1) * p0p1 + fade_t1 * p2p3);
    return value;
}