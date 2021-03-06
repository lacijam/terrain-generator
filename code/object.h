#ifndef OBJECT_H
#define OBJECT_H

#include <vector>

#include "types.h"
#include "maths.h"

struct app_state;

struct SpookyVertex {
    V3 pos;
    V3 nor;
};

struct Poly {
    u32 indices[3];
};

struct Object {
    u32 vbos[2];
    std::vector<SpookyVertex *> vertices;
    std::vector<Poly*> polygons;
};

extern Object *load_object(const char *filename);
extern void create_vbos(Object *obj);
extern void draw_object(Object *obj, app_state *state, float *model);

inline real32 atof_ex(const char *text) { return (real32)atof(text); }
inline u32 atoi_ex(const char *text) { return (u32)(atoi(text) - 1); }

#endif