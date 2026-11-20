#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gfx.h"
#include "math.h"
#include "assets.h"

constexpr float TILE = 832.0f;
constexpr float BLOCK = 84.0f;
constexpr float ROAD = 20.0f;
constexpr float CELL = BLOCK + ROAD;
constexpr int GRID = 8;

inline float frand(uint32_t& s) {
  s = hash32(s + 0x9e3779b9u);
  return (s >> 8) * (1.0f / 16777216.0f);
}

inline int irand(uint32_t& s, int lo, int hi) {
  return lo + (int)(frand(s) * (float)(hi - lo + 1)) * (hi > lo ? 1 : 0);
}

struct Builder {
  Verts v;

  void quad(const V3& a, const V3& b, const V3& c, const V3& d, const V3& n, float layer,
            uint32_t col, float ua, float va, float ub, float vb, float uc, float vc,
            float ud, float vd) {
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

  void top(float x0, float z0, float x1, float z1, float y, float layer, uint32_t col,
           float uvScale = 8.0f) {
    quad({x0, y, z1}, {x1, y, z1}, {x1, y, z0}, {x0, y, z0}, {0, 1, 0}, layer, col,
         x0 / uvScale, z1 / uvScale, x1 / uvScale, z1 / uvScale,
         x1 / uvScale, z0 / uvScale, x0 / uvScale, z0 / uvScale);
  }

  void wallZ(float x0, float x1, float y0, float y1, float z, int dir, float layer,
             uint32_t col, float uOff = 0.0f, float uvScale = 8.0f) {
    float u0 = x0 / uvScale + uOff, u1 = x1 / uvScale + uOff;
    float v0 = y0 / uvScale, v1 = y1 / uvScale;
    if (dir < 0) {
      quad({x0, y0, z}, {x1, y0, z}, {x1, y1, z}, {x0, y1, z}, {0, 0, -1}, layer, col,
           u0, v0, u1, v0, u1, v1, u0, v1);
    } else {
      quad({x0, y0, z}, {x0, y1, z}, {x1, y1, z}, {x1, y0, z}, {0, 0, 1}, layer, col,
           u0, v0, u0, v1, u1, v1, u1, v0);
    }
  }

  void wallX(float z0, float z1, float y0, float y1, float x, int dir, float layer,
             uint32_t col, float uOff = 0.0f, float uvScale = 8.0f) {
    float u0 = z0 / uvScale + uOff, u1 = z1 / uvScale + uOff;
    float v0 = y0 / uvScale, v1 = y1 / uvScale;
    if (dir > 0) {
      quad({x, y0, z0}, {x, y0, z1}, {x, y1, z1}, {x, y1, z0}, {1, 0, 0}, layer, col,
           u0, v0, u1, v0, u1, v1, u0, v1);
    } else {
      quad({x, y0, z0}, {x, y1, z0}, {x, y1, z1}, {x, y0, z1}, {-1, 0, 0}, layer, col,
           u0, v0, u0, v1, u1, v1, u1, v0);
    }
  }

  void box(float x0, float z0, float x1, float z1, float y0, float y1, float layer,
           uint32_t col, float uOff = 0.0f, float uvScale = 8.0f, bool roof = true) {
    wallZ(x0, x1, y0, y1, z0, -1, layer, col, uOff, uvScale);
    wallZ(x0, x1, y0, y1, z1, 1, layer, col, uOff, uvScale);
    wallX(z0, z1, y0, y1, x0, -1, layer, col, uOff, uvScale);
    wallX(z0, z1, y0, y1, x1, 1, layer, col, uOff, uvScale);
    if (roof) top(x0, z0, x1, z1, y1, L_ROOF, col, uvScale);
  }
};

inline float ground_clearance(const AABB& b) { return b.mx.y; }

inline void tint_verts(Verts& v, size_t from, float r, float g, float b) {
  for (size_t i = from; i < v.v.size(); ++i) {
    Vertex& vt = v.v[i];
    uint32_t c = vt.color;
    int cr = (int)((c & 255) * r), cg = (int)(((c >> 8) & 255) * g),
        cb = (int)(((c >> 16) & 255) * b);
    cr = cr > 255 ? 255 : cr;
    cg = cg > 255 ? 255 : cg;
    cb = cb > 255 ? 255 : cb;
    vt.color = rgba(cr, cg, cb, (int)((c >> 24) & 255));
  }
}

inline void lot_palette(uint32_t& s, float& r, float& g, float& b) {
  float p = frand(s);
  if (p < 0.28f) { r = 1.00f; g = 0.98f; b = 0.94f; }
  else if (p < 0.50f) { r = 1.00f; g = 0.91f; b = 0.83f; }
  else if (p < 0.72f) { r = 0.90f; g = 0.94f; b = 0.98f; }
  else if (p < 0.88f) { r = 0.97f; g = 0.84f; b = 0.72f; }
  else { r = 0.82f; g = 0.83f; b = 0.80f; }
  float k = 0.92f + frand(s) * 0.10f;
  r *= k; g *= k; b *= k;
}

inline void append_model(Verts& out, const AssetModel& m, float x, float z, float yaw,
                         float s, float baseY) {
  if (m.verts.empty()) return;
  uint32_t base = (uint32_t)out.v.size();
  float c = std::cos(yaw), sn = std::sin(yaw);
  out.v.reserve(out.v.size() + m.verts.size());
  for (const Vertex& src : m.verts) {
    Vertex v = src;
    float px = src.px * s, pz = src.pz * s;
    v.px = x + px * c + pz * sn;
    v.pz = z - px * sn + pz * c;
    v.py = baseY + src.py * s;
    float nx = src.nx, nz = src.nz;
    v.nx = nx * c + nz * sn;
    v.nz = -nx * sn + nz * c;
    v.layer = -1.0f;
    float wr = 0.94f, wg = 0.91f, wb = 0.87f;
    if (v.ny > 0.72f) { wr *= 0.82f; wg *= 0.82f; wb *= 0.83f; }
    uint32_t cc = src.color;
    int cr = (int)((cc & 255) * wr), cg = (int)(((cc >> 8) & 255) * wg),
        cb = (int)(((cc >> 16) & 255) * wb);
    v.color = rgba(cr > 255 ? 255 : cr, cg > 255 ? 255 : cg, cb > 255 ? 255 : cb,
                   (int)((cc >> 24) & 255));
    out.v.push_back(v);
  }
  out.i.reserve(out.i.size() + m.idx.size());
  for (uint32_t i : m.idx) out.i.push_back(base + i);
}

inline void asset_world_aabb(const AssetModel& m, float x, float z, float yaw, float s,
                             float baseY, AABB& out) {
  float c = std::cos(yaw), sn = std::sin(yaw);
  float mnx = 1e9f, mnz = 1e9f, mxx = -1e9f, mxz = -1e9f;
  const float cx[4] = {m.bounds.mn.x, m.bounds.mx.x, m.bounds.mx.x, m.bounds.mn.x};
  const float cz[4] = {m.bounds.mn.z, m.bounds.mn.z, m.bounds.mx.z, m.bounds.mx.z};
  for (int i = 0; i < 4; ++i) {
    float px = cx[i] * s, pz = cz[i] * s;
    float wx = x + px * c + pz * sn;
    float wz = z - px * sn + pz * c;
    mnx = std::min(mnx, wx); mxx = std::max(mxx, wx);
    mnz = std::min(mnz, wz); mxz = std::max(mxz, wz);
  }
  out.mn = {mnx, -1.0f, mnz};
  out.mx = {mxx, baseY + m.bounds.mx.y * s, mxz};
}

inline float facing_yaw(float bx, float bz, float cx, float cz) {
  float dl = cx - bx, dr = bx + BLOCK - cx, df = cz - bz, db = bz + BLOCK - cz;
  float m = std::min(std::min(dl, dr), std::min(df, db));
  if (m == dl) return 1.5707964f;
  if (m == dr) return -1.5707964f;
  if (m == df) return 0.0f;
  return 3.1415927f;
}

inline bool downtown_area(float px, float pz) {
  return px > -260.0f && px < 700.0f && pz > 40.0f && pz < 760.0f;
}

enum District { D_BAIXA = 0, D_ALTA = 1, D_MUSSEQUE = 2 };

// faixa da baía (pz < 780) reparte-se entre Baixa (centro) e Cidade Alta/Kilamba
// (flancos leste/oeste); tudo a norte das últimas filas é musseque.
inline int district_at(float px, float pz) {
  if (downtown_area(px, pz)) return D_BAIXA;
  if (px > 700.0f && pz > -860.0f && pz < 1660.0f) return D_ALTA;
  return D_MUSSEQUE;
}

// ---------------------------------------------------------------- POIs

struct Poi {
  const char* name;
  int kind;  // 0 casa, 1 mercearia, 2 restaurante, 3 garagem, 4 armas, 5 missao, 6 candongueiro
  float x, z;
};

inline std::vector<Poi>& poi_list() {
  static std::vector<Poi> v;
  return v;
}

struct PoiBlock {
  float px, pz;
  const char* model;
  int kind;
  const char* name;
};

inline const PoiBlock* poi_block_at(float px, float pz) {
  static const PoiBlock list[] = {
      {0.0f, 104.0f, "comercial/mercado", 1, "Mercearia do Bairro"},
      {104.0f, 104.0f, "comercial/restaurante", 2, "Restaurante Kalulu"},
      {208.0f, 208.0f, "meus/loja_downtown", 4, "Loja do Kambuta"},
      {0.0f, 312.0f, "industrial/oficina", 3, "Garagem do Musseque"},
      {-104.0f, 208.0f, "meus/predio_residencial", 0, "Apartamento 25 000 Kz"},
      {-104.0f, 416.0f, "meus/predio_residencial_b", 0, "Apartamento 45 000 Kz"},
      {208.0f, 416.0f, "meus/predio_residencial_c", 0, "Apartamento 90 000 Kz"},
      {312.0f, 104.0f, "comercial/banco", 5, "Contacto: Kixikila"},
      {312.0f, 416.0f, "publicos/prefeitura", 5, "Contacto: A encomenda"},
      {-104.0f, 104.0f, "publicos/museu", 5, "Contacto: Corrida na Ilha"},
      {104.0f, 416.0f, nullptr, 6, "Praca dos candongueiros"},
  };
  for (const auto& b : list) {
    if (b.px == px && b.pz == pz) return &b;
  }
  return nullptr;
}

inline uint32_t poi_color(int kind) {
  switch (kind) {
    case 0: return tint3(0.35f, 0.95f, 0.45f);
    case 1: return tint3(1.00f, 0.76f, 0.22f);
    case 2: return tint3(1.00f, 0.36f, 0.26f);
    case 3: return tint3(0.38f, 0.66f, 1.00f);
    case 4: return tint3(0.78f, 0.46f, 1.00f);
    case 5: return tint3(0.22f, 0.86f, 1.00f);
    default: return tint3(1.00f, 0.94f, 0.32f);
  }
}

inline Poi poi_from_block(const PoiBlock& p) {
  return {p.name, p.kind, p.px + BLOCK - 2.0f, p.pz + BLOCK * 0.5f};
}

inline void add_poi_sign(Builder& b, const PoiBlock& p) {
  float x = p.px + BLOCK - 2.0f;
  float z = p.pz + BLOCK * 0.5f;
  uint32_t col = poi_color(p.kind);
  b.box(x - 0.07f, z - 0.07f, x + 0.07f, z + 0.07f, 0.16f, 2.45f, L_CONCRETE,
        tint3(0.26f, 0.27f, 0.29f), 0, 2.0f, false);
  b.box(x - 0.06f, z - 0.80f, x + 0.10f, z + 0.80f, 2.05f, 2.70f, L_EMIT, col, 0, 1.0f, false);
  b.top(x - 1.7f, z - 1.7f, x + 1.7f, z + 1.7f, 0.176f, L_SIDEWALK,
        tint3(0.82f, 0.80f, 0.76f), 3.0f);
}

// ---------------------------------------------------------------- sinais

struct SignalLight {
  float x, z;
};

inline std::vector<SignalLight>& signal_list() {
  static std::vector<SignalLight> v;
  return v;
}

inline void build_signals() {
  auto& v = signal_list();
  if (!v.empty()) return;
  for (int j = 0; j < 4; ++j) {
    for (int i = 0; i < 4; ++i) {
      v.push_back({94.0f + 104.0f * (float)i, 94.0f + 104.0f * (float)j});
    }
  }
}

// ciclo de 12 s: 0 NS verde, 1 NS ambar, 2 EW verde, 3 EW ambar
inline int signal_phase_at(float t) {
  float ph = t - std::floor(t / 12.0f) * 12.0f;
  if (ph < 5.0f) return 0;
  if (ph < 6.0f) return 1;
  if (ph < 11.0f) return 2;
  return 3;
}

inline const char* pick_lot_model(uint32_t& s, int dist, bool lod1) {
  float r = frand(s);
  if (dist == D_MUSSEQUE) {
    static const char* houses[] = {
      "meus/casa_kj", "meus/casa_estudio", "residencial/casa_simples",
      "residencial/casa_sobrado", "residencial/casas_geminadas",
      "comercial/loja_esquina", "comercial/mercado",
    };
    return houses[(int)(r * 7) % 7];
  }
  if (dist == D_ALTA) {
    if (!lod1 && r < 0.40f) {
      static const char* towers[] = {
        "meus/predio_moderno", "meus/predio_vidro", "meus/predio_escritorio_b",
        "meus/predio_moderno_b", "meus/predio_flatiron", "meus/predio_classico",
      };
      return towers[(int)(frand(s) * 6) % 6];
    }
    if (r < 0.86f) {
      static const char* mid[] = {
        "meus/predio_residencial", "meus/predio_residencial_b", "meus/predio_residencial_c",
        "meus/predio_brownstone", "meus/predio_brownstone_b", "meus/predio_classico",
        "meus/predio_artdeco", "meus/predio_escritorio", "meus/predio_publico",
        "residencial/predio_apartamentos",
      };
      return mid[(int)(frand(s) * 10) % 10];
    }
    static const char* civic[] = {"meus/predio_publico", "meus/predio_classico", "publicos/prefeitura"};
    return civic[(int)(frand(s) * 3) % 3];
  }
  if (r < 0.34f) {
    static const char* towers[] = {
      "meus/predio_flatiron", "meus/predio_moderno", "meus/predio_vidro",
      "meus/predio_escritorio_b", "meus/predio_moderno_b", "meus/predio_classico",
      "meus/predio_artdeco", "meus/predio_brutalista",
    };
    return towers[(int)(frand(s) * 8) % 8];
  }
  if (r < 0.76f) {
    static const char* mid[] = {
      "meus/predio_classico", "meus/predio_artdeco", "meus/predio_moderno_b", "meus/predio_brutalista",
      "meus/predio_residencial", "meus/predio_residencial_b", "meus/predio_residencial_c",
      "meus/predio_escritorio", "meus/predio_escritorio_c", "meus/predio_escritorio_d",
      "meus/predio_brownstone", "meus/predio_brownstone_b", "meus/loja_downtown",
      "meus/predio_sanzio", "meus/predio_publico", "meus/predio_escritorio_b",
      "comercial/shopping", "comercial/restaurante",
    };
    return mid[(int)(frand(s) * 18) % 18];
  }
  static const char* civic[] = {
    "meus/predio_publico", "meus/predio_classico", "meus/predio_residencial",
    "meus/predio_artdeco", "publicos/prefeitura",
  };
  return civic[(int)(frand(s) * 5) % 5];
}

inline float model_scale_max(const std::string& n) {  if (n.find("predio_alto") != std::string::npos) return 9.5f;
  if (n.find("predio_apartamentos") != std::string::npos) return 9.0f;
  if (n.find("escritorio") != std::string::npos || n.find("hotel") != std::string::npos) return 9.0f;
  if (n.find("publicos/") != std::string::npos) return 6.2f;
  if (n.find("shopping") != std::string::npos) return 6.0f;
  return 7.3f;
}

// altura maxima em metros para modelos externos (evita casas do tamanho de torres)
inline float model_height_cap(const std::string& n) {
  if (n.find("meus/casa") != std::string::npos) return 9.5f;
  if (n.find("meus/predio_flatiron") != std::string::npos) return 92.0f;
  if (n.find("meus/predio_moderno") != std::string::npos) return 62.0f;
  if (n.find("meus/predio_vidro") != std::string::npos) return 70.0f;
  if (n.find("meus/predio_escritorio_b") != std::string::npos) return 56.0f;
  if (n.find("meus/predio_sanzio") != std::string::npos) return 18.0f;
  if (n.find("meus/predio_") != std::string::npos) return 46.0f;
  if (n.find("meus/loja_") != std::string::npos) return 20.0f;
  if (n.find("meus/condominio_") != std::string::npos) return 46.0f;
  if (n.find("meus/complexo_") != std::string::npos) return 14.0f;
  if (n.find("publicos/") != std::string::npos) return 26.0f;
  if (n.find("industrial/") != std::string::npos) return 16.0f;
  if (n.find("comercial/") != std::string::npos) return 44.0f;
  if (n.find("residencial/") != std::string::npos) return 58.0f;
  return 0.0f;
}



inline Mesh mesh_upload(Verts& v) { return make_mesh(v.v, v.i); }

inline void draw_instanced(const Mesh& m, GLsizei instances) {
  if (!m.vao || !m.count || instances <= 0) return;
  glBindVertexArray(m.vao);
  glDrawElementsInstanced(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr, instances);
  stat_draw_calls() += 1;
  stat_draw_tris() += (double)m.count / 3.0 * (double)instances;
}

// ------------------------------------------------------------- small props

inline Mesh make_palm_mesh() {
  Builder b;
  const int SEG = 5, SIDES = 8;
  uint32_t trunkA = tint3(0.46f, 0.36f, 0.26f);
  uint32_t trunkB = tint3(0.38f, 0.29f, 0.21f);
  float top = 0.0f, cx = 0.0f, cz = 0.0f;
  for (int s = 0; s < SEG; ++s) {
    float y0 = s * 1.2f, y1 = y0 + 1.2f;
    float r0 = 0.24f - s * 0.024f, r1 = r0 - 0.024f;
    float l0 = std::sin(s * 0.55f) * 0.30f, l1 = std::sin((s + 1) * 0.55f) * 0.30f;
    uint32_t c = (s & 1) ? trunkA : trunkB;
    for (int i = 0; i < SIDES; ++i) {
      float a0 = i * 6.2831853f / SIDES, a1 = (i + 1) * 6.2831853f / SIDES;
      float am = (a0 + a1) * 0.5f;
      V3 n = {std::cos(am), 0.0f, std::sin(am)};
      V3 p0 = {l0 + r0 * std::cos(a0), y0, r0 * std::sin(a0)};
      V3 p1 = {l0 + r0 * std::cos(a1), y0, r0 * std::sin(a1)};
      V3 p2 = {l1 + r1 * std::cos(a1), y1, r1 * std::sin(a1)};
      V3 p3 = {l1 + r1 * std::cos(a0), y1, r1 * std::sin(a0)};
      b.quad(p0, p1, p2, p3, n, L_CONCRETE, c, 0, 0, 0.2f, 0.2f, 0.35f, 0.6f, 0.2f, 0.6f);
    }
    top = y1;
    cx = l1;
    cz = 0.0f;
  }
  // crown husk
  b.box(cx - 0.26f, cz - 0.26f, cx + 0.26f, cz + 0.26f, top - 0.25f, top + 0.35f, L_CONCRETE,
        tint3(0.34f, 0.44f, 0.22f), 0.0f, 2.0f, true);
  uint32_t leaf = tint3(0.29f, 0.50f, 0.24f);
  uint32_t leafDark = tint3(0.21f, 0.40f, 0.18f);
  for (int i = 0; i < 10; ++i) {
    float a = i * 0.6283185f + (i & 1 ? 0.14f : -0.1f);
    float dx = std::cos(a), dz = std::sin(a);
    float px = -dz, pz = dx;
    uint32_t c = (i % 2) ? leaf : leafDark;
    V3 base = {cx, top + 0.1f, cz};
    V3 q1L = {cx + dx * 0.9f + px * 0.42f, top + 0.35f, cz + dz * 0.9f + pz * 0.42f};
    V3 q1R = {cx + dx * 0.9f - px * 0.42f, top + 0.35f, cz + dz * 0.9f - pz * 0.42f};
    V3 q2L = {cx + dx * 1.9f + px * 0.32f, top - 0.15f, cz + dz * 1.9f + pz * 0.32f};
    V3 q2R = {cx + dx * 1.9f - px * 0.32f, top - 0.15f, cz + dz * 1.9f - pz * 0.32f};
    V3 tip = {cx + dx * 2.9f, top - 1.05f, cz + dz * 2.9f};
    b.quad(base, q1L, q1R, base, {0, 1, 0}, L_GRASS, c, 0, 0, 0.4f, 1, 0.9f, 0.4f, 0.4f, 0);
    b.quad(q1L, q2L, q2R, q1R, {0, 0.8f, 0}, L_GRASS, c, 0, 0, 0.3f, 0.9f, 0.6f, 0.5f, 0.3f, 0.1f);
    b.quad(q2L, tip, tip, q2R, {0, 0.5f, 0}, L_GRASS, c, 0, 0, 0.2f, 0.8f, 0.5f, 0.3f, 0.2f, 0);
  }
  return mesh_upload(b.v);
}

inline Mesh make_lamp_mesh() {
  Builder b;
  uint32_t dark = tint3(0.30f, 0.31f, 0.33f);
  b.box(-0.1f, -0.1f, 0.1f, 0.1f, 0.0f, 0.5f, L_CONCRETE, tint3(0.55f, 0.54f, 0.52f), 0, 3.0f, false);
  b.box(-0.07f, -0.07f, 0.07f, 0.07f, 0.5f, 5.3f, L_CONCRETE, dark, 0, 3.0f, false);
  b.box(0.0f, -0.05f, 1.5f, 0.05f, 5.2f, 5.34f, L_CONCRETE, dark, 0, 3.0f, false);
  b.box(1.25f, -0.16f, 1.75f, 0.16f, 5.1f, 5.24f, L_WHITE, tint3(1.0f, 0.86f, 0.60f), 0, 2.0f, false);
  b.box(1.20f, -0.21f, 1.80f, 0.21f, 5.04f, 5.30f, L_EMIT, tint3(1.0f, 0.88f, 0.62f), 0, 2.0f, false);
  return mesh_upload(b.v);
}

inline Mesh make_signal_masts() {
  Builder b;
  for (const auto& sl : signal_list()) {
    for (int corner = 0; corner < 4; ++corner) {
      float sx = (corner & 1) ? 1.0f : -1.0f;
      float sz = (corner & 2) ? 1.0f : -1.0f;
      float x = sl.x + sx * 11.6f;
      float z = sl.z + sz * 11.6f;
      b.box(x - 0.09f, z - 0.09f, x + 0.09f, z + 0.09f, 0.0f, 3.9f, L_CONCRETE,
            tint3(0.35f, 0.36f, 0.38f), 0, 2.0f, false);
      b.box(x - 0.22f, z - 0.22f, x + 0.22f, z + 0.22f, 0.0f, 0.18f, L_CONCRETE,
            tint3(0.28f, 0.28f, 0.30f), 0, 2.0f, false);
      b.box(x - 0.15f, z - 0.12f, x + 0.15f, z + 0.12f, 2.20f, 3.62f, L_CONCRETE,
            tint3(0.16f, 0.17f, 0.18f), 0, 1.0f, false);
    }
  }
  return mesh_upload(b.v);
}

inline Mesh make_signal_lamps(int phase) {
  Builder b;
  uint32_t dim = tint3(0.10f, 0.10f, 0.11f);
  uint32_t red = tint3(1.00f, 0.14f, 0.10f);
  uint32_t amber = tint3(1.00f, 0.60f, 0.10f);
  uint32_t green = tint3(0.20f, 0.95f, 0.38f);
  for (const auto& sl : signal_list()) {
    for (int corner = 0; corner < 4; ++corner) {
      float sx = (corner & 1) ? 1.0f : -1.0f;
      float sz = (corner & 2) ? 1.0f : -1.0f;
      bool pole_ns = (sx * sz) > 0.0f;
      bool phase_ns = (phase == 0 || phase == 1);
      bool phase_amber = (phase == 1 || phase == 3);
      bool active = (pole_ns == phase_ns);
      uint32_t cg = dim, ca = dim, cr = dim;
      if (!active) cr = red;
      else if (phase_amber) ca = amber;
      else cg = green;
      float x = sl.x + sx * 11.6f - sx * 0.20f;
      float z = sl.z + sz * 11.6f - sz * 0.20f;
      b.box(x - 0.17f, z - 0.14f, x + 0.17f, z + 0.14f, 2.36f, 2.58f, L_EMIT, cg, 0, 1.0f, false);
      b.box(x - 0.17f, z - 0.14f, x + 0.17f, z + 0.14f, 2.76f, 2.98f, L_EMIT, ca, 0, 1.0f, false);
      b.box(x - 0.17f, z - 0.14f, x + 0.17f, z + 0.14f, 3.16f, 3.38f, L_EMIT, cr, 0, 1.0f, false);
    }
  }
  return mesh_upload(b.v);
}

// ------------------------------------------------------------- chunks

struct Chunk {
  int cx = 0, cz = 0;
  bool lod1 = false;
  bool built = false;
  Mesh ground, bld, props;
  Mesh treeMesh, lampMesh;
  GLuint treeInst = 0, lampInst = 0;
  int treeCount = 0, lampCount = 0;
  std::vector<float> treeData, lampData;
  std::vector<AABB> colliders;
  AABB bounds{};
};

struct City {
  std::vector<Chunk> chunks;
  Mesh feature, sea, ilha;
  Mesh farCity;
  GLuint ilhaPalmInst = 0;
  int ilhaPalms = 0;
  std::vector<float> ilhaPalmData;
  Mesh promenade, railing;
  Mesh ilhaPalmMesh;
  std::vector<AABB> colliders;
  GLuint emptyVao = 0;
};

// ---------------------------------------- debug: registo de lotes (mw_debug_lot)
struct LotRecord {
  std::string name;
  float cx = 0.0f, cz = 0.0f;   // centro em XZ
  float hx = 0.0f, hz = 0.0f;   // meia largura em XZ
  float scale = 1.0f;
  float height = 0.0f;
  int layer = -1;               // camada dominante do modelo (-2 = procedural)
  int tris = 0;
};

inline std::vector<LotRecord>& lot_records() {
  static std::vector<LotRecord> v;
  return v;
}

inline void lot_records_clear() { lot_records().clear(); }

inline int model_dominant_layer(const AssetModel& m) {
  int lv[16], cnt[16];
  int n = 0;
  auto add = [&](int l) {
    int k = -1;
    for (int i = 0; i < n; ++i) {
      if (lv[i] == l) { k = i; break; }
    }
    if (k < 0) {
      if (n >= 16) return;
      lv[n] = l;
      cnt[n] = 0;
      k = n++;
    }
    cnt[k] += 1;
  };
  // Para diagnostico de fachada conta so as faces laterais (|ny| <= 0.7):
  // telhados e lajes interiores dominam o vertice count e escondem o material da parede.
  int side = 0;
  for (const auto& vert : m.verts) {
    if (std::fabs(vert.ny) > 0.7f) continue;
    add((int)std::lround(vert.layer));
    side += 1;
  }
  if (side == 0) {
    for (const auto& vert : m.verts) add((int)std::lround(vert.layer));
  }
  int best = -1, best_n = -1;
  for (int i = 0; i < n; ++i) {
    if (cnt[i] > best_n) { best_n = cnt[i]; best = lv[i]; }
  }
  return best;
}

inline void record_lot(const char* name, const AABB& a, float scale, int layer, int tris) {
  LotRecord r;
  r.name = name;
  r.cx = (a.mn.x + a.mx.x) * 0.5f;
  r.cz = (a.mn.z + a.mx.z) * 0.5f;
  r.hx = (a.mx.x - a.mn.x) * 0.5f;
  r.hz = (a.mx.z - a.mn.z) * 0.5f;
  r.scale = scale;
  r.height = a.mx.y - a.mn.y;
  r.layer = layer;
  r.tris = tris;
  lot_records().push_back(r);
}

inline void upload_instances(Chunk& c) {
  if (!c.treeData.empty()) {
    c.treeInst = make_instance_buffer();
    glBindBuffer(GL_ARRAY_BUFFER, c.treeInst);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(c.treeData.size() * 4), c.treeData.data(),
                 GL_STATIC_DRAW);
    c.treeMesh = clone_with_instances(c.treeMesh, c.treeInst, false);
    c.treeCount = (int)(c.treeData.size() / 16);
  }
  if (!c.lampData.empty()) {
    c.lampInst = make_instance_buffer();
    glBindBuffer(GL_ARRAY_BUFFER, c.lampInst);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(c.lampData.size() * 4), c.lampData.data(),
                 GL_STATIC_DRAW);
    c.lampMesh = clone_with_instances(c.lampMesh, c.lampInst, false);
    c.lampCount = (int)(c.lampData.size() / 16);
  }
}

inline void push_mat(std::vector<float>& out, const Mat4& m) {
  for (int col = 0; col < 4; ++col) {
    out.push_back(m.m[col * 4 + 0]);
    out.push_back(m.m[col * 4 + 1]);
    out.push_back(m.m[col * 4 + 2]);
    out.push_back(m.m[col * 4 + 3]);
  }
}

inline void add_tree(Builder& b, float x, float z, uint32_t& seed) {
  static const char* names[] = {"vegetacao/ipe_amarelo", "vegetacao/buriti", "vegetacao/araucaria"};
  const AssetModel* m = asset_pick(names, 3, seed);
  if (!m) return;
  float s = 2.9f + frand(seed) * 1.5f;
  float a = frand(seed) * 6.2831853f;
  append_model(b.v, *m, x, z, a, s, 0.16f);
}

inline void add_lamp(Builder& b, float x, float z, float yaw) {
  const AssetModel* m = asset_get("mobiliario/poste_luz");
  if (!m) return;
  append_model(b.v, *m, x, z, yaw, 3.3f, 0.16f);
}

struct LotStyle {
  float h;
  float tintR, tintG, tintB;
  float layer;
};

inline LotStyle pick_style(uint32_t& s, int dist, bool lod1) {
  float r = frand(s);
  int floors;
  if (dist == D_MUSSEQUE) floors = 1 + (int)(frand(s) * 3.0f);
  else if (dist == D_ALTA) {
    if (lod1) floors = 3 + (int)(frand(s) * 5.0f);
    else floors = r < 0.30f ? 4 + (int)(frand(s) * 5.0f) : 8 + (int)(frand(s) * 9.0f);
  } else if (r < 0.60f) floors = 2 + (int)(frand(s) * 5);
  else if (r < 0.82f) floors = 7 + (int)(frand(s) * 6);
  else if (r < 0.95f) floors = 13 + (int)(frand(s) * 8);
  else floors = 21 + (int)(frand(s) * 12);
  float h = floors * 3.0f + 0.8f;
  float v = 0.80f + frand(s) * 0.26f;
  LotStyle st;
  st.h = h;
  float pick = frand(s);
  if (dist == D_MUSSEQUE) {
    if (pick < 0.40f)      { st.tintR = 1.06f * v; st.tintG = 0.93f * v; st.tintB = 0.84f * v; }
    else if (pick < 0.68f) { st.tintR = 1.08f * v; st.tintG = 0.98f * v; st.tintB = 0.86f * v; }
    else if (pick < 0.86f) { st.tintR = 0.98f * v; st.tintG = 0.93f * v; st.tintB = 0.86f * v; }
    else                   { st.tintR = 0.92f * v; st.tintG = 0.90f * v; st.tintB = 0.92f * v; }
    st.layer = pick < 0.70f ? L_FACADE_B : L_SHOP;
    return st;
  }
  if (dist == D_ALTA && pick < 0.55f) {
    st.tintR = 0.93f * v; st.tintG = 0.98f * v; st.tintB = 1.03f * v;
    st.layer = pick < 0.28f ? L_GLASS : L_FACADE_C;
    return st;
  }
  if (pick < 0.28f) {
    st.tintR = 1.02f * v; st.tintG = 0.99f * v; st.tintB = 0.93f * v;
  } else if (pick < 0.58f) {
    st.tintR = 1.05f * v; st.tintG = 0.92f * v; st.tintB = 0.87f * v;
  } else if (pick < 0.78f) {
    st.tintR = 0.92f * v; st.tintG = 0.95f * v; st.tintB = 0.97f * v;
  } else if (pick < 0.90f) {
    st.tintR = 0.88f * v; st.tintG = 0.94f * v; st.tintB = 1.0f * v;
  } else {
    st.tintR = 1.10f * v; st.tintG = 0.90f * v; st.tintB = 0.78f * v;
  }
  st.layer = pick < 0.28f ? L_FACADE_A : pick < 0.58f ? L_FACADE_B
            : pick < 0.78f ? L_FACADE_C : pick < 0.90f ? L_GLASS : L_SHOP;
  return st;
}

inline void build_building(Chunk& c, Builder& bld, float x0, float z0, float x1, float z1,
                           uint32_t& s, float baseY, int dist, bool lod1) {
  LotStyle st = pick_style(s, dist, lod1);
  uint32_t col = tint3(st.tintR, st.tintG, st.tintB);
  float uOff = frand(s) * 8.0f;
  float h = st.h;
  bool tall = h > 30.0f;
  float roofY = baseY + h - 0.9f;

  // Tiered massing: podium + set-back tower(s) break up the plain prism silhouette.
  float tx0 = x0, tz0 = z0, tx1 = x1, tz1 = z1;
  int tiers = (tall && frand(s) < 0.72f) ? (h > 52.0f ? 3 : 2) : 1;
  float curY = baseY;
  for (int t = 0; t < tiers; ++t) {
    float topY;
    if (t == tiers - 1) topY = roofY;
    else {
      float frac = t == 0 ? (tiers == 2 ? 0.42f : 0.30f) : 0.34f;
      topY = curY + (roofY - baseY) * frac;
    }
    uint32_t tc = t == 0 ? col
      : tint3(std::min(1.0f, st.tintR * (1.05f + 0.04f * t)),
              std::min(1.0f, st.tintG * (1.03f + 0.03f * t)),
              std::min(1.0f, st.tintB * (1.00f + 0.02f * t)));
    bld.box(tx0, tz0, tx1, tz1, curY, topY, st.layer, tc, uOff + 2.0f * t, 8.0f, false);
    if (t < tiers - 1) {
      uint32_t cc = tint3(0.64f * st.tintR, 0.62f * st.tintG, 0.60f * st.tintB);
      bld.wallZ(tx0, tx1, topY - 0.55f, topY, tz0, -1, L_CONCRETE, cc, 0, 4.0f);
      bld.wallZ(tx0, tx1, topY - 0.55f, topY, tz1, 1, L_CONCRETE, cc, 0, 4.0f);
      bld.wallX(tz0, tz1, topY - 0.55f, topY, tx0, -1, L_CONCRETE, cc, 0, 4.0f);
      bld.wallX(tz0, tz1, topY - 0.55f, topY, tx1, 1, L_CONCRETE, cc, 0, 4.0f);
      bld.top(tx0 + 0.02f, tz0 + 0.02f, tx1 - 0.02f, tz1 - 0.02f, topY, L_ROOF,
                tint3(0.40f, 0.40f, 0.42f), 6.0f);
      float ix = (tx1 - tx0) * (0.08f + frand(s) * 0.06f);
      float iz = (tz1 - tz0) * (0.08f + frand(s) * 0.06f);
      tx0 += ix; tz0 += iz; tx1 -= ix; tz1 -= iz;
    }
    curY = topY;
  }

  // parapet on the top tier
  uint32_t pc = tint3(0.62f * st.tintR, 0.62f * st.tintG, 0.62f * st.tintB);
  bld.wallZ(tx0, tx1, roofY, roofY + 0.9f, tz0, -1, L_CONCRETE, pc, 0, 4.0f);
  bld.wallZ(tx0, tx1, roofY, roofY + 0.9f, tz1, 1, L_CONCRETE, pc, 0, 4.0f);
  bld.wallX(tz0, tz1, roofY, roofY + 0.9f, tx0, -1, L_CONCRETE, pc, 0, 4.0f);
  bld.wallX(tz0, tz1, roofY, roofY + 0.9f, tx1, 1, L_CONCRETE, pc, 0, 4.0f);
  bld.top(tx0 + 0.3f, tz0 + 0.3f, tx1 - 0.3f, tz1 - 0.3f, roofY, L_ROOF,
            tint3(0.55f, 0.53f, 0.50f), 6.0f);

  // ground-floor storefronts + canopy
  if (dist != D_MUSSEQUE) {
    uint32_t gc = tint3(0.50f, 0.60f, 0.66f);
    uint32_t cc2 = tint3(0.30f * st.tintR + 0.16f, 0.30f * st.tintG + 0.16f,
                         0.30f * st.tintB + 0.16f);
    float sg = 0.07f, cy = baseY + 2.85f;
    float gy0 = baseY + 0.35f;
    bld.wallZ(x0 + 0.25f, x1 - 0.25f, gy0, cy - 0.10f, z0 - sg, -1, L_GLASS, gc, 0, 6.0f);
    bld.wallZ(x0 + 0.25f, x1 - 0.25f, gy0, cy - 0.10f, z1 + sg, 1, L_GLASS, gc, 0, 6.0f);
    bld.wallX(z0 + 0.25f, z1 - 0.25f, gy0, cy - 0.10f, x0 - sg, -1, L_GLASS, gc, 0, 6.0f);
    bld.wallX(z0 + 0.25f, z1 - 0.25f, gy0, cy - 0.10f, x1 + sg, 1, L_GLASS, gc, 0, 6.0f);
    bld.box(x0 + 0.15f, z0 - 0.42f, x1 - 0.15f, z0 - 0.02f, cy, cy + 0.22f,
              L_CONCRETE, cc2, 0, 3.0f, true);
    bld.box(x0 + 0.15f, z1 + 0.02f, x1 - 0.15f, z1 + 0.42f, cy, cy + 0.22f,
              L_CONCRETE, cc2, 0, 3.0f, true);
    bld.box(x0 - 0.42f, z0 + 0.15f, x0 - 0.02f, z1 - 0.15f, cy, cy + 0.22f,
              L_CONCRETE, cc2, 0, 3.0f, true);
    bld.box(x1 + 0.02f, z0 + 0.15f, x1 + 0.42f, z1 - 0.15f, cy, cy + 0.22f,
              L_CONCRETE, cc2, 0, 3.0f, true);
  }

  // dark plinth
  uint32_t plc = tint3(st.tintR * 0.55f, st.tintG * 0.55f, st.tintB * 0.55f);
  float po = 0.04f;
  bld.wallZ(x0 - po, x1 + po, baseY, baseY + 1.3f, z0 - po, -1, L_CONCRETE, plc, 0, 4.0f);
  bld.wallZ(x0 - po, x1 + po, baseY, baseY + 1.3f, z1 + po, 1, L_CONCRETE, plc, 0, 4.0f);
  bld.wallX(z0 - po, z1 + po, baseY, baseY + 1.3f, x0 - po, -1, L_CONCRETE, plc, 0, 4.0f);
  bld.wallX(z0 - po, z1 + po, baseY, baseY + 1.3f, x1 + po, 1, L_CONCRETE, plc, 0, 4.0f);

  if (!c.lod1) {
    // AC boxes and a water tank for close chunks
    int ac = (int)(frand(s) * 3.0f);
    for (int i = 0; i < ac; ++i) {
      float face = frand(s);
      float y = baseY + 3.0f + frand(s) * std::max(1.0f, h - 6.0f);
      float w = 0.55f + frand(s) * 0.4f;
      if (face < 0.5f) {
        float fx = tx0 + 1.0f + frand(s) * std::max(0.1f, (tx1 - tx0) - 2.0f);
        float z = face < 0.25f ? tz0 - 0.22f : tz1 + 0.22f;
        bld.box(fx - w * 0.5f, z - 0.25f, fx + w * 0.5f, z + 0.25f, y, y + w * 0.8f,
                  L_CONCRETE, tint3(0.88f, 0.88f, 0.84f), 0, 2.0f, true);
      } else {
        float fz = tz0 + 1.0f + frand(s) * std::max(0.1f, (tz1 - tz0) - 2.0f);
        float x = face < 0.75f ? tx0 - 0.22f : tx1 + 0.22f;
        bld.box(x - 0.25f, fz - w * 0.5f, x + 0.25f, fz + w * 0.5f, y, y + w * 0.8f,
                  L_CONCRETE, tint3(0.88f, 0.88f, 0.84f), 0, 2.0f, true);
      }
    }
    if (frand(s) < 0.5f) {
      float t = 1.4f + frand(s) * 0.8f;
      float tx = tx0 + 1.5f + frand(s) * std::max(0.2f, (tx1 - tx0) - 4.0f);
      float tz = tz0 + 1.5f + frand(s) * std::max(0.2f, (tz1 - tz0) - 4.0f);
      bld.box(tx - t * 0.5f, tz - t * 0.5f, tx + t * 0.5f, tz + t * 0.5f,
                roofY, roofY + t, L_CONCRETE,
                tint3(0.36f, 0.40f, 0.42f), 0, 3.0f, true);
    }
  }

  if (h > 26.0f && frand(s) < 0.35f) {
    uint32_t bc = tint3(0.9f, 0.55f, 0.18f);
    float face = frand(s);
    float bw = 8.0f + frand(s) * 5.0f, bh = 3.6f;
    float bx = (tx0 + tx1) * 0.5f, bz = (tz0 + tz1) * 0.5f;
    float y0 = baseY + h + 4.0f;
    float offx = 0, offz = 0;
    int dir = 0;
    if (face < 0.25f) { offz = -(tz1 - tz0) * 0.5f - 0.4f; dir = 0; }
    else if (face < 0.5f) { offz = (tz1 - tz0) * 0.5f + 0.4f; dir = 1; }
    else if (face < 0.75f) { offx = -(tx1 - tx0) * 0.5f - 0.4f; dir = 2; }
    else { offx = (tx1 - tx0) * 0.5f + 0.4f; dir = 3; }
    float px = bx + offx, pz = bz + offz;
    bld.box(px - 0.25f, pz - 0.25f, px + 0.25f, pz + 0.25f, y0 - 3.0f, y0 + bh * 0.5f,
              L_CONCRETE, tint3(0.4f, 0.4f, 0.42f), 0, 2.0f, false);
    if (dir < 2) {
      bld.box(px - bw * 0.5f, pz - 0.25f, px + bw * 0.5f, pz + 0.25f, y0,
                y0 + bh, L_WHITE, bc, 0, 2.0f, false);
      bld.wallZ(px - bw * 0.5f, px + bw * 0.5f, y0, y0 + bh,
                  pz + (dir == 1 ? 0.25f : -0.25f), dir == 1 ? 1 : -1, L_WHITE, bc, 0, 4.0f);
    } else {
      bld.box(px - 0.25f, pz - bw * 0.5f, px + 0.25f, pz + bw * 0.5f, y0,
                y0 + bh, L_WHITE, bc, 0, 2.0f, false);
      bld.wallX(pz - bw * 0.5f, pz + bw * 0.5f, y0, y0 + bh,
                  px + (dir == 3 ? 0.25f : -0.25f), dir == 3 ? 1 : -1, L_WHITE, bc, 0, 4.0f);
    }
  }

  bld.top(x0 - 0.9f, z0 - 0.9f, x1 + 0.9f, z1 + 0.9f, 0.172f, L_SIDEWALK,
          tint3(0.68f, 0.66f, 0.63f), 4.0f);
  c.colliders.push_back({{x0 - 0.35f, -1.0f, z0 - 0.35f}, {x1 + 0.35f, h, z1 + 0.35f}});
}

inline void place_lot(Chunk& c, Builder& bld, float bx, float bz, int dist,
                      float x0, float z0, float x1, float z1, uint32_t& s, float baseY) {
  const char* name = pick_lot_model(s, dist, c.lod1);
  const AssetModel* m = asset_get(name);
  if (!m || m->bounds.mx.x <= m->bounds.mn.x) {
    build_building(c, bld, x0, z0, x1, z1, s, baseY, dist, c.lod1);
    AABB pa;
    pa.mn = {x0, baseY, z0};
    pa.mx = {x1, baseY + 1.0f, z1};
    record_lot("procedural", pa, 1.0f, -2, 0);
    return;
  }
  float fw = m->bounds.mx.x - m->bounds.mn.x;
  float fd = m->bounds.mx.z - m->bounds.mn.z;
  float fit = std::min(x1 - x0, z1 - z0) * 0.9f / std::max(fw, fd);
  float sc = std::min(fit, model_scale_max(name)) * (0.94f + frand(s) * 0.08f);
  float cap = model_height_cap(name);
  if (cap > 0.0f) sc = std::min(sc, cap / m->bounds.mx.y);
  float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
  float yaw = facing_yaw(bx, bz, cx, cz);
  size_t from = bld.v.v.size();
  append_model(bld.v, *m, cx, cz, yaw, sc, baseY);
  if (!m->textured) {
    float tr2, tg2, tb2;
    lot_palette(s, tr2, tg2, tb2);
    tint_verts(bld.v, from, tr2, tg2, tb2);
  }
  AABB a;
  asset_world_aabb(*m, cx, cz, yaw, sc, baseY, a);
  // Dados do lote para mw_debug_lot(x,z): nome, escala, camada dominante, tris.
  record_lot(name, a, sc, model_dominant_layer(*m), m->tris);
  if (dist == D_BAIXA) {
    bld.top(a.mn.x - 0.9f, a.mn.z - 0.9f, a.mx.x + 0.9f, a.mx.z + 0.9f, 0.172f, L_SIDEWALK,
            tint3(0.68f, 0.66f, 0.63f), 4.0f);
  } else if (dist == D_ALTA) {
    bld.top(a.mn.x - 0.9f, a.mn.z - 0.9f, a.mx.x + 0.9f, a.mx.z + 0.9f, 0.172f, L_GRASS,
            tint3(0.52f, 0.66f, 0.44f), 4.0f);
  } else {
    bld.top(a.mn.x - 0.9f, a.mn.z - 0.9f, a.mx.x + 0.9f, a.mx.z + 0.9f, 0.172f, L_SAND,
            tint3(0.78f, 0.70f, 0.56f), 4.0f);
  }
  a.mn.x -= 0.25f; a.mn.z -= 0.25f; a.mx.x += 0.25f; a.mx.z += 0.25f;
  c.colliders.push_back(a);
}

// quarteirao inteiro com um unico complexo de grande escala
inline bool place_landmark(Chunk& c, Builder& bld, const char* name, float bx, float bz,
                           float x0, float z0, float x1, float z1, uint32_t& s, float baseY) {
  const AssetModel* m = asset_get(name);
  if (!m || m->bounds.mx.x <= m->bounds.mn.x) return false;
  float fw = m->bounds.mx.x - m->bounds.mn.x;
  float fd = m->bounds.mx.z - m->bounds.mn.z;
  float sc = std::min(x1 - x0, z1 - z0) * 0.96f / std::max(fw, fd);
  float cap = model_height_cap(name);
  if (cap > 0.0f) sc = std::min(sc, cap / m->bounds.mx.y);
  float cx = (x0 + x1) * 0.5f, cz = (z0 + z1) * 0.5f;
  float yaw = facing_yaw(bx, bz, cx, cz) + (frand(s) < 0.5f ? 1.5708f : 0.0f);
  if (std::strstr(name, "condominio") || std::strstr(name, "sanzio")) yaw = facing_yaw(bx, bz, cx, cz);
  append_model(bld.v, *m, cx, cz, yaw, sc, baseY);
  AABB a;
  asset_world_aabb(*m, cx, cz, yaw, sc, baseY, a);
  record_lot(name, a, sc, model_dominant_layer(*m), m->tris);
  bld.top(a.mn.x - 1.2f, a.mn.z - 1.2f, a.mx.x + 1.2f, a.mx.z + 1.2f, 0.172f, L_SIDEWALK,
          tint3(0.66f, 0.64f, 0.61f), 4.0f);
  a.mn.x -= 0.25f; a.mn.z -= 0.25f; a.mx.x += 0.25f; a.mx.z += 0.25f;
  c.colliders.push_back(a);
  return true;
}

inline void build_chunk(Chunk& c, uint32_t seed, Mesh& treeMesh, Mesh& lampMesh) {
  c.treeMesh = treeMesh;
  c.lampMesh = lampMesh;
  Builder g, b, p;
  float ox = (float)c.cx * TILE, oz = (float)c.cz * TILE;

  g.top(ox, oz, ox + TILE, oz + TILE, 0.0f, L_ASPHALT, 0xffffffff, 8.0f);

  // center lines on vertical/horizontal roads (few quads)
  for (int i = 0; i < GRID; ++i) {
    float rx = ox + i * CELL + BLOCK + ROAD * 0.5f;
    g.top(rx - 0.12f, oz + 12.0f, rx + 0.12f, oz + TILE - 12.0f, 0.012f, L_WHITE,
          tint3(0.62f, 0.60f, 0.55f), 8.0f);
  }
  for (int j = 0; j < GRID; ++j) {
    float rz = oz + j * CELL + BLOCK + ROAD * 0.5f;
    g.top(ox + 12.0f, rz - 0.12f, ox + TILE - 12.0f, rz + 0.12f, 0.012f, L_WHITE,
          tint3(0.62f, 0.60f, 0.55f), 8.0f);
  }

  // zebra crossings at intersections (near chunks only)
  if (!c.lod1) {
    uint32_t wc = tint3(0.74f, 0.72f, 0.67f);
    for (int i = 0; i < GRID; ++i) {
      for (int j = 0; j < GRID; ++j) {
        float ix = ox + i * CELL + BLOCK + ROAD * 0.5f;
        float iz = oz + j * CELL + BLOCK + ROAD * 0.5f;
        for (int side = 0; side < 2; ++side) {
          float dir = side == 0 ? -1.0f : 1.0f;
          float zc = iz + dir * (ROAD * 0.5f + 2.6f);
          for (int k = 0; k < 8; ++k) {
            float sx = ix - 9.0f + k * 2.4f;
            g.top(sx, zc - 1.4f, sx + 1.3f, zc + 1.4f, 0.013f, L_WHITE, wc, 6.0f);
          }
          float xc = ix + dir * (ROAD * 0.5f + 2.6f);
          for (int k = 0; k < 8; ++k) {
            float sz = iz - 9.0f + k * 2.4f;
            g.top(xc - 1.4f, sz, xc + 1.4f, sz + 1.3f, 0.013f, L_WHITE, wc, 6.0f);
          }
        }
      }
    }
  }

  for (int bx = 0; bx < GRID; ++bx) {
    for (int bz = 0; bz < GRID; ++bz) {
      float px = ox + bx * CELL;
      float pz = oz + bz * CELL;
      uint32_t lotSeed = hash32(seed ^ (uint32_t)(bx * 374761 + bz * 668265 + c.cx * 97 + c.cz * 131));

      // sidewalk slab
      g.top(px, pz, px + BLOCK, pz + BLOCK, 0.16f, L_SIDEWALK, 0xffffffff, 6.0f);
      float curb = 0.16f;
      uint32_t cc = tint3(0.72f, 0.72f, 0.70f);
      g.wallZ(px, px + BLOCK, 0.0f, curb, pz, -1, L_CONCRETE, cc, 0, 4.0f);
      g.wallZ(px, px + BLOCK, 0.0f, curb, pz + BLOCK, 1, L_CONCRETE, cc, 0, 4.0f);
      g.wallX(pz, pz + BLOCK, 0.0f, curb, px, -1, L_CONCRETE, cc, 0, 4.0f);
      g.wallX(pz, pz + BLOCK, 0.0f, curb, px + BLOCK, 1, L_CONCRETE, cc, 0, 4.0f);

      uint32_t s = lotSeed;
      bool down = downtown_area(px, pz);
      int dist = district_at(px, pz);
      const PoiBlock* poi = poi_block_at(px, pz);

      if (poi && poi->kind != 6 && !c.lod1) {
        const char* model = poi->model ? poi->model : "comercial/loja_esquina";
        if (place_landmark(c, b, model, px, pz, px + 5.0f, pz + 5.0f, px + BLOCK - 5.0f,
                           pz + BLOCK - 5.0f, s, 0.16f)) {
          g.top(px + 4.0f, pz + 4.0f, px + BLOCK - 4.0f, pz + BLOCK - 4.0f, 0.165f,
                L_SIDEWALK, tint3(0.88f, 0.86f, 0.82f), 5.0f);
          add_poi_sign(b, *poi);
          poi_list().push_back(poi_from_block(*poi));
          continue;
        }
      }

      const char* special = nullptr;
      if (px == 104.0f && pz == 208.0f) special = "meus/condominio_apartamentos";
      else if (px == 104.0f && pz == 312.0f) special = "meus/predio_sanzio";
      if (down && (special || frand(s) < 0.02f) &&
          place_landmark(c, b, special ? special : "meus/condominio_apartamentos", px, pz,
                         px + 5.0f, pz + 5.0f, px + BLOCK - 5.0f, pz + BLOCK - 5.0f, s, 0.16f)) {
        g.top(px + 4.0f, pz + 4.0f, px + BLOCK - 4.0f, pz + BLOCK - 4.0f, 0.165f, L_SIDEWALK,
              tint3(0.88f, 0.86f, 0.82f), 5.0f);
        continue;
      }

      float plaza_chance = dist == D_MUSSEQUE ? 0.05f : dist == D_ALTA ? 0.22f : 0.15f;
      bool plaza = poi ? (poi->kind == 6) : frand(s) < plaza_chance;
      if (plaza) {
        uint32_t gc = tint3(0.85f, 0.9f, 0.85f);
        g.top(px + 8.0f, pz + 8.0f, px + BLOCK - 8.0f, pz + BLOCK - 8.0f, 0.19f, L_GRASS,
              gc, 5.0f);
        g.top(px + 30.0f, pz + 30.0f, px + BLOCK - 30.0f, pz + BLOCK - 30.0f, 0.22f,
              L_SIDEWALK, tint3(0.92f, 0.90f, 0.86f), 4.0f);
        int nt = 1 + (int)(frand(s) * 3.0f);
        for (int t = 0; t < nt; ++t) {
          float tx = px + 10.0f + frand(s) * (BLOCK - 20.0f);
          float tz = pz + 10.0f + frand(s) * (BLOCK - 20.0f);
          add_tree(b, tx, tz, s);
        }
        p.box(px + 36.0f, pz + 36.0f, px + 48.0f, pz + 42.0f, 0.19f, 0.75f,
              L_CONCRETE, tint3(0.85f, 0.82f, 0.76f), 0, 3.0f, true);
        if (!c.lod1) {
          for (int l = 0; l < 2; ++l) {
            add_lamp(b, px + (l ? 6.0f : BLOCK - 6.0f), pz + BLOCK * 0.5f,
                     l ? 1.5708f : -1.5708f);
          }
        }
        if (poi) {
          add_poi_sign(b, *poi);
          poi_list().push_back(poi_from_block(*poi));
        }
        continue;
      }

      // lots fill the block interior
      int nx = 2 + (int)(frand(s) * 2.0f);
      int nz = 2 + (int)(frand(s) * 2.0f);
      float inner = BLOCK - 8.0f;
      float cellW = inner / nx, cellD = inner / nz;
      for (int lx = 0; lx < nx; ++lx) {
        for (int lz = 0; lz < nz; ++lz) {
          uint32_t s2 = hash32(lotSeed ^ (uint32_t)(lx * 7717 + lz * 104729));
          if (frand(s2) < 0.06f) continue;
          float gx0 = px + 4.0f + lx * cellW, gz0 = pz + 4.0f + lz * cellD;
          if (dist == D_BAIXA) {
            g.top(gx0 + 0.4f, gz0 + 0.4f, gx0 + cellW - 0.4f, gz0 + cellD - 0.4f, 0.165f,
                  L_SIDEWALK, tint3(0.90f, 0.88f, 0.84f), 5.0f);
          } else if (dist == D_ALTA) {
            g.top(gx0 + 0.4f, gz0 + 0.4f, gx0 + cellW - 0.4f, gz0 + cellD - 0.4f, 0.165f,
                  L_GRASS, tint3(0.60f, 0.74f, 0.50f), 5.0f);
          } else {
            g.top(gx0 + 0.4f, gz0 + 0.4f, gx0 + cellW - 0.4f, gz0 + cellD - 0.4f, 0.165f,
                  L_SAND, tint3(0.82f, 0.74f, 0.60f), 5.0f);
          }
          float mx = 1.0f + frand(s2) * 2.6f;
          float mz = 1.0f + frand(s2) * 2.6f;
          float x0 = px + 4.0f + lx * cellW + mx;
          float z0 = pz + 4.0f + lz * cellD + mz;
          float x1 = px + 4.0f + (lx + 1) * cellW - mx;
          float z1 = pz + 4.0f + (lz + 1) * cellD - mz;
          if (lx == nx - 1) x1 = px + BLOCK - 4.0f - 0.4f;
          if (lz == nz - 1) z1 = pz + BLOCK - 4.0f - 0.4f;
          if (x1 - x0 < 4.0f || z1 - z0 < 4.0f) continue;
          if (nx > 1 && lx == 0) x0 = px + 4.0f + 0.4f;
          if (nz > 1 && lz == 0) z0 = pz + 4.0f + 0.4f;
          place_lot(c, b, px, pz, dist, x0, z0, x1, z1, s2, 0.16f);
        }
      }

      if (dist == D_MUSSEQUE) {
        int stalls = 1 + (int)(frand(s) * 2.0f);
        for (int st = 0; st < stalls; ++st) {
          float bx2 = px + 1.7f + frand(s) * 1.4f;
          float bz2 = pz + 10.0f + frand(s) * (BLOCK - 22.0f);
          float bw = 0.9f, bd = 2.0f + frand(s) * 0.8f;
          uint32_t wood = tint3(0.55f, 0.42f, 0.30f);
          uint32_t cloth = tint3(0.86f, 0.38f + frand(s) * 0.34f, 0.26f);
          b.box(bx2, bz2, bx2 + bw, bz2 + bd, 0.16f, 0.95f, L_CONCRETE, wood, 0, 2.0f, true);
          b.box(bx2 - 0.10f, bz2 - 0.12f, bx2 + bw + 0.10f, bz2 + bd + 0.12f, 2.02f, 2.14f,
                L_FACADE_A, cloth, 0, 2.0f, true);
          b.box(bx2 + 0.02f, bz2 - 0.06f, bx2 + 0.08f, bz2, 0.16f, 2.10f, L_CONCRETE,
                tint3(0.42f, 0.42f, 0.42f), 0, 1.5f, false);
          b.box(bx2 + bw - 0.08f, bz2 + bd, bx2 + bw - 0.02f, bz2 + bd + 0.06f, 0.16f, 2.10f,
                L_CONCRETE, tint3(0.42f, 0.42f, 0.42f), 0, 1.5f, false);
          c.colliders.push_back({{bx2, -1.0f, bz2}, {bx2 + bw, 1.2f, bz2 + bd}});
        }
      }

      if (!c.lod1) {
        for (int side = 0; side < 4; ++side) {
          if (frand(s) > 0.55f) continue;
          float t = 9.0f + frand(s) * (BLOCK - 18.0f);
          float tx = side == 0 ? px + 2.1f : side == 1 ? px + BLOCK - 2.1f : px + t;
          float tz = side < 2 ? pz + t : side == 2 ? pz + 2.1f : pz + BLOCK - 2.1f;
          add_tree(b, tx, tz, s);
        }
        add_lamp(b, px + 1.5f, pz + 1.5f, 0.0f);
        add_lamp(b, px + BLOCK - 1.5f, pz + BLOCK - 1.5f, 3.1416f);
      }
    }
  }

  c.ground = mesh_upload(g.v);
  c.bld = mesh_upload(b.v);
  c.props = mesh_upload(p.v);
  upload_instances(c);
  c.built = true;
  c.bounds = c.ground.bounds;
  AABB bb = c.bld.bounds;
  c.bounds.mn.x = std::min(c.bounds.mn.x, bb.mn.x);
  c.bounds.mn.y = -2.0f;
  c.bounds.mn.z = std::min(c.bounds.mn.z, bb.mn.z);
  c.bounds.mx.x = std::max(c.bounds.mx.x, bb.mx.x);
  c.bounds.mx.y = 300.0f;
  c.bounds.mx.z = std::max(c.bounds.mx.z, bb.mx.z);
}

inline void build_features(City& city, Mesh& treeMesh, Mesh& lampMesh) {
  Builder b;
  // promenade along the bay
  b.top(-1700.0f, -843.0f, 1700.0f, -830.0f, 0.18f, L_CONCRETE, 0xffffffff, 5.0f);
  b.wallZ(-1700.0f, 1700.0f, 0.0f, 0.18f, -843.0f, -1, L_CONCRETE, tint3(0.7f, 0.7f, 0.68f), 0, 4.0f);
  // railing posts + top rail (com abertura para a ponte da Ilha)
  for (int i = 0; i < 170; ++i) {
    float x = -1700.0f + i * 20.0f;
    if (std::fabs(x) < 20.0f) continue;
    b.box(x - 0.08f, -842.8f, x + 0.08f, -842.4f, 0.18f, 1.25f, L_WHITE,
          tint3(0.8f, 0.8f, 0.78f), 0, 2.0f, false);
  }
  b.box(-1700.0f, -842.9f, -20.0f, -842.3f, 1.15f, 1.3f, L_WHITE, tint3(0.8f, 0.8f, 0.78f), 0, 2.0f, false);
  b.box(20.0f, -842.9f, 1700.0f, -842.3f, 1.15f, 1.3f, L_WHITE, tint3(0.8f, 0.8f, 0.78f), 0, 2.0f, false);

  // causeway Marginal -> Ilha do Cabo
  b.top(-14.0f, -1022.0f, 14.0f, -843.0f, 0.02f, L_SAND, tint3(0.85f, 0.78f, 0.64f), 7.0f);
  b.top(-7.0f, -1022.0f, 7.0f, -843.0f, 0.10f, L_ASPHALT, 0xffffffff, 8.0f);
  b.wallZ(-14.0f, -7.0f, -0.6f, 0.10f, -1020.0f, -1, L_SAND, tint3(0.78f, 0.72f, 0.58f), 0, 7.0f);
  b.wallZ(7.0f, 14.0f, -0.6f, 0.10f, -1020.0f, -1, L_SAND, tint3(0.78f, 0.72f, 0.58f), 0, 7.0f);
  for (int i = 0; i < 2; ++i) {
    float x = i ? 6.4f : -6.4f;
    b.top(x - 0.12f, -1018.0f, x + 0.12f, -847.0f, 0.112f, L_WHITE,
          tint3(0.70f, 0.68f, 0.62f), 8.0f);
  }
  for (int i = 0; i < 14; ++i) {
    float z = -850.0f - (float)i * 12.0f;
    b.top(-0.12f, z, 0.12f, z + 5.0f, 0.112f, L_WHITE, tint3(0.72f, 0.70f, 0.64f), 8.0f);
  }
  for (int i = 0; i < 16; ++i) {
    float z = -847.0f - (float)i * 11.5f;
    for (int side = 0; side < 2; ++side) {
      float x = side ? 13.4f : -13.4f;
      b.box(x - 0.05f, z - 0.05f, x + 0.05f, z + 0.05f, 0.02f, 1.1f, L_WHITE,
            tint3(0.78f, 0.76f, 0.70f), 0, 2.0f, false);
    }
  }

  // sand beach between promenade and water
  b.top(-1700.0f, -855.0f, 1700.0f, -843.0f, 0.02f, L_SAND, 0xffffffff, 7.0f);

  // palm alley on the promenade
  uint32_t s = 77u;
  for (int i = 0; i < 34; ++i) {
    float x = -620.0f + i * 38.0f;
    float jitter = (frand(s) - 0.5f) * 2.0f;
    Mat4 m = mat_mul(mat_mul(mat_translate(v3(x + jitter, 0.18f, -832.0f)), mat_rot_y(frand(s) * 6.28f)),
                     mat_scale(v3(0.95f + frand(s) * 0.4f, 1.0f, 0.95f + frand(s) * 0.4f)));
    push_mat(city.ilhaPalmData, m);
  }
  // palms on the islands
  for (int i = 0; i < 26; ++i) {
    float x = -780.0f + frand(s) * 1500.0f;
    float z = -1052.0f + (frand(s) - 0.5f) * 34.0f;
    Mat4 m = mat_mul(mat_mul(mat_translate(v3(x, 0.1f, z)), mat_rot_y(frand(s) * 6.28f)),
                     mat_scale(v3(0.9f + frand(s) * 0.5f, 1.0f, 0.9f + frand(s) * 0.5f)));
    push_mat(city.ilhaPalmData, m);
  }
  (void)lampMesh;
  city.promenade = mesh_upload(b.v);

  // Ilha do Cabo sandbar
  Builder il;
  il.top(-820.0f, -1097.0f, 760.0f, -1022.0f, 0.06f, L_SAND, 0xffffffff, 9.0f);
  il.wallZ(-820.0f, 760.0f, -1.5f, 0.06f, -1022.0f, 1, L_SAND, tint3(0.8f, 0.8f, 0.7f), 0, 9.0f);
  il.wallZ(-820.0f, 760.0f, -1.5f, 0.06f, -1097.0f, -1, L_SAND, tint3(0.8f, 0.8f, 0.7f), 0, 9.0f);
  uint32_t sx = 991u;
  for (int i = 0; i < 14; ++i) {
    float x = -700.0f + frand(sx) * 1300.0f;
    float z = -1082.0f + frand(sx) * 50.0f;
    float w = 4.0f + frand(sx) * 4.0f, h = 3.0f + frand(sx) * 1.8f;
    uint32_t c = tint3(0.95f, 0.85f, 0.75f);
    il.box(x, z, x + w, z + w * 0.8f, 0.06f, h, L_FACADE_B, c, frand(sx) * 4.0f, 8.0f, true);
  }
  for (int i = 0; i < 3; ++i) {
    float x = -300.0f + i * 320.0f;
    il.box(x - 2.0f, -1022.0f, x + 2.0f, -962.0f, -0.4f, 0.34f, L_CONCRETE,
           tint3(0.62f, 0.58f, 0.5f), 0, 6.0f, true);
  }
  // quiosques da Ilha do Cabo
  for (int i = 0; i < 8; ++i) {
    float kx = -640.0f + (float)i * 160.0f + frand(sx) * 40.0f;
    float kz = -1064.0f + frand(sx) * 20.0f;
    float w = 3.4f, d = 2.6f, h = 2.5f;
    il.box(kx - w * 0.5f, kz - d * 0.5f, kx + w * 0.5f, kz + d * 0.5f, 0.06f, h,
           L_FACADE_A, tint3(0.93f, 0.86f, 0.74f), frand(sx) * 4.0f, 6.0f, true);
    il.box(kx - w * 0.5f - 0.25f, kz - d * 0.5f - 0.25f, kx + w * 0.5f + 0.25f,
           kz + d * 0.5f + 0.25f, h, h + 0.18f, L_ROOF, tint3(0.62f, 0.46f, 0.30f), 0, 6.0f,
           true);
    il.box(kx - w * 0.5f + 0.2f, kz - d * 0.5f - 0.05f, kx + w * 0.5f - 0.2f,
           kz - d * 0.5f + 0.12f, h - 0.55f, h - 0.18f, L_EMIT, tint3(1.00f, 0.72f, 0.30f), 0,
           1.0f, false);
  }
  city.ilha = mesh_upload(il.v);

  // quarteiroes antigos do outro lado da baia (scan aereo em baixa densidade)
  const AssetModel* fm = asset_get("meus/complexo_baia");
  if (fm) {
    Builder fc;
    static const float spots[][3] = {
      {-1050.0f, -1290.0f, 0.35f},
      {80.0f, -1260.0f, 2.10f},
      {1080.0f, -1315.0f, -0.45f},
      {1950.0f, -1275.0f, 1.20f},
    };
    for (const auto& sp : spots) append_model(fc.v, *fm, sp[0], sp[1], sp[2], 6.0f, -0.30f);
    city.farCity = mesh_upload(fc.v);
  }

  // sea surface
  Builder w;
  const int WX = 40, WZ = 26;
  const float x0 = -3600.0f, x1 = 3600.0f, z0 = -3200.0f, z1 = -842.0f;
  for (int iz = 0; iz < WZ; ++iz) {
    for (int ix = 0; ix < WX; ++ix) {
      float ax = x0 + (x1 - x0) * ix / WX, bx = x0 + (x1 - x0) * (ix + 1) / WX;
      float az = z0 + (z1 - z0) * iz / WZ, bz = z0 + (z1 - z0) * (iz + 1) / WZ;
      V3 p0 = {ax, -0.28f, bz}, p1 = {bx, -0.28f, bz}, p2 = {bx, -0.28f, az}, p3 = {ax, -0.28f, az};
      uint32_t base = (uint32_t)w.v.v.size();
      w.v.v.push_back({p0.x, p0.y, p0.z, 0, 1, 0, 0, 0, 0, 0xffffffff});
      w.v.v.push_back({p1.x, p1.y, p1.z, 0, 1, 0, 0, 0, 0, 0xffffffff});
      w.v.v.push_back({p2.x, p2.y, p2.z, 0, 1, 0, 0, 0, 0, 0xffffffff});
      w.v.v.push_back({p3.x, p3.y, p3.z, 0, 1, 0, 0, 0, 0, 0xffffffff});
      w.v.i.push_back(base);
      w.v.i.push_back(base + 1);
      w.v.i.push_back(base + 2);
      w.v.i.push_back(base);
      w.v.i.push_back(base + 2);
      w.v.i.push_back(base + 3);
    }
  }
  city.sea = mesh_upload(w.v);

  city.ilhaPalmInst = make_instance_buffer();
  glBindBuffer(GL_ARRAY_BUFFER, city.ilhaPalmInst);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(city.ilhaPalmData.size() * 4),
               city.ilhaPalmData.data(), GL_STATIC_DRAW);
  city.ilhaPalmMesh = clone_with_instances(treeMesh, city.ilhaPalmInst, false);
  city.ilhaPalms = (int)(city.ilhaPalmData.size() / 16);

  glGenVertexArrays(1, &city.emptyVao);
  (void)lampMesh;
}

inline void build_city(City& city, Mesh& treeMesh, Mesh& lampMesh) {
  build_signals();
  lot_records_clear();
  city.chunks.resize(12);
  int idx = 0;
  for (int cz = -1; cz <= 1; ++cz) {
    for (int cx = -2; cx <= 1; ++cx) {
      Chunk& c = city.chunks[idx];
      c.cx = cx;
      c.cz = cz;
      c.lod1 = !(cx >= -1 && cx <= 0 && cz >= -1 && cz <= 0);
      uint32_t seed = hash32(0xC0FFEEu + (uint32_t)(cx * 92821 + cz * 68917));
      build_chunk(c, seed, treeMesh, lampMesh);
      ++idx;
    }
  }
  build_features(city, treeMesh, lampMesh);
}
