#include "aquarium.h"

#include <math.h>
#include <string.h>

#define VW 32
#define VH 9
#define WW 56
#define WH 18
#define FISH_N 12
#define PLANT_N 6
#define ROCK_N 5
#define BUBBLE_N 8

struct Fish { float x, y, z, dz, speed, phase; int8_t dir; };
struct Scenery { float x, y, z; };
struct Bubble { float x, y, z, speed; };

static Fish fish[FISH_N];
static Scenery plants[PLANT_N], rocks[ROCK_N];
static Bubble bubbles[BUBBLE_N];
static float camX = 20.0f, camY = 4.5f;
static uint32_t tickN = 0;
static bool focusLatch = false;
static uint8_t *liveFb = nullptr;
static ESP8266WebServer *srv = nullptr;

static float frand() { return random(0, 10000) / 10000.0f; }
static float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}
static void plot(uint8_t fb[][32], int x, int y, uint8_t c) {
  if ((unsigned)x >= VW || (unsigned)y >= VH) return;
  if (c > fb[y][x]) fb[y][x] = c;
}

void aquariumBegin() {
  // This is the only automatic seed. Mode entry never rebuilds aquarium state.
  tickN = 0;
  camX = 20.0f;
  camY = 4.5f;
  for (int i = 0; i < FISH_N; i++) {
    fish[i].x = 3.0f + frand() * (WW - 6.0f);
    fish[i].y = 2.0f + frand() * (WH - 6.0f);
    fish[i].z = 0.08f + frand() * 0.88f;
    fish[i].dir = (i & 1) ? -1 : 1;
    fish[i].speed = 0.055f + frand() * 0.09f;
    fish[i].dz = (frand() - 0.5f) * 0.0032f;
    if (fabsf(fish[i].dz) < 0.0006f) fish[i].dz = (i & 1) ? -0.0011f : 0.0011f;
    fish[i].phase = frand() * 6.283185f;
  }
  for (int i = 0; i < PLANT_N; i++) {
    plants[i].x = 3.0f + frand() * (WW - 6.0f);
    plants[i].y = WH - 1.0f;
    plants[i].z = 0.10f + frand() * 0.55f;
  }
  for (int i = 0; i < ROCK_N; i++) {
    rocks[i].x = 2.0f + frand() * (WW - 4.0f);
    rocks[i].y = WH - 1.0f;
    rocks[i].z = 0.10f + frand() * 0.75f;
  }
  for (int i = 0; i < BUBBLE_N; i++) {
    bubbles[i].x = frand() * WW;
    bubbles[i].y = 1.0f + frand() * (WH - 2.0f);
    bubbles[i].z = frand();
    bubbles[i].speed = 0.045f + frand() * 0.08f;
  }
}

void aquariumStep() {
  tickN++;
  float sx = 0, sy = 0, sz[4] = {-1, -1, -1, -1};
  int si[4] = {0, 0, 0, 0};
  for (int i = 0; i < FISH_N; i++) {
    Fish &f = fish[i];
    f.phase += 0.045f + f.speed * 0.08f;
    f.x += f.dir * f.speed * (0.70f + f.z * 0.65f);
    f.y = clampf(f.y + 0.018f * sinf(f.phase), 1.0f, WH - 3.0f);
    f.z += f.dz;
    if (f.z <= 0.06f) { f.z = 0.06f; f.dz = fabsf(f.dz); }
    else if (f.z >= 0.98f) { f.z = 0.98f; f.dz = -fabsf(f.dz); }
    if ((tickN % 700) == (uint32_t)(i * 43)) f.dz = clampf(f.dz + (frand() - 0.5f) * 0.0012f, -0.0030f, 0.0030f);
    if (f.x <= 1.0f) { f.x = 1.0f; f.dir = 1; }
    else if (f.x >= WW - 2.0f) { f.x = WW - 2.0f; f.dir = -1; }
    for (int k = 0; k < 4; k++) {
      if (f.z <= sz[k]) continue;
      for (int j = 3; j > k; j--) { sz[j] = sz[j - 1]; si[j] = si[j - 1]; }
      sz[k] = f.z; si[k] = i; break;
    }
  }
  for (int i = 0; i < BUBBLE_N; i++) {
    Bubble &b = bubbles[i];
    b.y -= b.speed * (0.75f + b.z * 0.50f);
    if (b.y < 0.5f) {
      b.y = WH - 1.5f;
      b.x = fmodf(b.x + 11.0f + frand() * 17.0f, (float)WW);
      b.z = frand();
    }
  }
  for (int k = 0; k < 4; k++) { sx += fish[si[k]].x; sy += fish[si[k]].y; }
  float tx = clampf(sx * 0.25f - VW * 0.5f, 0, WW - VW);
  float ty = clampf(sy * 0.25f - VH * 0.5f, 0, WH - VH);
  camX += (tx - camX) * 0.025f;
  camY += (ty - camY) * 0.025f;
}

void aquariumRender(uint8_t fb[][32], uint8_t bright) {
  memset(fb, 0, VW * VH);
  for (int i = 0; i < ROCK_N; i++) {
    int x = (int)lroundf(rocks[i].x - camX), y = (int)lroundf(rocks[i].y - camY);
    uint8_t c = (uint8_t)(bright * (0.12f + rocks[i].z * 0.20f));
    plot(fb, x, y, c);
    if (rocks[i].z > 0.55f) plot(fb, x + 1, y, c / 2);
  }
  for (int i = 0; i < PLANT_N; i++) {
    int x = (int)lroundf(plants[i].x - camX), y = (int)lroundf(plants[i].y - camY);
    uint8_t c = (uint8_t)(bright * (0.10f + plants[i].z * 0.18f));
    int h = 1 + (int)(plants[i].z * 3.0f);
    for (int stem = 0; stem < h; stem++) plot(fb, x + (stem & 1), y - stem, c);
  }
  for (int i = 0; i < BUBBLE_N; i++) {
    int x = (int)lroundf(bubbles[i].x - camX), y = (int)lroundf(bubbles[i].y - camY);
    plot(fb, x, y, (uint8_t)(bright * (0.10f + bubbles[i].z * 0.24f)));
  }
  // Draw back-to-front so near fish win pixel collisions. Render must not mutate simulation state.
  bool drawn[FISH_N] = {false};
  for (int pass = 0; pass < FISH_N; pass++) {
    int pick = -1;
    float pz = 2.0f;
    for (int i = 0; i < FISH_N; i++) {
      if (!drawn[i] && fish[i].z < pz) { pz = fish[i].z; pick = i; }
    }
    if (pick < 0) break;
    drawn[pick] = true;
    const Fish &f = fish[pick];
    int x = (int)lroundf(f.x - camX), y = (int)lroundf(f.y - camY), d = f.dir;
    uint8_t c = (uint8_t)(bright * (0.28f + f.z * 0.72f));
    plot(fb, x, y, c); plot(fb, x - d, y, c / 2);
    if (f.z >= 0.34f) plot(fb, x + d, y, c * 3 / 4);
    if (f.z >= 0.67f) {
      plot(fb, x, y - 1, c / 2);
      plot(fb, x - 2 * d, y - 1, c / 3);
      plot(fb, x - 2 * d, y + 1, c / 3);
    }
  }
}

bool aquariumTakeFocus() {
  if (!focusLatch) return false;
  focusLatch = false;
  return true;
}
void aquariumNudgeCam(int dx, int dy) {
  camX = clampf(camX + dx, 0, WW - VW);
  camY = clampf(camY + dy, 0, WH - VH);
}
void aquariumSetFb(uint8_t (*fb)[32]) { liveFb = &fb[0][0]; }

static int argI(const char *key, int fallback) {
  return srv->hasArg(key) ? srv->arg(key).toInt() : fallback;
}
static void sendState() {
  char buf[360];
  snprintf(buf, sizeof(buf),
           "{\"ok\":true,\"sim\":\"aquarium\",\"w\":32,\"h\":9,\"world\":[56,18],"
           "\"fish\":12,\"plants\":6,\"rocks\":5,\"bubbles\":8,\"tick\":%lu,"
           "\"cam\":[%.1f,%.1f],\"imu\":false,\"ldr\":false}",
           (unsigned long)tickN, camX, camY);
  srv->send(200, "application/json", buf);
}
static void sendFb() {
  static char buf[VW * VH * 2 + 120];
  char *p = buf;
  p += sprintf(p, "{\"ok\":true,\"sim\":\"aquarium\",\"w\":32,\"h\":9,\"px\":\"");
  static const char *hex = "0123456789abcdef";
  if (liveFb) for (int i = 0; i < VW * VH; i++) { *p++ = hex[liveFb[i] >> 4]; *p++ = hex[liveFb[i] & 15]; }
  *p++ = '"'; *p++ = '}'; *p = 0;
  srv->send(200, "application/json", buf);
}
void aquariumHttpBegin(ESP8266WebServer &http) {
  srv = &http;
  http.on("/aquarium", HTTP_GET, sendState);
  http.on("/aquarium.json", HTTP_GET, sendState);
  http.on("/aquarium/fb.json", HTTP_GET, sendFb);
  http.on("/aquarium/focus", HTTP_POST, []() { focusLatch = true; sendState(); });
  http.on("/aquarium/pan", HTTP_POST, []() { aquariumNudgeCam(argI("dx", 0), argI("dy", 0)); sendState(); });
}
