#define _USE_MATH_DEFINES
#include <math.h>
#include <memory.h>

/*
  All 4x4 matrices are assumed to be stored as a 1D array as illustrated below:
  m[0] m[4] m[8]  m[12]
  m[1] m[5] m[9]  m[13]
  m[2] m[6] m[10] m[14]
  m[3] m[7] m[11] m[15]
  */
struct V3;

namespace Matrix {
	extern V3 operator*(float x, const V3& v);
	extern void identity(float* matrix);
	extern void multiply(float* result, const float* lhs, const float* rhs);
	extern void translate(float* matrix, const float tx, const float ty, const float tz);
	extern void scale(float* matrix, const float sx, const float sy, const float sz);
	extern void rotate_x(float* matrix, const float degs);
	extern void rotate_y(float* matrix, const float degs);
	extern void rotate_z(float* matrix, const float degs);
	extern void ortho(float* matrix, float left, float right, float bottom, float top, float near, float far);
	extern void frustrum(float* matrix, float left, float right, float bottom, float top, float near, float far);
	extern void look_at(float* matrix, V3 eye, V3 centre, V3 up);
	extern float radians(const float degrees);
	extern float dot(V3 a, V3 b);
	extern V3 cross(V3 a, V3 b);
	extern V3 normalise(V3 v);
}