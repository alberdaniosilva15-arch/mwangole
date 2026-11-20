#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gfx.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

static_assert(sizeof(Vertex) == 40, "layout do Vertex tem de casar com o MWB");

// carrega citytex.bin (MTX1): camadas de textura reais 256x256 RGBA para o atlas
inline std::vector<std::vector<uint8_t>> assets_load_textures(const char* path, int S = 256) {
  std::vector<std::vector<uint8_t>> out;
  std::FILE* f = std::fopen(path, "rb");
  if (!f) return out;
  std::fseek(f, 0, SEEK_END);
  long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (size < 8) { std::fclose(f); return out; }
  std::vector<uint8_t> data((size_t)size);
  size_t got = std::fread(data.data(), 1, data.size(), f);
  std::fclose(f);
  if (got != data.size() || std::memcmp(data.data(), "MTX1", 4) != 0) return out;
  uint32_t count = 0;
  std::memcpy(&count, data.data() + 4, 4);
  size_t o = 8;
  for (uint32_t i = 0; i < count; ++i) {
    if (o + 4 > data.size()) break;
    uint32_t len = 0;
    std::memcpy(&len, data.data() + o, 4);
    o += 4;
    if (o + len > data.size()) break;
    int w = 0, h = 0, comp = 0;
    stbi_uc* px = stbi_load_from_memory(data.data() + o, (int)len, &w, &h, &comp, 4);
    o += len;
    if (px && w == S && h == S) {
      out.emplace_back(px, px + (size_t)S * S * 4);
    }
    if (px) stbi_image_free(px);
  }
  std::fprintf(stderr, "[assets] %u texturas reais carregadas de %s\n", (unsigned)out.size(), path);
  return out;
}

struct AssetModel {
  std::string name;
  std::vector<Vertex> verts;
  std::vector<uint32_t> idx;
  AABB bounds{};
  int tris = 0;
  bool textured = false;
  Mesh mesh;
};

inline std::vector<AssetModel>& assets_all() {
  static std::vector<AssetModel> list;
  return list;
}

inline const AssetModel* asset_get(const char* name) {
  for (const auto& m : assets_all()) {
    if (m.name == name) return &m;
  }
  return nullptr;
}

inline const AssetModel* asset_pick(const char* const* names, int count, uint32_t& seed) {
  if (count <= 0) return nullptr;
  for (int tries = 0; tries < 8; ++tries) {
    int i = (int)((seed = hash32(seed + 0x9e3779b9u)) >> 8) % count;
    if (const AssetModel* m = asset_get(names[i])) return m;
  }
  return nullptr;
}

inline bool assets_load(const char* path) {
  std::FILE* f = std::fopen(path, "rb");
  if (!f) {
    std::fprintf(stderr, "[assets] nao consegui abrir %s\n", path);
    return false;
  }
  std::fseek(f, 0, SEEK_END);
  long size = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  if (size < 8) { std::fclose(f); return false; }
  std::vector<uint8_t> data((size_t)size);
  size_t got = std::fread(data.data(), 1, data.size(), f);
  std::fclose(f);
  if (got != data.size()) return false;
  if (std::memcmp(data.data(), "MWB1", 4) != 0) {
    std::fprintf(stderr, "[assets] formato invalido\n");
    return false;
  }

  uint32_t count = 0;
  std::memcpy(&count, data.data() + 4, 4);
  auto& list = assets_all();
  list.clear();
  list.reserve(count);

  const size_t header = 8;
  for (uint32_t i = 0; i < count; ++i) {
    size_t o = header + (size_t)i * 56;
    uint32_t nameOff, nameLen, vertOff, idxOff, vertCount, idxCount, tris;
    float mn[3], mx[3];
    std::memcpy(&nameOff, data.data() + o, 4);
    std::memcpy(&nameLen, data.data() + o + 4, 4);
    std::memcpy(&vertOff, data.data() + o + 8, 4);
    std::memcpy(&idxOff, data.data() + o + 12, 4);
    std::memcpy(&vertCount, data.data() + o + 16, 4);
    std::memcpy(&idxCount, data.data() + o + 20, 4);
    std::memcpy(&mn[0], data.data() + o + 24, 4);
    std::memcpy(&mn[1], data.data() + o + 28, 4);
    std::memcpy(&mn[2], data.data() + o + 32, 4);
    std::memcpy(&mx[0], data.data() + o + 36, 4);
    std::memcpy(&mx[1], data.data() + o + 40, 4);
    std::memcpy(&mx[2], data.data() + o + 44, 4);
    std::memcpy(&tris, data.data() + o + 48, 4);

    if ((size_t)vertOff + (size_t)vertCount * sizeof(Vertex) > data.size() ||
        (size_t)idxOff + (size_t)idxCount * 4 > data.size() ||
        (size_t)nameOff + nameLen > data.size()) {
      std::fprintf(stderr, "[assets] modelo %u corrompido\n", i);
      list.clear();
      return false;
    }

    AssetModel m;
    m.name.assign((const char*)data.data() + nameOff, nameLen);
    m.verts.resize(vertCount);
    std::memcpy(m.verts.data(), data.data() + vertOff, (size_t)vertCount * sizeof(Vertex));
    m.idx.resize(idxCount);
    std::memcpy(m.idx.data(), data.data() + idxOff, (size_t)idxCount * 4);
    m.bounds.mn = {mn[0], mn[1], mn[2]};
    m.bounds.mx = {mx[0], mx[1], mx[2]};
    m.tris = (int)tris;
    for (const auto& v : m.verts) {
      if (v.layer >= 15.0f) { m.textured = true; break; }
    }
    m.mesh = make_mesh(m.verts, m.idx);
    list.push_back(std::move(m));
  }
  std::fprintf(stderr, "[assets] %u modelos carregados de %s\n", count, path);
  return true;
}
