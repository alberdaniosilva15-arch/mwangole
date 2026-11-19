import io
import sys

p = 'src/main.cpp'
s = io.open(p, encoding='utf-8').read()
orig = s


def rep(old, new, count=1):
    global s
    if old not in s:
        sys.exit('MISSING: ' + old[:80])
    s = s.replace(old, new, count)


# ---------------- 1. time-of-day globals ----------------
rep("""static double g_prev_ms = 0.0;
static float g_time = 0.0f;
""",
"""static double g_prev_ms = 0.0;
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
""")

# ---------------- 2. job globals after g_parked ----------------
rep("""static std::vector<ParkedCar> g_parked;
""",
"""static std::vector<ParkedCar> g_parked;

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
""")

# ---------------- 3. HUD EM_JS signature + marker ----------------
rep("""EM_JS(void, mw_hud, (float kmh, float health, float cash, int wanted, int in_car_arg,
                     float x, float z, float yaw, const char* prompt), {""",
"""EM_JS(void, mw_job_js, (int stage, const char* title, const char* sub), {
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
                     float jx, float jz, int jstage), {""")

rep("""    ctx.restore();
    // border ring""",
"""    ctx.restore();
    // mission marker
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
    // border ring""")

# ---------------- 4. action 14/15 ----------------
rep("""    case 13: g_in_run = value; break;""",
"""    case 13: g_in_run = value; break;
    case 14: if (value > 0.5f) g_want_time_jump = 1; break;
    case 15: if (value > 0.5f) g_want_job = 1; break;""")

# ---------------- 5. set_common_uniforms dynamic ----------------
rep("""  static const V3 sunDir_c = {0.66f, 0.55f, -0.51f};
  V3 sunCol_c = {1.00f, 0.92f, 0.79f};
  V3 ambSky_c = {0.32f, 0.36f, 0.44f};
  V3 ambGround_c = {0.17f, 0.16f, 0.15f};
  V3 fog_c = {0.72f, 0.78f, 0.85f};
  glUniform3f(sunDir, sunDir_c.x, sunDir_c.y, sunDir_c.z);
  glUniform3f(sunCol, sunCol_c.x, sunCol_c.y, sunCol_c.z);
  glUniform3f(ambSky, ambSky_c.x, ambSky_c.y, ambSky_c.z);
  glUniform3f(ambGround, ambGround_c.x, ambGround_c.y, ambGround_c.z);
  glUniform3f(camPos, cam.x, cam.y, cam.z);
  glUniform3f(fogCol, fog_c.x, fog_c.y, fog_c.z);""",
"""  glUniform3f(sunDir, g_sun_dir.x, g_sun_dir.y, g_sun_dir.z);
  glUniform3f(sunCol, g_sun_col.x, g_sun_col.y, g_sun_col.z);
  glUniform3f(ambSky, g_amb_sky.x, g_amb_sky.y, g_amb_sky.z);
  glUniform3f(ambGround, g_amb_gnd.x, g_amb_gnd.y, g_amb_gnd.z);
  glUniform3f(camPos, cam.x, cam.y, cam.z);
  glUniform3f(fogCol, g_fog_col.x, g_fog_col.y, g_fog_col.z);""")

# ---------------- 6. time + job systems before render ----------------
rep("""// ------------------------------------------------------------------ render
""",
"""// ------------------------------------------------------------------ tempo

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

  V3 sun = mixv(v3(1.00f, 0.92f, 0.79f), v3(1.15f, 0.52f, 0.22f), dusk * 0.9f);
  sun = mixv(v3(0.13f, 0.16f, 0.24f), sun, day);
  g_sun_col = sun;

  g_amb_sky = mixv(mixv(v3(0.055f, 0.07f, 0.12f), v3(0.32f, 0.36f, 0.44f), day),
                   v3(0.30f, 0.20f, 0.15f), dusk * 0.35f);
  g_amb_gnd = mixv(mixv(v3(0.030f, 0.035f, 0.050f), v3(0.17f, 0.16f, 0.15f), day),
                   v3(0.20f, 0.13f, 0.10f), dusk * 0.4f);
  g_fog_col = mixv(mixv(v3(0.045f, 0.065f, 0.105f), v3(0.72f, 0.78f, 0.85f), day),
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
""")

# ---------------- 7. water uniforms ----------------
rep("""  glUniform3f(g_water.sunDir, 0.66f, 0.55f, -0.51f);
  glUniform3f(g_water.sunCol, 1.12f, 1.02f, 0.88f);
  glUniform3f(g_water.fogCol, 0.90f, 0.67f, 0.53f);""",
"""  glUniform3f(g_water.sunDir, g_sun_dir.x, g_sun_dir.y, g_sun_dir.z);
  glUniform3f(g_water.sunCol, g_sun_col.x * 1.12f, g_sun_col.y * 1.11f, g_sun_col.z * 1.11f);
  glUniform3f(g_water.fogCol, g_fog_col.x, g_fog_col.y, g_fog_col.z);
  glUniform3f(g_water.skyMul, g_sky_mul.x, g_sky_mul.y, g_sky_mul.z);""")

# ---------------- 8. sky uniforms ----------------
rep("""  glUniform3f(g_sky.sunDir, 0.66f, 0.55f, -0.51f);
  glUniform3f(g_sky.sunCol, 1.12f, 1.02f, 0.88f);
  glUniform1f(g_sky.time, time);""",
"""  glUniform3f(g_sky.sunDir, g_sun_dir.x, g_sun_dir.y, g_sun_dir.z);
  glUniform3f(g_sky.sunCol, g_sun_col.x * 1.12f, g_sun_col.y * 1.11f, g_sun_col.z * 1.11f);
  glUniform3f(g_sky.horizon, g_sky_hor.x, g_sky_hor.y, g_sky_hor.z);
  glUniform3f(g_sky.zenith, g_sky_zen.x, g_sky_zen.y, g_sky_zen.z);
  glUniform1f(g_sky.day, g_day);
  glUniform1f(g_sky.time, time);""")

# ---------------- 9. frame hooks ----------------
rep("""  g_time += dt;

  // input""",
"""  g_time += dt;
  update_time_of_day(dt);

  // input""")

rep("""  V3 eye, target;
  update_camera(dt, eye, target);""",
"""  update_job(dt);

  V3 eye, target;
  update_camera(dt, eye, target);""")

rep("""  glClearColor(0.88f, 0.64f, 0.50f, 1.0f);""",
"""  glClearColor(g_fog_col.x, g_fog_col.y, g_fog_col.z, 1.0f);""")

rep("""  render_cop_lights(eye, vp);
  render_player(eye, vp);""",
"""  render_cop_lights(eye, vp);
  render_beacons(eye, vp);
  render_player(eye, vp);""")

# ---------------- 10. HUD prompt + calls ----------------
rep("""    if (g_in_car) {
      prompt = "E \u00b7 Sair do carro";
    } else if (near_car < 4.4f) {
      prompt = "E \u00b7 Entrar no carro";
    } else {
      prompt = "Segue o mapa at\u00e9 \u00e0 Marginal";
    }""",
"""    if (g_in_car) {
      prompt = "E \u00b7 Sair do carro";
    } else if (near_car < 4.4f) {
      prompt = "E \u00b7 Entrar no carro";
    } else if (g_job.stage == JOB_PICK) {
      prompt = "Recolhe a encomenda";
    } else if (g_job.stage == JOB_DROP) {
      prompt = "Entrega a encomenda";
    } else if (g_job.cooldown <= 0.0f) {
      prompt = "F \u00b7 Trabalho de estafeta";
    } else {
      prompt = "Segue o mapa at\u00e9 \u00e0 Marginal";
    }""")

rep("""    mw_hud(kmh, g_health, g_cash, g_wanted, g_in_car ? 1 : 0,
           g_in_car ? g_car.pos.x : g_player.pos.x,
           g_in_car ? g_car.pos.z : g_player.pos.z,
           g_in_car ? g_car.yaw : g_player.yaw, prompt);""",
"""    mw_hud(kmh, g_health, g_cash, g_wanted, g_in_car ? 1 : 0,
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
          snprintf(js, sizeof(js), "%d m \u00b7 %d:%02d", (int)length(g_job.target - at),
                   tl / 60, tl % 60);
        }
      } else if (g_job.stage != JOB_IDLE) {
        jt = g_job.stage == JOB_PICK ? "Recolhe a encomenda" : "Entrega a encomenda";
        snprintf(js, sizeof(js), "%d m \u00b7 %d:%02d", (int)length(g_job.target - at), tl / 60,
                 tl % 60);
      } else if (g_job.cooldown > 0.0f) {
        jt = "Estafeta";
        snprintf(js, sizeof(js), "Pronta em %d s", (int)std::ceil(g_job.cooldown));
      } else {
        jt = "Estafeta";
        snprintf(js, sizeof(js), "F \u00b7 iniciar trabalho");
      }
      mw_job_js(g_job.stage, jt, js);
      int hh = (int)(g_tod * 24.0f);
      int mm = (int)((g_tod * 24.0f - (float)hh) * 60.0f);
      mw_clock_js(hh, mm, g_night > 0.55f ? 1 : 0);
    }""")

# ---------------- 11. init beacons ----------------
rep("""  g_cop_light_mesh = make_cop_light_mesh();""",
"""  g_cop_light_mesh = make_cop_light_mesh();
  g_beacon_pick = make_beacon_mesh(tint3(0.16f, 0.80f, 1.00f), tint3(0.06f, 0.30f, 0.38f));
  g_beacon_drop = make_beacon_mesh(tint3(1.00f, 0.70f, 0.16f), tint3(0.38f, 0.26f, 0.06f));""")

# ---------------- 12. debug time export ----------------
rep("""EMSCRIPTEN_KEEPALIVE void mw_debug_top(int on) { g_debug_top = on; }""",
"""EMSCRIPTEN_KEEPALIVE void mw_debug_top(int on) { g_debug_top = on; }

EMSCRIPTEN_KEEPALIVE void mw_debug_time(float tod) {
  g_tod = clampf(tod, 0.0f, 0.999f);
  g_want_time_jump = 0;
}""")

io.open(p, 'w', encoding='utf-8', newline='\n').write(s)
print('patched', len(orig), '->', len(s))
