#include "maths.h"

V3& operator+=(V3 &v, V3 w)
{
    v.x += w.x;
    v.y += w.y;
    v.z += w.z;
    return v;
}

V3 &operator+=(V3 &v, real32 w)
{
	v.x += w;
	v.y += w;
	v.z += w;
	return v;
}

V3& operator-=(V3 &v, V3 w)
{
    v.x -= w.x;
    v.y -= w.y;
    v.z -= w.z;
    return v;
}

bool32 operator==(V3 v, V3 w)
{
	return v.x == w.x
		&& v.y == w.y
		&& v.z == w.z;
}

V3 operator+(const V3 &v, const V3 &w)
{
	return {
		v.x + w.x,
		v.y + w.y,
		v.z + w.z
	};
}

V3 operator-(const V3 &v, const V3 &w)
{
	return {
		v.x - w.x,
		v.y - w.y,
		v.z - w.z
	};
}

V3 operator*(const V3 &v, const real32 s)
{
    return {
        v.x * s,
        v.y * s,
        v.z * s
    };
}

V3 operator*(const real32 s, const V3 &v)
{
    return v * s;
}

V2 operator+(const V2 &v, const V2 &w)
{
	return {
		v.x + w.x,
		v.y + w.y
	};
}

V2 operator-(const V2 &v, const V2 &w)
{
	return {
		v.x - w.x,
		v.y - w.y
	};
}

V2 operator+(const V2 &v, const real32 &a)
{
	return {
		v.x + a,
		v.y + a
	};
}

V2 operator-(const V2 &v, const real32 &a)
{
	return {
		v.x - a,
		v.y - a
	};
}

V2 operator*(const V2 &v, const real32& a)
{
	return {
		v.x * a,
		v.y * a
	};
}

V2 operator*(const real32& a, const V2 &v)
{
	return v * a;
}

real32 radians(const real32 degrees)
{
	return degrees * (real32)(M_PI / 180.0);
}

V3 v3_normalise(V3 v)
{
	real32 magnitude = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
	return {
		v.x / magnitude,
		v.y / magnitude,
		v.z / magnitude,
	};
}

real32 v3_dot(V3 a, V3 b)
{
	return a.x * b.x + a.y * b.y + a.z * b.z;
}

real32 v2_dot(V2 a, V2 b)
{
	return a.x * b.x + a.y * b.y;
}

V3 v3_cross(V3 a, V3 b)
{
	return {
		(a.y * b.z) - (a.z * b.y),
		(a.z * b.x) - (a.x * b.z),
		(a.x * b.y) - (a.y * b.x)
	};
}

void mat4_copy(real32* dest, real32* src)
{
	for (u32 i = 0; i < 16; i++) {
		dest[i] = src[i];
	}
}

void mat4_multiply(real32* result, const real32* lhs, const real32* rhs)
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

void mat4_translate(real32* matrix, const real32 tx, const real32 ty, const real32 tz)
{
	matrix[12] += (matrix[0] * tx) + (matrix[4] * ty) + (matrix[8]  * tz);
	matrix[13] += (matrix[1] * tx) + (matrix[5] * ty) + (matrix[9]  * tz);
	matrix[14] += (matrix[2] * tx) + (matrix[6] * ty) + (matrix[10] * tz);
}

void mat4_remove_translation(real32* matrix)
{
	matrix[12] = 0.f;
	matrix[13] = 0.f;
	matrix[14] = 0.f;
}

void mat4_scale(real32* matrix, const real32 sx, const real32 sy, const real32 sz)
{
	for (u32 i = 0; i < 4; ++i) {
		matrix[i]     *= sx;
		matrix[i + 4] *= sy;
		matrix[i + 8] *= sz;
	}
}

void mat4_rotate_x(real32* matrix, const real32 degs)
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

void mat4_rotate_y(real32* matrix, const real32 degs)
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

void mat4_rotate_z(real32* matrix, const real32 degs)
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

void mat4_identity(real32* matrix)
{
	for (u8 i = 0; i < 16; i++) {
		matrix[i] = 0;
	}

	matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
}

void mat4_ortho(real32* matrix, real32 left, real32 right, real32 bottom, real32 top, real32 near, real32 far)
{
	mat4_identity(matrix);
	matrix[0] = 2.f / (right - left);
	matrix[5] = 2.f / (top - bottom);
	matrix[10] = -1.f * 2.f / (far - near);
	matrix[12] = -1.f * (right + left) / (right - left);
	matrix[13] = -1.f * (top + bottom) / (top - bottom);
	matrix[14] = -1.f * (far + near) / (far - near);
}

void mat4_frustrum(real32* matrix, real32 left, real32 right, real32 bottom, real32 top, real32 near, real32 far)
{
	mat4_identity(matrix);
	matrix[0] = (2 * near) / (right - left);
	matrix[5] = (2 * near) / (top - bottom);
	matrix[8] = (right + left) / (right - left);
	matrix[9] = (top + bottom) / (top - bottom);
	matrix[10] = - 1 * (far + near) / (far - near);
	matrix[11] = - 1;
	matrix[14] = - (2 * far * near) / (far - near);
}

void mat4_look_at(real32* matrix, V3 eye, V3 centre, V3 up)
{
	V3 F, T, S, U;
	
	F = v3_normalise(centre - eye);
	T = v3_normalise(up);
	S = v3_normalise(v3_cross(F, T));
	U = v3_normalise(v3_cross(S, F));

	mat4_identity(matrix);
	matrix[0] = S.x;
	matrix[1] = U.x;
	matrix[2] = -F.x;
	matrix[4] = S.y;
	matrix[5] = U.y;
	matrix[6] = -F.y;
	matrix[8] = S.z;
	matrix[9] = U.z;
	matrix[10] = -F.z;
	matrix[12] = -v3_dot(S, eye);
	matrix[13] = -v3_dot(U, eye);
	matrix[14] = v3_dot(F, eye);
}