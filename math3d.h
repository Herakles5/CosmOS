#ifndef MATH3D_H
#define MATH3D_H

#include <math.h>

struct vec2 {
    float x, y;
    vec2() : x(0), y(0) {}
    vec2(float x, float y) : x(x), y(y) {}
};

struct vec3 {
    float x, y, z;
    vec3() : x(0), y(0), z(0) {}
    vec3(float x, float y, float z) : x(x), y(y), z(z) {}
    vec3 operator+(const vec3& o) const { return vec3(x + o.x, y + o.y, z + o.z); }
    vec3 operator*(float s) const { return vec3(x * s, y * s, z * s); }
};

struct quat {
    float x, y, z, w;
    quat() : x(0), y(0), z(0), w(1) {}
    quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

struct mat4 {
    float m[16];
    mat4() {
        for(int i=0; i<16; i++) m[i] = (i%5 == 0) ? 1.0f : 0.0f;
    }
};

inline quat quat_slerp(quat q1, quat q2, float t) {
    float cosHalfTheta = q1.x*q2.x + q1.y*q2.y + q1.z*q2.z + q1.w*q2.w;
    if (cosHalfTheta < 0) {
        q2.x = -q2.x; q2.y = -q2.y; q2.z = -q2.z; q2.w = -q2.w;
        cosHalfTheta = -cosHalfTheta;
    }
    if (cosHalfTheta >= 1.0f) {
        return q1;
    }
    float halfTheta = acosf(cosHalfTheta);
    float sinHalfTheta = sqrtf(1.0f - cosHalfTheta*cosHalfTheta);
    if (fabs(sinHalfTheta) < 0.001f) {
        return quat(
            q1.x*0.5f + q2.x*0.5f,
            q1.y*0.5f + q2.y*0.5f,
            q1.z*0.5f + q2.z*0.5f,
            q1.w*0.5f + q2.w*0.5f
        );
    }
    float ratioA = sinf((1 - t) * halfTheta) / sinHalfTheta;
    float ratioB = sinf(t * halfTheta) / sinHalfTheta;
    return quat(
        q1.x*ratioA + q2.x*ratioB,
        q1.y*ratioA + q2.y*ratioB,
        q1.z*ratioA + q2.z*ratioB,
        q1.w*ratioA + q2.w*ratioB
    );
}

inline vec3 vec3_lerp(vec3 v1, vec3 v2, float t) {
    return vec3(
        v1.x + (v2.x - v1.x) * t,
        v1.y + (v2.y - v1.y) * t,
        v1.z + (v2.z - v1.z) * t
    );
}

inline mat4 mat4_mul(const mat4& a, const mat4& b) {
    mat4 res;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            res.m[c*4 + r] = 
                a.m[0*4 + r] * b.m[c*4 + 0] +
                a.m[1*4 + r] * b.m[c*4 + 1] +
                a.m[2*4 + r] * b.m[c*4 + 2] +
                a.m[3*4 + r] * b.m[c*4 + 3];
        }
    }
    return res;
}

inline vec3 mat4_mul_vec3(const mat4& m, const vec3& v) {
    return vec3(
        m.m[0]*v.x + m.m[4]*v.y + m.m[8]*v.z + m.m[12],
        m.m[1]*v.x + m.m[5]*v.y + m.m[9]*v.z + m.m[13],
        m.m[2]*v.x + m.m[6]*v.y + m.m[10]*v.z + m.m[14]
    );
}

inline mat4 mat4_from_trs(vec3 t, quat q, vec3 s) {
    mat4 m;
    float qxx = q.x * q.x;
    float qyy = q.y * q.y;
    float qzz = q.z * q.z;
    float qxz = q.x * q.z;
    float qxy = q.x * q.y;
    float qyz = q.y * q.z;
    float qwx = q.w * q.x;
    float qwy = q.w * q.y;
    float qwz = q.w * q.z;

    m.m[0] = s.x * (1.0f - 2.0f * (qyy +  qzz));
    m.m[1] = s.x * (2.0f * (qxy + qwz));
    m.m[2] = s.x * (2.0f * (qxz - qwy));
    m.m[3] = 0.0f;

    m.m[4] = s.y * (2.0f * (qxy - qwz));
    m.m[5] = s.y * (1.0f - 2.0f * (qxx +  qzz));
    m.m[6] = s.y * (2.0f * (qyz + qwx));
    m.m[7] = 0.0f;

    m.m[8] = s.z * (2.0f * (qxz + qwy));
    m.m[9] = s.z * (2.0f * (qyz - qwx));
    m.m[10] = s.z * (1.0f - 2.0f * (qxx +  qyy));
    m.m[11] = 0.0f;

    m.m[12] = t.x;
    m.m[13] = t.y;
    m.m[14] = t.z;
    m.m[15] = 1.0f;

    return m;
}

#endif
