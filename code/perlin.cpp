#include <random>

#include "perlin.h"

static u8 P[512];

#include <windows.h>

void seed_perlin()
{
	std::random_device rd; 
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> distr(0, 255);

    for (u32 i = 0; i < 256; i++) {
        P[i] = i;
    }

    for (u32 i = 0; i < 256; i++) {
        const u8 index = distr(gen);
        const u8 temp = P[index];
        P[i] = P[index];
        P[index] = temp;
    }

    for (u32 i = 256; i < 512; i++) {
        P[i] = P[i - 256];
    }
}

// 6t5-15t4+10t3
static real32 fade(real32 t)
{
    return ((6 * t - 15) * t + 10) * t * t * t;
}

static V2 get_vector(u32 i)
{
    switch (i & 3) {
        case 0: return { 1.f, 1.f };
        case 1: return { -1.f, 1.f };
        case 2: return { -1.f, -1.f };
        case 3: return { 1.f, -1.f };
    }

	return { 0, 0 };
}

real32 lerp(real32 t, real32 a, real32 b)
{
    return a + t * (b - a);
}

real32 perlin(V2 p)
{
    const real32 x = p.x;
    const real32 y = p.y;

    const u32 X = (u32)floor(p.x) & 255;
	const u32 Y = (u32)floor(p.y) & 255;

	const real32 xf = x - floor(x);
	const real32 yf = y - floor(y);

	const V2 topRight = { xf - 1.f, yf - 1.f };
	const V2 topLeft = { xf, yf - 1.f };
	const V2 bottomRight = { xf - 1.f, yf };
	const V2 bottomLeft = { xf, yf };
	
	//Select a value in the array for each of the 4 corners
	const real32 valueTopRight = P[P[X+1]+Y+1];
	const real32 valueTopLeft = P[P[X]+Y+1];
	const real32 valueBottomRight = P[P[X+1]+Y];
	const real32 valueBottomLeft = P[P[X]+Y];
	
	const real32 dotTopRight = v2_dot(topRight, get_vector(valueTopRight));
	const real32 dotTopLeft = v2_dot(topLeft, get_vector(valueTopLeft));
	const real32 dotBottomRight = v2_dot(bottomRight, get_vector(valueBottomRight));
	const real32 dotBottomLeft = v2_dot(bottomLeft, get_vector(valueBottomLeft));
	
	const real32 u = fade(xf);
	const real32 v = fade(yf);
	
	return lerp(u,
		lerp(v, dotBottomLeft, dotTopLeft),
		lerp(v, dotBottomRight, dotTopRight)
	);
}