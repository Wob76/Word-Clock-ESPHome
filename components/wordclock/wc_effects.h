#pragma once
// ---------------------------------------------------------------------------
// Animations for the ESPHome word clock - line-by-line ports of
// Crossbow78/word_clock controller/animations/*.py
//
//   Fire, Plasma, Matrix Rain, Ripple, Spiral, Lava Lamp, Bouncing Balls,
//   Game of Life, Lightning, Flags, Sand, Flow Field, Weather, Presence,
//   Heart (the ESP32 hourly GIF) and the Word Clock itself, which can play
//   any of them between time updates and on the hour
//
// Every effect keeps the constants, palettes and state machines of its Python
// module.  Timing: each Python module advances once per FRAME_DELAY; here a
// Stepper converts real elapsed time (scaled by the "Effect Speed" slider,
// 50 = original pace) into the right number of those steps, so the YAML
// update_interval only affects smoothness, never speed.
//
//   template<typename L> void fx_xxx(L &it, bool init, float speed)
// ---------------------------------------------------------------------------
#include "wordclock.h"

namespace wc {

// =========================================================== Perlin noise ==
// Ken Perlin's improved noise; stands in for the `noise` package's
// pnoise2/pnoise3 (same family, same octave maths, not bit-identical).
constexpr uint8_t PERM[256] = {
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103, 30,  69,  142,
    8,   99,  37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,  197, 62,  94,  252, 219, 203,
    117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174, 20,  125, 136, 171, 168, 68,  175, 74,  165,
    71,  134, 139, 48,  27,  166, 77,  146, 158, 231, 83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,
    55,  46,  245, 40,  244, 102, 143, 54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,
    18,  169, 200, 196, 135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250,
    124, 123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,  182, 189,
    28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101, 155, 167, 43,  172, 9,
    129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185, 112, 104, 218, 246, 97,  228, 251, 34,
    242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,  51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,
    181, 199, 106, 157, 184, 84,  204, 176, 115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114,
    67,  29,  24,  72,  243, 141, 128, 195, 78,  66,  215, 61,  156, 180};
inline int pm(int i) { return PERM[i & 255]; }
inline float nz_fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
inline float nz_lerp(float t, float a, float b) { return a + t * (b - a); }
inline float nz_grad(int h, float x, float y, float z) {
  h &= 15;
  float u = h < 8 ? x : y;
  float v = h < 4 ? y : ((h == 12 || h == 14) ? x : z);
  return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}
inline float noise3(float x, float y, float z) {
  float fx = floorf(x), fy = floorf(y), fz = floorf(z);
  int X = (int)fx & 255, Y = (int)fy & 255, Z = (int)fz & 255;
  x -= fx;
  y -= fy;
  z -= fz;
  float u = nz_fade(x), v = nz_fade(y), w = nz_fade(z);
  int A = pm(X) + Y, AA = pm(A) + Z, AB = pm(A + 1) + Z;
  int B = pm(X + 1) + Y, BA = pm(B) + Z, BB = pm(B + 1) + Z;
  return nz_lerp(w,
                 nz_lerp(v, nz_lerp(u, nz_grad(pm(AA), x, y, z), nz_grad(pm(BA), x - 1, y, z)),
                         nz_lerp(u, nz_grad(pm(AB), x, y - 1, z), nz_grad(pm(BB), x - 1, y - 1, z))),
                 nz_lerp(v, nz_lerp(u, nz_grad(pm(AA + 1), x, y, z - 1), nz_grad(pm(BA + 1), x - 1, y, z - 1)),
                         nz_lerp(u, nz_grad(pm(AB + 1), x, y - 1, z - 1), nz_grad(pm(BB + 1), x - 1, y - 1, z - 1))));
}
// Octave sum normalised by total amplitude, as in the noise package
inline float pnoise3(float x, float y, float z, int octaves, float persistence, float lacunarity) {
  float freq = 1.0f, amp = 1.0f, mx = 1.0f, total = noise3(x, y, z);
  for (int i = 1; i < octaves; i++) {
    freq *= lacunarity;
    amp *= persistence;
    mx += amp;
    total += noise3(x * freq, y * freq, z * freq) * amp;
  }
  return total / mx;
}
inline float pnoise2(float x, float y, int octaves, float persistence, float lacunarity) {
  return pnoise3(x, y, 0.0f, octaves, persistence, lacunarity);
}

// ==================================================================== Fire ==
struct PalStop {
  float t;
  uint8_t r, g, b;
};
constexpr PalStop FIRE_PALETTE[8] = {{0.00f, 0, 0, 0},       {0.21f, 60, 0, 0},      {0.40f, 160, 10, 0},
                                     {0.58f, 220, 60, 0},    {0.72f, 255, 130, 0},   {0.84f, 255, 200, 20},
                                     {0.95f, 255, 200, 160}, {1.00f, 255, 255, 235}};

inline Color fire_palette(float x) {
  x = clamp01(x);
  for (int i = 0; i < 7; i++) {
    const PalStop &a = FIRE_PALETTE[i], &b = FIRE_PALETTE[i + 1];
    if (a.t <= x && x <= b.t) {
      float t = (x - a.t) / (b.t - a.t);
      return Color((uint8_t)(a.r + (b.r - a.r) * t), (uint8_t)(a.g + (b.g - a.g) * t), (uint8_t)(a.b + (b.b - a.b) * t));
    }
  }
  return Color(255, 255, 235);
}

template<typename L> void fx_fire(L &it, bool init, float speed) {
  static float t = 0;
  static Stepper sp;
  int n = sp.steps(init, speed, 0.05f);
  if (init) t = 0;
  if (!n) return;
  t += 0.08f * (n - 1);
  for (int col = 0; col < W; col++)
    for (int row = 0; row < H; row++) {
      float raw = pnoise2(col * 0.1f, t + row * 0.1f * 0.5f, 2, 1.5f, 2.0f);
      float in = (raw + 0.7f) / 1.4f;
      in = powf(std::max(0.0f, in), 2.8f);
      float heat = row / (float)(H - 1);
      in = in * 0.5f + heat * 0.6f;
      put(it, col, row, fire_palette(in));
    }
  t += 0.08f;
}

// ================================================================== Plasma ==
template<typename L> void fx_plasma(L &it, bool init, float speed) {
  static float t = 0;
  static Stepper sp;
  int n = sp.steps(init, speed, 0.04f);
  if (init) t = 0;
  if (!n) return;
  t += 0.05f * (n - 1);
  const float cx = (W - 1) / 2.0f, cy = (H - 1) / 2.0f;
  const float maxd = sqrtf(cx * cx + cy * cy);
  for (int col = 0; col < W; col++)
    for (int row = 0; row < H; row++) {
      float nx = col / (float)W * PI_F * 2.0f, ny = row / (float)H * PI_F * 2.0f;
      float dist = sqrtf((col - cx) * (col - cx) + (row - cy) * (row - cy));
      float nd = dist / maxd * PI_F;
      float v1 = sinf(nx * 1.0f + t);
      float v2 = sinf(ny * 1.2f + t * 0.7f);
      float v3 = sinf((nx * 0.6f + ny * 0.6f) + t * 0.5f);
      float v4 = sinf(nd * 0.8f - t * 0.8f);
      float hue = ((v1 + v2 + v3 + v4) / 4.0f + 1.0f) / 2.0f;
      put(it, col, row, hsv(hue, 1.0f, 1.0f));
    }
  t += 0.05f;
}

// =============================================================== Matrix Rain ==
template<typename L> void fx_matrix(L &it, bool init, float speed) {
  constexpr int TRAIL = 6;
  constexpr float FALLOFF[TRAIL] = {0.7f, 0.45f, 0.28f, 0.15f, 0.07f, 0.03f};
  static int row[W];
  static Stepper sp;
  int n = sp.steps(init, speed, 0.06f);
  if (init) {
    seed(esphome::millis());
    for (int c = 0; c < W; c++) row[c] = -rnd_between(0, H + TRAIL);
  }
  if (!n) return;
  int buf[W][H][3];
  for (int k = 0; k < n; k++) {
    memset(buf, 0, sizeof buf);
    for (int c = 0; c < W; c++) {
      if (row[c] >= 0 && row[c] < H) {
        buf[c][row[c]][0] = 180;
        buf[c][row[c]][1] = 255;
        buf[c][row[c]][2] = 180;
      }
      for (int t = 0; t < TRAIL; t++) {
        int tr = row[c] - 1 - t;
        if (tr >= 0 && tr < H) buf[c][tr][1] = std::min(255, buf[c][tr][1] + (int)(255 * FALLOFF[t]));
      }
      row[c]++;
      if (row[c] - TRAIL >= H) row[c] = -rnd_between(2, 20);
    }
  }
  for (int c = 0; c < W; c++)
    for (int r = 0; r < H; r++) put(it, c, r, rgb(buf[c][r][0], buf[c][r][1], buf[c][r][2]));
}

// ================================================================== Ripple ==
template<typename L> void fx_ripple(L &it, bool init, float speed) {
  struct R {
    float cx, cy, radius;
    uint8_t color;
  };
  constexpr uint8_t COL[6][3] = {{0, 180, 255}, {0, 255, 120}, {180, 0, 255}, {255, 100, 0}, {255, 0, 100}, {0, 120, 255}};
  constexpr float RING = 1.2f, SPEED = 0.4f;
  const float MAXR = sqrtf((float)(W * W + H * H));
  static R rip[4];
  static int cnt = 0;
  static Stepper sp;
  auto spawn = [&]() {
    rip[cnt++] = {rnd_range(0, W - 1), rnd_range(0, H - 1), 0.0f, (uint8_t)rndi(6)};
  };
  int n = sp.steps(init, speed, 0.05f);
  if (init) {
    seed(esphome::millis());
    cnt = 0;
    spawn();
  }
  for (int k = 0; k < n; k++) {
    if (cnt < 4 && rndf() < 0.08f) spawn();
    if (k == n - 1) {
      for (int c = 0; c < W; c++)
        for (int r = 0; r < H; r++) {
          float R_ = 0, G = 0, B = 0;
          for (int i = 0; i < cnt; i++) {
            float dist = sqrtf((c - rip[i].cx) * (c - rip[i].cx) + (r - rip[i].cy) * (r - rip[i].cy));
            float delta = fabsf(dist - rip[i].radius);
            if (delta > RING) continue;
            float br = (1.0f + cosf(PI_F * delta / RING)) / 2.0f;
            br *= std::max(0.0f, 1.0f - rip[i].radius / MAXR);
            R_ += COL[rip[i].color][0] * br;
            G += COL[rip[i].color][1] * br;
            B += COL[rip[i].color][2] * br;
          }
          put(it, c, r, rgb((int)R_, (int)G, (int)B));
        }
    }
    int m = 0;
    for (int i = 0; i < cnt; i++) {
      rip[i].radius += SPEED;
      if (!(rip[i].radius - RING > MAXR)) rip[m++] = rip[i];
    }
    cnt = m;
  }
}

// ================================================================== Spiral ==
template<typename L> void fx_spiral(L &it, bool init, float speed) {
  constexpr int ARMS = 3;
  constexpr float ROT = 0.05f, ARM_W = 0.4f, DECAY = 0.2f, HUE_SPEED = 0.003f, TIP = 1.5f;
  static float buf[H][W][3];
  static float sweep = 0, hue = 0;
  static float ang[H][W], dst[H][W];
  static bool tables = false;
  static Stepper sp;
  const float cx = (W - 1) / 2.0f, cy = (H - 1) / 2.0f, maxd = sqrtf(cx * cx + cy * cy);
  if (!tables) {
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++) {
        ang[r][c] = atan2f(r - cy, c - cx);
        dst[r][c] = sqrtf((c - cx) * (c - cx) + (r - cy) * (r - cy));
      }
    tables = true;
  }
  int n = sp.steps(init, speed, 0.05f);
  if (init) {
    sweep = hue = 0;
    memset(buf, 0, sizeof buf);
  }
  for (int k = 0; k < n; k++) {
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++)
        for (int i = 0; i < 3; i++) buf[r][c][i] *= (1.0f - DECAY);
    Color hc = hsv(hue, 1.0f, 1.0f);
    float col[3] = {hc.r / 255.0f, hc.g / 255.0f, hc.b / 255.0f};
    for (int arm = 0; arm < ARMS; arm++) {
      float a = fmodf(sweep + arm * PI_F * 2.0f / ARMS, PI_F * 2.0f);
      for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++) {
          float d = fmodf(ang[r][c] - a, PI_F * 2.0f);
          if (d < 0) d += PI_F * 2.0f;
          if (d > PI_F) d -= PI_F * 2.0f;
          d = fabsf(d);
          if (d < ARM_W) {
            float af = (1.0f + cosf(PI_F * d / ARM_W)) / 2.0f;
            float df = dst[r][c] / maxd;
            float strength = af * df * (1.0f + (TIP - 1.0f) * df);
            for (int i = 0; i < 3; i++) buf[r][c][i] = std::min(1.0f, buf[r][c][i] + col[i] * strength);
          }
        }
    }
    sweep = fmodf(sweep + ROT, PI_F * 2.0f);
    hue = fmodf(hue + HUE_SPEED, 1.0f);
  }
  if (!n) return;
  for (int r = 0; r < H; r++)
    for (int c = 0; c < W; c++)
      put(it, c, r, Color((uint8_t)(buf[r][c][0] * 255), (uint8_t)(buf[r][c][1] * 255), (uint8_t)(buf[r][c][2] * 255)));
}

// ================================================================ Lava Lamp ==
template<typename L> void fx_lavalamp(L &it, bool init, float speed) {
  struct Blob {
    float cxb, cyb, ax, ay, fx, fy, px, py;
    uint8_t color;
  };
  constexpr uint8_t COL[7][3] = {{255, 20, 0},  {255, 80, 0},  {220, 0, 100}, {160, 0, 255},
                                 {255, 140, 0}, {200, 0, 60},  {255, 40, 120}};
  constexpr float RADIUS = 2.2f, TS = 0.015f;
  static Blob b[5];
  static float t = 0;
  static Stepper sp;
  int n = sp.steps(init, speed, 0.05f);
  if (init) {
    seed(esphome::millis());
    t = 0;
    for (auto &p : b)
      p = {rnd_range(1, W - 2), rnd_range(1, H - 2), rnd_range(1.5f, W / 2.0f - 1),  rnd_range(1.0f, H / 2.0f - 1),
           rnd_range(0.3f, 1.0f), rnd_range(0.3f, 1.0f), rnd_range(0, PI_F * 2), rnd_range(0, PI_F * 2), (uint8_t)rndi(7)};
  }
  if (!n) return;
  t += TS * (n - 1);
  float cxs[5], cys[5];
  for (int i = 0; i < 5; i++) {
    cxs[i] = b[i].cxb + b[i].ax * sinf(t * b[i].fx + b[i].px);
    cys[i] = b[i].cyb + b[i].ay * sinf(t * b[i].fy + b[i].py);
  }
  for (int c = 0; c < W; c++)
    for (int r = 0; r < H; r++) {
      float R_ = 4, G = 0, B = 8;
      for (int i = 0; i < 5; i++) {
        float d = ((c - cxs[i]) / RADIUS) * ((c - cxs[i]) / RADIUS) + ((r - cys[i]) / RADIUS) * ((r - cys[i]) / RADIUS);
        float bri = expf(-d);
        R_ += COL[b[i].color][0] * bri;
        G += COL[b[i].color][1] * bri;
        B += COL[b[i].color][2] * bri;
      }
      put(it, c, r, rgb((int)R_, (int)G, (int)B));
    }
  t += TS;
}

// ========================================================= Bouncing Balls ==
template<typename L> void fx_balls(L &it, bool init, float speed) {
  constexpr int NB = 6, TRAIL = 4;
  constexpr float FALLOFF[TRAIL] = {1.0f, 0.45f, 0.18f, 0.06f};
  constexpr uint8_t COL[10][3] = {{255, 40, 40}, {255, 160, 0}, {220, 220, 0}, {0, 220, 0},   {0, 180, 255},
                                  {60, 60, 255}, {200, 0, 255}, {255, 0, 160}, {0, 220, 180}, {255, 220, 0}};
  struct Ball {
    float x, y, vx, vy;
    uint8_t color;
    float tx[TRAIL], ty[TRAIL];
    int tn;
  };
  static Ball b[NB];
  static Stepper sp;
  int n = sp.steps(init, speed, 0.05f);
  if (init) {
    seed(esphome::millis());
    for (auto &p : b) {
      float angle = rnd_range(0, PI_F * 2), spd = rnd_range(0.4f, 0.9f);
      p = {rnd_range(0, W - 1), rnd_range(0, H - 1), cosf(angle) * spd, sinf(angle) * spd, (uint8_t)rndi(10), {}, {}, 0};
    }
  }
  int buf[H][W][3];
  for (int k = 0; k < n; k++) {
    memset(buf, 0, sizeof buf);
    for (auto &p : b) {
      // step
      for (int i = TRAIL - 1; i > 0; i--) {
        p.tx[i] = p.tx[i - 1];
        p.ty[i] = p.ty[i - 1];
      }
      p.tx[0] = p.x;
      p.ty[0] = p.y;
      if (p.tn < TRAIL) p.tn++;
      p.x += p.vx;
      p.y += p.vy;
      if (p.x < 0) { p.x = -p.x; p.vx = fabsf(p.vx); }
      else if (p.x > W - 1) { p.x = 2 * (W - 1) - p.x; p.vx = -fabsf(p.vx); }
      if (p.y < 0) { p.y = -p.y; p.vy = fabsf(p.vy); }
      else if (p.y > H - 1) { p.y = 2 * (H - 1) - p.y; p.vy = -fabsf(p.vy); }
      // draw (bilinear paint)
      auto paint = [&](float fx, float fy, float factor) {
        int x0 = (int)fx, y0 = (int)fy;
        float dx = fx - x0, dy = fy - y0;
        for (int dc = 0; dc < 2; dc++)
          for (int dr = 0; dr < 2; dr++) {
            int c = x0 + dc, r = y0 + dr;
            if (c < 0 || c >= W || r < 0 || r >= H) continue;
            float w = (dc == 0 ? 1 - dx : dx) * (dr == 0 ? 1 - dy : dy) * factor;
            for (int i = 0; i < 3; i++) buf[r][c][i] = std::min(255, buf[r][c][i] + (int)(COL[p.color][i] * w));
          }
      };
      paint(p.x, p.y, 1.0f);
      for (int i = 0; i < p.tn; i++) paint(p.tx[i], p.ty[i], FALLOFF[i]);
    }
  }
  if (!n) return;
  for (int r = 0; r < H; r++)
    for (int c = 0; c < W; c++) put(it, c, r, rgb(buf[r][c][0], buf[r][c][1], buf[r][c][2]));
}

// ============================================================ Game of Life ==
template<typename L> void fx_life(L &it, bool init, float speed) {
  constexpr int MAX_AGE = 8, STAG_LIMIT = 8;
  constexpr uint8_t AGE_COL[8][3] = {{180, 255, 180}, {80, 255, 80}, {0, 220, 0}, {0, 180, 0},
                                     {0, 140, 0},     {0, 100, 0},   {0, 60, 0},  {0, 30, 0}};
  static uint8_t grid[H][W], prev[H][W];
  static bool have_prev = false;
  static int stag = 0;
  static Stepper sp;
  auto make_grid = [&]() {
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++) grid[r][c] = rndf() < 0.4f ? 1 : 0;
  };
  int n = sp.steps(init, speed, 0.15f);
  if (init) {
    seed(esphome::millis());
    make_grid();
    have_prev = false;
    stag = 0;
  }
  for (int k = 0; k < n; k++) {
    if (k == n - 1)
      for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++) {
          int age = grid[r][c];
          put(it, c, r, age > 0 ? Color(AGE_COL[std::min(age - 1, 7)][0], AGE_COL[std::min(age - 1, 7)][1], AGE_COL[std::min(age - 1, 7)][2]) : Color(0, 0, 0));
        }
    uint8_t nx[H][W];
    memset(nx, 0, sizeof nx);
    int pop = 0;
    bool equal = true;
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++) {
        int cnt = 0;
        for (int dr = -1; dr <= 1; dr++)
          for (int dc = -1; dc <= 1; dc++)
            if (dr || dc) cnt += grid[(r + dr + H) % H][(c + dc + W) % W] > 0;
        bool alive = grid[r][c] > 0;
        if (alive && (cnt == 2 || cnt == 3)) nx[r][c] = std::min(grid[r][c] + 1, MAX_AGE);
        else if (!alive && cnt == 3) nx[r][c] = 1;
        pop += nx[r][c] > 0;
        if (nx[r][c] != grid[r][c]) equal = false;
      }
    if (have_prev && equal) stag++;
    else stag = 0;
    memcpy(prev, grid, sizeof grid);
    have_prev = true;
    memcpy(grid, nx, sizeof grid);
    if (pop == 0 || stag >= STAG_LIMIT) {
      make_grid();
      stag = 0;
    }
  }
}

// =============================================================== Lightning ==
template<typename L> void fx_lightning(L &it, bool init, float speed) {
  enum { IDLE, GROWING, FLASH, RECHARGE };
  constexpr float STEP_B[7] = {1.00f, 0.80f, 1.00f, 0.60f, 0.30f, 0.10f, 0.00f};
  constexpr float STEP_D[7] = {0.05f, 0.03f, 0.02f, 0.04f, 0.05f, 0.06f, 0.00f};
  static int state = IDLE;
  static float acc = 0, calm_until = 0, vt = 0, flash_acc = 0;
  static int grow_row = 0, flash_step = 0;
  static int bolt_c[H];
  static int branch_c[4], branch_r[4], branch_n = 0;
  static float bg[H][W];
  static Stepper sp;
  sp.steps(init, speed, 0.03f);
  float dt = sp.dt;
  if (init) {
    seed(esphome::millis());
    state = IDLE;
    acc = 0;
    vt = 0;
    calm_until = rnd_range(2.0f, 8.0f);
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++) bg[r][c] = rnd_range(0.15f, 0.35f);
  }
  vt += dt;
  acc += dt;
  const Color BG(50, 20, 0), HEAD(220, 240, 255), CORE(180, 210, 255), BRANCH(120, 160, 220);
  auto draw_bg = [&]() {
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++) {
        float target = rnd_range(0.15f - 0.15f * 0.5f, 0.15f + 0.15f);
        bg[r][c] += (target - bg[r][c]) * 0.10f;
        put(it, c, r, scl(BG, std::max(0.0f, bg[r][c])));
      }
  };
  auto draw_bolt = [&](int rows, float brightness) {
    for (int r = 0; r < rows; r++) put(it, bolt_c[r], r, scl(CORE, brightness));
    if (rows > 0) put(it, bolt_c[rows - 1], rows - 1, scl(HEAD, brightness));
    for (int i = 0; i < branch_n; i++)
      if (branch_r[i] < rows) put(it, branch_c[i], branch_r[i], scl(BRANCH, brightness * 0.7f));
  };
  if (state == IDLE) {
    draw_bg();
    if (vt >= calm_until) {
      int col = rnd_between(3, W - 4);
      for (int r = 0; r < H; r++) {
        bolt_c[r] = col;
        static const int CH[7] = {-1, -1, 0, 0, 0, 1, 1};
        col = std::max(0, std::min(W - 1, col + CH[rndi(7)]));
      }
      branch_n = 0;
      if (!(rndf() > 0.6f)) {
        int start = rnd_between(2, H - 3);
        int c = bolt_c[start], r = start, dir = rndi(2) ? 1 : -1, len = rnd_between(2, 3);
        for (int i = 0; i < len; i++) {
          c += dir;
          r += 1;
          if (!(c >= 0 && c < W && r >= 0 && r < H)) break;
          branch_c[branch_n] = c;
          branch_r[branch_n] = r;
          branch_n++;
        }
      }
      grow_row = 0;
      flash_step = 0;
      flash_acc = 0;
      state = GROWING;
      acc = 0;
    }
  } else if (state == GROWING) {
    if (acc < 0.03f) return;
    acc = 0;
    grow_row++;
    draw_bg();
    draw_bolt(grow_row, 1.0f);
    if (grow_row >= H) {
      state = FLASH;
      flash_step = 0;
      flash_acc = 0;
    }
  } else if (state == FLASH) {
    if (flash_step >= 7) {
      state = RECHARGE;
      acc = 0;
      return;
    }
    draw_bg();
    draw_bolt(H, STEP_B[flash_step]);
    flash_acc += dt;
    if (flash_acc >= STEP_D[flash_step]) {
      flash_acc = 0;
      flash_step++;
    }
  } else {  // RECHARGE
    draw_bg();
    if (acc >= 0.3f) {
      calm_until = vt + rnd_range(2.0f, 8.0f);
      state = IDLE;
      acc = 0;
    }
  }
}

// ==================================================================== Flags ==
struct Stripe {
  float f;
  uint8_t r, g, b;
};
struct FlagDef {
  bool vertical;
  int ns;
  Stripe s[5];
  bool disc;
  float dcx, dcy, dr;
  uint8_t dcol[3];
  bool cross;
  float rects[2][4];  // col_start, col_end, row_start, row_end (fractions)
};
constexpr float T3 = 1.0f / 3.0f;
constexpr FlagDef FLAGS[14] = {
    {false, 3, {{T3, 193, 0, 20}, {2 * T3, 255, 255, 255}, {1.0f, 15, 20, 159}}, false, 0, 0, 0, {}, false, {}},        // Netherlands
    {true, 3, {{T3, 10, 10, 10}, {2 * T3, 253, 218, 36}, {1.0f, 193, 0, 20}}, false, 0, 0, 0, {}, false, {}},           // Belgium
    {false, 2, {{0.5f, 0, 87, 183}, {1.0f, 255, 215, 0}}, false, 0, 0, 0, {}, false, {}},                              // Ukraine
    {true, 3, {{T3, 0, 35, 149}, {2 * T3, 255, 255, 255}, {1.0f, 227, 0, 20}}, false, 0, 0, 0, {}, false, {}},          // France
    {false, 1, {{1.0f, 255, 0, 20}}, false, 0, 0, 0, {}, true, {{0.35f, 0.65f, 0.20f, 0.80f}, {0.20f, 0.80f, 0.35f, 0.65f}}},  // Switzerland
    {true, 3, {{T3, 0, 43, 127}, {2 * T3, 252, 209, 22}, {1.0f, 206, 0, 20}}, false, 0, 0, 0, {}, false, {}},          // Romania
    {false, 1, {{1.0f, 255, 255, 255}}, true, 0.5f, 0.5f, 0.25f, {196, 0, 20}, false, {}},                              // Japan
    {false, 3, {{0.25f, 193, 0, 22}, {0.75f, 241, 189, 0}, {1.0f, 193, 0, 22}}, false, 0, 0, 0, {}, false, {}},        // Spain
    {false, 2, {{0.5f, 206, 0, 22}, {1.0f, 255, 255, 255}}, false, 0, 0, 0, {}, false, {}},                            // Indonesia
    {false, 3, {{T3, 10, 10, 10}, {2 * T3, 221, 0, 10}, {1.0f, 255, 206, 0}}, false, 0, 0, 0, {}, false, {}},          // Germany
    {true, 3, {{T3, 0, 170, 30}, {2 * T3, 255, 255, 255}, {1.0f, 206, 0, 15}}, false, 0, 0, 0, {}, false, {}},         // Italy
    {false, 3, {{T3, 255, 255, 255}, {2 * T3, 0, 57, 166}, {1.0f, 195, 0, 20}}, false, 0, 0, 0, {}, false, {}},        // Russia
    {false, 3, {{T3, 237, 11, 27}, {2 * T3, 255, 255, 255}, {1.0f, 237, 11, 27}}, false, 0, 0, 0, {}, false, {}},      // Austria
    {false, 5, {{2 / 11.0f, 195, 0, 18}, {4 / 11.0f, 255, 255, 255}, {7 / 11.0f, 25, 26, 160}, {9 / 11.0f, 255, 255, 255}, {1.0f, 195, 0, 18}}, false, 0, 0, 0, {}, false, {}},  // Thailand
};
constexpr int FLAG_COUNT = 14;

inline Color flag_color(const FlagDef &f, float nh, float nv) {
  float pos = f.vertical ? nv : nh;
  Color c(f.s[f.ns - 1].r, f.s[f.ns - 1].g, f.s[f.ns - 1].b);
  for (int i = 0; i < f.ns; i++)
    if (pos <= f.s[i].f) {
      c = Color(f.s[i].r, f.s[i].g, f.s[i].b);
      break;
    }
  if (f.disc) {
    float r = f.dr * std::min(W, H), dx = (nv - f.dcx) * W, dy = (nh - f.dcy) * H;
    if (sqrtf(dx * dx + dy * dy) <= r) c = Color(f.dcol[0], f.dcol[1], f.dcol[2]);
  }
  if (f.cross)
    for (int i = 0; i < 2; i++)
      if (f.rects[i][0] <= nv && nv <= f.rects[i][1] && f.rects[i][2] <= nh && nh <= f.rects[i][3]) {
        c = Color(255, 255, 255);
        break;
      }
  return c;
}

template<typename L> void fx_flags(L &it, bool init, float speed) {
  static float t = 0, flag_time = 0, fade_time = 0;
  static int order[FLAG_COUNT], index = 0;
  static bool fading = false;
  static Stepper sp;
  sp.steps(init, speed, 0.04f);
  float dt = sp.dt;
  if (init) {
    seed(esphome::millis());
    for (int i = 0; i < FLAG_COUNT; i++) order[i] = i;  // random.shuffle(FLAGS) on activate()
    for (int i = FLAG_COUNT - 1; i > 0; i--) std::swap(order[i], order[rndi(i + 1)]);
    t = flag_time = fade_time = 0;
    index = 0;
    fading = false;
  }
  t += dt;
  flag_time += dt;
  const FlagDef &cur = FLAGS[order[index]];
  const FlagDef &nxt = FLAGS[order[(index + 1) % FLAG_COUNT]];
  float progress = 0.0f;
  if (!fading) {
    if (flag_time >= 5.0f) {
      fading = true;
      fade_time = 0;
    }
  } else {
    fade_time += dt;
    progress = std::min(1.0f, fade_time / 1.5f);
  }
  for (int c = 0; c < W; c++) {
    float wave = 0.4f + (1.0f - 0.4f) * (sinf(c / (float)W * PI_F * 2.0f * 1.6f - t * 1.8f) * 0.5f + 0.5f);
    float nv = (c + 0.5f) / W;
    for (int r = 0; r < H; r++) {
      float nh = (r + 0.5f) / H;
      Color a = flag_color(cur, nh, nv);
      Color pa((uint8_t)(a.r * wave), (uint8_t)(a.g * wave), (uint8_t)(a.b * wave));
      if (!fading) {
        put(it, c, r, pa);
      } else {
        Color b = flag_color(nxt, nh, nv);
        Color pb((uint8_t)(b.r * wave), (uint8_t)(b.g * wave), (uint8_t)(b.b * wave));
        put(it, c, r, Color((uint8_t)(pa.r + (pb.r - pa.r) * progress), (uint8_t)(pa.g + (pb.g - pa.g) * progress),
                            (uint8_t)(pa.b + (pb.b - pa.b) * progress)));
      }
    }
  }
  if (fading && progress >= 1.0f) {
    index = (index + 1) % FLAG_COUNT;
    flag_time = 0;
    fading = false;
  }
}

// ===================================================================== Sand ==
template<typename L> void fx_sand(L &it, bool init, float speed) {
  constexpr int EXTRA = 4, LROWS = H + EXTRA, MAXG = 48;
  constexpr uint8_t COL[8][3] = {{220, 180, 80}, {200, 160, 60}, {240, 200, 100}, {180, 120, 40},
                                 {255, 200, 80}, {80, 160, 80},  {160, 80, 40},   {200, 100, 60}};
  enum { FILLING, FULL_PAUSE, FADING, RESTARTING };
  static int state = FILLING, fade_step = 0;
  static float acc = 0;
  static int8_t grid[LROWS][W];  // -1 empty, else colour index
  static int gcol[MAXG], grow[MAXG], gcolor[MAXG], gn = 0;
  static uint8_t snap[H][W][3];
  static Stepper sp;
  auto reset = [&]() {
    memset(grid, -1, sizeof grid);
    gn = 0;
  };
  auto is_empty = [&](int c, int r) { return !(c < 0 || c >= W || r < 0 || r >= LROWS) && grid[r][c] < 0; };
  int n = sp.steps(init, speed, 0.04f);
  float dt = sp.dt;
  if (init) {
    seed(esphome::millis());
    reset();
    state = FILLING;
    acc = 0;
  }
  acc += dt;
  auto draw = [&]() {
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++) {
        int g = grid[EXTRA + r][c];
        put(it, c, r, g >= 0 ? Color(COL[g][0], COL[g][1], COL[g][2]) : Color(0, 0, 0));
      }
    for (int i = 0; i < gn; i++) {
      int vr = grow[i] - EXTRA;
      if (gcol[i] >= 0 && gcol[i] < W && vr >= 0 && vr < H) put(it, gcol[i], vr, Color(COL[gcolor[i]][0], COL[gcolor[i]][1], COL[gcolor[i]][2]));
    }
  };
  if (state == FILLING) {
    for (int k = 0; k < n; k++) {
      if (rndf() < 0.6f) {
        int col = rnd_between(4, 7);
        if (grid[0][col] < 0 && gn < MAXG) {
          gcol[gn] = col;
          grow[gn] = 0;
          gcolor[gn] = rndi(8);
          gn++;
        }
      }
      int m = 0;
      for (int i = 0; i < gn; i++) {
        int c = gcol[i], r = grow[i];
        bool active = true;
        if (is_empty(c, r + 1)) {
          grow[i]++;
        } else {
          int sides[2] = {-1, 1};
          if (rnd() & 1) std::swap(sides[0], sides[1]);
          bool moved = false;
          for (int s = 0; s < 2 && !moved; s++)
            if (is_empty(c + sides[s], r + 1)) {
              gcol[i] += sides[s];
              grow[i]++;
              moved = true;
            }
          if (!moved) {
            grid[r][c] = gcolor[i];
            active = false;
          }
        }
        if (active) {
          gcol[m] = gcol[i];
          grow[m] = grow[i];
          gcolor[m] = gcolor[i];
          m++;
        }
      }
      gn = m;
    }
    if (n) draw();
    bool full = true;
    for (int c = 0; c < W; c++)
      if (grid[EXTRA][c] < 0) full = false;
    if (full) {
      for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++) {
          int g = grid[EXTRA + r][c];
          snap[r][c][0] = g >= 0 ? COL[g][0] : 0;
          snap[r][c][1] = g >= 0 ? COL[g][1] : 0;
          snap[r][c][2] = g >= 0 ? COL[g][2] : 0;
        }
      state = FULL_PAUSE;
      acc = 0;
    }
  } else if (state == FULL_PAUSE) {
    if (acc >= 0.8f) {
      fade_step = 0;
      state = FADING;
      acc = 0;
    }
  } else if (state == FADING) {
    if (acc < 0.04f) return;
    acc = 0;
    float f = std::max(0.0f, 1.0f - fade_step / 20.0f);
    for (int r = 0; r < H; r++)
      for (int c = 0; c < W; c++) put(it, c, r, Color((uint8_t)(snap[r][c][0] * f), (uint8_t)(snap[r][c][1] * f), (uint8_t)(snap[r][c][2] * f)));
    fade_step++;
    if (fade_step > 20) {
      state = RESTARTING;
      acc = 0;
    }
  } else if (acc >= 0.4f) {
    reset();
    state = FILLING;
    acc = 0;
  }
}

// ================================================================ Flow Field ==
template<typename L> void fx_flowfield(L &it, bool init, float speed) {
  constexpr int NP = 20, MAXLIFE = 120;
  constexpr float PSPEED = 0.35f, DECAY = 0.18f, SSCALE = 0.35f, TSCALE = 0.008f;
  struct P {
    float x, y, hue;
    int age, life;
  };
  static P p[NP];
  static FB fb;
  static float t = 0;
  static Stepper sp;
  auto respawn = [&](P &q) { q = {rnd_range(0, W - 1), rnd_range(0, H - 1), rndf(), 0, rnd_between(MAXLIFE / 2, MAXLIFE)}; };
  auto add = [&](float col, float row, float r, float g, float b, float strength) {
    int x0 = (int)col, y0 = (int)row;
    float dx = col - x0, dy = row - y0;
    for (int dc = 0; dc < 2; dc++)
      for (int dr = 0; dr < 2; dr++) {
        int c = x0 + dc, rr = y0 + dr;
        if (c < 0 || c >= W || rr < 0 || rr >= H) continue;
        float w = (dc == 0 ? 1 - dx : dx) * (dr == 0 ? 1 - dy : dy) * strength;
        int i = rr * W + c;
        fb.r[i] = std::min(1.0f, fb.r[i] + r * w);
        fb.g[i] = std::min(1.0f, fb.g[i] + g * w);
        fb.b[i] = std::min(1.0f, fb.b[i] + b * w);
      }
  };
  int n = sp.steps(init, speed, 0.05f);
  if (init) {
    seed(esphome::millis());
    fb.clear();
    for (auto &q : p) {
      respawn(q);
      q.age = rnd_between(0, q.life);
    }
    t = 0;
  }
  for (int k = 0; k < n; k++) {
    for (int i = 0; i < N; i++) {
      fb.r[i] *= 1.0f - DECAY;
      fb.g[i] *= 1.0f - DECAY;
      fb.b[i] *= 1.0f - DECAY;
    }
    for (auto &q : p) {
      float nn = pnoise3(q.x * SSCALE, q.y * SSCALE, t, 2, 0.5f, 2.0f);
      float angle = nn * PI_F * 4.0f;
      float af = std::min(q.age / 10.0f, 1.0f) * std::min((q.life - q.age) / 10.0f, 1.0f);
      Color c = hsv(q.hue, 0.9f, 1.0f);
      add(q.x, q.y, c.r / 255.0f * af, c.g / 255.0f * af, c.b / 255.0f * af, 0.6f);
      q.x += cosf(angle) * PSPEED;
      q.y += sinf(angle) * PSPEED;
      q.age++;
      if (q.x < 0 || q.x >= W || q.y < 0 || q.y >= H || q.age >= q.life) respawn(q);
    }
    t += TSCALE;
  }
  if (n) flush(it, fb);
}

// ================================================================== Weather ==
// Inputs come from Home Assistant (see wordclock.yaml): condition + temperature.
enum Icon : uint8_t { ICON_SUN, ICON_NIGHT, ICON_CLOUDY, ICON_PARTLY, ICON_RAIN, ICON_THUNDER, ICON_SNOW, ICON_FOG };
struct WeatherState {
  Icon icon = ICON_SUN;
  float temp = 15.0f;
  bool is_day = true;
  std::string condition = "sunny";
};
inline WeatherState &weather() {
  static WeatherState w;
  return w;
}
// HA weather condition (+ sun.sun) -> icon, mirroring weather.py _wmo_to_icon()
inline Icon icon_for(const std::string &c, bool is_day) {
  if (c == "sunny") return ICON_SUN;
  if (c == "clear-night") return ICON_NIGHT;
  if (c == "partlycloudy") return is_day ? ICON_PARTLY : ICON_NIGHT;
  if (c == "cloudy" || c == "windy" || c == "windy-variant") return ICON_CLOUDY;
  if (c == "fog") return ICON_FOG;
  if (c == "rainy" || c == "pouring") return ICON_RAIN;
  if (c == "snowy" || c == "snowy-rainy" || c == "hail") return ICON_SNOW;
  if (c == "lightning" || c == "lightning-rainy") return ICON_THUNDER;
  return is_day ? ICON_SUN : ICON_NIGHT;
}
inline void weather_set(const std::string &condition, bool is_day) {
  WeatherState &w = weather();
  w.condition = condition;
  w.is_day = is_day;
  w.icon = icon_for(condition, is_day);
}

template<typename L> void fx_weather(L &it, bool init, float speed) {
  constexpr uint8_t FONT[10][5] = {{7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
                                   {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 1, 1, 1}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};
  constexpr float BLOB[5][3] = {{0.0f, 0.0f, 2.8f}, {2.5f, 0.2f, 2.2f}, {-2.5f, 0.2f, 2.0f}, {1.0f, -1.5f, 1.8f}, {-1.0f, -1.5f, 1.5f}};
  constexpr float CLOUD_CX = 4.5f, CLOUD_ROW = 2.5f;
  constexpr int RAIN_START = 5;
  constexpr int STARS[14][2] = {{1, 0}, {7, 0}, {10, 0}, {3, 2}, {9, 2}, {0, 4}, {6, 4}, {11, 4}, {2, 6}, {8, 6}, {4, 8}, {10, 8}, {1, 10}, {7, 10}};
  struct Star {
    bool sparkling;
    float sparkle_t, next;
  };
  struct Drop {
    float c, r, drift;
  };
  static Star stars[14];
  static Drop rain[14], snow[12];
  static float t = 0, vt = 0, thunder_timer = 0;
  static bool thunder_flash = false, ready = false;
  static int buf[H][W][3];
  static Stepper sp;
  int n = sp.steps(init, speed, 0.05f);
  if (init || !ready) {
    seed(esphome::millis());
    t = 0;
    vt = 0;
    for (auto &s : stars) s = {false, 0, rnd_range(0.5f, 12.0f)};
    for (auto &d : rain) d = {rnd_range(1, 7), rnd_range(RAIN_START, H), 0};
    for (auto &d : snow) d = {rnd_range(1, 7), rnd_range(RAIN_START, H), rnd_range(-0.05f, 0.05f)};
    thunder_timer = 0;
    thunder_flash = false;
    ready = true;
  }
  if (!n) return;
  auto setp = [&](int r, int c, int R_, int G, int B) {
    if (r >= 0 && r < H && c >= 0 && c < W) {
      buf[r][c][0] = R_;
      buf[r][c][1] = G;
      buf[r][c][2] = B;
    }
  };
  for (int k = 0; k < n; k++) {
    vt += 0.05f;
    memset(buf, 0, sizeof buf);
    const WeatherState &w = weather();
    auto cloud = [&](float oc, float orow, float bri) {
      for (auto &b : BLOB)
        for (int r = 0; r < H; r++)
          for (int c = 0; c < W; c++) {
            float dist = sqrtf((c - (oc + b[0])) * (c - (oc + b[0])) + (r - (orow + b[1])) * (r - (orow + b[1])));
            if (dist < b[2]) {
              float bb = std::max(0.0f, (1.0f - dist / b[2]) * bri);
              buf[r][c][0] = std::min(255, buf[r][c][0] + (int)(180 * bb));
              buf[r][c][1] = std::min(255, buf[r][c][1] + (int)(180 * bb));
              buf[r][c][2] = std::min(255, buf[r][c][2] + (int)(190 * bb));
            }
          }
    };
    float col_min = 1e9f, col_max = -1e9f;
    for (auto &b : BLOB) {
      col_min = std::min(col_min, CLOUD_CX + b[0] - b[2]);
      col_max = std::max(col_max, CLOUD_CX + b[0] + b[2]);
    }
    int cmin = std::max(0, (int)col_min), cmax = std::min(W - 1, (int)col_max);
    auto sun_disc = [&](float cx, float cy, float rad, int d0, int d1, float rays_a) {
      for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++) {
          float dist = sqrtf((c - cx) * (c - cx) + (r - cy) * (r - cy));
          if (dist < rad) {
            float f = std::max(0.0f, 1.0f - dist / rad) * 0.75f;
            setp(r, c, (int)(255 * f), (int)(180 * f), (int)(45 * f));
          }
        }
      for (int ray = 0; ray < 8; ray++) {
        float angle = rays_a + ray * PI_F / 4;
        for (int d = d0; d < d1; d++) {
          int ri = (int)lroundf(cy + sinf(angle) * d), ci = (int)lroundf(cx + cosf(angle) * d);
          float f = std::max(0.0f, 1.0f - (d - d0) / 2.0f) * 0.45f;
          setp(ri, ci, (int)(255 * f), (int)(180 * f), (int)(45 * f));
        }
      }
    };
    switch (w.icon) {
      case ICON_SUN: sun_disc(4.0f, 3.0f, 2.5f, 3, 5, t); break;
      case ICON_NIGHT:
        for (int i = 0; i < 14; i++) {
          Star &s = stars[i];
          float bri = 0.06f;
          if (s.sparkling) {
            float prog = (vt - s.sparkle_t) / 1.5f;
            if (prog >= 1.0f) {
              s.sparkling = false;
              s.next = vt + rnd_range(3.0f, 12.0f);
            } else {
              bri = 0.06f + (1.0f - 0.06f) * sinf(prog * PI_F);
            }
          } else if (vt >= s.next) {
            s.sparkling = true;
            s.sparkle_t = vt;
          }
          setp(STARS[i][1], STARS[i][0], (int)(210 * bri), (int)(220 * bri), (int)(255 * bri));
        }
        break;
      case ICON_CLOUDY: cloud(CLOUD_CX + sinf(t * 0.5f) * 1.5f, CLOUD_ROW, 1.0f); break;
      case ICON_PARTLY:
        sun_disc(3.0f, 2.0f, 2.0f, 2, 4, t);
        cloud(CLOUD_CX + sinf(t * 0.4f) * 1.0f, CLOUD_ROW + 2.0f, 0.95f);
        break;
      case ICON_RAIN:
      case ICON_THUNDER:
        cloud(CLOUD_CX, CLOUD_ROW, 0.85f);
        for (auto &d : rain) {
          d.r += 0.3f;
          if (d.r >= H) {
            d.r = (float)RAIN_START;
            d.c = rnd_range((float)cmin, (float)cmax);
          }
          setp((int)d.r, (int)d.c, 80, 120, 255);
          if (w.icon == ICON_RAIN) setp((int)d.r - 1, (int)d.c, 30, 60, 140);
        }
        if (w.icon == ICON_THUNDER) {
          thunder_timer -= 1;
          if (thunder_timer <= 0) {
            thunder_flash = !thunder_flash;
            thunder_timer = thunder_flash ? rnd_between(1, 3) : rnd_between(3, 25);
          }
          if (thunder_flash) {
            static const int BOLT[6][2] = {{4, 5}, {4, 6}, {5, 6}, {5, 7}, {4, 7}, {4, 8}};
            for (auto &b : BOLT) setp(b[1], b[0], 255, 255, 100);
          }
        }
        break;
      case ICON_SNOW:
        cloud(CLOUD_CX, CLOUD_ROW, 0.7f);
        for (auto &d : snow) {
          d.r += 0.15f;
          d.c += d.drift + sinf(t + d.r) * 0.05f;
          d.c = std::max(0.0f, std::min((float)(W - 1), d.c));
          if (d.r >= H) {
            d.r = (float)RAIN_START;
            d.c = rnd_range((float)cmin, (float)cmax);
          }
          setp((int)d.r, (int)d.c, 200, 220, 255);
        }
        break;
      case ICON_FOG:
        for (int r = 0; r < H; r++)
          for (int c = 0; c < W; c++) {
            float nn = sinf(c * 0.6f + t * 0.4f + r * 0.3f) * sinf(r * 0.5f + t * 0.3f) * 0.5f + 0.5f;
            setp(r, c, (int)(180 * nn * 0.45f), (int)(190 * nn * 0.45f), (int)(200 * nn * 0.45f));
          }
        break;
    }
    // Temperature overlay (rows 6-10, right aligned, 3x5 font)
    {
      float tp = w.temp;
      float u = clamp01((tp + 10.0f) / 50.0f);
      int a[3], b[3];
      float u2;
      if (u < 0.5f) {
        int A[3] = {0, 20, 255}, B[3] = {255, 200, 0};
        memcpy(a, A, sizeof a);
        memcpy(b, B, sizeof b);
        u2 = u * 2;
      } else {
        int A[3] = {255, 200, 0}, B[3] = {255, 10, 0};
        memcpy(a, A, sizeof a);
        memcpy(b, B, sizeof b);
        u2 = (u - 0.5f) * 2;
      }
      int tc[3] = {(int)(a[0] + (b[0] - a[0]) * u2), (int)(a[1] + (b[1] - a[1]) * u2), (int)(a[2] + (b[2] - a[2]) * u2)};
      int abs_t = (int)fabsf(tp), tens = abs_t / 10, units = abs_t % 10;
      setp(7, 11, tc[0], tc[1], tc[2]);  // degree pixel
      auto digit = [&](int d, int left) {
        if (d < 0 || d > 9) return;
        for (int dr = 0; dr < 5; dr++)
          for (int dc = 0; dc < 3; dc++)
            if (FONT[d][dr] & (4 >> dc)) setp(6 + dr, left + dc, tc[0], tc[1], tc[2]);
      };
      int minus_a;
      if (tens > 0) {
        digit(tens, 3);
        digit(units, 7);
        minus_a = 0;
      } else {
        digit(units, 7);
        minus_a = 5;
      }
      if (tp < 0) {
        setp(8, minus_a, tc[0], tc[1], tc[2]);
        setp(8, minus_a + 1, tc[0], tc[1], tc[2]);
      }
    }
    t = fmodf(t + 0.05f, PI_F * 40.0f);
  }
  for (int r = 0; r < H; r++)
    for (int c = 0; c < W; c++) put(it, c, r, rgb(buf[r][c][0], buf[r][c][1], buf[r][c][2]));
}

// ================================================================= Presence ==
// Same layout/colours as presence.py FAMILY_MEMBERS.  Edit positions/colours
// here; the Home Assistant entities that drive them are set in wordclock.yaml.
constexpr int PRESENCE_COUNT = 4;
struct PresenceMember {
  int col, row;
  uint8_t r, g, b;
};
constexpr PresenceMember PRESENCE[PRESENCE_COUNT] = {
    {6, 4, 60, 60, 255}, {2, 5, 180, 70, 70}, {7, 8, 45, 200, 45}, {2, 1, 80, 10, 255}};
inline bool *presence_home() {
  static bool home[PRESENCE_COUNT] = {};
  return home;
}
inline bool *presence_enabled() {
  static bool enabled[PRESENCE_COUNT] = {true, true, true, true};
  return enabled;
}
inline void presence_set(int i, bool home) {
  if (i >= 0 && i < PRESENCE_COUNT) presence_home()[i] = home;
}
inline void presence_enable(int i, bool enabled) {
  if (i >= 0 && i < PRESENCE_COUNT) presence_enabled()[i] = enabled;
}

template<typename L> void fx_presence(L &it, bool init, float speed) {
  constexpr float ABSENT_DIM = 0.2f, DISC_R = 2.8f, PMAX = 0.3f, PMIN = 0.04f, PERIOD = 3.0f, ATTACK = 0.2f, FADE = 0.2f;
  static float bri[PRESENCE_COUNT], pulse_t[PRESENCE_COUNT], phase[PRESENCE_COUNT];
  static bool ready = false;
  static Stepper sp;
  sp.steps(init, speed, 0.05f);
  float dt = sp.dt;
  if (init || !ready) {
    seed(esphome::millis());
    for (int i = 0; i < PRESENCE_COUNT; i++) {
      bri[i] = ABSENT_DIM;
      pulse_t[i] = 0;
      phase[i] = rndf();
    }
    ready = true;
  }
  int buf[H][W][3];
  memset(buf, 0, sizeof buf);
  for (int i = 0; i < PRESENCE_COUNT; i++) {
    if (!presence_enabled()[i]) continue;
    const PresenceMember &m = PRESENCE[i];
    bool here = presence_home()[i];
    float target = here ? 1.0f : ABSENT_DIM, diff = target - bri[i], step = FADE * dt;
    if (fabsf(diff) <= step) bri[i] = target;
    else bri[i] += step * (diff > 0 ? 1 : -1);
    if (here) pulse_t[i] += dt;
    buf[m.row][m.col][0] = (int)(m.r * bri[i]);
    buf[m.row][m.col][1] = (int)(m.g * bri[i]);
    buf[m.row][m.col][2] = (int)(m.b * bri[i]);
    if (here && bri[i] > ABSENT_DIM) {
      float frac = fmodf(pulse_t[i] / PERIOD + phase[i], 1.0f), level;
      if (frac < ATTACK) level = sinf(frac / ATTACK * PI_F / 2.0f);
      else level = powf(1.0f - (frac - ATTACK) / (1.0f - ATTACK), 2.0f);
      float disc = (PMIN + (PMAX - PMIN) * level) * bri[i];
      for (int r = 0; r < H; r++)
        for (int c = 0; c < W; c++) {
          if (r == m.row && c == m.col) continue;
          if (sqrtf((float)((c - m.col) * (c - m.col) + (r - m.row) * (r - m.row))) <= DISC_R) {
            buf[r][c][0] = std::min(255, buf[r][c][0] + (int)(m.r * disc));
            buf[r][c][1] = std::min(255, buf[r][c][1] + (int)(m.g * disc));
            buf[r][c][2] = std::min(255, buf[r][c][2] + (int)(m.b * disc));
          }
        }
    }
  }
  for (int r = 0; r < H; r++)
    for (int c = 0; c < W; c++) put(it, c, r, rgb(buf[r][c][0], buf[r][c][1], buf[r][c][2]));
}

// ===================================================================== Heart ==
// The hourly GIF of the original ESP32 build, as a normal animation.
template<typename L> void fx_heart(L &it, bool init, float speed) {
  static float el = 0;
  static Stepper sp;
  sp.steps(init, speed, 0.05f);
  if (init) el = 0;
  else el += sp.dt;
  draw_heart(it, (uint32_t)(el * 1000.0f));
}

// ============================================================ Item catalogue ==
// Everything the clock can show as a transition or hourly animation.
// Items 0..ANIM_ITEMS-1 are the "Animations" group (== wc::G_ANIMS order) and
// can be enabled in the transition pool; Weather and Presence can only be
// picked explicitly for the hourly display.
constexpr int ANIM_ITEMS = 13;
constexpr int ITEM_COUNT = 15;
constexpr int ITEM_WEATHER = 13, ITEM_PRESENCE = 14, ITEM_HEART = 12;
inline const char *item_name(int i) {
  if (i == ITEM_WEATHER) return "Weather";
  if (i == ITEM_PRESENCE) return "Presence";
  return G_ANIMS[i];
}

template<typename L> void run_item(int item, L &it, bool init, float speed) {
  switch (item) {
    case 0: fx_matrix(it, init, speed); break;
    case 1: fx_ripple(it, init, speed); break;
    case 2: fx_fire(it, init, speed); break;
    case 3: fx_plasma(it, init, speed); break;
    case 4: fx_sand(it, init, speed); break;
    case 5: fx_lavalamp(it, init, speed); break;
    case 6: fx_balls(it, init, speed); break;
    case 7: fx_life(it, init, speed); break;
    case 8: fx_flowfield(it, init, speed); break;
    case 9: fx_spiral(it, init, speed); break;
    case 10: fx_flags(it, init, speed); break;
    case 11: fx_lightning(it, init, speed); break;
    case 12: fx_heart(it, init, speed); break;
    case ITEM_WEATHER: fx_weather(it, init, speed); break;
    default: fx_presence(it, init, speed); break;
  }
}

// Pick an item from the enabled pool (bit i = item i, 0 <= i < ANIM_ITEMS).
// sequential: next enabled item after the previous pick.  random: any enabled
// item, never the same one twice in a row.  Returns -1 for an empty pool.
inline int pick_from_pool(uint32_t pool, bool sequential) {
  static int last = -1;
  pool &= (1u << ANIM_ITEMS) - 1;
  if (!pool) return -1;
  int n = 0, list[ANIM_ITEMS];
  for (int i = 0; i < ANIM_ITEMS; i++)
    if (pool & (1u << i)) list[n++] = i;
  int pick;
  if (sequential) {
    pick = list[0];
    for (int i = 0; i < n; i++)
      if (list[i] > last) {
        pick = list[i];
        break;
      }
  } else {
    pick = list[rndi(n)];
    if (n > 1)
      while (pick == last) pick = list[rndi(n)];
  }
  last = pick;
  return pick;
}

// ============================================================== Word Clock ==
struct ClockCfg {
  int color_mode = 1;      // 0 random/word (ESP32), 1 shuffled palette (Pi), 2 light colour, 3 rainbow
  int hourly_choice = 2;   // 0 off, 1 random from pool, 2+i = item i (see item_name)
  float hourly_secs = 6;   // how long the hourly item is shown
  int trans_mode = 0;      // 0 off (opt-in), 1 random from pool, 2 sequential through the pool
  float trans_secs = 5;    // how long a transition animation plays
  uint32_t pool = 0x1FFF;  // enabled transition animations
  float speed = 50;        // Effect Speed slider (applies to the animations)
  float fade_secs = 0.35f;
};

struct ClockState {
  uint32_t key = 0;         // words currently painted
  uint32_t shown_key = 0;   // words of the last time we showed (0 = nothing yet)
  bool painted = false;
  int mode = -1;
  Color col[W_COUNT];
  uint8_t last_r = 0, last_g = 0, last_b = 0;
  float last_k = -1.0f;
  float hue = 0.0f;
  int last_anim_hour = -1;
  bool anim_on = false, anim_init = false;
  int anim_item = 0;
  uint32_t anim_start = 0, anim_ms = 0;
  bool post = false;        // fading the words in after an animation
  uint32_t end_ms = 0;
};

// trigger (HA buttons): 1 = play the hourly display now, 2 = play a transition animation now
template<typename L>
void fx_clock(L &it, const Color &cur, bool init, const esphome::ESPTime &t, const ClockCfg &cfg, int &trigger,
              float k) {
  static ClockState s;
  static Stepper sp;
  uint32_t now = esphome::millis();
  sp.steps(init, 50.0f, 1.0f);
  float dt = sp.dt;

  if (init) {
    seed(now ^ 0x9E3779B9u);
    s.painted = false;
    s.anim_on = false;
    s.post = false;
    s.shown_key = 0;  // coming back to the clock is not a "transition"
  }

  // Time not synced yet: breathe the top-left LED red so you can tell it's alive
  if (!t.is_valid()) {
    black(it);
    put(it, 0, 0, Color(u8(0.1f + 0.4f * (0.5f + 0.5f * sinf(now / 400.0f))), 0, 0));
    s.painted = false;
    return;
  }

  uint8_t words[8];
  int nw = time_words(t.hour, t.minute, words);
  uint32_t key = 0;
  for (int i = 0; i < nw; i++) key |= bit(words[i]);

  // ---- decide whether to start an animation ----
  if (!s.anim_on) {
    int item = -1;
    float secs = 0;
    auto hourly_item = [&]() {
      if (cfg.hourly_choice >= 2) return cfg.hourly_choice - 2;
      return pick_from_pool(cfg.pool, false);
    };
    if (trigger == 1) {
      item = cfg.hourly_choice == 0 ? ITEM_HEART : hourly_item();
      secs = cfg.hourly_secs;
    } else if (trigger == 2) {
      item = pick_from_pool(cfg.pool, cfg.trans_mode == 2);
      if (item < 0) item = ITEM_HEART;
      secs = cfg.trans_secs;
    } else if (cfg.hourly_choice != 0 && t.minute == 0 && t.hour != s.last_anim_hour) {
      s.last_anim_hour = t.hour;  // hourly display replaces the transition animation
      item = hourly_item();
      secs = cfg.hourly_secs;
    } else if (cfg.trans_mode != 0 && s.shown_key != 0 && key != s.shown_key) {
      item = pick_from_pool(cfg.pool, cfg.trans_mode == 2);
      secs = cfg.trans_secs;
    }
    trigger = 0;
    if (item >= 0 && secs >= 0.5f) {
      s.anim_on = true;
      s.anim_init = true;
      s.anim_item = item;
      s.anim_start = now;
      s.anim_ms = (uint32_t)(secs * 1000.0f);
    }
  }
  s.shown_key = key;  // the new time is "owed" to the display once the animation ends

  // ---- play the animation (fades in and out around it) ----
  if (s.anim_on) {
    uint32_t el = now - s.anim_start;
    if (el < s.anim_ms) {
      float f = std::min(cfg.fade_secs, s.anim_ms / 3000.0f) * 1000.0f;
      float kin = f > 1 ? clamp01(el / f) : 1.0f, kout = f > 1 ? clamp01((s.anim_ms - el) / f) : 1.0f;
      auto d = dim(it, std::min(kin, kout));
      run_item(s.anim_item, d, s.anim_init, cfg.speed);
      s.anim_init = false;
      s.painted = false;
      return;
    }
    s.anim_on = false;
    s.painted = false;
    s.post = true;
    s.end_ms = now;
  }

  // ---- the words (fade in after an animation) ----
  float kc = 1.0f;
  if (s.post) {
    float f = cfg.fade_secs * 1000.0f;
    kc = f > 1 ? clamp01((now - s.end_ms) / f) : 1.0f;
    if (kc >= 1.0f) s.post = false;
  }
  float ktot = k * kc;
  bool changed = !s.painted || key != s.key || cfg.color_mode != s.mode;
  bool repaint = changed || fabsf(ktot - s.last_k) > 0.002f;
  if (cfg.color_mode == 2 && (cur.r != s.last_r || cur.g != s.last_g || cur.b != s.last_b)) repaint = true;
  if (cfg.color_mode == 3) {
    s.hue += dt * 0.04f;
    repaint = true;
  }
  if (!repaint) return;

  if (changed && cfg.color_mode == 0) {
    for (int i = 0; i < nw; i++) {
      uint32_t c = PALETTE_ESP[rndi(8)];
      s.col[words[i]] = Color((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
    }
  } else if (changed && cfg.color_mode == 1) {
    uint8_t order[8] = {0, 1, 2, 3, 4, 5, 6, 7};  // random.shuffle(COLORS), then hand out in order
    for (int i = 7; i > 0; i--) std::swap(order[i], order[rndi(i + 1)]);
    for (int i = 0; i < nw; i++) {
      const uint8_t *p = PALETTE_PI[order[i % 8]];
      s.col[words[i]] = Color(p[0], p[1], p[2]);
    }
  }
  s.key = key;
  s.mode = cfg.color_mode;
  s.painted = true;
  s.last_k = ktot;
  s.last_r = cur.r;
  s.last_g = cur.g;
  s.last_b = cur.b;

  auto d = dim(it, kc);
  black(d);
  for (int i = 0; i < nw; i++) {
    int w = words[i];
    Color c = cfg.color_mode <= 1 ? s.col[w] : (cfg.color_mode == 2 ? cur : hsv(s.hue + i * 0.09f, 1.0f, 1.0f));
    for (int p = SPANS[w].s; p <= SPANS[w].e; p++) d[p] = c;
  }
}

}  // namespace wc
