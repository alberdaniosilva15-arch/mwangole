#pragma once
#include <GLES3/gl3.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "math.h"

inline char g_gfx_error[512] = {0};

inline void gfx_fail(const char* msg) {
  if (g_gfx_error[0] == 0) {
    std::snprintf(g_gfx_error, sizeof(g_gfx_error), "%s", msg);
  }
  std::fprintf(stderr, "[gfx] %s\n", msg);
}

struct Vertex {
  float px, py, pz;
  float nx, ny, nz;
  float u, v;
  float layer;
  uint32_t color;
};

inline uint32_t rgba(int r, int g, int b, int a = 255) {
  return (uint32_t)(r & 255) | ((uint32_t)(g & 255) << 8) |
         ((uint32_t)(b & 255) << 16) | ((uint32_t)(a & 255) << 24);
}

inline uint32_t tint3(float r, float g, float b, float a = 1.0f) {
  return rgba((int)(clampf(r, 0, 1) * 255), (int)(clampf(g, 0, 1) * 255),
              (int)(clampf(b, 0, 1) * 255), (int)(clampf(a, 0, 1) * 255));
}

struct Mesh {
  GLuint vao = 0, vbo = 0, ibo = 0;
  GLsizei count = 0;
  AABB bounds{};
};

struct Verts {
  std::vector<Vertex> v;
  std::vector<uint32_t> i;

  void reserve(size_t nv, size_t ni) {
    v.reserve(nv);
    i.reserve(ni);
  }
};

inline void mesh_attribs() {
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)12);
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)24);
  glEnableVertexAttribArray(3);
  glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)32);
  glEnableVertexAttribArray(4);
  glVertexAttribPointer(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)36);
}

inline void mesh_bounds(Mesh& m, const std::vector<Vertex>& verts) {
  if (verts.empty()) return;
  V3 mn = {verts[0].px, verts[0].py, verts[0].pz};
  V3 mx = mn;
  for (const auto& v : verts) {
    mn.x = std::min(mn.x, v.px); mn.y = std::min(mn.y, v.py); mn.z = std::min(mn.z, v.pz);
    mx.x = std::max(mx.x, v.px); mx.y = std::max(mx.y, v.py); mx.z = std::max(mx.z, v.pz);
  }
  m.bounds.mn = mn;
  m.bounds.mx = mx;
}

inline Mesh make_mesh(const std::vector<Vertex>& verts, const std::vector<uint32_t>& idx) {
  Mesh m;
  if (verts.empty() || idx.empty()) return m;
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);
  glGenBuffers(1, &m.vbo);
  glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(Vertex)), verts.data(), GL_STATIC_DRAW);
  glGenBuffers(1, &m.ibo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ibo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size() * 4), idx.data(), GL_STATIC_DRAW);
  mesh_attribs();
  glBindVertexArray(0);
  m.count = (GLsizei)idx.size();
  mesh_bounds(m, verts);
  return m;
}

inline void draw_mesh(const Mesh& m) {
  if (!m.vao || !m.count) return;
  glBindVertexArray(m.vao);
  glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
}

inline GLuint make_instance_buffer() {
  GLuint b = 0;
  glGenBuffers(1, &b);
  return b;
}

inline void attach_instances(Mesh& m, GLuint instanceVbo, bool withTint) {
  glBindVertexArray(m.vao);
  glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
  GLsizei stride = withTint ? 80 : 64;
  for (int i = 0; i < 4; ++i) {
    glEnableVertexAttribArray(5 + i);
    glVertexAttribPointer(5 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(intptr_t)(i * 16));
    glVertexAttribDivisor(5 + i, 1);
  }
  if (withTint) {
    glEnableVertexAttribArray(9);
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, (void*)(intptr_t)64);
    glVertexAttribDivisor(9, 1);
  }
  glBindVertexArray(0);
}

inline Mesh clone_with_instances(const Mesh& src, GLuint instanceVbo, bool withTint) {
  Mesh m;
  m.count = src.count;
  m.bounds = src.bounds;
  m.vbo = src.vbo;
  m.ibo = src.ibo;
  glGenVertexArrays(1, &m.vao);
  glBindVertexArray(m.vao);
  glBindBuffer(GL_ARRAY_BUFFER, src.vbo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, src.ibo);
  mesh_attribs();
  glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
  GLsizei stride = withTint ? 80 : 64;
  for (int i = 0; i < 4; ++i) {
    glEnableVertexAttribArray(5 + i);
    glVertexAttribPointer(5 + i, 4, GL_FLOAT, GL_FALSE, stride, (void*)(intptr_t)(i * 16));
    glVertexAttribDivisor(5 + i, 1);
  }
  if (withTint) {
    glEnableVertexAttribArray(9);
    glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, stride, (void*)(intptr_t)64);
    glVertexAttribDivisor(9, 1);
  }
  glBindVertexArray(0);
  return m;
}

// ---------------------------------------------------------------- shaders

inline GLuint compile_shader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, nullptr);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[4096] = {0};
    glGetShaderInfoLog(s, sizeof(log) - 1, nullptr, log);
    gfx_fail(log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

inline GLuint link_program(const char* vs, const char* fs) {
  GLuint v = compile_shader(GL_VERTEX_SHADER, vs);
  if (!v) return 0;
  GLuint f = compile_shader(GL_FRAGMENT_SHADER, fs);
  if (!f) {
    glDeleteShader(v);
    return 0;
  }
  GLuint p = glCreateProgram();
  glAttachShader(p, v);
  glAttachShader(p, f);
  glLinkProgram(p);
  glDeleteShader(v);
  glDeleteShader(f);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[4096] = {0};
    glGetProgramInfoLog(p, sizeof(log) - 1, nullptr, log);
    gfx_fail(log);
    glDeleteProgram(p);
    return 0;
  }
  return p;
}

enum Layer {
  L_WHITE = 0,
  L_ASPHALT,
  L_SIDEWALK,
  L_ROOF,
  L_FACADE_A,
  L_FACADE_B,
  L_FACADE_C,
  L_GLASS,
  L_SAND,
  L_GRASS,
  L_CONCRETE,
  L_SHOP,
  L_BLOB,
  L_RUBBER,
  L_EMIT,
  L_COUNT
};

// ---------------------------------------------------------------- atlas

inline uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU;
  x ^= x >> 15; x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

inline float hashf(int x, int y) {
  return (hash32((uint32_t)x * 374761393U + (uint32_t)y * 668265263U) & 0xffff) / 65535.0f;
}

inline float vnoise(float x, float y) {
  int xi = (int)std::floor(x), yi = (int)std::floor(y);
  float xf = x - xi, yf = y - yi;
  float u = xf * xf * (3 - 2 * xf), v = yf * yf * (3 - 2 * yf);
  float a = hashf(xi, yi), b = hashf(xi + 1, yi);
  float c = hashf(xi, yi + 1), d = hashf(xi + 1, yi + 1);
  return (a + (b - a) * u) + ((c + (d - c) * u) - (a + (b - a) * u)) * v;
}

inline float fbm(float x, float y, int oct = 3) {
  float s = 0.0f, a = 0.5f;
  for (int i = 0; i < oct; ++i) {
    s += vnoise(x, y) * a;
    x *= 2.03f; y *= 2.03f; a *= 0.5f;
  }
  return s;
}

inline void set_px(std::vector<uint8_t>& data, int S, int x, int y, int r, int g, int b, int a) {
  size_t o = ((size_t)y * S + x) * 4;
  data[o + 0] = (uint8_t)clampf((float)r, 0, 255);
  data[o + 1] = (uint8_t)clampf((float)g, 0, 255);
  data[o + 2] = (uint8_t)clampf((float)b, 0, 255);
  data[o + 3] = (uint8_t)clampf((float)a, 0, 255);
}

inline GLuint make_atlas(int S = 256) {
  std::vector<uint8_t> layer((size_t)S * S * 4);
  std::vector<std::vector<uint8_t>> layers;

  auto push = [&]() {
    layers.push_back(layer);
  };

  // 0 white
  std::fill(layer.begin(), layer.end(), 255);
  push();

  // 1 asphalt
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float n = hashf(x, y);
      float patch = fbm(x * 0.035f, y * 0.035f, 2);
      int v = (int)(70 + n * 13 + patch * 18);
      if (((x * 7 + y * 13) % 97) == 0) v += 16;
      set_px(layer, S, x, y, v + 4, v, v - 7, 255);
    }
  }
  push();
  // 2 sidewalk
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      int v = 112 + (int)(hashf(x, y) * 12);
      if ((x % 32) < 2 || (y % 32) < 2) v = 90;
      if ((x % 32) < 3 && (y % 32) < 3) v = 100;
      set_px(layer, S, x, y, v + 3, v - 2, v - 9, 255);
    }
  }
  push();
  // 3 roof
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float n = fbm(x * 0.05f, y * 0.05f, 2);
      int v = 104 + (int)(n * 26);
      if (fbm(x * 0.012f + 40, y * 0.012f, 2) > 0.62f) v -= 14;
      set_px(layer, S, x, y, v, v - 2, v - 6, 255);
    }
  }
  push();
  // 4 facade A - cream with deep windows
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      int r = 202, g = 190, b = 166;
      int wx = x % 60, wy = y % 76;
      if (wy < 6) { r -= 18; g -= 18; b -= 18; }
      if (wx >= 10 && wx < 54 && wy >= 14 && wy < 62) {
        float grad = (float)(wy - 14) / 48.0f;
        float warm = 1.0f - grad;
        r = (int)(60 + warm * 112);
        g = (int)(72 + warm * 74);
        b = (int)(92 + warm * 38);
        uint32_t cell = hash32((uint32_t)(x / 60) * 9781U + (uint32_t)(y / 76) * 6271U + 17U);
        if ((cell & 7U) == 0U && wx > 16 && wx < 48 && wy > 20 && wy < 56) {
          r = 238; g = 198; b = 132;
        }
        if (wx < 13 || wx > 50) { r = 214; g = 206; b = 190; }
        if (wy > 56 && wy < 62) { r = 222; g = 214; b = 198; }
        if (wy == 62) { r = 112; g = 106; b = 96; }
      }
      float n = (hashf(x, y) - 0.5f) * 5;
      set_px(layer, S, x, y, (int)(r + n), (int)(g + n), (int)(b + n), 255);
    }
  }
  push();
  // 5 facade B - terracotta with balconies
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      int r = 172, g = 102, b = 74;
      int wx = x % 64, wy = y % 84;
      if (wy < 7) { r = 232; g = 222; b = 204; }
      else if (wy < 10) { r -= 26; g -= 26; b -= 26; }
      if (wx >= 12 && wx < 52 && wy >= 20 && wy < 70) {
        float grad = (float)(wy - 20) / 50.0f;
        float warm = 1.0f - grad;
        r = (int)(55 + warm * 125);
        g = (int)(66 + warm * 80);
        b = (int)(88 + warm * 36);
        uint32_t cell = hash32((uint32_t)(x / 64) * 7331U + (uint32_t)(y / 84) * 4657U + 91U);
        if ((cell & 7U) == 0U && wx > 18 && wx < 46 && wy > 26 && wy < 64) {
          r = 244; g = 196; b = 128;
        }
        if (wx < 15 || wx > 49) { r = 236; g = 226; b = 208; }
        if (wy > 64 && wy < 70) { r = 244; g = 236; b = 220; }
      }
      float n = (hashf(x, y) - 0.5f) * 7;
      set_px(layer, S, x, y, (int)(r + n), (int)(g + n), (int)(b + n), 255);
    }
  }
  push();
  // 6 facade C - modern ribbon glass
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      int r = 180, g = 188, b = 184;
      int wy = y % 86;
      if (wy < 5) { r -= 14; g -= 14; b -= 12; }
      if (wy >= 24 && wy < 66) {
        float grad = (float)(wy - 24) / 42.0f;
        float warm = 1.0f - grad;
        r = (int)(58 + warm * 118);
        g = (int)(74 + warm * 84);
        b = (int)(96 + warm * 42);
        uint32_t cell = hash32((uint32_t)(x / 40) * 5113U + (uint32_t)(y / 86) * 3571U + 203U);
        if ((cell & 11U) == 0U) { r = 240; g = 200; b = 140; }
        if ((x % 40) < 3) { r = 214; g = 214; b = 206; }
        if ((x % 40) < 5 && wy < 28) { r = 246; g = 244; b = 236; }
      }
      float n = (hashf(x, y) - 0.5f) * 5;
      set_px(layer, S, x, y, (int)(r + n), (int)(g + n), (int)(b + n), 255);
    }
  }
  push();
  // 7 glass
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float t = (float)y / S;
      int r = (int)(34 + t * 52), g = (int)(44 + t * 54), b = (int)(58 + t * 50);
      int d = x + y;
      if (d % 97 < 18) { r += 62; g += 64; b += 60; }
      if ((x % 64) == 0 || (y % 64) == 0) { r = 18; g = 24; b = 30; }
      set_px(layer, S, x, y, r, g, b, 255);
    }
  }
  push();
  // 8 sand
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float n = hashf(x, y) * 12 + fbm(x * 0.02f, y * 0.02f, 2) * 20;
      set_px(layer, S, x, y, (int)(198 + n), (int)(180 + n), (int)(142 + n * 0.7f), 255);
    }
  }
  push();
  // 9 grass
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float n = fbm(x * 0.04f, y * 0.04f, 3);
      int r = (int)(56 + n * 24), g = (int)(92 + n * 36), b = (int)(48 + n * 18);
      if (hashf(x, y) > 0.94f) { r -= 12; g -= 8; b -= 8; }
      set_px(layer, S, x, y, r, g, b, 255);
    }
  }
  push();
  // 10 concrete
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float n = fbm(x * 0.03f, y * 0.03f, 2);
      int v = (int)(146 + n * 22 + (hashf(x, y) - 0.5f) * 8);
      set_px(layer, S, x, y, v, v - 4, v - 11, 255);
    }
  }
  push();
  // 11 shopfront
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      int r = 32, g = 30, b = 34;
      if (y < 28) {
        r = (x / 24) % 2 ? 120 : 220;
        g = (x / 24) % 2 ? 44 : 214;
        b = (x / 24) % 2 ? 40 : 196;
        if (y > 22) { r /= 2; g /= 2; b /= 2; }
      } else if (y > 96 && y < 232) {
        int wx = x % 52;
        if (wx > 6 && wx < 46) {
          float grad = (float)(y - 96) / 136.0f;
          r = (int)(232 - grad * 80);
          g = (int)(180 - grad * 70);
          b = (int)(104 - grad * 50);
          if (wx < 10 || wx > 42) { r = 18; g = 16; b = 18; }
          if (y > 104 && y < 112) { r = 255; g = 232; b = 170; }
        }
        if (y > 228) { r = 20; g = 20; b = 22; }
      }
      set_px(layer, S, x, y, r, g, b, 255);
    }
  }
  push();
  // 12 blob shadow
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float dx = (x + 0.5f) / S * 2.0f - 1.0f;
      float dy = (y + 0.5f) / S * 2.0f - 1.0f;
      float d = std::sqrt(dx * dx + dy * dy);
      float a = clampf(1.0f - d, 0.0f, 1.0f);
      a = a * a * 0.62f;
      set_px(layer, S, x, y, 0, 0, 0, (int)(a * 255));
    }
  }
  push();
  // 13 rubber
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      int v = 26 + (int)(hashf(x, y) * 8);
      set_px(layer, S, x, y, v, v, v + 2, 255);
    }
  }
  push();
  // 14 emissive (unlit: lamps, headlights, beacons)
  std::fill(layer.begin(), layer.end(), 255);
  push();

  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
  glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, S, S, (GLsizei)layers.size(), 0,
               GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  for (size_t i = 0; i < layers.size(); ++i) {
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)i, S, S, 1, GL_RGBA,
                    GL_UNSIGNED_BYTE, layers[i].data());
  }
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
  glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
  return tex;
}

inline GLuint make_cloud_texture(int S = 512) {
  std::vector<uint8_t> data((size_t)S * S * 4);
  for (int y = 0; y < S; ++y) {
    for (int x = 0; x < S; ++x) {
      float n = fbm(x * 0.006f, y * 0.006f, 4);
      float m = fbm(x * 0.019f + 31, y * 0.019f + 17, 3);
      float a = smoothstepf(0.48f, 0.74f, n * 0.75f + m * 0.35f);
      size_t o = ((size_t)y * S + x) * 4;
      data[o + 0] = 255;
      data[o + 1] = 246;
      data[o + 2] = 236;
      data[o + 3] = (uint8_t)(a * 235);
    }
  }
  GLuint tex = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, S, S, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glGenerateMipmap(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, 0);
  return tex;
}

// ---------------------------------------------------------------- programs

struct WorldProg {
  GLuint id = 0;
  GLint vp = -1, atlas = -1, sunDir = -1, sunCol = -1, ambSky = -1, ambGround = -1;
  GLint camPos = -1, fogCol = -1, fogNear = -1, fogFar = -1, model = -1;
};

struct CarProg {
  GLuint id = 0;
  GLint vp = -1, atlas = -1, sunDir = -1, sunCol = -1, ambSky = -1, ambGround = -1;
  GLint camPos = -1, fogCol = -1, fogNear = -1, fogFar = -1, tintMode = -1, spec = -1;
};

struct SkyProg {
  GLuint id = 0;
  GLint right = -1, up = -1, fwd = -1, tanHalf = -1, aspect = -1, sunDir = -1,
        sunCol = -1, clouds = -1, time = -1, horizon = -1, zenith = -1, day = -1;
};

struct WaterProg {
  GLuint id = 0;
  GLint vp = -1, camPos = -1, sunDir = -1, sunCol = -1, fogCol = -1;
  GLint fogNear = -1, fogFar = -1, time = -1, skyMul = -1;
};

struct BlobProg {
  GLuint id = 0;
  GLint vp = -1, atlas = -1, strength = -1;
};

static const char* k_common_fs = R"(#version 300 es
precision highp float;
precision highp sampler2DArray;
in vec3 vN;
in vec2 vUv;
in float vLayer;
in vec4 vCol;
in vec3 vWorld;
uniform sampler2DArray uAtlas;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uAmbSky;
uniform vec3 uAmbGround;
uniform vec3 uCamPos;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform float uSpec;
out vec4 outColor;

void main() {
  if (vLayer > 13.5) {
    vec3 e = vCol.rgb;
    e = (e * (2.51 * e + 0.03)) / (e * (2.43 * e + 0.59) + 0.14);
    outColor = vec4(e, 1.0);
    return;
  }
  vec4 tex = vLayer < 0.0 ? vec4(1.0) : texture(uAtlas, vec3(vUv, vLayer));
  vec3 albedo = tex.rgb * vCol.rgb;
  vec3 n = normalize(vN);
  float ndl = max(dot(n, uSunDir), 0.0);
  vec3 amb = mix(uAmbGround, uAmbSky, n.y * 0.5 + 0.5);
  vec3 lit = albedo * (amb + uSunColor * ndl);
  if (uSpec > 0.0) {
    vec3 V = normalize(uCamPos - vWorld);
    vec3 Hv = normalize(uSunDir + V);
    float lum = dot(vCol.rgb, vec3(0.299, 0.587, 0.114));
    float glassy = 1.0 - smoothstep(0.14, 0.34, lum);
    float sp = pow(max(dot(n, Hv), 0.0), 42.0) * (0.16 + 0.80 * glassy);
    float fres = pow(1.0 - max(dot(n, V), 0.0), 4.0) * glassy * 0.22;
    lit += uSunColor * (sp + fres) * uSpec;
  }
  float dist = distance(vWorld, uCamPos);
  float fog = smoothstep(uFogNear, uFogFar, dist);
  vec3 col = mix(lit, uFogColor, fog);
  col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
  outColor = vec4(col, tex.a);
}
)";

inline const char* world_vs_src() {
  return R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUv;
layout(location=3) in float aLayer;
layout(location=4) in vec4 aCol;
uniform mat4 uViewProj;
out vec3 vN;
out vec2 vUv;
out float vLayer;
out vec4 vCol;
out vec3 vWorld;
void main() {
  vWorld = aPos;
  vN = aNrm;
  vUv = aUv;
  vLayer = aLayer;
  vCol = aCol;
  gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";
}

inline const char* inst_vs_src() {
  return R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUv;
layout(location=3) in float aLayer;
layout(location=4) in vec4 aCol;
layout(location=5) in mat4 aModel;
layout(location=9) in vec4 aTint;
uniform mat4 uViewProj;
uniform int uTintMode;
out vec3 vN;
out vec2 vUv;
out float vLayer;
out vec4 vCol;
out vec3 vWorld;
void main() {
  vec4 wp = aModel * vec4(aPos, 1.0);
  vWorld = wp.xyz;
  vN = mat3(aModel) * aNrm;
  vUv = aUv;
  vLayer = aLayer;
  vCol = uTintMode == 1 ? vec4(aCol.rgb * mix(vec3(1.0), aTint.rgb, aCol.a), aCol.a) : aCol;
  gl_Position = uViewProj * wp;
}
)";
}

inline const char* ped_vs_src() {
  return R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUv;
layout(location=3) in float aLayer;
layout(location=4) in vec4 aCol;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vN;
out vec2 vUv;
out float vLayer;
out vec4 vCol;
out vec3 vWorld;
void main() {
  vec4 wp = uModel * vec4(aPos, 1.0);
  vWorld = wp.xyz;
  vN = mat3(uModel) * aNrm;
  vUv = aUv;
  vLayer = aLayer;
  vCol = aCol;
  gl_Position = uViewProj * wp;
}
)";
}

inline bool init_world_program(WorldProg& p) {
  p.id = link_program(world_vs_src(), k_common_fs);
  if (!p.id) return false;
  p.vp = glGetUniformLocation(p.id, "uViewProj");
  p.atlas = glGetUniformLocation(p.id, "uAtlas");
  p.sunDir = glGetUniformLocation(p.id, "uSunDir");
  p.sunCol = glGetUniformLocation(p.id, "uSunColor");
  p.ambSky = glGetUniformLocation(p.id, "uAmbSky");
  p.ambGround = glGetUniformLocation(p.id, "uAmbGround");
  p.camPos = glGetUniformLocation(p.id, "uCamPos");
  p.fogCol = glGetUniformLocation(p.id, "uFogColor");
  p.fogNear = glGetUniformLocation(p.id, "uFogNear");
  p.fogFar = glGetUniformLocation(p.id, "uFogFar");
  return true;
}

inline bool init_ped_program(WorldProg& p) {
  p.id = link_program(ped_vs_src(), k_common_fs);
  if (!p.id) return false;
  p.vp = glGetUniformLocation(p.id, "uViewProj");
  p.model = glGetUniformLocation(p.id, "uModel");
  p.atlas = glGetUniformLocation(p.id, "uAtlas");
  p.sunDir = glGetUniformLocation(p.id, "uSunDir");
  p.sunCol = glGetUniformLocation(p.id, "uSunColor");
  p.ambSky = glGetUniformLocation(p.id, "uAmbSky");
  p.ambGround = glGetUniformLocation(p.id, "uAmbGround");
  p.camPos = glGetUniformLocation(p.id, "uCamPos");
  p.fogCol = glGetUniformLocation(p.id, "uFogColor");
  p.fogNear = glGetUniformLocation(p.id, "uFogNear");
  p.fogFar = glGetUniformLocation(p.id, "uFogFar");
  return true;
}

inline bool init_car_program(CarProg& p) {
  p.id = link_program(inst_vs_src(), k_common_fs);
  if (!p.id) return false;
  p.vp = glGetUniformLocation(p.id, "uViewProj");
  p.atlas = glGetUniformLocation(p.id, "uAtlas");
  p.sunDir = glGetUniformLocation(p.id, "uSunDir");
  p.sunCol = glGetUniformLocation(p.id, "uSunColor");
  p.ambSky = glGetUniformLocation(p.id, "uAmbSky");
  p.ambGround = glGetUniformLocation(p.id, "uAmbGround");
  p.camPos = glGetUniformLocation(p.id, "uCamPos");
  p.fogCol = glGetUniformLocation(p.id, "uFogColor");
  p.fogNear = glGetUniformLocation(p.id, "uFogNear");
  p.fogFar = glGetUniformLocation(p.id, "uFogFar");
  p.tintMode = glGetUniformLocation(p.id, "uTintMode");
  p.spec = glGetUniformLocation(p.id, "uSpec");
  return true;
}

inline bool init_sky_program(SkyProg& p) {
  const char* vs = R"(#version 300 es
out vec2 vUv;
void main() {
  vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
  vUv = p;
  gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
  const char* fs = R"(#version 300 es
precision highp float;
in vec2 vUv;
uniform vec3 uRight;
uniform vec3 uUp;
uniform vec3 uFwd;
uniform float uTanHalf;
uniform float uAspect;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform sampler2D uClouds;
uniform float uTime;
uniform vec3 uHorizon;
uniform vec3 uZenith;
uniform float uDay;
out vec4 outColor;

void main() {
  vec2 ndc = vUv * 2.0 - 1.0;
  vec3 dir = normalize(uFwd + uRight * (ndc.x * uTanHalf * uAspect) + uUp * (ndc.y * uTanHalf));

  float h = clamp(dir.y, 0.0, 1.0);
  vec3 horizon = uHorizon;
  vec3 zenith = uZenith;
  vec3 col = mix(horizon, zenith, pow(h, 0.55));

  float sunAmt = max(dot(dir, uSunDir), 0.0);
  col += uSunColor * pow(sunAmt, 900.0) * 12.0;
  col += uSunColor * pow(sunAmt, 18.0) * 0.35;
  col += uSunColor * pow(sunAmt, 4.0) * 0.10;

  if (dir.y > 0.015) {
    vec2 cuv = dir.xz / max(dir.y, 0.06) * 0.22 + vec2(uTime * 0.0035, uTime * 0.0018);
    float c = texture(uClouds, cuv).a;
    float fade = smoothstep(0.015, 0.30, dir.y);
    vec3 cloudCol = mix(vec3(0.72, 0.76, 0.82), vec3(1.0, 0.98, 0.94), pow(sunAmt, 3.0));
    cloudCol *= mix(0.08, 1.0, uDay);
    col = mix(col, cloudCol, c * fade * 0.62);
  }

  col = mix(col, horizon * 0.92, smoothstep(0.06, -0.02, dir.y));
  float n = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.5453);
  col += (n - 0.5) * 0.012;
  col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
  outColor = vec4(col, 1.0);
}
)";
  p.id = link_program(vs, fs);
  if (!p.id) return false;
  p.right = glGetUniformLocation(p.id, "uRight");
  p.up = glGetUniformLocation(p.id, "uUp");
  p.fwd = glGetUniformLocation(p.id, "uFwd");
  p.tanHalf = glGetUniformLocation(p.id, "uTanHalf");
  p.aspect = glGetUniformLocation(p.id, "uAspect");
  p.sunDir = glGetUniformLocation(p.id, "uSunDir");
  p.sunCol = glGetUniformLocation(p.id, "uSunColor");
  p.clouds = glGetUniformLocation(p.id, "uClouds");
  p.time = glGetUniformLocation(p.id, "uTime");
  p.horizon = glGetUniformLocation(p.id, "uHorizon");
  p.zenith = glGetUniformLocation(p.id, "uZenith");
  p.day = glGetUniformLocation(p.id, "uDay");
  return true;
}

inline bool init_water_program(WaterProg& p) {
  const char* vs = R"(#version 300 es
layout(location=0) in vec3 aPos;
uniform mat4 uViewProj;
out vec3 vWorld;
void main() {
  vWorld = aPos;
  gl_Position = uViewProj * vec4(aPos, 1.0);
}
)";
  const char* fs = R"(#version 300 es
precision highp float;
in vec3 vWorld;
uniform vec3 uCamPos;
uniform vec3 uSunDir;
uniform vec3 uSunColor;
uniform vec3 uFogColor;
uniform float uFogNear;
uniform float uFogFar;
uniform float uTime;
uniform vec3 uSkyMul;
out vec4 outColor;

void main() {
  vec3 view = normalize(uCamPos - vWorld);
  float w1 = sin(vWorld.x * 0.09 + uTime * 0.55) * 0.03;
  float w2 = sin(vWorld.z * 0.13 - uTime * 0.42) * 0.025;
  float w3 = sin((vWorld.x + vWorld.z) * 0.21 + uTime * 0.8) * 0.012;
  vec3 n = normalize(vec3(w1 + w3, 1.0, w2 + w3));
  float fres = pow(1.0 - max(dot(n, view), 0.0), 3.0);
  vec3 deep = vec3(0.035, 0.145, 0.19) * uSkyMul;
  vec3 sky = vec3(0.62, 0.70, 0.80) * uSkyMul;
  vec3 col = mix(deep, sky, fres * 0.65);
  vec3 halfV = normalize(uSunDir + view);
  float spec = pow(max(dot(n, halfV), 0.0), 180.0);
  col += uSunColor * spec * 2.2;
  float dist = distance(vWorld, uCamPos);
  float fog = smoothstep(uFogNear, uFogFar, dist);
  col = mix(col, uFogColor, fog);
  col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14);
  outColor = vec4(col, 1.0);
}
)";
  p.id = link_program(vs, fs);
  if (!p.id) return false;
  p.vp = glGetUniformLocation(p.id, "uViewProj");
  p.camPos = glGetUniformLocation(p.id, "uCamPos");
  p.sunDir = glGetUniformLocation(p.id, "uSunDir");
  p.sunCol = glGetUniformLocation(p.id, "uSunColor");
  p.fogCol = glGetUniformLocation(p.id, "uFogColor");
  p.fogNear = glGetUniformLocation(p.id, "uFogNear");
  p.fogFar = glGetUniformLocation(p.id, "uFogFar");
  p.time = glGetUniformLocation(p.id, "uTime");
  p.skyMul = glGetUniformLocation(p.id, "uSkyMul");
  return true;
}

inline bool init_blob_program(BlobProg& p) {
  const char* vs = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=2) in vec2 aUv;
layout(location=5) in mat4 aModel;
uniform mat4 uViewProj;
out vec2 vUv;
void main() {
  vUv = aUv;
  gl_Position = uViewProj * aModel * vec4(aPos, 1.0);
}
)";
  const char* fs = R"(#version 300 es
precision highp float;
precision highp sampler2DArray;
in vec2 vUv;
uniform sampler2DArray uAtlas;
uniform float uStrength;
out vec4 outColor;
void main() {
  float a = texture(uAtlas, vec3(vUv, 12.0)).a * uStrength;
  outColor = vec4(0.0, 0.0, 0.0, a);
}
)";
  p.id = link_program(vs, fs);
  if (!p.id) return false;
  p.vp = glGetUniformLocation(p.id, "uViewProj");
  p.atlas = glGetUniformLocation(p.id, "uAtlas");
  p.strength = glGetUniformLocation(p.id, "uStrength");
  return true;
}
