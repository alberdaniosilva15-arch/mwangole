#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "math.h"
#include "gfx.h"
#include "world.h"
#include "car.h"
#include "player.h"
#include "street.h"

static int g_w = 1280;
static int g_h = 720;
static double g_prev_ms = 0.0;
static float g_time = 0.0f;

// ciclo dia/noite
static const float DAY_SECONDS = 600.0f;
static float g_tod = 0.36f;  // 0..1 (0 = meia-noite, 0.5 = meio-dia)
static V3 g_sun_dir = {0.66f, 0.55f, -0.51f};
static V3 g_sun_col = {1.00f, 0.92f, 0.79f};
static V3 g_amb_sky = {0.32f, 0.36f, 0.44f};
static V3 g_amb_gnd = {0.17f, 0.16f, 0.15f};
static V3 g_fog_col = {0.72f, 0.78f, 0.85f};
static V3 g_sky_hor = {0.78f, 0.85f, 0.93f};
static V3 g_sky_zen = {0.20f, 0.42f, 0.74f};
static V3 g_sky_mul = {1.0f, 1.0f, 1.0f};
static float g_day = 1.0f;
static float g_night = 0.0f;
static int g_want_time_jump = 0;

static GLuint g_atlas = 0;
static GLuint g_clouds = 0;
static GLuint g_empty_vao = 0;
static GLuint g_blob_buf = 0;

static WorldProg g_world;
static WorldProg g_pedProg;
static CarProg g_carProg;
static SkyProg g_sky;
static WaterProg g_water;
static BlobProg g_blob;

static City g_city;
static Mesh g_palm_src;
static Mesh g_lamp_src;
static Mesh g_tree_mesh;
static Mesh g_lamp_mesh;
static Mesh g_blob_mesh;
static Mesh g_blob_inst;
static CarMeshes g_car_base;
static CarMeshes g_car_inst;
static CarBuffers g_car_bufs;

static PlayerMeshes g_player_mesh;

static Car g_car;
static Walker g_player;
static bool g_in_car = true;

static std::vector<TrafficCar> g_traffic;
static std::vector<StreetPed> g_peds;
static std::vector<PoliceCar> g_cops;
static PlayerMeshes g_ped_looks[4];
static uint32_t g_street_seed = 0x51ed7a11u;
static Mesh g_cop_light_mesh;
static float g_heat = 0.0f;
static float g_offense_age = 100.0f;
static float g_offense_cd = 0.0f;
static float g_cop_hit_cd = 0.0f;
static float g_cop_spawn_cd = 0.0f;
static float g_impact_cd = 0.0f;

static Mesh make_cop_light_mesh() {
  Verts v;
  cbox(v, -0.28f, -0.11f, 0.0f, 0.11f, 0.0f, 0.10f, L_WHITE, tint3(0.86f, 0.10f, 0.07f));
  cbox(v, 0.0f, -0.11f, 0.28f, 0.11f, 0.0f, 0.10f, L_WHITE, tint3(0.10f, 0.24f, 0.92f));
  return mesh_upload(v);
}

static float g_cam_yaw = 3.14159265f;
static float g_orbit_yaw = 0.0f;
static float g_orbit_pitch = 0.0f;
static float g_last_drag = -10.0f;
static int g_cam_mode = 0;

static float g_in_f = 0.0f, g_in_b = 0.0f, g_in_l = 0.0f, g_in_r = 0.0f, g_in_hb = 0.0f;
static float g_axis_x = 0.0f, g_axis_y = 0.0f;
static float g_in_run = 0.0f;

static int g_want_exit = 0;
static int g_want_cam = 0;
static int g_debug_top = 0;
static int g_want_reset = 0;

static bool g_ready = false;
static int g_frame_count = 0;

static float g_health = 100.0f;
static float g_cash = 7500.0f;
static int g_wanted = 0;

static std::vector<AABB> g_cols;
static std::vector<float> g_map_data;

struct ParkedCar {
  V3 pos;
  float yaw;
  uint32_t color;
};
static std::vector<ParkedCar> g_parked;

enum { JOB_IDLE = 0, JOB_PICK = 1, JOB_DROP = 2 };

struct JobState {
  int stage = JOB_IDLE;
  V3 target = {0.0f, 0.0f, 0.0f};
  float time_left = 0.0f;
  float payout = 0.0f;
  float cooldown = 0.0f;
};
static JobState g_job;
static int g_want_job = 0;
static uint32_t g_job_seed = 0x9e3779b9u;
static Mesh g_beacon_pick;
static Mesh g_beacon_drop;
static char g_job_msg[72] = "";
static float g_job_msg_t = 0.0f;

static int g_poi_near = -1;
static float g_poi_dist = 1e9f;
static bool g_poi_bought[16] = {false};
static Mesh g_signal_masts;
static Mesh g_signal_lamps[4];

static float g_render_scale = 1.0f;
static double g_quality_time = 0.0;
static int g_quality_frames = 0;
static double g_hud_timer = 0.0;

static const V3 SPAWN_POS = {97.4f, 0.0f, 248.0f};
static const float SPAWN_YAW = 0.0f;

// ------------------------------------------------------------------ bridge

EM_JS(void, mw_boot_msg, (const char* s), {
  const el = document.getElementById('boot-msg');
  if (el) el.textContent = UTF8ToString(s);
});

EM_JS(void, mw_boot_ready, (), {
  document.body.dataset.mw = 'ready';
  window.__mwReady = true;
});

EM_JS(void, mw_fail_js, (const char* msg), {
  document.body.dataset.mw = 'error';
  const el = document.getElementById('boot-msg');
  if (el) {
    el.textContent = UTF8ToString(msg);
    el.classList.add('err');
  }
  const s = document.getElementById('boot-sub');
  if (s) s.textContent = 'O navegador nao conseguiu iniciar o WebGL2.';
});

EM_JS(void, mw_dbg, (float thr, float brk, float steer, float speed, float ax, float ay, float x,
                     float z, float yaw), {
  window.__dbg = { thr, brk, steer, speed, ax, ay, x, z, yaw };
});

EM_JS(void, mw_fps_js, (double fps), {
  const el = document.getElementById('fps');
  if (el) el.textContent = fps.toFixed(0) + ' fps';
  window.__mwFps = fps;
});

EM_JS(void, mw_street_js, (int cars, int peds, float nearCar, float nearPed, float heat,
                           int cops, float nearCop, float health), {
  window.__mwStreet = { cars, peds, nearCar, nearPed, heat, cops, nearCop, health };
});

EM_JS(void, mw_assets_js, (int models, int tris, int verts), {
  window.__mwAssets = { models, tris, verts };
});

EM_JS(void, mw_siren_js, (int on), {
  if (window.__mwSiren) window.__mwSiren(!!on);
});

EM_JS(void, mw_impact_js, (float strength), {
  if (window.__mwImpact) window.__mwImpact(strength);
});

EM_JS(void, mw_traffic_js, (int i, float x, float z, float yaw, float speed), {
  if (!window.__mwTraffic) window.__mwTraffic = [];
  window.__mwTraffic[i] = { x, z, yaw, speed };
});

EM_JS(void, mw_map_js, (int ptr, int count), {
  try {
    window.__mwMap = HEAPF32.slice(ptr >> 2, (ptr >> 2) + count * 4);
  } catch (e) {
    window.__mwMap = new Float32Array(0);
  }
});

EM_JS(void, mw_pois_js, (int ptr, int count), {
  try {
    window.__mwPois = HEAPF32.slice(ptr >> 2, (ptr >> 2) + count * 3);
  } catch (e) {
    window.__mwPois = new Float32Array(0);
  }
});

EM_JS(void, mw_set_scale_js, (float s), {
  const c = document.getElementById('game');
  if (!c) return;
  const dpr = Math.min(2, window.devicePixelRatio || 1);
  const w = Math.max(320, Math.round((c.clientWidth || 1280) * s * dpr));
  const h = Math.max(180, Math.round((c.clientHeight || 720) * s * dpr));
  if (c.width !== w || c.height !== h) {
    c.width = w;
    c.height = h;
    const M = (typeof Module !== 'undefined') ? Module : window.__mwModule;
    if (M && M._mw_resize) M._mw_resize(w, h);
  }
});

EM_JS(void, mw_job_js, (int stage, const char* title, const char* sub), {
  const card = document.getElementById('job');
  if (!card) return;
  const t = UTF8ToString(title), s = UTF8ToString(sub);
  card.classList.toggle('on', !!t);
  card.classList.toggle('live', stage > 0);
  const tt = card.querySelector('.job-t');
  const ss = card.querySelector('.job-s');
  if (tt && tt.textContent !== t) tt.textContent = t;
  if (ss && ss.textContent !== s) ss.textContent = s;
});

EM_JS(void, mw_clock_js, (int h, int m, int night), {
  const el = document.getElementById('clock');
  if (!el) return;
  const txt = (h < 10 ? '0' + h : String(h)) + ':' + (m < 10 ? '0' + m : String(m));
  if (el.textContent !== txt) el.textContent = txt;
  el.classList.toggle('night', !!night);
});

EM_JS(void, mw_hud, (float kmh, float health, float cash, int wanted, int in_car_arg,
                     float x, float z, float yaw, const char* prompt,
                     float jx, float jz, int jstage), {
  const $ = (id) => document.getElementById(id);
  const sp = $('kmh');
  if (sp) {
    const v = Math.round(kmh);
    if (sp.textContent !== String(v)) sp.textContent = v;
  }
  const st = $('state');
  if (st) st.textContent = in_car_arg ? 'A conduzir' : 'A pe';
  window.__mwInCar = !!in_car_arg;
  const gear = $('gear');
  if (gear) gear.textContent = in_car_arg ? 'D' : '—';
  const dz = $('district');
  if (dz) {
    let name = 'Musseque';
    if (z <= -860) name = 'Ilha do Cabo';
    else if (z < -660) name = 'Marginal';
    else if (x > -260 && x < 700 && z > 40 && z < 760) name = 'Baixa';
    else if (x > 700) name = 'Cidade Alta';
    if (dz.textContent !== name) dz.textContent = name;
  }
  if (window.__mwHudAnim) window.__mwHudAnim(kmh, health, cash, wanted, !!in_car_arg, UTF8ToString(prompt));

  const cv = $('minimap');
  if (cv && cv.getContext) {
    const ctx = cv.getContext('2d');
    const W = cv.width, H = cv.height;
    const scale = 0.29;
    const cx = W / 2, cy = H / 2;
    const POI_COL = ['#59e873', '#ffc33a', '#ff5c42', '#61a8ff', '#c77aff', '#38dcff', '#fff05c'];
    ctx.clearRect(0, 0, W, H);
    ctx.fillStyle = '#161c1e';
    ctx.fillRect(0, 0, W, H);

    if (z < -636) {
      ctx.fillStyle = '#16394a';
      ctx.fillRect(0, 0, W, H);
    }
    // bay water
    const waterTop = cy + (-832 - z) * scale;
    if (waterTop > 0) {
      ctx.fillStyle = '#17475c';
      ctx.fillRect(0, 0, W, Math.min(H, waterTop));
    }
    // roads
    ctx.save();
    ctx.beginPath();
    ctx.rect(0, Math.max(0, waterTop), W, H - Math.max(0, waterTop));
    ctx.clip();
    ctx.fillStyle = '#2c3436';
    const cell = 104, road = 20;
    const k0 = Math.floor((x - 260) / cell), k1 = Math.ceil((x + 260) / cell);
    for (let k = k0; k <= k1; ++k) {
      const wx = k * cell + 84;
      const sx = cx + (wx - x) * scale;
      ctx.fillRect(sx, 0, road * scale, H);
    }
    const j0 = Math.floor((z - 260) / cell), j1 = Math.ceil((z + 260) / cell);
    for (let j = j0; j <= j1; ++j) {
      const wz = j * cell + 84;
      const sy = cy + (wz - z) * scale;
      if (sy > 0 && sy < H) ctx.fillRect(0, sy, W, road * scale);
    }
    // buildings
    const map = window.__mwMap;
    if (map) {
      ctx.fillStyle = '#4a5456';
      for (let i = 0; i < map.length; i += 4) {
        const x0 = map[i], z0 = map[i + 1], x1 = map[i + 2], z1 = map[i + 3];
        if (x1 < x - 300 || x0 > x + 300 || z1 < z - 300 || z0 > z + 300) continue;
        ctx.fillRect(cx + (x0 - x) * scale, cy + (z0 - z) * scale,
                     Math.max(1, (x1 - x0) * scale), Math.max(1, (z1 - z0) * scale));
      }
    }
    ctx.restore();
    // ilha
    const iy0 = cy + (-1097 - z) * scale, iy1 = cy + (-1022 - z) * scale;
    if (iy1 > 0 && iy0 < H) {
      ctx.fillStyle = '#7d6f4e';
      ctx.fillRect(cx + (-820 - x) * scale, iy0, 1580 * scale, iy1 - iy0);
    }
    // ponte para a ilha
    const by0 = cy + (-1022 - z) * scale, by1 = cy + (-843 - z) * scale;
    if (by1 > 0 && by0 < H) {
      ctx.fillStyle = '#4c5354';
      ctx.fillRect(cx + (0 - x) * scale - 6 * scale, by0, 12 * scale, by1 - by0);
    }
    // POIs
    const pois = window.__mwPois;
    if (pois) {
      for (let i = 0; i < pois.length; i += 3) {
        const which = pois[i + 2] | 0;
        const sx = cx + (pois[i] - x) * scale, sy = cy + (pois[i + 1] - z) * scale;
        if (sx < 5 || sx > W - 5 || sy < 5 || sy > H - 5) continue;
        ctx.fillStyle = POI_COL[which] || '#ffffff';
        ctx.strokeStyle = 'rgba(0,0,0,0.65)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        if (which === 5) {
          ctx.moveTo(sx, sy - 4); ctx.lineTo(sx + 4, sy); ctx.lineTo(sx, sy + 4);
          ctx.lineTo(sx - 4, sy);
        } else if (which === 4) {
          ctx.moveTo(sx, sy - 4); ctx.lineTo(sx + 4, sy + 3); ctx.lineTo(sx - 4, sy + 3);
        } else if (which === 0 || which === 6) {
          ctx.arc(sx, sy, 3.4, 0, Math.PI * 2);
        } else {
          ctx.rect(sx - 3, sy - 3, 6, 6);
        }
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
      }
    }
    // player marker
    ctx.save();
    ctx.translate(cx, cy);
    if (in_car_arg) {
      ctx.rotate(Math.PI - yaw);
      ctx.fillStyle = '#ff8a1e';
      ctx.beginPath();
      ctx.moveTo(0, -7);
      ctx.lineTo(5, 5);
      ctx.lineTo(0, 2.5);
      ctx.lineTo(-5, 5);
      ctx.closePath();
      ctx.fill();
      ctx.strokeStyle = 'rgba(0,0,0,0.55)';
      ctx.lineWidth = 1;
      ctx.stroke();
    } else {
      ctx.fillStyle = '#ffd36b';
      ctx.beginPath();
      ctx.arc(0, 0, 3.2, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.restore();
    // mission marker
    window.__mwJob = jstage > 0 ? { x: jx, z: jz, stage: jstage } : null;
    if (jstage > 0) {
      let mx = cx + (jx - x) * scale, my = cy + (jz - z) * scale;
      const bx = mx - cx, by = my - cy;
      const rr = Math.hypot(bx, by), lim = Math.min(W, H) * 0.45;
      if (rr > lim && rr > 0.001) { mx = cx + bx / rr * lim; my = cy + by / rr * lim; }
      ctx.save();
      ctx.translate(mx, my);
      ctx.rotate(Math.PI / 4);
      ctx.fillStyle = jstage === 1 ? '#39d8ff' : '#ffc44d';
      ctx.fillRect(-3.6, -3.6, 7.2, 7.2);
      ctx.strokeStyle = 'rgba(0,0,0,0.65)';
      ctx.lineWidth = 1;
      ctx.strokeRect(-3.6, -3.6, 7.2, 7.2);
      ctx.restore();
    }
    // border ring
    ctx.strokeStyle = 'rgba(255,255,255,0.13)';
    ctx.lineWidth = 2;
    ctx.strokeRect(1, 1, W - 2, H - 2);
  }

  if (window.__mwAudio) window.__mwAudio(kmh, in_car_arg);
});

// ------------------------------------------------------------------ input

extern "C" {

EMSCRIPTEN_KEEPALIVE void mw_resize(int w, int h) {
  if (w < 16 || h < 16) return;
  g_w = w;
  g_h = h;
  glViewport(0, 0, w, h);
}

EMSCRIPTEN_KEEPALIVE void mw_action(int code, float value) {
  switch (code) {
    case 0: g_in_f = value; break;
    case 1: g_in_b = value; break;
    case 2: g_in_l = value; break;
    case 3: g_in_r = value; break;
    case 4: g_in_hb = value; break;
    case 10: if (value > 0.5f) g_want_exit = 1; break;
    case 11: if (value > 0.5f) g_want_cam = 1; break;
    case 12: if (value > 0.5f) g_want_reset = 1; break;
    case 13: g_in_run = value; break;
    case 14: if (value > 0.5f) g_want_time_jump = 1; break;
    case 15: if (value > 0.5f) g_want_job = 1; break;
    default: break;
  }
}

EMSCRIPTEN_KEEPALIVE void mw_axis(float x, float y) {
  g_axis_x = clampf(x, -1.0f, 1.0f);
  g_axis_y = clampf(y, -1.0f, 1.0f);
}

EMSCRIPTEN_KEEPALIVE void mw_look(float dx, float dy) {
  g_orbit_yaw -= dx * 0.006f;
  g_orbit_pitch = clampf(g_orbit_pitch + dy * 0.004f, -0.14f, 0.42f);
  g_last_drag = g_time;
}

EMSCRIPTEN_KEEPALIVE void mw_debug_top(int on) { g_debug_top = on; }

// Devolve "nome|escala|camada|tris|centroX|centroZ|altura|meiaLarguraX|meiaLarguraZ"
// do lote que contem o ponto. Camada -2 = edificio procedural; -1 = cor de vertice.
EMSCRIPTEN_KEEPALIVE const char* mw_debug_lot(float x, float z) {
  static std::string out;
  const std::vector<LotRecord>& lots = lot_records();
  for (const auto& r : lots) {
    if (x < r.cx - r.hx || x > r.cx + r.hx) continue;
    if (z < r.cz - r.hz || z > r.cz + r.hz) continue;
    char buf[320];
    std::snprintf(buf, sizeof(buf), "%s|%.3f|%d|%d|%.1f|%.1f|%.1f|%.1f|%.1f", r.name.c_str(),
                  r.scale, r.layer, r.tris, r.cx, r.cz, r.height, r.hx, r.hz);
    out.assign(buf);
    return out.c_str();
  }
  out.assign("none|0|0|0|0|0|0|0|0");
  return out.c_str();
}

EMSCRIPTEN_KEEPALIVE int mw_debug_lot_count() { return (int)lot_records().size(); }

// Uma linha por registo: indice, nome, centro, meia-largura, escala, camada, tris.
EMSCRIPTEN_KEEPALIVE const char* mw_debug_lot_dump(int i) {
  static std::string out;
  const std::vector<LotRecord>& lots = lot_records();
  if (i < 0 || i >= (int)lots.size()) { out.assign("none|0|0|0|0|0|0|0|0"); return out.c_str(); }
  const LotRecord& r = lots[i];
  char buf[320];
  std::snprintf(buf, sizeof(buf), "%s|%.3f|%d|%d|%.1f|%.1f|%.1f|%.1f|%.1f", r.name.c_str(),
                r.scale, r.layer, r.tris, r.cx, r.cz, r.height, r.hx, r.hz);
  out.assign(buf);
  return out.c_str();
}

// Lote mais proximo do ponto (mesmo que o ponto caia na rua). Util para achar o muro.
EMSCRIPTEN_KEEPALIVE const char* mw_debug_lot_nearest(float x, float z) {
  static std::string out;
  const std::vector<LotRecord>& lots = lot_records();
  int best = -1;
  float best_d = 1e30f;
  for (size_t i = 0; i < lots.size(); ++i) {
    const LotRecord& r = lots[i];
    float dx = std::max(0.0f, std::max(r.cx - r.hx - x, x - (r.cx + r.hx)));
    float dz = std::max(0.0f, std::max(r.cz - r.hz - z, z - (r.cz + r.hz)));
    float d = dx * dx + dz * dz;
    if (d < best_d) { best_d = d; best = (int)i; }
  }
  if (best < 0) { out.assign("none|0|0|0|0|0|0|0|0"); return out.c_str(); }
  const LotRecord& r = lots[(size_t)best];
  char buf[320];
  std::snprintf(buf, sizeof(buf), "%s|%.3f|%d|%d|%.1f|%.1f|%.1f|%.1f|%.1f", r.name.c_str(),
                r.scale, r.layer, r.tris, r.cx, r.cz, r.height, r.hx, r.hz);
  out.assign(buf);
  return out.c_str();
}

// Estatisticas do ultimo frame desenhado: "draw_calls|triangulos".
EMSCRIPTEN_KEEPALIVE const char* mw_debug_stats() {
  static std::string out;
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%d|%.0f", stat_draw_calls(), stat_draw_tris());
  out.assign(buf);
  return out.c_str();
}

EMSCRIPTEN_KEEPALIVE void mw_debug_time(float tod) {
  g_tod = clampf(tod, 0.0f, 0.999f);
  g_want_time_jump = 0;
}

EMSCRIPTEN_KEEPALIVE void mw_debug_place(float x, float z, float yaw) {
  g_car.pos = {x, 0.0f, z};
  g_car.yaw = yaw;
  g_car.speed = 0.0f;
  g_player.pos = {x, 0.0f, z};
  g_player.yaw = yaw;
  g_cam_yaw = yaw;
}

EMSCRIPTEN_KEEPALIVE double mw_debug_tris() {
  double t = 0.0;
  for (auto& c : g_city.chunks) {
    t += (double)c.ground.count + (double)c.bld.count + (double)c.props.count;
    t += (double)c.treeCount * 64.0 + (double)c.lampCount * 64.0;
  }
  t += (double)g_city.promenade.count + (double)g_city.ilha.count + (double)g_city.sea.count;
  t += (double)g_signal_masts.count + (double)g_signal_lamps[0].count;
  return t / 3.0;
}

EMSCRIPTEN_KEEPALIVE void mw_debug_walk(float x, float z, float yaw) {
  g_in_car = false;
  g_car.speed = 0.0f;
  g_player.pos = {x, 0.0f, z};
  g_player.yaw = yaw;
  g_cam_yaw = yaw;
}

}  // extern "C"

// ------------------------------------------------------------------ helpers

static float angle_lerp(float a, float b, float t) {
  float d = b - a;
  while (d > 3.14159265f) d -= 6.28318531f;
  while (d < -3.14159265f) d += 6.28318531f;
  return a + d * t;
}

static void reset_car() {
  g_car.pos = SPAWN_POS;
  g_car.yaw = SPAWN_YAW;
  g_car.speed = 0.0f;
  g_car.steerVis = 0.0f;
  g_in_car = true;
  g_cam_yaw = SPAWN_YAW;
  g_orbit_yaw = 0.0f;
  g_orbit_pitch = 0.0f;
}

static void set_common_uniforms(GLint sunDir, GLint sunCol, GLint ambSky, GLint ambGround,
                                GLint camPos, GLint fogCol, GLint fogNear, GLint fogFar,
                                const V3& cam) {
  glUniform3f(sunDir, g_sun_dir.x, g_sun_dir.y, g_sun_dir.z);
  glUniform3f(sunCol, g_sun_col.x, g_sun_col.y, g_sun_col.z);
  glUniform3f(ambSky, g_amb_sky.x, g_amb_sky.y, g_amb_sky.z);
  glUniform3f(ambGround, g_amb_gnd.x, g_amb_gnd.y, g_amb_gnd.z);
  glUniform3f(camPos, cam.x, cam.y, cam.z);
  glUniform3f(fogCol, g_fog_col.x, g_fog_col.y, g_fog_col.z);
  glUniform1f(fogNear, 220.0f);
  glUniform1f(fogFar, 1150.0f);
}

static void update_camera(float dt, V3& out_eye, V3& out_target) {
  float base_yaw = g_in_car ? g_car.yaw : g_player.yaw;
  float follow = 1.0f - std::exp(-7.0f * dt);
  g_cam_yaw = angle_lerp(g_cam_yaw, base_yaw, follow);

  if (g_time - g_last_drag > 1.4f) {
    g_orbit_yaw *= std::exp(-1.8f * dt);
    g_orbit_pitch *= std::exp(-1.8f * dt);
  }

  V3 target = g_in_car ? g_car.pos + v3(0.0f, 1.30f, 0.0f)
                       : g_player.pos + v3(0.0f, 1.55f, 0.0f);
  float aspect = (float)g_w / (float)std::max(1, g_h);
  float narrow = clampf(1.0f - aspect, 0.0f, 0.6f);
  float yaw = g_cam_yaw + g_orbit_yaw;
  float pitch = (g_in_car ? 0.24f : 0.16f) + g_orbit_pitch + narrow * 0.22f;
  float dist = (g_in_car ? (g_cam_mode ? 9.6f : 6.9f) : 4.1f) * (1.0f + narrow * 0.75f);

  V3 dir = {std::sin(yaw) * std::cos(pitch), -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
  V3 eye = target - dir * dist;

  V3 to_eye = normalize(eye - target);
  float max_t = dist;
  for (const auto& b : g_cols) {
    float t = ray_aabb(target, to_eye, b, max_t);
    if (t >= 0.0f && t < max_t) max_t = t;
  }
  if (max_t < dist) {
    eye = target + to_eye * std::max(1.1f, max_t - 0.35f);
  }
  if (eye.y < 0.5f) eye.y = 0.5f;
  if (g_debug_top) eye = target + v3(0.0f, 130.0f, 70.0f);
  out_eye = eye;
  out_target = target;
}

// ------------------------------------------------------------------ tempo

static V3 mixv(const V3& a, const V3& b, float t) {
  return v3(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
}

static void update_time_of_day(float dt) {
  if (g_want_time_jump) {
    g_want_time_jump = 0;
    g_tod += 0.125f;
    if (g_tod >= 1.0f) g_tod -= 1.0f;
  }
  g_tod += dt / DAY_SECONDS;
  if (g_tod >= 1.0f) g_tod -= 1.0f;

  float phi = 6.2831853f * (g_tod - 0.25f);
  float el = std::sin(phi);
  float hz = std::cos(phi);
  g_sun_dir = normalize(v3(0.72f * hz, el, -0.52f * hz));

  float day = clampf((el + 0.04f) / 0.30f, 0.0f, 1.0f);
  float night = 1.0f - clampf((el + 0.18f) / 0.34f, 0.0f, 1.0f);
  float dusk = clampf(1.0f - std::fabs(el) / 0.30f, 0.0f, 1.0f) * day;
  g_day = day;
  g_night = night;

  V3 sun = mixv(v3(0.88f, 0.82f, 0.72f), v3(1.15f, 0.52f, 0.22f), dusk * 0.9f);
  sun = mixv(v3(0.13f, 0.16f, 0.24f), sun, day);
  g_sun_col = sun;

  g_amb_sky = mixv(mixv(v3(0.095f, 0.115f, 0.175f), v3(0.27f, 0.31f, 0.38f), day),
                   v3(0.30f, 0.20f, 0.15f), dusk * 0.35f);
  g_amb_gnd = mixv(mixv(v3(0.055f, 0.060f, 0.080f), v3(0.145f, 0.135f, 0.125f), day),
                   v3(0.20f, 0.13f, 0.10f), dusk * 0.4f);
  g_fog_col = mixv(mixv(v3(0.060f, 0.080f, 0.125f), v3(0.72f, 0.78f, 0.85f), day),
                   v3(0.86f, 0.55f, 0.35f), dusk * 0.7f);
  g_sky_hor = mixv(mixv(v3(0.05f, 0.07f, 0.12f), v3(0.78f, 0.85f, 0.93f), day),
                   v3(0.98f, 0.55f, 0.30f), dusk * 0.75f);
  g_sky_zen = mixv(mixv(v3(0.012f, 0.020f, 0.055f), v3(0.20f, 0.42f, 0.74f), day),
                   v3(0.22f, 0.22f, 0.42f), dusk * 0.6f);
  g_sky_mul = mixv(mixv(v3(0.12f, 0.15f, 0.22f), v3(1.0f, 1.0f, 1.0f), day),
                   v3(1.05f, 0.75f, 0.58f), dusk * 0.5f);
}

// ------------------------------------------------------------------ estafeta

static Mesh make_beacon_mesh(uint32_t c_lo, uint32_t c_hi) {
  Builder b;
  const int N = 12;
  const float r = 0.95f;
  for (int i = 0; i < N; ++i) {
    float a0 = (float)i * 6.2831853f / (float)N;
    float a1 = (float)(i + 1) * 6.2831853f / (float)N;
    float am = (a0 + a1) * 0.5f;
    V3 n = {std::cos(am), 0.0f, std::sin(am)};
    V3 b0 = {r * std::cos(a0), 0.02f, r * std::sin(a0)};
    V3 b1 = {r * std::cos(a1), 0.02f, r * std::sin(a1)};
    V3 m0 = {r * std::cos(a0), 13.0f, r * std::sin(a0)};
    V3 m1 = {r * std::cos(a1), 13.0f, r * std::sin(a1)};
    V3 t0 = {r * 0.5f * std::cos(a0), 22.0f, r * 0.5f * std::sin(a0)};
    V3 t1 = {r * 0.5f * std::cos(a1), 22.0f, r * 0.5f * std::sin(a1)};
    b.quad(b0, b1, m1, m0, n, L_EMIT, c_lo, 0, 0, 0.2f, 0.2f, 0.35f, 0.6f, 0.2f, 0.6f);
    b.quad(m0, m1, t1, t0, n, L_EMIT, c_hi, 0, 0, 0.2f, 0.2f, 0.35f, 0.6f, 0.2f, 0.6f);
  }
  return mesh_upload(b.v);
}

static V3 road_point(uint32_t& s, V3 from, float min_d, float max_d) {
  const float rx0 = BLOCK + ROAD * 0.5f;
  V3 best = {rx0 + 5.0f * CELL + 5.0f, 0.0f, rx0 + 3.0f * CELL + 5.0f};
  bool have = false;
  for (int i = 0; i < 80; ++i) {
    int kx = -3 + (int)(frand(s) * 10.0f);
    int kz = -1 + (int)(frand(s) * 9.0f);
    V3 p = {rx0 + (float)kx * CELL + 5.0f, 0.0f, rx0 + (float)kz * CELL + 5.0f};
    float d = length(p - from);
    if (d >= min_d && d <= max_d) return p;
    if (!have && d >= min_d) {
      best = p;
      have = true;
    }
  }
  return best;
}

static void job_start() {
  V3 at = g_in_car ? g_car.pos : g_player.pos;
  g_job.stage = JOB_PICK;
  g_job.target = road_point(g_job_seed, at, 150.0f, 420.0f);
  float d = length(g_job.target - at);
  g_job.time_left = clampf(d / 13.0f + 22.0f, 40.0f, 95.0f);
  g_job.payout = 0.0f;
  snprintf(g_job_msg, sizeof(g_job_msg), "Novo trabalho: recolhe a encomenda");
  g_job_msg_t = 3.5f;
}

static void update_job(float dt) {
  g_job.cooldown = std::max(0.0f, g_job.cooldown - dt);
  g_job_msg_t = std::max(0.0f, g_job_msg_t - dt);

  if (g_want_job) {
    g_want_job = 0;
    if (g_job.stage == JOB_IDLE && g_job.cooldown <= 0.0f) job_start();
  }
  if (g_job.stage == JOB_IDLE) return;

  V3 at = g_in_car ? g_car.pos : g_player.pos;
  g_job.time_left -= dt;
  if (g_job.time_left <= 0.0f) {
    g_job.stage = JOB_IDLE;
    g_job.cooldown = 6.0f;
    snprintf(g_job_msg, sizeof(g_job_msg), "Tempo esgotado. Entrega falhada.");
    g_job_msg_t = 4.0f;
    return;
  }
  if (length(g_job.target - at) >= 9.0f) return;

  if (g_job.stage == JOB_PICK) {
    g_job.target = road_point(g_job_seed, at, 380.0f, 900.0f);
    float d = length(g_job.target - at);
    g_job.payout = 700.0f + d * 1.15f;
    g_job.time_left = clampf(d / 14.0f + 28.0f, 55.0f, 150.0f);
    g_job.stage = JOB_DROP;
    snprintf(g_job_msg, sizeof(g_job_msg), "Encomenda a bordo. Entrega-a!");
    g_job_msg_t = 3.0f;
  } else {
    int pay = (int)g_job.payout;
    g_cash += (float)pay;
    g_job.stage = JOB_IDLE;
    g_job.cooldown = 3.0f;
    snprintf(g_job_msg, sizeof(g_job_msg), "Entrega concluida: + Kz %d", pay);
    g_job_msg_t = 4.5f;
  }
}

static void poi_msg(const char* s, float dur = 3.5f) {
  snprintf(g_job_msg, sizeof(g_job_msg), "%s", s);
  g_job_msg_t = dur;
}

static void interact_poi(int idx) {
  if (idx < 0 || idx >= (int)poi_list().size()) return;
  const Poi& p = poi_list()[idx];
  switch (p.kind) {
    case 0: {
      int price = 25000;
      if (std::strstr(p.name, "45")) price = 45000;
      else if (std::strstr(p.name, "90")) price = 90000;
      if (g_poi_bought[idx]) {
        poi_msg("Ja tens esta casa. Bem-vindo a casa.");
        return;
      }
      if (g_cash < (float)price) {
        poi_msg("Nao tens Kz suficientes para esta casa.");
        return;
      }
      g_cash -= (float)price;
      g_poi_bought[idx] = true;
      char b[72];
      snprintf(b, sizeof(b), "Casa comprada por Kz %d. E tua!", price);
      poi_msg(b, 4.5f);
      break;
    }
    case 1:
      if (g_cash < 60.0f) {
        poi_msg("Sem Kz para a merenda.");
        return;
      }
      g_cash -= 60.0f;
      g_health = std::min(100.0f, g_health + 25.0f);
      poi_msg("Merenda comprada: +25 de saude");
      break;
    case 2:
      if (g_cash < 300.0f) {
        poi_msg("O Kalulu nao fia. Volta com Kz 300.");
        return;
      }
      g_cash -= 300.0f;
      g_health = 100.0f;
      poi_msg("Calulu e funge: saude a 100%");
      break;
    case 3:
      if (g_job.stage != JOB_IDLE || g_job.cooldown > 0.0f) {
        poi_msg("O estafeta esta ocupado.");
        return;
      }
      job_start();
      break;
    case 4:
      poi_msg("Loja do Kambuta: armas chegam numa proxima fase.");
      break;
    default:
      poi_msg("Contacto recebido. Segue o mapa.");
      break;
  }
}

static void render_signals(const V3& eye, const Mat4& vp) {
  if (!g_signal_masts.count) return;
  glUseProgram(g_world.id);
  glUniformMatrix4fv(g_world.vp, 1, GL_FALSE, vp.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
  glUniform1i(g_world.atlas, 0);
  set_common_uniforms(g_world.sunDir, g_world.sunCol, g_world.ambSky, g_world.ambGround,
                      g_world.camPos, g_world.fogCol, g_world.fogNear, g_world.fogFar, eye);
  draw_mesh(g_signal_masts);
  draw_mesh(g_signal_lamps[signal_phase_at(g_time)]);
}

static void render_beacons(const V3& eye, const Mat4& vp) {
  if (g_job.stage == JOB_IDLE) return;
  glUseProgram(g_pedProg.id);
  glUniformMatrix4fv(g_pedProg.vp, 1, GL_FALSE, vp.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
  glUniform1i(g_pedProg.atlas, 0);
  set_common_uniforms(g_pedProg.sunDir, g_pedProg.sunCol, g_pedProg.ambSky, g_pedProg.ambGround,
                      g_pedProg.camPos, g_pedProg.fogCol, g_pedProg.fogNear, g_pedProg.fogFar, eye);
  Mat4 m = mat_mul(mat_translate(g_job.target), mat_rot_y(g_time * 0.8f));
  glUniformMatrix4fv(g_pedProg.model, 1, GL_FALSE, m.m);
  draw_mesh(g_job.stage == JOB_PICK ? g_beacon_pick : g_beacon_drop);
}

// ------------------------------------------------------------------ render

static void render_world(const Frustum& fr, const V3& eye, const Mat4& vp, float time) {
  glUseProgram(g_world.id);
  glUniformMatrix4fv(g_world.vp, 1, GL_FALSE, vp.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
  glUniform1i(g_world.atlas, 0);
  set_common_uniforms(g_world.sunDir, g_world.sunCol, g_world.ambSky, g_world.ambGround,
                      g_world.camPos, g_world.fogCol, g_world.fogNear, g_world.fogFar, eye);

  for (auto& c : g_city.chunks) {
    if (!c.built) continue;
    if (!aabb_visible(fr, c.bounds)) continue;
    draw_mesh(c.ground);
    draw_mesh(c.bld);
    if (c.treeCount) draw_instanced(c.treeMesh, c.treeCount);
    if (c.lampCount) draw_instanced(c.lampMesh, c.lampCount);
  }

  if (aabb_visible(fr, g_city.promenade.bounds)) draw_mesh(g_city.promenade);
  if (aabb_visible(fr, g_city.ilha.bounds)) draw_mesh(g_city.ilha);
  if (g_city.farCity.count && aabb_visible(fr, g_city.farCity.bounds)) draw_mesh(g_city.farCity);
  if (g_city.ilhaPalms > 0 && aabb_visible(fr, g_city.ilha.bounds)) {
    draw_instanced(g_city.ilhaPalmMesh, g_city.ilhaPalms);
  }
  (void)time;
}

static void render_water(const Frustum& fr, const V3& eye, const Mat4& vp, float time) {
  if (!aabb_visible(fr, g_city.sea.bounds)) return;
  glUseProgram(g_water.id);
  glUniformMatrix4fv(g_water.vp, 1, GL_FALSE, vp.m);
  glUniform3f(g_water.camPos, eye.x, eye.y, eye.z);
  glUniform3f(g_water.sunDir, g_sun_dir.x, g_sun_dir.y, g_sun_dir.z);
  glUniform3f(g_water.sunCol, g_sun_col.x * 1.12f, g_sun_col.y * 1.11f, g_sun_col.z * 1.11f);
  glUniform3f(g_water.fogCol, g_fog_col.x, g_fog_col.y, g_fog_col.z);
  glUniform3f(g_water.skyMul, g_sky_mul.x, g_sky_mul.y, g_sky_mul.z);
  glUniform1f(g_water.fogNear, 220.0f);
  glUniform1f(g_water.fogFar, 1150.0f);
  glUniform1f(g_water.time, time);
  draw_mesh(g_city.sea);
}

static void render_sky(const Frustum& fr, const Mat4& view, const Mat4& proj, float time) {
  (void)fr;
  (void)view;
  float tan_half = 1.0f / proj.m[5];
  float aspect = proj.m[5] / proj.m[0];

  V3 right = {view.m[0], view.m[4], view.m[8]};
  V3 up = {view.m[1], view.m[5], view.m[9]};
  V3 fwd = {-view.m[2], -view.m[6], -view.m[10]};

  glUseProgram(g_sky.id);
  glUniform3f(g_sky.right, right.x, right.y, right.z);
  glUniform3f(g_sky.up, up.x, up.y, up.z);
  glUniform3f(g_sky.fwd, fwd.x, fwd.y, fwd.z);
  glUniform1f(g_sky.tanHalf, tan_half);
  glUniform1f(g_sky.aspect, aspect);
  glUniform3f(g_sky.sunDir, g_sun_dir.x, g_sun_dir.y, g_sun_dir.z);
  glUniform3f(g_sky.sunCol, g_sun_col.x * 1.12f, g_sun_col.y * 1.11f, g_sun_col.z * 1.11f);
  glUniform3f(g_sky.horizon, g_sky_hor.x, g_sky_hor.y, g_sky_hor.z);
  glUniform3f(g_sky.zenith, g_sky_zen.x, g_sky_zen.y, g_sky_zen.z);
  glUniform1f(g_sky.day, g_day);
  glUniform1f(g_sky.time, time);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, g_clouds);
  glUniform1i(g_sky.clouds, 0);
  glBindVertexArray(g_empty_vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
}

static void render_cars(const V3& eye, const Mat4& vp) {
  glUseProgram(g_carProg.id);
  glUniformMatrix4fv(g_carProg.vp, 1, GL_FALSE, vp.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
  glUniform1i(g_carProg.atlas, 0);
  set_common_uniforms(g_carProg.sunDir, g_carProg.sunCol, g_carProg.ambSky, g_carProg.ambGround,
                      g_carProg.camPos, g_carProg.fogCol, g_carProg.fogNear, g_carProg.fogFar,
                      eye);

  for (const auto& p : g_parked) {
    draw_car(g_carProg, g_car_inst, g_car_bufs, p.pos, p.yaw, p.color, 0.0f, 0.0f);
  }
  for (const auto& t : g_traffic) {
    if (length(t.pos - eye) > 420.0f) continue;
    draw_car(g_carProg, g_car_inst, g_car_bufs, t.pos, t.yaw, t.color, t.steerVis, t.wheelSpin);
  }
  for (const auto& c : g_cops) {
    draw_car(g_carProg, g_car_inst, g_car_bufs, c.pos, c.yaw, tint3(0.88f, 0.89f, 0.91f),
             c.steerVis, c.wheelSpin);
  }
  draw_car(g_carProg, g_car_inst, g_car_bufs, g_car.pos, g_car.yaw, tint3(0.85f, 0.16f, 0.10f),
           g_car.steerVis, g_car.wheelSpin);
}

static void render_cop_lights(const V3& eye, const Mat4& vp) {
  if (g_cops.empty()) return;
  glUseProgram(g_pedProg.id);
  glUniformMatrix4fv(g_pedProg.vp, 1, GL_FALSE, vp.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
  glUniform1i(g_pedProg.atlas, 0);
  set_common_uniforms(g_pedProg.sunDir, g_pedProg.sunCol, g_pedProg.ambSky, g_pedProg.ambGround,
                      g_pedProg.camPos, g_pedProg.fogCol, g_pedProg.fogNear, g_pedProg.fogFar,
                      eye);
  for (const auto& c : g_cops) {
    Mat4 m = mat_mul(mat_mul(mat_translate(c.pos), mat_rot_y(c.yaw)),
                     mat_translate(v3(0.0f, 1.56f, -0.32f)));
    glUniformMatrix4fv(g_pedProg.model, 1, GL_FALSE, m.m);
    draw_mesh(g_cop_light_mesh);
  }
}

static void render_player(const V3& eye, const Mat4& vp) {
  if (g_in_car) return;
  glUseProgram(g_pedProg.id);
  glUniformMatrix4fv(g_pedProg.vp, 1, GL_FALSE, vp.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
  glUniform1i(g_pedProg.atlas, 0);
  set_common_uniforms(g_pedProg.sunDir, g_pedProg.sunCol, g_pedProg.ambSky, g_pedProg.ambGround,
                      g_pedProg.camPos, g_pedProg.fogCol, g_pedProg.fogNear, g_pedProg.fogFar,
                      eye);
  draw_player(g_pedProg, g_player_mesh, g_player);
}

static void render_peds(const V3& eye, const Mat4& vp) {
  bool started = false;
  for (const auto& p : g_peds) {
    if (length(p.w.pos - eye) > 130.0f) continue;
    if (!started) {
      started = true;
      glUseProgram(g_pedProg.id);
      glUniformMatrix4fv(g_pedProg.vp, 1, GL_FALSE, vp.m);
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
      glUniform1i(g_pedProg.atlas, 0);
      set_common_uniforms(g_pedProg.sunDir, g_pedProg.sunCol, g_pedProg.ambSky,
                          g_pedProg.ambGround, g_pedProg.camPos, g_pedProg.fogCol,
                          g_pedProg.fogNear, g_pedProg.fogFar, eye);
    }
    draw_player(g_pedProg, g_ped_looks[p.variant & 3], p.w);
  }
}

static void render_blobs(const Mat4& vp, const V3& eye) {
  std::vector<float> blobs;
  blobs.reserve(64);
  add_car_blob(blobs, g_car.pos, g_car.yaw, 1.02f, 2.1f);
  for (const auto& p : g_parked) add_car_blob(blobs, p.pos, p.yaw, 1.02f, 2.1f);
  for (const auto& t : g_traffic) {
    if (length(t.pos - eye) < 220.0f) add_car_blob(blobs, t.pos, t.yaw, 1.02f, 2.1f);
  }
  for (const auto& c : g_cops) add_car_blob(blobs, c.pos, c.yaw, 1.02f, 2.1f);
  for (const auto& p : g_peds) {
    if (length(p.w.pos - eye) < 90.0f) add_round_blob(blobs, p.w.pos, 0.36f);
  }
  if (!g_in_car) add_round_blob(blobs, g_player.pos, 0.36f);
  if (blobs.empty()) return;

  glBindBuffer(GL_ARRAY_BUFFER, g_blob_buf);
  glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(blobs.size() * 4), blobs.data(), GL_DYNAMIC_DRAW);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);
  glUseProgram(g_blob.id);
  glUniformMatrix4fv(g_blob.vp, 1, GL_FALSE, vp.m);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas);
  glUniform1i(g_blob.atlas, 0);
  glUniform1f(g_blob.strength, 0.72f);
  draw_instanced(g_blob_inst, (GLsizei)(blobs.size() / 16));
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

static void frame() {
  double now = emscripten_get_now();
  float dt = (float)((now - g_prev_ms) / 1000.0);
  g_prev_ms = now;
  if (dt > 0.08f) dt = 0.08f;
  if (dt < 0.0f) dt = 0.0f;
  g_time += dt;
  stat_frame_reset();
  update_time_of_day(dt);

  // input
  float throttle = clampf(g_in_f + std::max(0.0f, g_axis_y), 0.0f, 1.0f);
  float brake = clampf(g_in_b + std::max(0.0f, -g_axis_y), 0.0f, 1.0f);
  float steer = clampf((g_in_r - g_in_l) + g_axis_x, -1.0f, 1.0f);
  float handbrake = g_in_hb;

  if (g_want_reset) {
    g_want_reset = 0;
    reset_car();
  }
  if (g_want_cam) {
    g_want_cam = 0;
    g_cam_mode ^= 1;
  }

  float near_car = length(g_player.pos - g_car.pos);
  {
    float poi_d = 1e9f;
    int poi_near = -1;
    if (!g_in_car) {
      const auto& pl = poi_list();
      for (size_t i = 0; i < pl.size(); ++i) {
        float d = length(v3(pl[i].x, 0.0f, pl[i].z) - g_player.pos);
        if (d < poi_d) {
          poi_d = d;
          poi_near = (int)i;
        }
      }
      if (poi_d > 5.5f) poi_near = -1;
    }
    g_poi_near = poi_near;
    g_poi_dist = poi_d;
  }
  if (g_want_exit) {
    g_want_exit = 0;
    if (g_in_car) {
      V3 f = car_forward(g_car.yaw);
      V3 right = v3(f.z, 0.0f, -f.x);
      V3 p = g_car.pos + right * 2.1f;
      for (const auto& b : g_cols) {
        bool hit = false;
        resolve_circle(p, 0.45f, b, hit);
      }
      clamp_world(p, 0.6f);
      g_player.pos = p;
      g_player.yaw = g_car.yaw;
      g_in_car = false;
      g_car.speed = 0.0f;
    } else if (g_poi_near >= 0 && g_poi_dist < near_car) {
      interact_poi(g_poi_near);
    } else if (near_car < 4.4f) {
      g_in_car = true;
      g_cam_yaw = g_car.yaw;
      g_orbit_yaw = 0.0f;
    }
  }

  if (g_in_car) {
    float preSpeed = std::fabs(g_car.speed);
    bool wallHit = update_car(g_car, throttle, brake, steer, handbrake, dt, g_cols);
    if (wallHit && preSpeed > 5.0f && g_impact_cd <= 0.0f) {
      mw_impact_js(std::min(1.0f, preSpeed / 22.0f));
      g_impact_cd = 0.35f;
    }
    g_player.pos = g_car.pos;
  } else {
    V3 cf = {std::sin(g_cam_yaw), 0.0f, std::cos(g_cam_yaw)};
    V3 cr = {-std::cos(g_cam_yaw), 0.0f, std::sin(g_cam_yaw)};
    V3 wish = cf * ((g_in_f + std::max(0.0f, g_axis_y)) - (g_in_b + std::max(0.0f, -g_axis_y)));
    wish += cr * ((g_in_r - g_in_l) + g_axis_x);
    bool run = g_in_run > 0.5f || std::fabs(g_axis_x) + std::fabs(g_axis_y) > 1.05f;
    update_walker(g_player, wish, run, dt, g_cols);
  }

  update_job(dt);

  V3 eye, target;
  update_camera(dt, eye, target);

  update_traffic(g_traffic, g_in_car ? g_car.pos : g_player.pos, dt, g_street_seed, g_time);
  update_peds(g_peds, g_in_car ? g_car.pos : g_player.pos, dt, g_street_seed);
  {
    V3 at = g_in_car ? g_car.pos : g_player.pos;
    bool trafficHit = false;
    float preSpeed = std::fabs(g_car.speed);
    if (g_in_car) trafficHit = resolve_player_traffic(g_car, g_traffic);
    else resolve_walker_traffic(g_player, g_traffic);
    if (trafficHit) {
      if (preSpeed > 5.0f && g_impact_cd <= 0.0f) {
        mw_impact_js(std::min(1.0f, preSpeed / 22.0f));
        g_impact_cd = 0.35f;
      }
      if (g_offense_cd <= 0.0f) {
        g_heat = clampf(g_heat + 1.2f, 0.0f, 5.0f);
        g_offense_age = 0.0f;
        g_offense_cd = 1.0f;
      }
    }

    g_offense_age += dt;
    g_offense_cd = std::max(0.0f, g_offense_cd - dt);
    g_cop_hit_cd = std::max(0.0f, g_cop_hit_cd - dt);
    g_cop_spawn_cd = std::max(0.0f, g_cop_spawn_cd - dt);
    g_impact_cd = std::max(0.0f, g_impact_cd - dt);
    if (g_offense_age > 12.0f) g_heat = std::max(0.0f, g_heat - 0.26f * dt);

    if (g_heat >= 1.2f) {
      int want = 2 + (g_heat >= 2.9f ? 1 : 0) + (g_heat >= 4.2f ? 1 : 0);
      if ((int)g_cops.size() < want && g_cop_spawn_cd <= 0.0f) {
        PoliceCar c;
        spawn_cop(c, at, g_street_seed);
        g_cops.push_back(c);
        g_cop_spawn_cd = 0.8f;
      }
    } else if (g_heat < 0.9f) {
      g_cops.clear();
    }
    for (auto& c : g_cops) update_cop(c, at, dt, g_street_seed, g_cols);
    if (g_in_car) {
      for (auto& c : g_cops) {
        if (resolve_player_cop(g_car, c)) {
          if (preSpeed > 4.0f && g_impact_cd <= 0.0f) {
            mw_impact_js(0.7f);
            g_impact_cd = 0.35f;
          }
          if (g_cop_hit_cd <= 0.0f) {
            g_health = std::max(12.0f, g_health - 7.0f);
            g_cop_hit_cd = 0.7f;
          }
        }
      }
    } else {
      for (const auto& c : g_cops) resolve_walker_cop(g_player, c);
    }
    if (g_health < 100.0f) g_health = std::min(100.0f, g_health + 0.35f * dt);
    g_wanted = (int)std::ceil(g_heat - 0.04f);
    if (g_wanted < 0) g_wanted = 0;
    if (g_wanted > 5) g_wanted = 5;
  }

  // render
  glViewport(0, 0, g_w, g_h);
  glClearColor(g_fog_col.x, g_fog_col.y, g_fog_col.z, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_CULL_FACE);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  float aspect = (float)g_w / (float)std::max(1, g_h);
  Mat4 proj = mat_perspective(58.0f * 3.14159265f / 180.0f, aspect, 0.25f, 2600.0f);
  Mat4 view = mat_look_at(eye, target, v3(0.0f, 1.0f, 0.0f));
  Mat4 vp = mat_mul(proj, view);
  Frustum fr = frustum_from(vp);

  glDepthMask(GL_FALSE);
  glDisable(GL_DEPTH_TEST);
  render_sky(fr, view, proj, g_time);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  render_world(fr, eye, vp, g_time);
  render_signals(eye, vp);
  render_water(fr, eye, vp, g_time);
  render_cars(eye, vp);
  render_cop_lights(eye, vp);
  render_beacons(eye, vp);
  render_player(eye, vp);
  render_peds(eye, vp);
  render_blobs(vp, eye);

  ++g_frame_count;
  if (!g_ready && g_frame_count >= 2) {
    g_ready = true;
    mw_boot_ready();
  }

  // hud
  if (g_time - g_hud_timer > 0.1) {
    g_hud_timer = g_time;
    float kmh = std::fabs(g_car.speed) * 3.6f;
    const char* prompt = "";
    char prompt_buf[96];
    if (g_in_car) {
      prompt = "E · Sair do carro";
    } else if (g_poi_near >= 0 && g_poi_dist < near_car) {
      snprintf(prompt_buf, sizeof(prompt_buf), "E · %s", poi_list()[g_poi_near].name);
      prompt = prompt_buf;
    } else if (near_car < 4.4f) {
      prompt = "E · Entrar no carro";
    } else if (g_job.stage == JOB_PICK) {
      prompt = "Recolhe a encomenda";
    } else if (g_job.stage == JOB_DROP) {
      prompt = "Entrega a encomenda";
    } else if (g_job.cooldown <= 0.0f) {
      prompt = "F · Trabalho de estafeta";
    } else {
      prompt = "Segue o mapa até à Marginal";
    }
    mw_dbg(throttle, brake, steer, g_car.speed, g_axis_x, g_axis_y, g_car.pos.x, g_car.pos.z,
           g_car.yaw);
    {
      V3 at = g_in_car ? g_car.pos : g_player.pos;
      float nc = 1e9f, np = 1e9f;
      for (const auto& t : g_traffic) nc = std::min(nc, length(t.pos - at));
      for (const auto& p : g_peds) np = std::min(np, length(p.w.pos - at));
      float ncop = 1e9f;
      for (const auto& c : g_cops) ncop = std::min(ncop, length(c.pos - at));
      mw_street_js((int)g_traffic.size(), (int)g_peds.size(), nc, np, g_heat, (int)g_cops.size(),
                   ncop, g_health);
      mw_siren_js(g_wanted >= 2 ? 1 : 0);
      for (size_t i = 0; i < g_traffic.size() && i < 24; ++i) {
        const TrafficCar& t = g_traffic[i];
        mw_traffic_js((int)i, t.pos.x, t.pos.z, t.yaw, t.speed);
      }
    }
    mw_hud(kmh, g_health, g_cash, g_wanted, g_in_car ? 1 : 0,
           g_in_car ? g_car.pos.x : g_player.pos.x,
           g_in_car ? g_car.pos.z : g_player.pos.z,
           g_in_car ? g_car.yaw : g_player.yaw, prompt,
           g_job.stage ? g_job.target.x : 0.0f, g_job.stage ? g_job.target.z : 0.0f,
           g_job.stage);
    {
      const char* jt = "";
      char js[64] = "";
      V3 at = g_in_car ? g_car.pos : g_player.pos;
      int tl = (int)std::ceil(g_job.time_left);
      if (g_job_msg_t > 0.0f) {
        jt = g_job_msg;
        if (g_job.stage != JOB_IDLE) {
          snprintf(js, sizeof(js), "%d m · %d:%02d", (int)length(g_job.target - at),
                   tl / 60, tl % 60);
        }
      } else if (g_job.stage != JOB_IDLE) {
        jt = g_job.stage == JOB_PICK ? "Recolhe a encomenda" : "Entrega a encomenda";
        snprintf(js, sizeof(js), "%d m · %d:%02d", (int)length(g_job.target - at), tl / 60,
                 tl % 60);
      } else if (g_job.cooldown > 0.0f) {
        jt = "Estafeta";
        snprintf(js, sizeof(js), "Pronta em %d s", (int)std::ceil(g_job.cooldown));
      } else {
        jt = "Estafeta";
        snprintf(js, sizeof(js), "F · iniciar trabalho");
      }
      mw_job_js(g_job.stage, jt, js);
      int hh = (int)(g_tod * 24.0f);
      int mm = (int)((g_tod * 24.0f - (float)hh) * 60.0f);
      mw_clock_js(hh, mm, g_night > 0.55f ? 1 : 0);
    }
  }

  // fps + adaptive resolution
  ++g_quality_frames;
  double q_elapsed = (now - g_quality_time) / 1000.0;
  if (q_elapsed >= 2.5) {
    float fps = (float)(g_quality_frames / q_elapsed);
    mw_fps_js(fps);
    if (fps < 48.0f && g_render_scale > 0.58f) {
      g_render_scale = std::max(0.56f, g_render_scale - 0.07f);
      mw_set_scale_js(g_render_scale);
    } else if (fps > 57.0f && g_render_scale < 1.0f) {
      g_render_scale = std::min(1.0f, g_render_scale + 0.04f);
      mw_set_scale_js(g_render_scale);
    }
    g_quality_time = now;
    g_quality_frames = 0;
  }
  if (g_quality_time == 0.0) {
    g_quality_time = now;
    g_quality_frames = 0;
  }
}

// ------------------------------------------------------------------ main

int main() {
  mw_boot_msg("A preparar o WebGL2...");

  EmscriptenWebGLContextAttributes attrs;
  emscripten_webgl_init_context_attributes(&attrs);
  attrs.majorVersion = 2;
  attrs.minorVersion = 0;
  attrs.alpha = false;
  attrs.depth = true;
  attrs.stencil = false;
  attrs.antialias = false;
  attrs.preserveDrawingBuffer = false;
  attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;

  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#game", &attrs);
  if (ctx <= 0) {
    mw_fail_js("WebGL2 nao esta disponivel neste navegador.");
    return 0;
  }
  emscripten_webgl_make_context_current(ctx);

  if (!init_world_program(g_world) || !init_car_program(g_carProg) || !init_sky_program(g_sky) ||
      !init_water_program(g_water) || !init_blob_program(g_blob) || !init_ped_program(g_pedProg)) {
    mw_fail_js(g_gfx_error[0] ? g_gfx_error : "Falha ao compilar shaders.");
    return 0;
  }

  g_atlas = make_atlas(256, assets_load_textures("/assets/citytex.bin", 256));
  if (!g_atlas) {
    mw_fail_js("Falha ao criar o atlas de texturas.");
    return 0;
  }
  g_clouds = make_cloud_texture(512);
  glGenVertexArrays(1, &g_empty_vao);

  mw_boot_msg("A gerar Mwangolé...");

  g_palm_src = make_palm_mesh();
  g_lamp_src = make_lamp_mesh();
  g_car_base = make_car_meshes();
  g_car_bufs = make_car_buffers();
  g_car_inst.body = clone_with_instances(g_car_base.body, g_car_bufs.body, true);
  g_car_inst.detail = clone_with_instances(g_car_base.detail, g_car_bufs.detail, false);
  g_car_inst.wheel = clone_with_instances(g_car_base.wheel, g_car_bufs.wheel, false);
  g_player_mesh = make_player_meshes(player_look());
  for (int i = 0; i < 4; ++i) g_ped_looks[i] = make_player_meshes(street_look(i));
  g_cop_light_mesh = make_cop_light_mesh();
  g_beacon_pick = make_beacon_mesh(tint3(0.16f, 0.80f, 1.00f), tint3(0.06f, 0.30f, 0.38f));
  g_beacon_drop = make_beacon_mesh(tint3(1.00f, 0.70f, 0.16f), tint3(0.38f, 0.26f, 0.06f));

  g_blob_mesh = make_blob_quad();
  g_blob_buf = make_instance_buffer();
  g_blob_inst = clone_with_instances(g_blob_mesh, g_blob_buf, false);

  uint32_t treeSeed = 1234u;
  g_tree_mesh = g_palm_src;
  g_lamp_mesh = g_lamp_src;
  (void)treeSeed;

  assets_load("/assets/city.mwb");
  {
    int tris = 0, verts = 0;
    for (const auto& m : assets_all()) {
      tris += m.tris;
      verts += (int)m.verts.size();
    }
    mw_assets_js((int)assets_all().size(), tris, verts);
  }

  build_city(g_city, g_tree_mesh, g_lamp_mesh);
  g_signal_masts = make_signal_masts();
  for (int i = 0; i < 4; ++i) g_signal_lamps[i] = make_signal_lamps(i);
  {
    std::vector<float> pd;
    for (const auto& p : poi_list()) {
      pd.push_back(p.x);
      pd.push_back(p.z);
      pd.push_back((float)p.kind);
    }
    if (!pd.empty()) mw_pois_js((int)(size_t)pd.data(), (int)(pd.size() / 3));
  }

  for (auto& c : g_city.chunks) {
    for (auto& b : c.colliders) g_cols.push_back(b);
  }

  g_parked.push_back({v3(87.0f, 0.0f, 470.0f), 3.14159265f, tint3(0.78f, 0.79f, 0.80f)});
  g_parked.push_back({v3(191.0f, 0.0f, 150.0f), 0.0f, tint3(0.86f, 0.84f, 0.78f)});
  g_parked.push_back({v3(295.0f, 0.0f, 560.0f), 0.0f, tint3(0.24f, 0.56f, 0.38f)});
  // candongueiros na praca (azul e branco)
  g_parked.push_back({v3(87.0f, 0.0f, 438.0f), 3.14159265f, tint3(0.26f, 0.38f, 0.66f)});
  g_parked.push_back({v3(87.0f, 0.0f, 500.0f), 3.14159265f, tint3(0.86f, 0.87f, 0.82f)});
  g_parked.push_back({v3(201.0f, 0.0f, 470.0f), 0.0f, tint3(0.42f, 0.52f, 0.72f)});
  for (const auto& p : g_parked) {
    g_cols.push_back({{p.pos.x - 1.2f, -1.0f, p.pos.z - 2.4f},
                      {p.pos.x + 1.2f, 1.6f, p.pos.z + 2.4f}});
  }

  g_cols.reserve(g_cols.size() + 8);

  reset_car();
  g_player.pos = g_car.pos + v3(2.0f, 0.0f, 0.0f);
  g_player.yaw = g_car.yaw;

  init_traffic(g_traffic, 20, SPAWN_POS, g_street_seed);
  init_peds(g_peds, 26, SPAWN_POS, g_street_seed);

  // a few fixed extras so the spawn street is busy from the first frame
  if (g_traffic.size() >= 4) {
    g_traffic[0].pos = {97.45f, 0.0f, 180.0f};
    g_traffic[0].dir = 3;
    g_traffic[0].ni = 0;
    g_traffic[0].nj = 0;
    g_traffic[0].yaw = 3.1415926f;
    g_traffic[0].speed = 6.2f;
    g_traffic[0].cruise = 6.4f;
    g_traffic[1].pos = {90.85f, 0.0f, 300.0f};
    g_traffic[1].dir = 1;
    g_traffic[1].ni = 0;
    g_traffic[1].nj = 2;
    g_traffic[1].yaw = 0.0f;
    g_traffic[1].speed = 8.5f;
    g_traffic[2].pos = {60.0f, 0.0f, 201.45f};
    g_traffic[2].dir = 0;
    g_traffic[2].ni = 0;
    g_traffic[2].nj = 1;
    g_traffic[2].yaw = 1.5707963f;
    g_traffic[2].speed = 9.0f;
    g_traffic[3].pos = {160.0f, 0.0f, 194.55f};
    g_traffic[3].dir = 2;
    g_traffic[3].ni = 0;
    g_traffic[3].nj = 1;
    g_traffic[3].yaw = -1.5707963f;
    g_traffic[3].speed = 8.0f;
  }
  if (g_peds.size() >= 4) {
    place_ped(g_peds[0], 0, 1, 8.0f, 1, 1);
    place_ped(g_peds[1], -1, 1, 75.0f, -1, 2);
    place_ped(g_peds[2], -1, 1, 120.0f, 1, 3);
    place_ped(g_peds[3], 0, 1, 270.0f, -1, 0);
  }

  for (auto& c : g_city.chunks) {
    for (auto& b : c.colliders) {
      g_map_data.push_back(b.mn.x);
      g_map_data.push_back(b.mn.z);
      g_map_data.push_back(b.mx.x);
      g_map_data.push_back(b.mx.z);
    }
  }
  if (!g_map_data.empty()) {
    mw_map_js((int)(size_t)g_map_data.data(), (int)(g_map_data.size() / 4));
  }

  mw_boot_msg("Pronto. Bem-vindo ao Mwangolé.");
  mw_set_scale_js(1.0f);

  g_prev_ms = emscripten_get_now();
  g_quality_time = g_prev_ms;
  emscripten_set_main_loop(frame, 0, 1);
  return 0;
}
