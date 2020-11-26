#include "matrix.h"

namespace Matrix {
	void multiply(float* result, const float* lhs, const float* rhs)
	{
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				float n = 0.f;
				
				for (int k = 0; k < 4; ++k) {
					n += lhs[i + k * 4] * rhs[k + j * 4];
				}

				result[i + j * 4] = n;
			}
		}
	}

	void translate(float* matrix, const float tx, const float ty, const float tz)
	{
		matrix[12] += (matrix[0] * tx) + (matrix[4] * ty) + (matrix[8]  * tz);
		matrix[13] += (matrix[1] * tx) + (matrix[5] * ty) + (matrix[9]  * tz);
		matrix[14] += (matrix[2] * tx) + (matrix[6] * ty) + (matrix[10] * tz);
	}

	void scale(float* matrix, const float sx, const float sy, const float sz)
	{
		for (int i = 0; i < 4; ++i) {
			matrix[i]     *= sx;
			matrix[i + 4] *= sy;
			matrix[i + 8] *= sz;
		}
	}

	void rotate_x(float* matrix, const float degs)
	{
		const float rads = radians(degs);
		const float sin_t = sinf(rads);
		const float cos_t = cosf(rads);

		for (int i = 0; i < 4; ++i) {
			const float a = matrix[i + 4];
			const float b = matrix[i + 8];
			matrix[i + 4] = a * cos_t + b * sin_t;
			matrix[i + 8] = b * cos_t - a * sin_t;
		}
	}

	void rotate_y(float* matrix, const float degs)
	{
		const float rads = radians(degs);
		const float sin_t = sinf(rads);
		const float cos_t = cosf(rads);

		for (int i = 0; i < 4; ++i) {
			const float a = matrix[i];
			const float b = matrix[i + 8];
			matrix[i]     = a * cos_t - b * sin_t;
			matrix[i + 8] = a * sin_t + b * cos_t;
		}
	}

	void rotate_z(float* matrix, const float degs)
	{
		const float rads = radians(degs);
		const float sin_t = sinf(rads);
		const float cos_t = cosf(rads);

		for (int i = 0; i < 4; ++i) {
			const float a = matrix[i];
			const float b = matrix[i + 4];
			matrix[i]     = a * cos_t + b * sin_t;
			matrix[i + 4] = b * cos_t - a * sin_t;
		}
	}

	void ortho(float* matrix, float left, float right, float bottom, float top, float near, float far)
	{
		identity(matrix);
		matrix[0] = 2.f / (right - left);
		matrix[5] = 2.f / (top - bottom);
		matrix[10] = -1.f * 2.f / (far - near);
		matrix[12] = -1.f * (right + left) / (right - left);
		matrix[13] = -1.f * (top + bottom) / (top - bottom);
		matrix[14] = -1.f * (far + near) / (far - near);
	}

	void frustrum(float* matrix, float left, float right, float bottom, float top, float near, float far)
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

	extern void look_at(float* matrix, v3 eye, v3 centre, v3 up)
	{
		v3 F, T, S, U;
		F = normalise(centre - eye);
		T = normalise(up);
		S = normalise(cross(F.x, F.y, F.z, T.x, T.y, T.z));
		U = normalise(cross(S.x, S.y, S.z, F.x, F.y, F.z));
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
		matrix[12] = -dot(S.x, S.y, S.z, eye.x, eye.y, eye.z);
		matrix[13] = -dot(U.x, U.y, U.z, eye.x, eye.y, eye.z);
		matrix[14] = dot(F.x, F.y, F.z, eye.x, eye.y, eye.z);
	}

	float radians(const float degrees)
	{
		return degrees * (float)(M_PI / 180.0);
	}

	extern v3 normalise(v3 v)
	{
		float magnitude = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return {
			v.x / magnitude,
			v.y / magnitude,
			v.z / magnitude,
		};
	}

	void identity(float* matrix)
	{
		memset(matrix, 0, sizeof(float) * 16);
		matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1;
	}
	
	float dot(float x1, float y1, float z1, float x2, float y2, float z2)
	{
		return x1 * x2 + y1 * y2 + z1 * z2;
	}

	v3 cross(float x1, float y1, float z1, float x2, float y2, float z2)
	{
		return {
			(y1 * z2) - (z1 * y2),
			(z1 * x2) - (x1 * z2),
			(x1 * y2) - (y1 * x2)
		};
	}

	v3 operator*(float x, const v3& v)
	{
		return {
			v.x * x,
			v.y * x,
			v.z * x
		};
	}
}