#pragma once
#include <cmath>
#include <algorithm>

struct V3 {
  float x, y, z;
};

inline V3 v3(float x, float y, float z) { return {x, y, z}; }
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline V3 operator*(float s, V3 a) { return a * s; }
inline V3& operator+=(V3& a, V3 b) { a.x += b.x; a.y += b.y; a.z += b.z; return a; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(V3 a) {
  float l = length(a);
  return l > 1e-6f ? a * (1.0f / l) : v3(0, 0, 0);
}
inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float smoothstepf(float a, float b, float x) {
  float t = clampf((x - a) / (b - a), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

struct Mat4 {
  float m[16];
};

inline Mat4 mat_identity() {
  Mat4 r{};
  r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
  return r;
}

inline Mat4 mat_mul(const Mat4& a, const Mat4& b) {
  Mat4 r{};
  for (int c = 0; c < 4; ++c) {
    for (int row = 0; row < 4; ++row) {
      float s = 0.0f;
      for (int k = 0; k < 4; ++k) s += a.m[k * 4 + row] * b.m[c * 4 + k];
      r.m[c * 4 + row] = s;
    }
  }
  return r;
}

inline Mat4 mat_translate(V3 t) {
  Mat4 r = mat_identity();
  r.m[12] = t.x;
  r.m[13] = t.y;
  r.m[14] = t.z;
  return r;
}

inline Mat4 mat_scale(V3 s) {
  Mat4 r = mat_identity();
  r.m[0] = s.x;
  r.m[5] = s.y;
  r.m[10] = s.z;
  return r;
}

inline Mat4 mat_rot_y(float a) {
  Mat4 r = mat_identity();
  float c = std::cos(a), s = std::sin(a);
  r.m[0] = c;
  r.m[2] = -s;
  r.m[8] = s;
  r.m[10] = c;
  return r;
}

inline Mat4 mat_rot_x(float a) {
  Mat4 r = mat_identity();
  float c = std::cos(a), s = std::sin(a);
  r.m[5] = c;
  r.m[6] = s;
  r.m[9] = -s;
  r.m[10] = c;
  return r;
}

inline Mat4 mat_perspective(float fovy, float aspect, float znear, float zfar) {
  Mat4 r{};
  float f = 1.0f / std::tan(fovy * 0.5f);
  r.m[0] = f / aspect;
  r.m[5] = f;
  r.m[10] = (zfar + znear) / (znear - zfar);
  r.m[11] = -1.0f;
  r.m[14] = (2.0f * zfar * znear) / (znear - zfar);
  return r;
}

inline Mat4 mat_look_at(V3 eye, V3 center, V3 up) {
  V3 f = normalize(center - eye);
  V3 s = normalize(cross(f, up));
  V3 u = cross(s, f);

  Mat4 r = mat_identity();
  r.m[0] = s.x;  r.m[4] = s.y;  r.m[8] = s.z;   r.m[12] = -dot(s, eye);
  r.m[1] = u.x;  r.m[5] = u.y;  r.m[9] = u.z;   r.m[13] = -dot(u, eye);
  r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z; r.m[14] = dot(f, eye);
  return r;
}

inline V3 mat_point(const Mat4& m, V3 p) {
  return {
      m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
      m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
      m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14],
  };
}

struct AABB {
  V3 mn, mx;
};

inline bool aabb_overlap(const AABB& a, const AABB& b) {
  return a.mn.x <= b.mx.x && a.mx.x >= b.mn.x &&
         a.mn.y <= b.mx.y && a.mx.y >= b.mn.y &&
         a.mn.z <= b.mx.z && a.mx.z >= b.mn.z;
}

struct Frustum {
  float p[6][4];
};

inline Frustum frustum_from(const Mat4& vp) {
  Frustum f{};
  for (int i = 0; i < 4; ++i) {
    f.p[0][i] = vp.m[i * 4 + 0] + vp.m[i * 4 + 3];
    f.p[1][i] = vp.m[i * 4 + 3] - vp.m[i * 4 + 0];
    f.p[2][i] = vp.m[i * 4 + 1] + vp.m[i * 4 + 3];
    f.p[3][i] = vp.m[i * 4 + 3] - vp.m[i * 4 + 1];
    f.p[4][i] = vp.m[i * 4 + 2] + vp.m[i * 4 + 3];
    f.p[5][i] = vp.m[i * 4 + 3] - vp.m[i * 4 + 2];
  }
  for (auto& pl : f.p) {
    float len = std::sqrt(pl[0] * pl[0] + pl[1] * pl[1] + pl[2] * pl[2]);
    if (len > 1e-6f) {
      pl[0] /= len;
      pl[1] /= len;
      pl[2] /= len;
      pl[3] /= len;
    }
  }
  return f;
}

inline bool aabb_visible(const Frustum& f, const AABB& b) {
  for (const auto& pl : f.p) {
    float px = pl[0] >= 0 ? b.mx.x : b.mn.x;
    float py = pl[1] >= 0 ? b.mx.y : b.mn.y;
    float pz = pl[2] >= 0 ? b.mx.z : b.mn.z;
    if (pl[0] * px + pl[1] * py + pl[2] * pz + pl[3] < 0.0f) return false;
  }
  return true;
}

// Slab ray test; returns entry distance or -1.
inline float ray_aabb(V3 o, V3 d, const AABB& b, float maxT) {
  float tmin = 0.0f, tmax = maxT;
  for (int i = 0; i < 3; ++i) {
    float oi = i == 0 ? o.x : (i == 1 ? o.y : o.z);
    float di = i == 0 ? d.x : (i == 1 ? d.y : d.z);
    float mn = i == 0 ? b.mn.x : (i == 1 ? b.mn.y : b.mn.z);
    float mx = i == 0 ? b.mx.x : (i == 1 ? b.mx.y : b.mx.z);
    if (std::fabs(di) < 1e-6f) {
      if (oi < mn || oi > mx) return -1.0f;
    } else {
      float inv = 1.0f / di;
      float t1 = (mn - oi) * inv;
      float t2 = (mx - oi) * inv;
      if (t1 > t2) std::swap(t1, t2);
      tmin = std::max(tmin, t1);
      tmax = std::min(tmax, t2);
      if (tmin > tmax) return -1.0f;
    }
  }
  return tmin;
}
