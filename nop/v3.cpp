#include "V3.h"

V3& V3::operator+=(V3 &v)
{
    this->x += v.x;
    this->y += v.y;
    this->z += v.z;
    return *this;
}

V3& V3::operator-=(V3 &v)
{
    this->x -= v.x;
    this->y -= v.y;
    this->z -= v.z;
    return *this;
}

V3 V3::operator+(V3 &v)
{
    return {
        x + v.x,
        y + v.y,
        z + v.z
    };
}

V3 V3::operator-(V3 &v)
{
    return {
        x - v.x,
        y - v.y,
        z - v.z
    };
}

V3 operator*(const V3 &v, const float s)
{
    return {
        v.x * s,
        v.y * s,
        v.z * s
    };
}

V3 operator*(const float s, const V3 &v)
{
    return v * s;
}
