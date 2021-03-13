#ifndef MATHS_H
#define MATHS_H

#include <math.h>

#include "types.h"

struct V4 {
	union {
		real32 E[4];
		struct {
    		real32 x, y, z, w;
		};
	};
};

struct V3 {
	union {
		real32 E[3];
		struct {
    		real32 x, y, z;
		};
	};
};

struct V2 {
	union {
		real32 E[2];
		struct {
			real32 x,y;
		};
	};
};

// Vector functions.
extern V3& operator+=(V3 &v, V3 w);
extern V3 &operator+=(V3 &v, real32 w);
extern V3& operator-=(V3 &v, V3 w);
extern bool32 operator==(V3 v, V3 w);
extern V3 operator+(const V3 &v, const V3 &w);
extern V3 operator-(const V3 &v, const V3 &w);
extern V3 operator*(const V3 &v, const real32 s);
extern V3 operator*(const real32 s, const V3 &v);
extern V2 operator+(const V2 &v, const V2 &w);
extern V2 operator-(const V2 &v, const V2 &w);
extern V2 operator+(const V2 &v, const real32 &a);
extern V2 operator-(const V2 &v, const real32 &a);
extern V2 operator*(const V2 &v, const real32& a);
extern V2 operator*(const real32& a, const V2 &v);

extern real32 radians(const real32 degrees);

extern V3 v3_normalise(V3 v);
extern real32 v3_dot(V3 a, V3 b);
extern real32 v2_dot(V2 a, V2 b);
extern V3 v3_cross(V3 a, V3 b);

// Matrix functions.
extern void mat4_copy(real32* dest, real32* src);
extern void mat4_multiply(real32* result, const real32* lhs, const real32* rhs);
extern void mat4_translate(real32* matrix, const real32 tx, const real32 ty, const real32 tz);
extern void mat4_remove_translation(real32* matrix);
extern void mat4_scale(real32* matrix, const real32 sx, const real32 sy, const real32 sz);
extern void mat4_rotate_x(real32* matrix, const real32 degs);
extern void mat4_rotate_y(real32* matrix, const real32 degs);
extern void mat4_rotate_z(real32* matrix, const real32 degs);
extern void mat4_identity(real32* matrix);
extern void mat4_ortho(real32* matrix, real32 left, real32 right, real32 bottom, real32 top, real32 near, real32 far);
extern void mat4_frustrum(real32* matrix, real32 left, real32 right, real32 bottom, real32 top, real32 near, real32 far);
extern void mat4_look_at(real32* matrix, V3 eye, V3 centre, V3 up);
#endif