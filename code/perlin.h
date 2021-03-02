#ifndef PERLIN_H
#define PERLIN_H

#include <random>

#include "types.h"
#include "maths.h"

extern real32 perlin(V2 p);
extern void seed_perlin(std::mt19937 &rng);

#endif