#pragma once

struct V3 {
    float x, y, z;
    V3& operator+=(V3 &v);
    V3& operator-=(V3 &v);
    V3 operator+(V3 &v);
    V3 operator-(V3 &v);
};

V3 operator*(const V3 &v, const float s);
V3 operator*(const float s, const V3 &v);
