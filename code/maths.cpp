#include "maths.h"

static V3& operator+=(V3 &v, V3 &w)
{
    v.x += w.x;
    v.y += w.y;
    v.z += w.z;
    return v;
}

static V3& operator-=(V3 &v, V3 &w)
{
    v.x -= w.x;
    v.y -= w.y;
    v.z -= w.z;
    return v;
}

static V3 operator+(const V3 &v, const V3 &w)
{
	return {
		v.x + w.x,
		v.y + w.y,
		v.z + w.z
	};
}

static V3 operator-(const V3 &v, const V3 &w)
{
	return {
		v.x - w.x,
		v.y - w.y,
		v.z - w.z
	};
}

static V3 operator*(const V3 &v, const real32 s)
{
    return {
        v.x * s,
        v.y * s,
        v.z * s
    };
}

static V3 operator*(const real32 s, const V3 &v)
{
    return v * s;
}

static real32 radians(const real32 degrees)
{
	return degrees * (real32)(M_PI / 180.0);
}

static V3 normalise(V3 v)
{
	real32 magnitude = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return {
		v.x / magnitude,
		v.y / magnitude,
		v.z / magnitude,
	};
}

static real32 dot(V3 a, V3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

static V3 cross(V3 a, V3 b)
{
	return {
		(a.y * b.z) - (a.z * b.y),
		(a.z * b.x) - (a.x * b.z),
		(a.x * b.y) - (a.y * b.x)
	};
}

static void multiply(real32* result, const real32* lhs, const real32* rhs)
{
	for (u32 i = 0; i < 4; ++i) {
		for (u32 j = 0; j < 4; ++j) {
			real32 n = 0.f;
			
			for (u32 k = 0; k < 4; ++k) {
				n += lhs[i + k * 4] * rhs[k + j * 4];
			}

			result[i + j * 4] = n;
		}
	}
}

static void translate(real32* matrix, const real32 tx, const real32 ty, const real32 tz)
{
	matrix[12] += (matrix[0] * tx) + (matrix[4] * ty) + (matrix[8]  * tz);
	matrix[13] += (matrix[1] * tx) + (matrix[5] * ty) + (matrix[9]  * tz);
	matrix[14] += (matrix[2] * tx) + (matrix[6] * ty) + (matrix[10] * tz);
}

static void scale(real32* matrix, const real32 sx, const real32 sy, const real32 sz)
{
	for (u32 i = 0; i < 4; ++i) {
		matrix[i]     *= sx;
		matrix[i + 4] *= sy;
		matrix[i + 8] *= sz;
	}
}

static void rotate_x(real32* matrix, const real32 degs)
{
	const real32 rads = radians(degs);
	const real32 sin_t = sinf(rads);
	const real32 cos_t = cosf(rads);

	for (u32 i = 0; i < 4; ++i) {
		const real32 a = matrix[i + 4];
		const real32 b = matrix[i + 8];
		matrix[i + 4] = a * cos_t + b * sin_t;
		matrix[i + 8] = b * cos_t - a * sin_t;
	}
}

static void rotate_y(real32* matrix, const real32 degs)
{
	const real32 rads = radians(degs);
	const real32 sin_t = sinf(rads);
	const real32 cos_t = cosf(rads);

	for (u32 i = 0; i < 4; ++i) {
		const real32 a = matrix[i];
		const real32 b = matrix[i + 8];
		matrix[i]     = a * cos_t - b * sin_t;
		matrix[i + 8] = a * sin_t + b * cos_t;
	}
}

static void rotate_z(real32* matrix, const real32 degs)
{
	const real32 rads = radians(degs);
	const real32 sin_t = sinf(rads);
	const real32 cos_t = cosf(rads);

	for (u32 i = 0; i < 4; ++i) {
		const real32 a = matrix[i];
		const real32 b = matrix[i + 4];
		matrix[i]     = a * cos_t + b * sin_t;
		matrix[i + 4] = b * cos_t - a * sin_t;
	}
}

static void identity(real32* matrix)
{
	for (u8 i = 0; i < 16; i++) {
		matrix[i] = 0;
	}

	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
}

static void ortho(real32* matrix, real32 left, real32 right, real32 bottom, real32 top, real32 near, real32 far)
{
	identity(matrix);
	matrix[0] = 2.f / (right - left);
	matrix[5] = 2.f / (top - bottom);
	matrix[10] = -1.f * 2.f / (far - near);
	matrix[12] = -1.f * (right + left) / (right - left);
	matrix[13] = -1.f * (top + bottom) / (top - bottom);
	matrix[14] = -1.f * (far + near) / (far - near);
}

static void frustrum(real32* matrix, real32 left, real32 right, real32 bottom, real32 top, real32 near, real32 far)
{
	identity(matrix);
	matrix[0] = (2 * near) / (right - left);
	matrix[5] = (2 * near) / (top - bottom);
	matrix[8] = (right + left) / (right - left);
	matrix[9] = (top + bottom) / (top - bottom);
	matrix[10] = - 1 * (far + near) / (far - near);
	matrix[11] = - 1;
	matrix[14] = - (2 * far * near) / (far - near);
}

static void look_at(real32* matrix, V3 eye, V3 centre, V3 up)
{
	V3 F, T, S, U;
	F = normalise(centre - eye);
	T = normalise(up);
	S = normalise(cross(F, T));
	U = normalise(cross(S, F));
	identity(matrix);
	matrix[0] = S.x;
	matrix[1] = U.x;
	matrix[2] = -F.x;
	matrix[4] = S.y;
	matrix[5] = U.y;
	matrix[6] = -F.y;
	matrix[8] = S.z;
	matrix[9] = U.z;
	matrix[10] = -F.z;
	matrix[12] = -dot(S, eye);
	matrix[13] = -dot(U, eye);
	matrix[14] = dot(F, eye);
}