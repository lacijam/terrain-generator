#pragma once

struct V3 {
	union {
		real32 E[3];
		struct {
    		float x, y, z;
		};
	};
};

static V3 operator*(const V3 &v, const real32 s);
static V3 operator*(const real32 s, const V3 &v);
static V3& operator+=(V3 &v, V3 &w);
static V3& operator-=(V3 &v, V3 &w);
static V3 operator+(const V3 &v, const V3 &w);
static V3 operator-(const V3 &v, const V3 &w);

namespace Matrix {
	static void update_mvp(float *view, float *frustrum, float *mvp);
	static void identity(float* matrix);
	static void multiply(float* result, const float* lhs, const float* rhs);
	static void translate(float* matrix, const float tx, const float ty, const float tz);
	static void scale(float* matrix, const float sx, const float sy, const float sz);
	static void rotate_x(float* matrix, const float degs);
	static void rotate_y(float* matrix, const float degs);
	static void rotate_z(float* matrix, const float degs);
	static void ortho(float* matrix, float left, float right, float bottom, float top, float near, float far);
	static void frustrum(float* matrix, float left, float right, float bottom, float top, float near, float far);
	static void look_at(float* matrix, V3 eye, V3 centre, V3 up);
	static float radians(const float degrees);
	static float dot(V3 a, V3 b);
	static V3 cross(V3 a, V3 b);
	static V3 normalise(V3 v);
}
