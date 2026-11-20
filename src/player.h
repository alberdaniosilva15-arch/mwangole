#pragma once
#include "car.h"
#include "gfx.h"
#include "math.h"

struct PlayerMeshes {
  Mesh body, arm, leg;
};

struct PedLook {
  uint32_t shirt, shirtDark, jeans, jeansDark, accent, skin;
};

inline PedLook player_look() {
  return {tint3(0.80f, 0.82f, 0.78f), tint3(0.66f, 0.68f, 0.64f), tint3(0.17f, 0.22f, 0.32f),
          tint3(0.14f, 0.18f, 0.27f), tint3(0.86f, 0.36f, 0.12f), tint3(0.54f, 0.34f, 0.21f)};
}

inline PedLook street_look(int i) {
  switch (i & 3) {
    case 0:
      return {tint3(0.78f, 0.26f, 0.19f), tint3(0.62f, 0.20f, 0.15f), tint3(0.15f, 0.17f, 0.22f),
              tint3(0.12f, 0.13f, 0.18f), tint3(0.90f, 0.78f, 0.30f), tint3(0.44f, 0.27f, 0.16f)};
    case 1:
      return {tint3(0.88f, 0.71f, 0.20f), tint3(0.72f, 0.57f, 0.15f), tint3(0.30f, 0.31f, 0.33f),
              tint3(0.24f, 0.25f, 0.27f), tint3(0.20f, 0.42f, 0.72f), tint3(0.60f, 0.41f, 0.27f)};
    case 2:
      return {tint3(0.20f, 0.50f, 0.46f), tint3(0.15f, 0.40f, 0.37f), tint3(0.20f, 0.26f, 0.36f),
              tint3(0.16f, 0.21f, 0.30f), tint3(0.82f, 0.44f, 0.18f), tint3(0.36f, 0.22f, 0.14f)};
    default:
      return {tint3(0.30f, 0.33f, 0.52f), tint3(0.23f, 0.26f, 0.42f), tint3(0.52f, 0.46f, 0.34f),
              tint3(0.44f, 0.39f, 0.29f), tint3(0.70f, 0.28f, 0.22f), tint3(0.66f, 0.46f, 0.31f)};
  }
}

inline Mesh make_player_body(const PedLook& L) {
  Verts v;
  uint32_t jeans = L.jeans;
  uint32_t shirt = L.shirt;
  uint32_t shirtDark = L.shirtDark;
  uint32_t accent = L.accent;
  uint32_t skin = L.skin;
  uint32_t skinDark = tint3((L.skin & 255) * (0.88f / 255.0f),
                            ((L.skin >> 8) & 255) * (0.88f / 255.0f),
                            ((L.skin >> 16) & 255) * (0.88f / 255.0f));
  uint32_t hair = tint3(0.07f, 0.055f, 0.05f);

  // hips + belt
  cbox(v, -0.155f, -0.095f, 0.155f, 0.095f, 0.88f, 1.045f, L_WHITE, jeans);
  cbox(v, -0.16f, -0.10f, 0.16f, 0.10f, 1.03f, 1.07f, L_WHITE, accent);

  // tapered torso: waist -> chest
  V3 w0 = {-0.155f, 1.07f, 0.095f}, w1 = {0.155f, 1.07f, 0.095f};
  V3 w2 = {0.155f, 1.07f, -0.095f}, w3 = {-0.155f, 1.07f, -0.095f};
  V3 c0 = {-0.20f, 1.31f, 0.11f}, c1 = {0.20f, 1.31f, 0.11f};
  V3 c2 = {0.20f, 1.31f, -0.11f}, c3 = {-0.20f, 1.31f, -0.11f};
  cquad(v, w0, w1, c1, c0, {0, 0.15f, 1}, L_WHITE, shirt);
  cquad(v, w3, c3, c2, w2, {0, 0.15f, -1}, L_WHITE, shirt);
  cquad(v, w0, c0, c3, w3, {-1, 0.15f, 0}, L_WHITE, shirt);
  cquad(v, w1, w2, c2, c1, {1, 0.15f, 0}, L_WHITE, shirt);
  // placket
  cbox(v, -0.012f, 0.109f, 0.012f, 0.116f, 1.08f, 1.29f, L_WHITE, shirtDark);
  // shoulders
  cbox(v, -0.218f, -0.112f, 0.218f, 0.112f, 1.30f, 1.38f, L_WHITE, shirt);

  // neck + head
  cbox(v, -0.048f, -0.045f, 0.048f, 0.045f, 1.36f, 1.432f, L_WHITE, skinDark);
  cbox(v, -0.085f, -0.09f, 0.085f, 0.09f, 1.43f, 1.62f, L_WHITE, skin);
  // nose, ears, eyes
  cbox(v, -0.018f, 0.088f, 0.018f, 0.107f, 1.502f, 1.542f, L_WHITE, skinDark);
  cbox(v, -0.097f, -0.018f, -0.085f, 0.020f, 1.488f, 1.532f, L_WHITE, skin);
  cbox(v, 0.085f, -0.018f, 0.097f, 0.020f, 1.488f, 1.532f, L_WHITE, skin);
  cbox(v, -0.058f, 0.089f, -0.024f, 0.095f, 1.546f, 1.564f, L_WHITE, hair);
  cbox(v, 0.024f, 0.089f, 0.058f, 0.095f, 1.546f, 1.564f, L_WHITE, hair);
  // brows
  cbox(v, -0.058f, 0.089f, -0.020f, 0.093f, 1.570f, 1.578f, L_WHITE, hair);
  cbox(v, 0.020f, 0.089f, 0.058f, 0.093f, 1.570f, 1.578f, L_WHITE, hair);
  // hair: top, back, short sides
  cbox(v, -0.092f, -0.10f, 0.092f, 0.076f, 1.615f, 1.67f, L_WHITE, hair);
  cbox(v, -0.092f, -0.10f, 0.092f, -0.075f, 1.44f, 1.62f, L_WHITE, hair);
  cbox(v, -0.093f, -0.075f, -0.084f, 0.070f, 1.525f, 1.618f, L_WHITE, hair);
  cbox(v, 0.084f, -0.075f, 0.093f, 0.070f, 1.525f, 1.618f, L_WHITE, hair);
  return mesh_upload(v);
}

inline Mesh make_player_arm(const PedLook& L) {
  Verts v;
  uint32_t shirt = L.shirt;
  uint32_t skin = L.skin;
  uint32_t hand = tint3((L.skin & 255) * (0.90f / 255.0f), ((L.skin >> 8) & 255) * (0.90f / 255.0f),
                        ((L.skin >> 16) & 255) * (0.90f / 255.0f));
  cbox(v, -0.066f, -0.066f, 0.066f, 0.066f, -0.145f, 0.03f, L_WHITE, shirt);
  cbox(v, -0.054f, -0.052f, 0.054f, 0.052f, -0.28f, -0.145f, L_WHITE, skin);
  cbox(v, -0.047f, -0.046f, 0.047f, 0.046f, -0.45f, -0.28f, L_WHITE, skin);
  cbox(v, -0.049f, -0.042f, 0.049f, 0.042f, -0.53f, -0.45f, L_WHITE, hand);
  return mesh_upload(v);
}

inline Mesh make_player_leg(const PedLook& L) {
  Verts v;
  uint32_t jeans = L.jeans;
  uint32_t jeansDark = L.jeansDark;
  uint32_t shoe = tint3(0.10f, 0.10f, 0.12f);
  uint32_t sole = tint3(0.20f, 0.20f, 0.22f);
  cbox(v, -0.082f, -0.085f, 0.082f, 0.085f, -0.20f, 0.03f, L_WHITE, jeans);
  cbox(v, -0.074f, -0.078f, 0.074f, 0.078f, -0.44f, -0.20f, L_WHITE, jeans);
  cbox(v, -0.061f, -0.066f, 0.061f, 0.066f, -0.83f, -0.44f, L_WHITE, jeansDark);
  cbox(v, -0.072f, -0.088f, 0.072f, 0.16f, -0.90f, -0.83f, L_WHITE, shoe);
  cbox(v, -0.073f, -0.089f, 0.073f, 0.161f, -0.92f, -0.90f, L_WHITE, sole);
  return mesh_upload(v);
}

inline PlayerMeshes make_player_meshes(const PedLook& L) {
  PlayerMeshes pm;
  pm.body = make_player_body(L);
  pm.arm = make_player_arm(L);
  pm.leg = make_player_leg(L);
  return pm;
}

// Personagens do GLB, cozidas em poses fixas: nao ha skinning no runtime, mas
// alternar entre 4 poses do ciclo de caminhar le-se como animacao.
inline void draw_player_poses(WorldProg& prog, const Mesh walk[4], const Mesh& idle, const Walker& w) {
  float amp = clampf(w.speed / 3.9f, 0.0f, 1.3f);
  float bob = std::fabs(std::sin(w.anim)) * 0.030f * amp;
  Mat4 m = mat_mul(mat_translate(v3(w.pos.x, w.pos.y + bob, w.pos.z)), mat_rot_y(w.yaw));
  glUniformMatrix4fv(prog.model, 1, GL_FALSE, m.m);
  const Mesh& mesh = (amp > 0.06f) ? walk[((int)(w.anim * 1.55f) % 4 + 4) % 4] : idle;
  if (!mesh.count) return;
  glBindVertexArray(mesh.vao);
  glDrawElements(GL_TRIANGLES, mesh.count, GL_UNSIGNED_INT, nullptr);
}

inline void draw_player(WorldProg& prog, const PlayerMeshes& pm, const Walker& w) {
  float amp = clampf(w.speed / 3.9f, 0.0f, 1.3f);
  float step = std::min(amp, 1.0f);
  float swing = std::sin(w.anim) * 0.62f * step;
  float bob = std::fabs(std::sin(w.anim)) * 0.032f * step;
  float twist = std::sin(w.anim) * 0.10f * step;

  Mat4 base = mat_mul(mat_translate(v3(w.pos.x, w.pos.y + bob, w.pos.z)), mat_rot_y(w.yaw));
  Mat4 torso = mat_mul(base, mat_rot_y(twist));
  auto draw = [&](const Mesh& m, const Mat4& mm) {
    glUniformMatrix4fv(prog.model, 1, GL_FALSE, mm.m);
    glBindVertexArray(m.vao);
    glDrawElements(GL_TRIANGLES, m.count, GL_UNSIGNED_INT, nullptr);
  };
  draw(pm.body, torso);
  for (int s = 0; s < 2; ++s) {
    float sx = s ? 1.0f : -1.0f;
    Mat4 hip = mat_mul(base, mat_mul(mat_translate(v3(0.095f * sx, 0.92f, 0.0f)),
                                     mat_rot_x(swing * sx)));
    draw(pm.leg, hip);
    Mat4 shoulder = mat_mul(torso, mat_mul(mat_translate(v3(0.258f * sx, 1.335f, 0.0f)),
                                           mat_rot_x(-swing * 0.85f * sx)));
    draw(pm.arm, shoulder);
  }
}
