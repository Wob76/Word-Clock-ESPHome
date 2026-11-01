#pragma once
// ---------------------------------------------------------------------------
// Word clock core for ESPHome.
//
//  * Port of johniak/word-clock esp/wordclock (ClockDisplayHAL + WordClock)
//  * plus the Crossbow78/word_clock controller features: shuffled colour
//    palette, time-of-day brightness, fade-in on mode change, groups/slideshow
//
// Pulled into the build by the `wordclock:` external component.  Everything lives in namespace wc.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#if __has_include("esphome/core/color.h")
#include "esphome/core/color.h"
#include "esphome/core/hal.h"
#include "esphome/core/time.h"
#endif

#include "heart_anim.h"

namespace wc {

using esphome::Color;

constexpr int W = 12;
constexpr int H = 11;
constexpr int N = W * H;
constexpr float PI_F = 3.14159265f;

// (x, y) -> strip index.  x=0 left, y=0 top.  Identical to the ESP32
// ClockDisplayHAL and to shared.xy() in the Pi controller.  -1 if outside.
inline int idx(int x, int y) {
  if (x < 0 || x >= W || y < 0 || y >= H) return -1;
  if (y % 2 == 0) return N - y * W - (x + 1);
  return N - (y + 1) * W + x;
}

// ----------------------------------------------------------------- helpers --
inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline uint8_t u8(float v) { return (uint8_t)(clamp01(v) * 255.0f + 0.5f); }
inline uint8_t cl8(int v) { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }
inline Color rgb(int r, int g, int b) { return Color(cl8(r), cl8(g), cl8(b)); }
// Python-style scale(): int(c * factor) with factor clamped to 0..1
inline Color scl(const Color &c, float f) {
  f = clamp01(f);
  return Color((uint8_t)(c.r * f), (uint8_t)(c.g * f), (uint8_t)(c.b * f));
}

// xorshift32 - tiny, fast, no dependency on the ESPHome RNG
inline uint32_t &rng_state() {
  static uint32_t s = 2463534242u;
  return s;
}
inline void seed(uint32_t s) { rng_state() = s ? s : 1u; }
inline uint32_t rnd() {
  uint32_t &x = rng_state();
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}
inline float rndf() { return (rnd() >> 8) * (1.0f / 16777216.0f); }  // [0,1)
inline float rnd_range(float a, float b) { return a + (b - a) * rndf(); }
inline int rndi(int n) { return (int)(rnd() % (uint32_t)n); }       // [0,n)
inline int rnd_between(int a, int b) { return a + rndi(b - a + 1); }  // [a,b] inclusive

// Same as shared.hsv_to_rgb(): h wrapped, truncating conversion
inline Color hsv(float h, float s, float v) {
  h -= floorf(h);
  v = clamp01(v);
  s = clamp01(s);
  float h6 = h * 6.0f;
  int i = (int)h6;
  float f = h6 - i;
  float p = v * (1.0f - s), q = v * (1.0f - f * s), t = v * (1.0f - (1.0f - f) * s);
  float r, g, b;
  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
  }
  return Color((uint8_t)(r * 255.0f), (uint8_t)(g * 255.0f), (uint8_t)(b * 255.0f));
}

// Effect speed slider (5..100, 50 = nominal) -> time scale
inline float ts(float speed) { return speed / 50.0f; }

// Turns real elapsed time into "how many Python frames should have run".
// frame_delay is the FRAME_DELAY of the original module; at speed 50 the
// animation runs at exactly the original pace.  Also exposes the scaled dt.
struct Stepper {
  float acc = 0.0f;
  uint32_t last = 0;
  float dt = 0.0f;  // scaled seconds since last call
  int steps(bool init, float speed, float frame_delay) {
    uint32_t n = esphome::millis();
    if (init || last == 0) {
      last = n;
      acc = 1.0f;  // always render the first frame
      dt = 0.0f;
    } else {
      float d = (n - last) / 1000.0f;
      last = n;
      if (d > 0.25f) d = 0.25f;  // after a stall
      dt = d * ts(speed);
      acc += dt / frame_delay;
    }
    int s = (int)acc;
    acc -= (float)s;
    return s > 8 ? 8 : s;
  }
};

// Float RGB accumulation buffer (indexed [y*W+x]) for blending effects
struct FB {
  float r[N], g[N], b[N];
  void clear() {
    memset(r, 0, sizeof r);
    memset(g, 0, sizeof g);
    memset(b, 0, sizeof b);
  }
};

// Wraps the light buffer: every write is scaled by k (time-of-day brightness
// and the fade-in after a mode change).  Reads are never needed by effects.
template<typename L> struct Dim {
  L &l;
  float k;
  struct Px {
    L &l;
    int i;
    float k;
    Px &operator=(const Color &c) {
      l[i] = Color((uint8_t)(c.r * k + 0.5f), (uint8_t)(c.g * k + 0.5f), (uint8_t)(c.b * k + 0.5f));
      return *this;
    }
  };
  Px operator[](int i) { return Px{l, i, k}; }
};
template<typename L> inline Dim<L> dim(L &l, float k) { return Dim<L>{l, clamp01(k)}; }

// Linear fade-in over fade_s seconds after an effect starts (init = true)
inline float fade_ramp(bool init, float fade_s) {
  static uint32_t start = 0;
  uint32_t n = esphome::millis();
  if (init) start = n;
  if (fade_s < 0.02f) return 1.0f;
  return clamp01((n - start) / (fade_s * 1000.0f));
}

template<typename L> inline void put(L &it, int x, int y, const Color &c) {
  int i = idx(x, y);
  if (i >= 0) it[i] = c;
}
template<typename L> inline void black(L &it) {
  for (int i = 0; i < N; i++) it[i] = Color(0, 0, 0);
}
template<typename L> inline void flush(L &it, const FB &fb) {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int i = y * W + x;
      it[idx(x, y)] = Color((uint8_t)(clamp01(fb.r[i]) * 255.0f), (uint8_t)(clamp01(fb.g[i]) * 255.0f),
                            (uint8_t)(clamp01(fb.b[i]) * 255.0f));
    }
}

// ------------------------------------------- time-of-day brightness (Pi) ----
inline bool hour_in(int h, int start, int end_incl) {
  return start <= end_incl ? (h >= start && h <= end_incl) : (h >= start || h <= end_incl);
}
// controller.py _time_brightness(): night 23-05 -> 0.15, evening 19-08 -> 0.40, else 1.0
inline float schedule_scale(int hour, int night_start, int night_end, int eve_start, int eve_end, float night_level,
                            float eve_level) {
  if (hour_in(hour, night_start, night_end)) return night_level;
  if (hour_in(hour, eve_start, eve_end)) return eve_level;
  return 1.0f;
}

// ------------------------------------------------------------------- words --
enum Word : uint8_t {
  W_IT, W_IS, W_OCLOCK, W_PAST, W_TO, W_MINUTES,
  W_FIVE, W_TEN, W_FIFTEEN, W_TWENTY, W_TWENTYFIVE, W_THIRTY,
  W_H1, W_H2, W_H3, W_H4, W_H5, W_H6, W_H7, W_H8, W_H9, W_H10, W_H11, W_H12,
  W_COUNT
};

struct Span {
  uint8_t s, e;  // inclusive strip indices
};

// Identical in the ESP32 firmware and the Pi controller
constexpr Span SPANS[W_COUNT] = {
    {130, 131}, {127, 128}, {0, 5},    {60, 63},  {63, 64},   {77, 83},
    {98, 101},  {91, 93},   {110, 116}, {102, 107}, {98, 107}, {84, 89},
    {20, 22},   {45, 47},   {15, 19},  {67, 70},  {40, 43},   {12, 14},
    {55, 59},   {31, 35},   {36, 39},  {9, 11},   {24, 29},   {48, 53},
};

// ESP32 firmware palette (WordClock.cpp)
constexpr uint32_t PALETTE_ESP[8] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF, 0x00FFFF, 0xFFFFFF, 0xA52A2A};
// Pi controller palette (animations/wordclock.py)
constexpr uint8_t PALETTE_PI[8][3] = {{255, 20, 20}, {20, 255, 20}, {20, 20, 255}, {255, 255, 20},
                                      {255, 20, 255}, {20, 255, 255}, {255, 255, 255}, {255, 120, 40}};

inline uint32_t bit(int w) { return 1u << w; }

inline Word minute_word(int minute) {
  if (minute < 5) return W_OCLOCK;
  if (minute < 10) return W_FIVE;
  if (minute < 15) return W_TEN;
  if (minute < 20) return W_FIFTEEN;
  if (minute < 25) return W_TWENTY;
  if (minute < 30) return W_TWENTYFIVE;
  if (minute < 35) return W_THIRTY;
  if (minute < 40) return W_TWENTYFIVE;
  if (minute < 45) return W_TWENTY;
  if (minute < 50) return W_FIFTEEN;
  if (minute < 55) return W_TEN;
  return W_FIVE;
}

// Words to light, in reading order (same set as WordClock::displayTime and
// _words_for_time).  Returns the count (max 6).
inline int time_words(int hour24, int minute, uint8_t out[8]) {
  int n = 0;
  int h = hour24 % 12;
  if (h == 0) h = 12;
  out[n++] = W_IT;
  out[n++] = W_IS;
  if (minute < 5) {
    out[n++] = W_OCLOCK;
  } else {
    out[n++] = W_MINUTES;
    if (minute < 35) {
      out[n++] = W_PAST;
    } else {
      out[n++] = W_TO;
      h = (h % 12) + 1;
    }
    out[n++] = minute_word(minute);
  }
  out[n++] = W_H1 + (h - 1);
  return n;
}

inline uint32_t time_mask(int hour24, int minute) {
  uint8_t w[8];
  int n = time_words(hour24, minute, w);
  uint32_t m = 0;
  for (int i = 0; i < n; i++) m |= bit(w[i]);
  return m;
}

// ------------------------------------------------------------- GIF playback --
template<typename L> void draw_heart(L &it, uint32_t elapsed_ms) {
  uint32_t t = elapsed_ms % wc_heart::TOTAL_MS;
  int f = wc_heart::FRAME_COUNT - 1;
  uint32_t acc = 0;
  for (int i = 0; i < wc_heart::FRAME_COUNT; i++) {
    acc += wc_heart::DELAYS_MS[i];
    if (t < acc) {
      f = i;
      break;
    }
  }
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++)
      put(it, x, y, Color(wc_heart::FRAMES[f][y][x][0], wc_heart::FRAMES[f][y][x][1], wc_heart::FRAMES[f][y][x][2]));
}

// ---------------------------------------------- groups / modes (Pi controller) --
// Names are the effect names in wordclock.yaml (= NAME in each Pi module).
// "Heart" is the hourly GIF of the original ESP32 build, now a regular animation.
constexpr const char *GROUP_NAMES[] = {"Info", "Animations"};
constexpr int GROUP_COUNT = 2;
constexpr const char *G_CURRENT[] = {"Word Clock", "Weather", "Presence"};
constexpr const char *G_ANIMS[] = {"Matrix Rain", "Ripple", "Fire",         "Plasma",    "Sand",      "Lava Lamp",
                                   "Bouncing Balls", "Game of Life", "Flow Field", "Spiral", "Flags", "Lightning",
                                   "Heart"};
constexpr const char *const *GROUP_MODES[] = {G_CURRENT, G_ANIMS};
constexpr int GROUP_SIZES[] = {3, 13};

inline std::string &last_mode(int g) {
  static std::string m[GROUP_COUNT];
  return m[g];
}
inline bool find_mode(const std::string &name, int &g, int &i) {
  for (g = 0; g < GROUP_COUNT; g++)
    for (i = 0; i < GROUP_SIZES[g]; i++)
      if (name == GROUP_MODES[g][i]) return true;
  g = 0;
  i = 0;
  return false;
}
// Remember the last mode used in each group (call regularly with the current effect name)
inline int note_mode(const std::string &name) {
  int g, i;
  if (find_mode(name, g, i)) {
    last_mode(g) = name;
    return g;
  }
  return -1;
}
inline const char *next_mode(const std::string &cur) {  // single click / slideshow
  int g, i;
  find_mode(cur, g, i);
  return GROUP_MODES[g][(i + 1) % GROUP_SIZES[g]];
}
inline const char *group_mode(int g) {  // the mode a group resumes on
  g = ((g % GROUP_COUNT) + GROUP_COUNT) % GROUP_COUNT;
  const std::string &m = last_mode(g);
  int gg, ii;
  if (!m.empty() && find_mode(m, gg, ii) && gg == g) return GROUP_MODES[g][ii];
  return GROUP_MODES[g][0];
}
inline const char *next_group_mode(const std::string &cur) {  // long press
  int g, i;
  find_mode(cur, g, i);
  return group_mode(g + 1);
}

}  // namespace wc
