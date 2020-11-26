#include <math.h>
#include <memory.h>

/*
  All 4x4 matrices are assumed to be stored as a 1D array as illustrated below:
  m[0] m[4] m[8]  m[12]
  m[1] m[5] m[9]  m[13]
  m[2] m[6] m[10] m[14]
  m[3] m[7] m[11] m[15]
  */
namespace Matrix {
	struct v3 {
		float x, y, z;
		v3& operator+=(v3 &v)
		{
			this->x += v.x;
			this->y += v.y;
			this->z += v.z;
			return *this;
		}
		v3 operator+(v3 &v)
		{
			return {
				x + v.x,
				y + v.y,
				z + v.z
			};
		}
		v3& operator-=(v3 &v)
		{
			this->x -= v.x;
			this->y -= v.y;
			this->z -= v.z;
			return *this;
		}
		v3 operator-(v3 &v)
		{
			return {
				x - v.x,
				y - v.y,
				z - v.z
			};
		}
		v3 operator*(float s)
		{
			return {
				x * s,
				y * s,
				z * s
			};
		}
	};

	extern v3 operator*(float x, const v3& v);
	extern void identity(float* matrix);
	extern void multiply(float* result, const float* lhs, const float* rhs);
	extern void translate(float* matrix, const float tx, const float ty, const float tz);
	extern void scale(float* matrix, const float sx, const float sy, const float sz);
	extern void rotate_x(float* matrix, const float degs);
	extern void rotate_y(float* matrix, const float degs);
	extern void rotate_z(float* matrix, const float degs);
	extern void ortho(float* matrix, float left, float right, float bottom, float top, float near, float far);
	extern void frustrum(float* matrix, float left, float right, float bottom, float top, float near, float far);
	extern void look_at(float* matrix, v3 eye, v3 centre, v3 up);
	extern float radians(const float degrees);
	// TODO: Change the parameters to v3
	extern float dot(float x1, float y1, float z1, float x2, float y2, float z2);
	extern v3 cross(float x1, float y1, float z1, float x2, float y2, float z2);
	extern v3 normalise(v3 v);
}