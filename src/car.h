#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

#include "gfx.h"
#include "math.h"

inline void cquad(Verts& v, V3 a, V3 b, V3 c, V3 d, V3 n, float layer, uint32_t col,
                  float ua = 0, float va = 0, float ub = 1, float vb = 0, float uc = 1,
                  float vc = 1, float ud = 0, float vd = 1) {
  uint32_t base = (uint32_t)v.v.size();
  v.v.push_back({a.x, a.y, a.z, n.x, n.y, n.z, ua, va, layer, col});
  v.v.push_back({b.x, b.y, b.z, n.x, n.y, n.z, ub, vb, layer, col});
  v.v.push_back({c.x, c.y, c.z, n.x, n.y, n.z, uc, vc, layer, col});
  v.v.push_back({d.x, d.y, d.z, n.x, n.y, n.z, ud, vd, layer, col});
  v.i.push_back(base);
  v.i.push_back(base + 1);
  v.i.push_back(base + 2);
  v.i.push_back(base);
  v.i.push_back(base + 2);
  v.i.push_back(base + 3);
}

inline void ctri(Verts& v, V3 a, V3 b, V3 c, V3 n, float layer, uint32_t col) {
  uint32_t base = (uint32_t)v.v.size();
  v.v.push_back({a.x, a.y, a.z, n.x, n.y, n.z, 0, 0, layer, col});
  v.v.push_back({b.x, b.y, b.z, n.x, n.y, n.z, 1, 0, layer, col});
  v.v.push_back({c.x, c.y, c.z, n.x, n.y, n.z, 0.5f, 1, layer, col});
  v.i.push_back(base);
  v.i.push_back(base + 1);
  v.i.push_back(base + 2);
}

inline void cbox(Verts& v, float x0, float z0, float x1, float z1, float y0, float y1,
                 float layer, uint32_t col) {
  cquad(v, {x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, {0, 0, 1}, layer, col);
  cquad(v, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, {x1, y0, z0}, {0, 0, -1}, layer, col);
  cquad(v, {x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, {-1, 0, 0}, layer, col);
  cquad(v, {x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, {1, 0, 0}, layer, col);
  cquad(v, {x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}, {0, 1, 0}, layer, col);
  cquad(v, {x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, {0, -1, 0}, layer, col);
}

struct CarMeshes {
  Mesh body, detail, wheel;
};

inline Mesh make_wheel_mesh() {
  Verts v;
  const int N = 16;
  const float w = 0.30f, r = 0.36f, hub = 0.115f;
  uint32_t tire = tint3(0.11f, 0.11f, 0.12f);
  uint32_t tireSide = tint3(0.08f, 0.08f, 0.09f);
  uint32_t silver = tint3(0.55f, 0.56f, 0.60f);
  for (int i = 0; i < N; ++i) {
    float a0 = i * 6.2831853f / N, a1 = (i + 1) * 6.2831853f / N;
    float am = (a0 + a1) * 0.5f;
    V3 n = {0.0f, std::cos(am), std::sin(am)};
    V3 p0 = {-w * 0.5f, r * std::cos(a0), r * std::sin(a0)};
    V3 p1 = {-w * 0.5f, r * std::cos(a1), r * std::sin(a1)};
    V3 p2 = {w * 0.5f, r * std::cos(a1), r * std::sin(a1)};
    V3 p3 = {w * 0.5f, r * std::cos(a0), r * std::sin(a0)};
    cquad(v, p0, p3, p2, p1, n, L_RUBBER, tire);
    // sidewalls
    V3 i0 = {-w * 0.5f, hub * std::cos(a0), hub * std::sin(a0)};
    V3 i1 = {-w * 0.5f, hub * std::cos(a1), hub * std::sin(a1)};
    V3 o0 = {-w * 0.5f, r * 0.92f * std::cos(a0), r * 0.92f * std::sin(a0)};
    V3 o1 = {-w * 0.5f, r * 0.92f * std::cos(a1), r * 0.92f * std::sin(a1)};
    ctri(v, o0, i0, i1, {-1, 0, 0}, L_RUBBER, tireSide);
    ctri(v, o0, i1, o1, {-1, 0, 0}, L_RUBBER, tireSide);
    V3 j0 = {w * 0.5f, hub * std::cos(a0), hub * std::sin(a0)};
    V3 j1 = {w * 0.5f, hub * std::cos(a1), hub * std::sin(a1)};
    V3 p0s = {w * 0.5f, r * 0.92f * std::cos(a0), r * 0.92f * std::sin(a0)};
    V3 p1s = {w * 0.5f, r * 0.92f * std::cos(a1), r * 0.92f * std::sin(a1)};
    ctri(v, p0s, j0, j1, {1, 0, 0}, L_RUBBER, tireSide);
    ctri(v, p0s, j1, p1s, {1, 0, 0}, L_RUBBER, tireSide);
    // hub caps
    V3 hx0 = {w * 0.52f, hub * std::cos(a0), hub * std::sin(a0)};
    V3 hx1 = {w * 0.52f, hub * std::cos(a1), hub * std::sin(a1)};
    ctri(v, {w * 0.52f, 0, 0}, hx0, hx1, {1, 0, 0}, L_WHITE, silver);
    V3 hz0 = {-w * 0.52f, hub * std::cos(a0), hub * std::sin(a0)};
    V3 hz1 = {-w * 0.52f, hub * std::cos(a1), hub * std::sin(a1)};
    ctri(v, {-w * 0.52f, 0, 0}, hz1, hz0, {-1, 0, 0}, L_WHITE, silver);
    // hub rim wall
    V3 a0w = {w * 0.5f, hub * std::cos(a0), hub * std::sin(a0)};
    V3 a1w = {w * 0.5f, hub * std::cos(a1), hub * std::sin(a1)};
    V3 b0w = {w * 0.52f, hub * std::cos(a0), hub * std::sin(a0)};
    V3 b1w = {w * 0.52f, hub * std::cos(a1), hub * std::sin(a1)};
    cquad(v, a0w, b0w, b1w, a1w, {0, std::cos(am), std::sin(am)}, L_WHITE, silver);
    // dark hub recess + center cap
    uint32_t hubDark = tint3(0.13f, 0.13f, 0.14f);
    float xr = w * 0.545f;
    ctri(v, {xr, 0, 0}, {xr, 0.068f * std::cos(a0), 0.068f * std::sin(a0)},
         {xr, 0.068f * std::cos(a1), 0.068f * std::sin(a1)}, {1, 0, 0}, L_WHITE, hubDark);
    ctri(v, {-xr, 0, 0}, {-xr, 0.068f * std::cos(a1), 0.068f * std::sin(a1)},
         {-xr, 0.068f * std::cos(a0), 0.068f * std::sin(a0)}, {-1, 0, 0}, L_WHITE, hubDark);
    float xc = w * 0.56f;
    ctri(v, {xc, 0, 0}, {xc, 0.026f * std::cos(a0), 0.026f * std::sin(a0)},
         {xc, 0.026f * std::cos(a1), 0.026f * std::sin(a1)}, {1, 0, 0}, L_WHITE, silver);
    ctri(v, {-xc, 0, 0}, {-xc, 0.026f * std::cos(a1), 0.026f * std::sin(a1)},
         {-xc, 0.026f * std::cos(a0), 0.026f * std::sin(a0)}, {-1, 0, 0}, L_WHITE, silver);
  }
  return mesh_upload(v);
}

inline void car_wheel_arch(Verts& v, float sx, float zc) {
  const int N = 10;
  uint32_t dark = tint3(0.12f, 0.12f, 0.13f);
  float x = 0.896f * sx;
  for (int i = 0; i < N; ++i) {
    float a0 = 0.10f + i * 2.94f / N, a1 = 0.10f + (i + 1) * 2.94f / N;
    V3 i0 = {x, 0.36f + 0.385f * std::sin(a0), zc + 0.385f * std::cos(a0)};
    V3 o0 = {x, 0.36f + 0.435f * std::sin(a0), zc + 0.435f * std::cos(a0)};
    V3 i1 = {x, 0.36f + 0.385f * std::sin(a1), zc + 0.385f * std::cos(a1)};
    V3 o1 = {x, 0.36f + 0.435f * std::sin(a1), zc + 0.435f * std::cos(a1)};
    cquad(v, i0, o0, o1, i1, {sx, 0, 0}, L_WHITE, dark);
  }
}

// Loft builder: rings of a rounded-rect cross-section swept along Z, with smooth normals.
struct RawMesh {
  std::vector<float> pos;
  std::vector<uint32_t> col;
  std::vector<uint32_t> idx;
  uint32_t add(float x, float y, float z, uint32_t c) {
    uint32_t i = (uint32_t)(pos.size() / 3);
    pos.push_back(x);
    pos.push_back(y);
    pos.push_back(z);
    col.push_back(c);
    return i;
  }
  void quad(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    idx.push_back(a);
    idx.push_back(b);
    idx.push_back(c);
    idx.push_back(a);
    idx.push_back(c);
    idx.push_back(d);
  }
};

inline void raw_to_verts(Verts& v, const RawMesh& m) {
  std::vector<float> nrm(m.pos.size(), 0.0f);
  for (size_t i = 0; i < m.idx.size(); i += 3) {
    uint32_t a = m.idx[i], b = m.idx[i + 1], c = m.idx[i + 2];
    V3 pa = {m.pos[a * 3], m.pos[a * 3 + 1], m.pos[a * 3 + 2]};
    V3 pb = {m.pos[b * 3], m.pos[b * 3 + 1], m.pos[b * 3 + 2]};
    V3 pc = {m.pos[c * 3], m.pos[c * 3 + 1], m.pos[c * 3 + 2]};
    V3 n = cross(pb - pa, pc - pa);
    for (uint32_t k : {a, b, c}) {
      nrm[k * 3 + 0] += n.x;
      nrm[k * 3 + 1] += n.y;
      nrm[k * 3 + 2] += n.z;
    }
  }
  uint32_t base = (uint32_t)v.v.size();
  size_t vcount = m.pos.size() / 3;
  for (size_t i = 0; i < vcount; ++i) {
    float nx = nrm[i * 3], ny = nrm[i * 3 + 1], nz = nrm[i * 3 + 2];
    float l = std::sqrt(nx * nx + ny * ny + nz * nz) || 1.0f;
    v.v.push_back({m.pos[i * 3], m.pos[i * 3 + 1], m.pos[i * 3 + 2], nx / l, ny / l, nz / l,
                   0.0f, 0.0f, L_WHITE, m.col[i]});
  }
  for (uint32_t i : m.idx) v.i.push_back(base + i);
}

struct CarRing {
  float z, w, y0, y1, rb, rt;
};

// Rounded-rect ring. Side point at the beltline lets the cabin split paint from glass.
inline uint32_t car_ring(RawMesh& m, const CarRing& r, bool cabin, uint32_t paint,
                         uint32_t glass) {
  const int NB = 3, NT = 4;
  uint32_t first = (uint32_t)(m.pos.size() / 3);
  auto push = [&](float x, float y) {
    uint32_t c = paint;
    if (cabin && y > 0.995f) {
      bool roof = y > 1.31f && r.z > -0.95f && r.z < 0.08f;
      c = roof ? paint : glass;
    }
    m.add(x, y, r.z, c);
  };
  push(0.0f, r.y0);
  for (int i = 1; i <= NB; ++i) {
    float a = -1.5707964f + 1.5707964f * (float)i / NB;
    push(r.w - r.rb + r.rb * std::cos(a), r.y0 + r.rb + r.rb * std::sin(a));
  }
  if (cabin) push(r.w, 0.965f);
  push(r.w, r.y1 - r.rt);
  for (int i = 1; i <= NT; ++i) {
    float a = 1.5707964f * (float)i / NT;
    push(r.w - r.rt + r.rt * std::cos(a), r.y1 - r.rt + r.rt * std::sin(a));
  }
  push(0.0f, r.y1);
  for (int i = NT; i >= 1; --i) {
    float a = 1.5707964f * (float)i / NT;
    push(-(r.w - r.rt + r.rt * std::cos(a)), r.y1 - r.rt + r.rt * std::sin(a));
  }
  push(-r.w, r.y1 - r.rt);
  if (cabin) push(-r.w, 0.965f);
  for (int i = NB; i >= 1; --i) {
    float a = -1.5707964f + 1.5707964f * (float)i / NB;
    push(-(r.w - r.rb + r.rb * std::cos(a)), r.y0 + r.rb + r.rb * std::sin(a));
  }
  return first;
}

inline int car_ring_count(bool cabin) { return cabin ? 20 : 18; }

inline void car_loft(RawMesh& m, const CarRing* rings, int n, bool cabin, uint32_t paint,
                     uint32_t glass) {
  int cnt = car_ring_count(cabin);
  uint32_t prev = car_ring(m, rings[0], cabin, paint, glass);
  // rear cap
  uint32_t rc = m.add(0.0f, (rings[0].y0 + rings[0].y1) * 0.5f, rings[0].z, paint);
  for (int i = 0; i < cnt; ++i) {
    int j = (i + 1) % cnt;
    m.idx.push_back(rc);
    m.idx.push_back(prev + j);
    m.idx.push_back(prev + i);
  }
  for (int k = 1; k < n; ++k) {
    uint32_t cur = car_ring(m, rings[k], cabin, paint, glass);
    for (int i = 0; i < cnt; ++i) {
      int j = (i + 1) % cnt;
      m.quad(prev + i, prev + j, cur + j, cur + i);
    }
    prev = cur;
  }
  uint32_t fc = m.add(0.0f, (rings[n - 1].y0 + rings[n - 1].y1) * 0.5f, rings[n - 1].z, paint);
  for (int i = 0; i < cnt; ++i) {
    int j = (i + 1) % cnt;
    m.idx.push_back(fc);
    m.idx.push_back(prev + i);
    m.idx.push_back(prev + j);
  }
}

inline Mesh make_car_body_mesh() {
  Verts v;
  uint32_t white = tint3(1.0f, 1.0f, 1.0f);
  uint32_t dark = tint3(0.16f, 0.16f, 0.18f);
  uint32_t glass = tint3(0.19f, 0.24f, 0.30f, 0.0f);
  uint32_t trim = tint3(0.55f, 0.57f, 0.60f);

  // lower tub / rocker, tucked under the lofted body
  cbox(v, -0.80f, -1.94f, 0.80f, 1.94f, 0.15f, 0.34f, L_RUBBER, dark);

  RawMesh m;
  static const CarRing body[] = {
      {-2.12f, 0.74f, 0.34f, 0.89f, 0.10f, 0.16f},
      {-1.95f, 0.83f, 0.29f, 0.95f, 0.08f, 0.14f},
      {-1.30f, 0.878f, 0.25f, 0.985f, 0.07f, 0.12f},
      {-0.60f, 0.885f, 0.24f, 0.98f, 0.07f, 0.11f},
      {0.10f, 0.885f, 0.24f, 0.975f, 0.07f, 0.11f},
      {0.80f, 0.885f, 0.24f, 0.97f, 0.07f, 0.11f},
      {1.45f, 0.875f, 0.25f, 0.945f, 0.07f, 0.12f},
      {1.95f, 0.82f, 0.28f, 0.90f, 0.08f, 0.14f},
      {2.12f, 0.72f, 0.34f, 0.84f, 0.10f, 0.16f},
  };
  car_loft(m, body, (int)(sizeof(body) / sizeof(body[0])), false, white, glass);

  static const CarRing cabin[] = {
      {-1.42f, 0.775f, 0.90f, 0.99f, 0.03f, 0.06f},
      {-1.32f, 0.755f, 0.90f, 1.10f, 0.03f, 0.07f},
      {-1.02f, 0.715f, 0.90f, 1.34f, 0.03f, 0.08f},
      {-0.72f, 0.705f, 0.90f, 1.40f, 0.03f, 0.09f},
      {-0.28f, 0.72f, 0.90f, 1.395f, 0.03f, 0.09f},
      {0.04f, 0.745f, 0.90f, 1.33f, 0.03f, 0.09f},
      {0.32f, 0.775f, 0.90f, 1.16f, 0.03f, 0.08f},
      {0.62f, 0.80f, 0.90f, 0.97f, 0.03f, 0.05f},
  };
  car_loft(m, cabin, (int)(sizeof(cabin) / sizeof(cabin[0])), true, white, glass);
  raw_to_verts(v, m);

  for (int s = 0; s < 2; ++s) {
    float sx = s ? 1.0f : -1.0f;
    // A / B / C pillars laid on the cabin sides (slightly proud of the glass)
    auto pillar = [&](float zb, float zt, float wb, float wt, float width) {
      float xb = (wb + 0.008f) * sx, xt = (wt + 0.008f) * sx;
      cquad(v, {xb, 0.97f, zb}, {xb, 0.97f, zb - width}, {xt, 1.335f, zt - width * 0.8f},
            {xt, 1.335f, zt}, {sx, 0, 0}, L_WHITE, white);
    };
    pillar(0.60f, 0.10f, 0.795f, 0.735f, 0.055f);   // A
    pillar(-0.10f, -0.10f, 0.775f, 0.715f, 0.05f);  // B
    pillar(-1.40f, -0.86f, 0.775f, 0.71f, 0.075f);  // C
    // beltline trim
    cquad(v, {0.892f * sx, 0.955f, 0.60f}, {0.892f * sx, 0.955f, -1.36f},
          {0.892f * sx, 0.972f, -1.36f}, {0.892f * sx, 0.972f, 0.60f}, {sx, 0, 0},
          L_WHITE, trim);
    // door seams
    uint32_t seam = tint3(0.44f, 0.44f, 0.46f);
    cquad(v, {0.893f * sx, 0.36f, 0.545f}, {0.893f * sx, 0.36f, 0.532f},
          {0.893f * sx, 0.95f, 0.532f}, {0.893f * sx, 0.95f, 0.545f}, {sx, 0, 0},
          L_WHITE, seam);
    cquad(v, {0.893f * sx, 0.36f, -0.115f}, {0.893f * sx, 0.36f, -0.128f},
          {0.893f * sx, 0.95f, -0.128f}, {0.893f * sx, 0.95f, -0.115f}, {sx, 0, 0},
          L_WHITE, seam);
    cquad(v, {0.893f * sx, 0.36f, -1.205f}, {0.893f * sx, 0.36f, -1.218f},
          {0.893f * sx, 0.95f, -1.218f}, {0.893f * sx, 0.95f, -1.205f}, {sx, 0, 0},
          L_WHITE, seam);
    // wheel arches
    car_wheel_arch(v, sx, 1.32f);
    car_wheel_arch(v, sx, -1.32f);
  }
  return mesh_upload(v);
}

inline Mesh make_car_detail_mesh() {
  Verts v;
  uint32_t bumper = tint3(0.20f, 0.20f, 0.22f);
  uint32_t lightC = tint3(1.0f, 0.97f, 0.85f);
  uint32_t tail = tint3(0.82f, 0.06f, 0.05f);
  uint32_t amber = tint3(1.0f, 0.62f, 0.16f);
  uint32_t plate = tint3(0.93f, 0.94f, 0.90f);
  uint32_t chrome = tint3(0.60f, 0.61f, 0.63f);
  uint32_t black = tint3(0.08f, 0.08f, 0.09f);
  // bumpers
  cbox(v, -0.75f, 2.03f, 0.75f, 2.14f, 0.30f, 0.54f, L_WHITE, bumper);
  cbox(v, -0.75f, -2.14f, 0.75f, -2.03f, 0.30f, 0.54f, L_WHITE, bumper);
  // headlights + grille + plate
  cbox(v, -0.62f, 2.09f, -0.30f, 2.14f, 0.68f, 0.81f, L_EMIT, lightC);
  cbox(v, 0.30f, 2.09f, 0.62f, 2.14f, 0.68f, 0.81f, L_EMIT, lightC);
  cbox(v, -0.28f, 2.10f, 0.28f, 2.135f, 0.52f, 0.66f, L_WHITE, black);
  cbox(v, -0.28f, 2.125f, 0.28f, 2.145f, 0.635f, 0.66f, L_WHITE, chrome);
  cbox(v, -0.20f, 2.14f, 0.20f, 2.15f, 0.38f, 0.48f, L_WHITE, plate);
  cbox(v, -0.60f, 2.14f, -0.50f, 2.15f, 0.38f, 0.44f, L_WHITE, amber);
  cbox(v, 0.50f, 2.14f, 0.60f, 2.15f, 0.38f, 0.44f, L_WHITE, amber);
  // taillights (wrap around corners)
  cbox(v, -0.70f, -2.14f, -0.28f, -2.08f, 0.64f, 0.79f, L_EMIT, tail);
  cbox(v, 0.28f, -2.14f, 0.70f, -2.08f, 0.64f, 0.79f, L_EMIT, tail);
  cbox(v, -0.79f, -2.06f, -0.74f, -1.96f, 0.66f, 0.77f, L_EMIT, tail);
  cbox(v, 0.74f, -2.06f, 0.79f, -1.96f, 0.66f, 0.77f, L_EMIT, tail);
  cbox(v, -0.20f, -2.15f, 0.20f, -2.14f, 0.38f, 0.48f, L_WHITE, plate);
  // mirrors
  cbox(v, -0.93f, 0.50f, -0.86f, 0.60f, 0.96f, 1.00f, L_WHITE, bumper);
  cbox(v, 0.86f, 0.50f, 0.93f, 0.60f, 0.96f, 1.00f, L_WHITE, bumper);
  cbox(v, -1.00f, 0.47f, -0.91f, 0.63f, 0.98f, 1.07f, L_WHITE, chrome);
  cbox(v, 0.91f, 0.47f, 1.00f, 0.63f, 0.98f, 1.07f, L_WHITE, chrome);
  // door handles
  cbox(v, -0.899f, 0.10f, -0.886f, 0.30f, 0.855f, 0.885f, L_WHITE, chrome);
  cbox(v, 0.886f, 0.10f, 0.899f, 0.30f, 0.855f, 0.885f, L_WHITE, chrome);
  cbox(v, -0.899f, -0.64f, -0.886f, -0.44f, 0.855f, 0.885f, L_WHITE, chrome);
  cbox(v, 0.886f, -0.64f, 0.899f, -0.44f, 0.855f, 0.885f, L_WHITE, chrome);
  // side indicators
  cbox(v, -0.872f, 1.70f, -0.858f, 1.80f, 0.72f, 0.76f, L_WHITE, amber);
  cbox(v, 0.858f, 1.70f, 0.872f, 1.80f, 0.72f, 0.76f, L_WHITE, amber);
  // exhaust
  cbox(v, 0.40f, -2.16f, 0.52f, -2.10f, 0.24f, 0.32f, L_WHITE, chrome);
  return mesh_upload(v);
}

inline CarMeshes make_car_meshes() {
  CarMeshes cm;
  cm.body = make_car_body_mesh();
  cm.detail = make_car_detail_mesh();
  cm.wheel = make_wheel_mesh();
  return cm;
}

inline Mesh make_blob_quad() {
  Verts v;
  cquad(v, {-1, 0, 1}, {1, 0, 1}, {1, 0, -1}, {-1, 0, -1}, {0, 1, 0}, L_BLOB,
        tint3(1, 1, 1), 0, 1, 1, 1, 1, 0, 0, 0);
  return mesh_upload(v);
}

inline void append_mat(std::vector<float>& out, const Mat4& m) {
  for (int col = 0; col < 4; ++col) {
    out.push_back(m.m[col * 4 + 0]);
    out.push_back(m.m[col * 4 + 1]);
    out.push_back(m.m[col * 4 + 2]);
    out.push_back(m.m[col * 4 + 3]);
  }
}

inline void add_car_blob(std::vector<float>& out, V3 pos, float yaw, float sx, float sz) {
  Mat4 m = mat_mul(mat_mul(mat_translate(v3(pos.x, 0.035f, pos.z)), mat_rot_y(yaw)),
                   mat_scale(v3(sx, 1.0f, sz)));
  append_mat(out, m);
}

inline void add_round_blob(std::vector<float>& out, V3 pos, float r) {
  Mat4 m = mat_mul(mat_translate(v3(pos.x, 0.04f, pos.z)), mat_scale(v3(r, 1.0f, r)));
  append_mat(out, m);
}

// ------------------------------------------------------------------ draw

struct CarBuffers {
  GLuint body = 0, detail = 0, wheel = 0;
};

inline CarBuffers make_car_buffers() {
  CarBuffers b;
  b.body = make_instance_buffer();
  b.detail = make_instance_buffer();
  b.wheel = make_instance_buffer();
  return b;
}

inline void draw_car(CarProg& prog, const CarMeshes& cm, const CarBuffers& bufs,
                     const V3& pos, float yaw, uint32_t color, float steer, float wheelSpin) {
  Mat4 model = mat_mul(mat_translate(pos), mat_rot_y(yaw));
  float bodyData[20];
  {
    for (int col = 0; col < 4; ++col) {
      bodyData[col * 4 + 0] = model.m[col * 4 + 0];
      bodyData[col * 4 + 1] = model.m[col * 4 + 1];
      bodyData[col * 4 + 2] = model.m[col * 4 + 2];
      bodyData[col * 4 + 3] = model.m[col * 4 + 3];
    }
    bodyData[16] = ((color >> 0) & 255) / 255.0f;
    bodyData[17] = ((color >> 8) & 255) / 255.0f;
    bodyData[18] = ((color >> 16) & 255) / 255.0f;
    bodyData[19] = 1.0f;
    glBindBuffer(GL_ARRAY_BUFFER, bufs.body);
    glBufferData(GL_ARRAY_BUFFER, sizeof(bodyData), bodyData, GL_DYNAMIC_DRAW);
  }
  float detailData[16];
  for (int i = 0; i < 16; ++i) detailData[i] = model.m[i];
  glBindBuffer(GL_ARRAY_BUFFER, bufs.detail);
  glBufferData(GL_ARRAY_BUFFER, sizeof(detailData), detailData, GL_DYNAMIC_DRAW);

  float wheelData[64];
  const float wx[2] = {-0.785f, 0.785f};
  const float wz[2] = {1.32f, -1.32f};
  int k = 0;
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 2; ++j) {
      Mat4 wm = mat_mul(model, mat_translate(v3(wx[i], 0.36f, wz[j])));
      if (j == 0) wm = mat_mul(wm, mat_rot_y(-steer * 0.5f));
      wm = mat_mul(wm, mat_rot_x(wheelSpin));
      for (int c = 0; c < 4; ++c) {
        wheelData[k++] = wm.m[c * 4 + 0];
        wheelData[k++] = wm.m[c * 4 + 1];
        wheelData[k++] = wm.m[c * 4 + 2];
        wheelData[k++] = wm.m[c * 4 + 3];
      }
    }
  }
  glBindBuffer(GL_ARRAY_BUFFER, bufs.wheel);
  glBufferData(GL_ARRAY_BUFFER, sizeof(wheelData), wheelData, GL_DYNAMIC_DRAW);

  glUniform1i(prog.tintMode, 1);
  glUniform1f(prog.spec, 1.0f);
  glBindVertexArray(cm.body.vao);
  glDrawElementsInstanced(GL_TRIANGLES, cm.body.count, GL_UNSIGNED_INT, nullptr, 1);
  glUniform1i(prog.tintMode, 0);
  glBindVertexArray(cm.detail.vao);
  glDrawElementsInstanced(GL_TRIANGLES, cm.detail.count, GL_UNSIGNED_INT, nullptr, 1);
  glUniform1f(prog.spec, 0.0f);
  glBindVertexArray(cm.wheel.vao);
  glDrawElementsInstanced(GL_TRIANGLES, cm.wheel.count, GL_UNSIGNED_INT, nullptr, 4);
}

// ------------------------------------------------------------------ physics

struct Car {
  V3 pos{0.0f, 0.0f, 0.0f};
  float yaw = 0.0f;
  float speed = 0.0f;
  float steerVis = 0.0f;
  float wheelSpin = 0.0f;
};

inline V3 car_forward(float yaw) { return {std::sin(yaw), 0.0f, std::cos(yaw)}; }

inline void resolve_circle(V3& p, float r, const AABB& b, bool& hit) {
  float cx = clampf(p.x, b.mn.x, b.mx.x);
  float cz = clampf(p.z, b.mn.z, b.mx.z);
  float dx = p.x - cx, dz = p.z - cz;
  float d2 = dx * dx + dz * dz;
  if (d2 >= r * r) return;
  hit = true;
  if (d2 > 1e-6f) {
    float d = std::sqrt(d2);
    p.x += dx / d * (r - d);
    p.z += dz / d * (r - d);
  } else {
    float pl = p.x - b.mn.x, pr = b.mx.x - p.x;
    float pt = p.z - b.mn.z, pb = b.mx.z - p.z;
    float m = std::min(std::min(pl, pr), std::min(pt, pb));
    if (m == pl) p.x = b.mn.x - r;
    else if (m == pr) p.x = b.mx.x + r;
    else if (m == pt) p.z = b.mn.z - r;
    else p.z = b.mx.z + r;
  }
}

inline void clamp_world(V3& p, float margin) {
  const float lim = 1660.0f - margin;
  p.x = clampf(p.x, -lim, lim);
  if (std::fabs(p.x) < 12.0f) {
    // corredor da ponte para a Ilha do Cabo
    p.z = clampf(p.z, -1100.0f + margin, lim);
  } else if (p.z < -925.0f) {
    // areal da ilha (fora da ponte)
    p.z = clampf(p.z, -1100.0f + margin, -1022.0f + margin);
  } else {
    p.z = clampf(p.z, -828.0f + margin, lim);
  }
}

inline bool update_car(Car& car, float throttle, float brake, float steer, float handbrake,
                       float dt, const std::vector<AABB>& cols) {
  float accel = throttle > 0.0f ? 13.0f * throttle * (1.0f - clampf(car.speed / 48.0f, 0.0f, 0.94f)) : 0.0f;
  car.speed += accel * dt;

  if (car.speed > 0.15f) {
    car.speed -= brake * 27.0f * dt;
  } else if (brake > 0.0f) {
    car.speed -= 7.0f * brake * dt;
  }
  if (car.speed < -13.0f) car.speed = -13.0f;
  if (car.speed > 55.0f) car.speed = 55.0f;

  float drag = 0.55f + handbrake * 2.6f;
  car.speed -= car.speed * drag * dt;
  if (std::fabs(car.speed) < 0.05f && throttle <= 0.0f && brake <= 0.0f) car.speed = 0.0f;

  float steerAngle = 0.55f * steer / (1.0f + std::fabs(car.speed) * 0.055f);
  car.yaw -= (car.speed / 2.7f) * std::tan(steerAngle) * dt * (1.0f + handbrake * 0.65f);
  car.steerVis += (steer - car.steerVis) * std::min(1.0f, dt * 10.0f);

  V3 f = car_forward(car.yaw);
  car.pos = car.pos + f * (car.speed * dt);

  bool hit = false;
  for (const auto& b : cols) {
    V3 pf = car.pos + f * 1.15f;
    V3 pb = car.pos - f * 1.15f;
    resolve_circle(pf, 0.95f, b, hit);
    resolve_circle(pb, 0.95f, b, hit);
    car.pos = (pf + pb) * 0.5f;
  }
  V3 before = car.pos;
  clamp_world(car.pos, 1.2f);
  if (before.x != car.pos.x || before.z != car.pos.z) hit = true;
  if (hit) car.speed *= 0.45f;

  car.wheelSpin += car.speed / 0.36f * dt;
  return hit;
}

struct Walker {
  V3 pos{0.0f, 0.0f, 0.0f};
  float yaw = 0.0f;
  float speed = 0.0f;
  float anim = 0.0f;
};

inline void update_walker(Walker& w, V3 wish, bool run, float dt, const std::vector<AABB>& cols) {
  float target = run ? 7.2f : 3.9f;
  V3 dir = v3(wish.x, 0.0f, wish.z);
  float l = length(dir);
  if (l > 0.001f) {
    dir = dir * (1.0f / l);
    w.yaw = std::atan2(dir.x, dir.z);
  }
  float gs = l > 0.001f ? target : 0.0f;
  w.speed += (gs - w.speed) * std::min(1.0f, dt * 9.0f);
  w.pos = w.pos + dir * (w.speed * dt);
  w.anim += w.speed * dt * 2.1f;
  bool hit = false;
  for (const auto& b : cols) resolve_circle(w.pos, 0.42f, b, hit);
  V3 before = w.pos;
  clamp_world(w.pos, 0.6f);
  if (before.x != w.pos.x || before.z != w.pos.z) w.speed = 0.0f;
  (void)hit;
}
