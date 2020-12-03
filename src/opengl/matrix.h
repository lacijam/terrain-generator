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
	void update_mvp(float *view, float *frustrum, float *mvp);
	void identity(float* matrix);
	void multiply(float* result, const float* lhs, const float* rhs);
	void translate(float* matrix, const float tx, const float ty, const float tz);
	void scale(float* matrix, const float sx, const float sy, const float sz);
	void rotate_x(float* matrix, const float degs);
	void rotate_y(float* matrix, const float degs);
	void rotate_z(float* matrix, const float degs);
	void ortho(float* matrix, float left, float right, float bottom, float top, float near, float far);
	void frustrum(float* matrix, float left, float right, float bottom, float top, float near, float far);
	void look_at(float* matrix, V3 eye, V3 centre, V3 up);
	float radians(const float degrees);
	float dot(V3 a, V3 b);
	V3 cross(V3 a, V3 b);
	V3 normalise(V3 v);
}