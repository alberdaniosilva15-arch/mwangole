#pragma once
#include <cmath>
#include <cstdint>
#include <vector>

#include "car.h"
#include "gfx.h"
#include "math.h"
#include "player.h"
#include "world.h"

// Road nodes sit at the centre of every intersection:
// x = NODE + CELL*i, z = NODE + CELL*j. Traffic keeps to the right lane.
constexpr float NODE = 94.0f;
constexpr float LANE = 3.45f;
constexpr float TRAFFIC_LIM = 1600.0f;

inline V3 lane_dir(int d) {
  switch (d) {
    case 0: return {1.0f, 0.0f, 0.0f};
    case 1: return {0.0f, 0.0f, 1.0f};
    case 2: return {-1.0f, 0.0f, 0.0f};
    default: return {0.0f, 0.0f, -1.0f};
  }
}

inline V3 lane_right(int d) {
  switch (d) {
    case 0: return {0.0f, 0.0f, 1.0f};
    case 1: return {-1.0f, 0.0f, 0.0f};
    case 2: return {0.0f, 0.0f, -1.0f};
    default: return {1.0f, 0.0f, 0.0f};
  }
}

inline bool node_ok(int ni, int nj) {
  float x = NODE + CELL * (float)ni;
  float z = NODE + CELL * (float)nj;
  return x > -TRAFFIC_LIM && x < TRAFFIC_LIM && z > -800.0f && z < TRAFFIC_LIM;
}

struct TrafficCar {
  V3 pos{0.0f, 0.0f, 0.0f};
  float yaw = 0.0f;
  float speed = 0.0f;
  float steerVis = 0.0f;
  float wheelSpin = 0.0f;
  int ni = 0, nj = 0, dir = 3;
  float cruise = 12.0f;
  uint32_t color = 0xffffffffu;
  uint32_t seed = 1u;
  float stuck = 0.0f;
};

inline V3 car_target(const TrafficCar& t) {
  V3 n = {NODE + CELL * (float)t.ni, 0.0f, NODE + CELL * (float)t.nj};
  return n + lane_right(t.dir) * LANE;
}

inline void respawn_traffic(TrafficCar& t, const V3& around, uint32_t& seed, bool anywhere) {
  int ci = (int)std::floor((around.x - NODE) / CELL + 0.5f);
  int cj = (int)std::floor((around.z - NODE) / CELL + 0.5f);
  for (int tries = 0; tries < 24; ++tries) {
    int ni = ci + (anywhere ? irand(seed, -4, 4) : irand(seed, -3, 3));
    int nj = cj + (anywhere ? irand(seed, -4, 4) : irand(seed, -3, 3));
    if (!node_ok(ni, nj)) continue;
    int d = irand(seed, 0, 3);
    V3 n = {NODE + CELL * (float)ni, 0.0f, NODE + CELL * (float)nj};
    V3 p = n + (lane_dir(d) * (10.0f + frand(seed) * 80.0f)) + lane_right(d) * LANE;
    if (!anywhere && length(p - around) < 14.0f) continue;
    t.pos = p;
    t.dir = d;
    t.ni = ni + (int)lane_dir(d).x;
    t.nj = nj + (int)lane_dir(d).z;
    if (!node_ok(t.ni, t.nj)) continue;
    t.yaw = std::atan2(lane_dir(d).x, lane_dir(d).z);
    t.speed = 6.0f + frand(seed) * 5.0f;
    t.cruise = 10.5f + frand(seed) * 5.5f;
    t.stuck = 0.0f;
    return;
  }
}

inline void init_traffic(std::vector<TrafficCar>& cars, int count, const V3& around,
                         uint32_t& seed) {
  const uint32_t palette[8] = {
      tint3(0.86f, 0.86f, 0.84f), tint3(0.62f, 0.64f, 0.66f), tint3(0.72f, 0.20f, 0.17f),
      tint3(0.20f, 0.34f, 0.58f), tint3(0.24f, 0.48f, 0.34f), tint3(0.82f, 0.66f, 0.30f),
      tint3(0.30f, 0.30f, 0.32f), tint3(0.80f, 0.78f, 0.70f)};
  cars.resize(count);
  for (auto& c : cars) {
    c.seed = hash32(seed + (uint32_t)(&c - cars.data()) * 7919u + 17u);
    c.color = palette[(size_t)(&c - cars.data()) % 8];
    respawn_traffic(c, around, c.seed, true);
  }
}

inline void update_traffic(std::vector<TrafficCar>& cars, const V3& player, float dt,
                           uint32_t& seed) {
  for (size_t i = 0; i < cars.size(); ++i) {
    TrafficCar& t = cars[i];
    if (length(t.pos - player) > 480.0f) {
      respawn_traffic(t, player, seed, false);
      continue;
    }

    V3 T = car_target(t);
    V3 to = T - t.pos;
    float dist = length(to);
    if (dist < 3.2f) {
      // arrived at the node: pick the next branch, straight most of the time
      float r = frand(t.seed);
      int nd = t.dir;
      if (r > 0.52f) nd = (t.dir + (r > 0.78f ? 1 : 3)) % 4;
      for (int tries = 0; tries < 4; ++tries) {
        int ni = t.ni + (int)lane_dir(nd).x;
        int nj = t.nj + (int)lane_dir(nd).z;
        if (node_ok(ni, nj)) {
          t.ni = ni;
          t.nj = nj;
          t.dir = nd;
          break;
        }
        nd = (nd + 1) % 4;
      }
      T = car_target(t);
      to = T - t.pos;
      dist = length(to);
    }

    float desiredYaw = std::atan2(to.x, to.z);
    float da = desiredYaw - t.yaw;
    while (da > 3.14159265f) da -= 6.2831853f;
    while (da < -3.14159265f) da += 6.2831853f;

    float target = t.cruise;
    if (dist < 18.0f) target = std::min(target, 8.5f);
    if (dist < 9.0f) target = std::min(target, 6.0f);
    float ad = std::fabs(da);
    if (ad > 0.35f) target = std::min(target, 6.5f);
    if (ad > 0.9f) target = std::min(target, 4.5f);

    // give way to the player and to the car ahead in the same lane
    V3 f = car_forward(t.yaw);
    V3 right = v3(f.z, 0.0f, -f.x);
    auto slows_for = [&](const V3& p, float extra) {
      V3 rel = p - t.pos;
      float fd = rel.x * f.x + rel.z * f.z;
      float ld = rel.x * right.x + rel.z * right.z;
      if (fd > 0.3f && fd < 11.0f + extra && std::fabs(ld) < 2.4f) {
        target = std::min(target, std::max(0.0f, (fd - 4.2f) * 1.9f));
      }
    };
    slows_for(player, 0.0f);
    for (size_t j = 0; j < cars.size(); ++j) {
      if (j == i) continue;
      const TrafficCar& o = cars[j];
      V3 of = car_forward(o.yaw);
      float align = of.x * f.x + of.z * f.z;
      if (align < 0.4f) continue;
      slows_for(o.pos, 2.5f);
    }

    float dv = target - t.speed;
    t.speed += clampf(dv, -15.0f * dt, 7.5f * dt);
    if (t.speed < 0.02f && target < 0.02f) t.speed = 0.0f;

    float turn = clampf(da, -1.9f * dt, 1.9f * dt);
    if (t.speed > 0.4f) t.yaw += turn;
    t.steerVis += (clampf(-da * 1.3f, -1.0f, 1.0f) - t.steerVis) * std::min(1.0f, dt * 8.0f);

    V3 ff = car_forward(t.yaw);
    t.pos = t.pos + ff * (t.speed * dt);
    t.wheelSpin += t.speed / 0.36f * dt;
    if (t.speed < 0.3f && target < 0.3f) t.stuck += dt;
    else t.stuck = 0.0f;
    if (t.stuck > 9.0f) {
      // wedged for too long (usually a parked player): back out along the lane
      t.pos = t.pos - ff * 2.5f;
      t.stuck = 0.0f;
      t.seed = hash32(t.seed + 1u);
    }
  }
}

inline bool resolve_player_traffic(Car& car, std::vector<TrafficCar>& cars) {
  bool any = false;
  V3 f = car_forward(car.yaw);
  for (auto& t : cars) {
    V3 tf = car_forward(t.yaw);
    for (int i = -1; i <= 1; i += 2) {
      for (int j = -1; j <= 1; j += 2) {
        V3 pa = car.pos + f * (1.15f * (float)i);
        V3 pb = t.pos + tf * (1.15f * (float)j);
        V3 d = pa - pb;
        float d2 = d.x * d.x + d.z * d.z;
        if (d2 > 3.8f || d2 < 1e-5f) continue;
        float dd = std::sqrt(d2);
        float push = (1.95f - dd) * 0.5f;
        V3 n = d * (1.0f / dd);
        car.pos = car.pos + n * push;
        t.pos = t.pos - n * push;
        car.speed *= 0.55f;
        t.speed = 0.0f;
        t.stuck = 0.0f;
        any = true;
      }
    }
  }
  return any;
}

inline void resolve_walker_traffic(Walker& w, std::vector<TrafficCar>& cars) {
  for (auto& t : cars) {
    V3 tf = car_forward(t.yaw);
    for (int j = -1; j <= 1; j += 2) {
      V3 pb = t.pos + tf * (1.15f * (float)j);
      V3 d = w.pos - pb;
      float d2 = d.x * d.x + d.z * d.z;
      if (d2 > 1.87f || d2 < 1e-5f) continue;
      float dd = std::sqrt(d2);
      w.pos = w.pos + d * ((1.37f - dd) / dd);
      w.speed *= 0.4f;
    }
  }
}

// ------------------------------------------------------------ police

struct PoliceCar {
  V3 pos{0.0f, 0.0f, 0.0f};
  float yaw = 0.0f;
  float speed = 0.0f;
  float wheelSpin = 0.0f;
  float steerVis = 0.0f;
  float stuck = 0.0f;
  float lastHit = -10.0f;
};

inline void spawn_cop(PoliceCar& c, const V3& player, uint32_t& seed) {
  int ci = (int)std::floor((player.x - NODE) / CELL + 0.5f);
  int cj = (int)std::floor((player.z - NODE) / CELL + 0.5f);
  for (int tries = 0; tries < 30; ++tries) {
    int ni = ci + irand(seed, -3, 3);
    int nj = cj + irand(seed, -3, 3);
    if (!node_ok(ni, nj)) continue;
    int d = irand(seed, 0, 3);
    V3 n = {NODE + CELL * (float)ni, 0.0f, NODE + CELL * (float)nj};
    V3 p = n + lane_dir(d) * (20.0f + frand(seed) * 60.0f) + lane_right(d) * LANE;
    float dist = length(p - player);
    if (dist < 55.0f || dist > 200.0f) continue;
    c.pos = p;
    c.yaw = std::atan2(player.x - p.x, player.z - p.z);
    c.speed = 9.0f;
    c.stuck = 0.0f;
    return;
  }
}

inline void update_cop(PoliceCar& c, const V3& player, float dt, uint32_t& seed,
                       const std::vector<AABB>& cols) {
  float dist = length(player - c.pos);
  V3 aim = player;
  if (dist > 40.0f) {
    int ni = (int)std::floor((player.x - NODE) / CELL + 0.5f);
    int nj = (int)std::floor((player.z - NODE) / CELL + 0.5f);
    aim = {NODE + CELL * (float)ni, 0.0f, NODE + CELL * (float)nj};
  }
  V3 to = aim - c.pos;
  float da = std::atan2(to.x, to.z) - c.yaw;
  while (da > 3.14159265f) da -= 6.2831853f;
  while (da < -3.14159265f) da += 6.2831853f;
  if (c.speed > 0.4f) c.yaw += clampf(da, -2.6f * dt, 2.6f * dt);
  c.steerVis += (clampf(-da * 1.4f, -1.0f, 1.0f) - c.steerVis) * std::min(1.0f, dt * 8.0f);

  float target = dist < 14.0f ? 11.0f : 18.5f;
  if (std::fabs(da) > 0.55f) target = std::min(target, 8.5f);
  c.speed += clampf(target - c.speed, -20.0f * dt, 11.0f * dt);

  V3 f = car_forward(c.yaw);
  c.pos = c.pos + f * (c.speed * dt);
  c.wheelSpin += c.speed / 0.36f * dt;

  bool hit = false;
  V3 pf = c.pos + f * 1.15f;
  V3 pb = c.pos - f * 1.15f;
  for (const auto& b : cols) {
    resolve_circle(pf, 0.95f, b, hit);
    resolve_circle(pb, 0.95f, b, hit);
  }
  c.pos = (pf + pb) * 0.5f;
  if (hit) {
    c.speed *= 0.55f;
    c.stuck += dt;
  } else {
    c.stuck = std::max(0.0f, c.stuck - dt * 1.5f);
  }
  if (c.stuck > 2.2f) {
    spawn_cop(c, player, seed);
    c.stuck = 0.0f;
  }
}

inline bool resolve_player_cop(Car& car, PoliceCar& c) {  bool hit = false;
  V3 f = car_forward(car.yaw);
  V3 cf = car_forward(c.yaw);
  for (int i = -1; i <= 1; i += 2) {
    for (int j = -1; j <= 1; j += 2) {
      V3 pa = car.pos + f * (1.15f * (float)i);
      V3 pb = c.pos + cf * (1.15f * (float)j);
      V3 d = pa - pb;
      float d2 = d.x * d.x + d.z * d.z;
      if (d2 > 3.8f || d2 < 1e-5f) continue;
      float dd = std::sqrt(d2);
      float push = (1.95f - dd) * 0.5f;
      V3 n = d * (1.0f / dd);
      car.pos = car.pos + n * push;
      c.pos = c.pos - n * push;
      car.speed *= 0.55f;
      c.speed *= 0.25f;
      hit = true;
    }
  }
  return hit;
}

inline void resolve_walker_cop(Walker& w, const PoliceCar& c) {
  V3 cf = car_forward(c.yaw);
  for (int j = -1; j <= 1; j += 2) {
    V3 pb = c.pos + cf * (1.15f * (float)j);
    V3 d = w.pos - pb;
    float d2 = d.x * d.x + d.z * d.z;
    if (d2 > 1.87f || d2 < 1e-5f) continue;
    float dd = std::sqrt(d2);
    w.pos = w.pos + d * ((1.37f - dd) / dd);
    w.speed *= 0.4f;
  }
}

// ------------------------------------------------------------ street people

struct StreetPed {
  Walker w;
  int bi = 0, bj = 0, variant = 0;
  float t = 0.0f;
  int dirSign = 1;
  float baseSpeed = 1.2f;
};

inline float ped_ring_len() { return 4.0f * (BLOCK - 4.4f); }

inline void ped_ring(int bi, int bj, float t, V3& out, float& yaw) {
  float x0 = NODE + CELL * (float)bi + 12.2f;
  float z0 = NODE + CELL * (float)bj + 12.2f;
  float x1 = x0 + (BLOCK - 4.4f);
  float z1 = z0 + (BLOCK - 4.4f);
  float w = x1 - x0;
  float P = 4.0f * w;
  t = std::fmod(t, P);
  if (t < 0.0f) t += P;
  if (t < w) {
    out = {x0 + t, 0.16f, z0};
    yaw = 1.5707963f;
  } else if (t < 2.0f * w) {
    out = {x1, 0.16f, z0 + (t - w)};
    yaw = 0.0f;
  } else if (t < 3.0f * w) {
    out = {x1 - (t - 2.0f * w), 0.16f, z1};
    yaw = -1.5707963f;
  } else {
    out = {x0, 0.16f, z1 - (t - 3.0f * w)};
    yaw = 3.1415926f;
  }
}

inline void respawn_ped(StreetPed& p, const V3& around, uint32_t& seed, bool nearby) {
  int ci = (int)std::floor((around.x - NODE - 10.0f) / CELL);
  int cj = (int)std::floor((around.z - NODE - 10.0f) / CELL);
  for (int tries = 0; tries < 20; ++tries) {
    int bi = ci + irand(seed, nearby ? -1 : -2, nearby ? 1 : 2);
    int bj = cj + irand(seed, nearby ? -1 : -2, nearby ? 1 : 2);
    if (!node_ok(bi, bj)) continue;
    p.bi = bi;
    p.bj = bj;
    p.t = frand(seed) * ped_ring_len();
    p.dirSign = frand(seed) < 0.5f ? 1 : -1;
    p.variant = irand(seed, 0, 3);
    p.baseSpeed = 1.0f + frand(seed) * 0.75f;
    V3 pos;
    float yaw;
    ped_ring(p.bi, p.bj, p.t, pos, yaw);
    if (!nearby && length(pos - around) < 20.0f) continue;
    p.w.pos = pos;
    p.w.yaw = p.dirSign > 0 ? yaw : yaw + 3.1415926f;
    p.w.speed = p.baseSpeed;
    p.w.anim = frand(seed) * 6.28f;
    return;
  }
}

inline void init_peds(std::vector<StreetPed>& peds, int count, const V3& around,
                      uint32_t& seed) {
  peds.resize(count);
  for (auto& p : peds) respawn_ped(p, around, seed, false);
}

inline void place_ped(StreetPed& p, int bi, int bj, float t, int dirSign, int variant) {
  p.bi = bi;
  p.bj = bj;
  p.t = t;
  p.dirSign = dirSign;
  p.variant = variant;
  V3 pos;
  float yaw;
  ped_ring(bi, bj, t, pos, yaw);
  p.w.pos = pos;
  p.w.yaw = dirSign > 0 ? yaw : yaw + 3.1415926f;
  p.w.speed = p.baseSpeed;
  p.w.anim = 0.0f;
}

inline void update_peds(std::vector<StreetPed>& peds, const V3& around, float dt,
                        uint32_t& seed) {
  for (auto& p : peds) {
    if (length(p.w.pos - around) > 300.0f) {
      respawn_ped(p, around, seed, false);
      continue;
    }
    p.t += p.w.speed * (float)p.dirSign * dt;
    V3 pos;
    float yaw;
    ped_ring(p.bi, p.bj, p.t, pos, yaw);
    p.w.pos = pos;
    p.w.yaw = p.dirSign > 0 ? yaw : yaw + 3.1415926f;
    p.w.speed = p.baseSpeed;
    p.w.anim += p.w.speed * dt * 2.1f;
  }
}
