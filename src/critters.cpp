#include "critters.h"

#include <math.h>
#include <string.h>

#define VW 32
#define VH 9
#define WW 64.0f
#define WH 18.0f
#define MAX_C 14
#define MAX_F 20
#define KIND_PREY 0
#define KIND_PRED 1

struct Critter {
  float x, y, a, e;
  uint8_t g[8];
  uint8_t gen;
  uint8_t kind;
  uint8_t alive;
};

struct Food {
  float x, y, a;
  uint8_t on;
};

static Critter cs[MAX_C];
static Food foods[MAX_F];
static uint8_t trail[VH][VW];
static float camX = 16.0f, camY = 4.5f;
static bool follow = true;
static bool brightAuto = false;
static uint16_t adcRaw = 0;
static uint16_t births = 0;
static uint32_t tick = 0;
static float shakeLeft = 0;
static bool focusLatch = false;
static uint8_t nPrey = 0, nPred = 0;

bool crittersTakeFocus() {
  if (!focusLatch) return false;
  focusLatch = false;
  return true;
}

static float frand() { return (random(0, 10000) / 10000.0f); }
static float clampf(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}
static float wrap(float v, float m) {
  while (v < 0) v += m;
  while (v >= m) v -= m;
  return v;
}
static float wrapd(float d, float m) {
  if (d > m * 0.5f) d -= m;
  if (d < -m * 0.5f) d += m;
  return d;
}

static void tally() {
  nPrey = nPred = 0;
  for (int i = 0; i < MAX_C; i++) {
    if (!cs[i].alive) continue;
    if (cs[i].kind == KIND_PRED) nPred++;
    else nPrey++;
  }
}

static void mutate(uint8_t *g) {
  for (int k = 0; k < 2; k++) {
    uint8_t i = random(0, 8);
    int v = (int)g[i] + (int)random(0, 51) - 25;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    g[i] = (uint8_t)v;
  }
}

static int spawn(float x, float y, const uint8_t *src, uint8_t gen, float e, uint8_t kind) {
  for (int i = 0; i < MAX_C; i++) {
    if (cs[i].alive) continue;
    cs[i].alive = 1;
    cs[i].kind = kind;
    cs[i].x = wrap(x, WW);
    cs[i].y = wrap(y, WH);
    cs[i].a = frand() * 6.2832f;
    cs[i].e = e;
    cs[i].gen = gen;
    if (src) {
      memcpy(cs[i].g, src, 8);
      mutate(cs[i].g);
    } else {
      for (int k = 0; k < 8; k++) cs[i].g[k] = (uint8_t)random(40, 220);
    }
    births++;
    return i;
  }
  return -1;
}

static void dropFood(float x, float y, float a) {
  int slot = -1;
  for (int i = 0; i < MAX_F; i++) {
    if (!foods[i].on) {
      slot = i;
      break;
    }
  }
  if (slot < 0) slot = random(0, MAX_F);
  foods[slot].on = 1;
  foods[slot].x = wrap(x, WW);
  foods[slot].y = wrap(y, WH);
  foods[slot].a = clampf(a, 0.25f, 1.2f);
}

void crittersSeed() {
  memset(cs, 0, sizeof(cs));
  memset(foods, 0, sizeof(foods));
  memset(trail, 0, sizeof(trail));
  births = 0;
  camX = 16.0f;
  camY = 4.5f;
  shakeLeft = 0;
  for (int i = 0; i < 8; i++)
    spawn(6 + frand() * 52, 2 + frand() * 14, nullptr, 0, 0.65f + frand() * 0.35f, KIND_PREY);
  for (int i = 0; i < 3; i++)
    spawn(frand() * WW, frand() * WH, nullptr, 0, 0.85f + frand() * 0.25f, KIND_PRED);
  for (int i = 0; i < 10; i++) dropFood(frand() * WW, frand() * WH, 0.6f + frand() * 0.4f);
  tally();
}

void crittersBegin() { crittersSeed(); }

static int nearestOf(const Critter &c, uint8_t wantKind, float range, float *odx, float *ody, float *od2) {
  int best = -1;
  float bd = range * range;
  for (int i = 0; i < MAX_C; i++) {
    if (!cs[i].alive || cs[i].kind != wantKind) continue;
    if (&cs[i] == &c) continue;
    float dx = wrapd(cs[i].x - c.x, WW);
    float dy = wrapd(cs[i].y - c.y, WH);
    float d2 = dx * dx + dy * dy;
    if (d2 < bd) {
      bd = d2;
      best = i;
      *odx = dx;
      *ody = dy;
    }
  }
  *od2 = bd;
  return best;
}

static int nearestFood(const Critter &c, float *odx, float *ody, float *od2) {
  int best = -1;
  float bd = 1e9f;
  float range = 4.0f + (c.g[2] / 255.0f) * 18.0f;
  float r2 = range * range;
  for (int i = 0; i < MAX_F; i++) {
    if (!foods[i].on) continue;
    float dx = wrapd(foods[i].x - c.x, WW);
    float dy = wrapd(foods[i].y - c.y, WH);
    float d2 = dx * dx + dy * dy;
    if (d2 < bd && d2 < r2) {
      bd = d2;
      best = i;
      *odx = dx;
      *ody = dy;
    }
  }
  *od2 = bd;
  return best;
}

static void steerTo(Critter &c, float dx, float dy, float turn, float k) {
  float want = atan2f(dy, dx);
  float da = wrapd(want - c.a, 6.2832f);
  c.a += clampf(da, -turn, turn) * k;
}

void crittersStep() {
  tick++;
  if ((tick & 7) == 0) adcRaw = analogRead(A0);
  if (shakeLeft > 0) shakeLeft *= 0.86f;
  if ((tick % 12) == 0) dropFood(frand() * WW, frand() * WH, 0.45f + 0.30f * frand());

  int n = 0;

  for (int i = 0; i < MAX_C; i++) {
    Critter &c = cs[i];
    if (!c.alive) continue;
    bool pred = c.kind == KIND_PRED;

    // Lotka–Volterra timescales, not per-frame burn.
    // ~55 fps: prey starve ~25s, pred ~14s; a kill must fund a pup.
    float speed = (pred ? 0.14f : 0.22f) + (c.g[0] / 255.0f) * (pred ? 0.26f : 0.38f);
    float turn = 0.14f + (c.g[1] / 255.0f) * 0.50f;
    float metab = pred ? (0.00055f + (c.g[7] / 255.0f) * 0.00055f)
                       : (0.00028f + (c.g[7] / 255.0f) * 0.00032f);
    float range = 6.0f + (c.g[2] / 255.0f) * (pred ? 18.0f : 12.0f);
    float conv = 0.70f + (c.g[3] / 255.0f) * 0.35f; // δ
    float preySplit = 0.48f + (c.g[3] / 255.0f) * 0.18f; // α threshold

    if (shakeLeft > 0.05f) {
      c.a += (frand() - 0.5f) * shakeLeft * 2.4f;
      speed += shakeLeft * 0.35f;
    }

    if (pred) {
      float pdx = 0, pdy = 0, pd2 = 1e9f;
      int pi = nearestOf(c, KIND_PREY, range, &pdx, &pdy, &pd2);
      if (pi >= 0) {
        steerTo(c, pdx, pdy, turn, 1.0f);
        if (pd2 < 1.45f) {
          float meal = 0.45f + cs[pi].e * conv;
          c.e = clampf(c.e + meal, 0, 2.2f);
          dropFood(cs[pi].x, cs[pi].y, 0.18f);
          cs[pi].alive = 0;
          // Classic LV: predator reproduces on a successful hunt
          if (nPred < 5) {
            float pup = 0.55f + 0.15f * (c.g[3] / 255.0f);
            if (spawn(c.x + (frand() - 0.5f) * 1.4f, c.y + (frand() - 0.5f) * 1.4f, c.g,
                      (uint8_t)(c.gen + 1), pup, KIND_PRED) >= 0) {
              c.e = clampf(c.e - 0.22f, 0.35f, 2.2f);
              nPred++;
            }
          }
        }
      } else {
        c.a += (frand() - 0.5f) * turn * 0.8f;
      }
    } else {
      // Always swim. Parking on sugar + camera-follow made prey look frozen.
      c.a += (frand() - 0.5f) * (0.22f + (c.g[1] / 255.0f) * 0.28f);
      float hdx = 0, hdy = 0, hd2 = 1e9f;
      int hi = nearestOf(c, KIND_PRED, range, &hdx, &hdy, &hd2);
      float fdx = 0, fdy = 0, fd2 = 1e9f;
      int fi = nearestFood(c, &fdx, &fdy, &fd2);
      float sdx = 0, sdy = 0, sd2 = 1e9f;
      int si = nearestOf(c, KIND_PREY, 9.0f, &sdx, &sdy, &sd2);
      if (hi >= 0 && hd2 < 49.0f) {
        steerTo(c, -hdx, -hdy, turn, 1.25f);
        speed += 0.20f;
      } else {
        if (fi >= 0 && fd2 > 1.8f) steerTo(c, fdx, fdy, turn, 0.45f);
        if (si >= 0 && sd2 > 2.8f && sd2 < 80.0f) steerTo(c, sdx, sdy, turn, 0.30f);
      }
      if (fi >= 0 && fd2 < 0.95f) {
        c.e = clampf(c.e + foods[fi].a * 0.62f, 0, 1.8f);
        foods[fi].a -= 0.40f;
        if (foods[fi].a < 0.12f) foods[fi].on = 0;
        c.a += 2.4f + (frand() - 0.5f) * 1.4f; // graze-and-go
      }
    }

    c.x = wrap(c.x + cosf(c.a) * speed, WW);
    c.y = wrap(c.y + sinf(c.a) * speed, WH);
    c.e -= metab;

    if (!pred && c.e > preySplit && nPrey < 10) {
      float baby = 0.38f;
      if (spawn(c.x + (frand() - 0.5f), c.y + (frand() - 0.5f), c.g, (uint8_t)(c.gen + 1), baby,
                KIND_PREY) >= 0) {
        c.e -= 0.28f;
        nPrey++;
      }
    }
    if (c.e < 0.04f) {
      if (!pred) dropFood(c.x, c.y, 0.28f);
      c.alive = 0;
      continue;
    }

    n++;
  }

  tally();
  // Keep both guilds on the dish without a hard reset
  if (nPred == 0 && nPrey >= 6 && (tick % 180) == 0)
    spawn(frand() * WW, frand() * WH, nullptr, 0, 0.9f, KIND_PRED);
  if (nPrey == 0 && nPred > 0 && (tick % 120) == 0)
    spawn(frand() * WW, frand() * WH, nullptr, 0, 0.7f, KIND_PREY);
  if (n == 0 && (tick % 40) == 0) crittersSeed();

  if (follow) {
    // Track hunters (or lag the herd) so prey stream across the 32×9, not sit in frame.
    float tx = 0, ty = 0;
    int tn = 0;
    for (int i = 0; i < MAX_C; i++) {
      if (!cs[i].alive) continue;
      if (nPred > 0 && cs[i].kind != KIND_PRED) continue;
      tx += wrapd(cs[i].x - camX, WW) + camX;
      ty += wrapd(cs[i].y - camY, WH) + camY;
      tn++;
    }
    if (tn > 0) {
      tx /= tn;
      ty /= tn;
      camX = wrap(camX + wrapd(tx - (VW * 0.5f) - camX, WW) * 0.04f, WW);
      camY = wrap(camY + wrapd(ty - (VH * 0.5f) - camY, WH) * 0.04f, WH);
    }
  }

  for (int y = 0; y < VH; y++)
    for (int x = 0; x < VW; x++)
      if (trail[y][x] > 4) trail[y][x] -= 4;
      else trail[y][x] = 0;
}

static void plot(uint8_t fb[][32], int x, int y, uint8_t c) {
  if ((unsigned)x >= VW || (unsigned)y >= VH) return;
  if (c > fb[y][x]) fb[y][x] = c;
}

static void plotTadpole(uint8_t fb[][32], int x, int y, float a, uint8_t core) {
  int dir = (int)lroundf(a / 0.785398f) & 7;
  static const int8_t fx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  static const int8_t fy[8] = {0, -1, -1, -1, 0, 1, 1, 1};
  plot(fb, x, y, core);
  plot(fb, x - fx[dir], y - fy[dir], core / 3); // tail
}

static void plotChevron(uint8_t fb[][32], int x, int y, float a, uint8_t core) {
  // 8-way arrow: bright tip + two dim wings
  int dir = (int)lroundf(a / 0.785398f) & 7;
  static const int8_t fx[8] = {1, 1, 0, -1, -1, -1, 0, 1};
  static const int8_t fy[8] = {0, -1, -1, -1, 0, 1, 1, 1};
  int8_t dx = fx[dir], dy = fy[dir];
  plot(fb, x, y, core);
  plot(fb, x + dx, y + dy, core); // nose
  uint8_t wing = core / 3;
  plot(fb, x - dy, y + dx, wing);
  plot(fb, x + dy, y - dx, wing);
}

void crittersRender(uint8_t fb[][32], uint8_t bright) {
  memset(fb, 0, VW * VH);
  uint8_t b = bright;

  for (int i = 0; i < MAX_F; i++) {
    if (!foods[i].on) continue;
    int x = (int)floorf(wrapd(foods[i].x - camX, WW));
    int y = (int)floorf(wrapd(foods[i].y - camY, WH));
    uint8_t c = (uint8_t)clampf(foods[i].a * b * 0.40f, 0, b * 0.42f);
    plot(fb, x, y, c);
  }

  for (int i = 0; i < MAX_C; i++) {
    if (!cs[i].alive) continue;
    int x = (int)lroundf(wrapd(cs[i].x - camX, WW));
    int y = (int)lroundf(wrapd(cs[i].y - camY, WH));
    uint8_t core = (uint8_t)clampf(b * (0.45f + 0.55f * clampf(cs[i].e, 0, 1)), 10, b);
    if ((unsigned)x < VW && (unsigned)y < VH) {
      uint8_t dep = cs[i].kind == KIND_PRED ? 18 : (uint8_t)(6 + (cs[i].g[5] / 255.0f) * 28);
      if (trail[y][x] < dep) trail[y][x] = dep;
    }
    if (cs[i].kind == KIND_PRED) plotChevron(fb, x, y, cs[i].a, core);
    else plotTadpole(fb, x, y, cs[i].a, core);
  }

  for (int y = 0; y < VH; y++)
    for (int x = 0; x < VW; x++)
      if (trail[y][x] > fb[y][x]) fb[y][x] = trail[y][x];

  if (shakeLeft > 0.2f) {
    uint8_t rim = (uint8_t)clampf(shakeLeft * b * 0.5f, 0, b);
    for (int x = 0; x < VW; x++) {
      plot(fb, x, 0, rim);
      plot(fb, x, VH - 1, rim);
    }
  }
}

void crittersFeed(int viewX, int viewY) {
  float x = camX + (float)viewX + 0.5f;
  float y = camY + (float)viewY + 0.5f;
  dropFood(x, y, 1.05f);
  dropFood(x + 0.8f, y - 0.4f, 0.7f);
}

void crittersFeedScatter() {
  for (int i = 0; i < 6; i++) dropFood(frand() * WW, frand() * WH, 0.8f);
}

void crittersShake(float amp) {
  shakeLeft = clampf(amp, 0.3f, 2.4f);
  for (int i = 0; i < MAX_C; i++) {
    if (!cs[i].alive) continue;
    cs[i].a += (frand() - 0.5f) * 3.2f * amp;
    cs[i].e *= 0.96f;
  }
  for (int i = 0; i < MAX_F; i++) {
    if (!foods[i].on) continue;
    foods[i].x = wrap(foods[i].x + (frand() - 0.5f) * 4.0f * amp, WW);
    foods[i].y = wrap(foods[i].y + (frand() - 0.5f) * 2.2f * amp, WH);
  }
}

void crittersDropHunter() {
  spawn(camX + 16.0f, camY + 4.0f, nullptr, 0, 1.0f, KIND_PRED);
  tally();
}

uint8_t crittersAlive() { return (uint8_t)(nPrey + nPred); }
uint16_t crittersBirths() { return births; }
uint8_t crittersMaxGen() {
  uint8_t m = 0;
  for (int i = 0; i < MAX_C; i++)
    if (cs[i].alive && cs[i].gen > m) m = cs[i].gen;
  return m;
}
uint16_t crittersAdc() { return adcRaw; }
float crittersVbatt() {
  float v = (adcRaw / 1024.0f) * (10.0f + 2.49f) / 2.49f;
  return v;
}
void crittersSetBrightAuto(bool on) { brightAuto = on; }
bool crittersBrightAuto() { return brightAuto; }
void crittersSetFollow(bool on) { follow = on; }
bool crittersFollow() { return follow; }
void crittersNudgeCam(int dx, int dy) {
  follow = false;
  camX = wrap(camX + dx, WW);
  camY = wrap(camY + dy, WH);
}

static ESP8266WebServer *srv = nullptr;
static uint8_t *liveFb = nullptr;

static void sendState() {
  tally();
  char buf[700];
  int food = 0;
  for (int i = 0; i < MAX_F; i++)
    if (foods[i].on) food++;
  snprintf(buf, sizeof(buf),
           "{\"ok\":true,\"sim\":\"tank\",\"w\":32,\"h\":9,\"alive\":%u,\"prey\":%u,\"pred\":%u,"
           "\"births\":%u,\"gen\":%u,\"food\":%d,\"adc\":%u,\"vbatt\":%.2f,"
           "\"follow\":%s,\"shake\":%.2f,\"cam\":[%.1f,%.1f],"
           "\"imu\":false,\"ldr\":false,"
           "\"note\":\"prey=dot pred=chevron; ADC=LiPo; easyC=sensor port\"}",
           (unsigned)(nPrey + nPred), (unsigned)nPrey, (unsigned)nPred, (unsigned)births,
           (unsigned)crittersMaxGen(), food, (unsigned)adcRaw, crittersVbatt(),
           follow ? "true" : "false", shakeLeft, camX, camY);
  srv->send(200, "application/json", buf);
}

static void sendBoardJson() {
  tally();
  static char buf[288 * 2 + 280];
  char *p = buf;
  p += snprintf(p, 220,
                "{\"ok\":true,\"sim\":\"tank\",\"w\":32,\"h\":9,\"alive\":%u,\"prey\":%u,\"pred\":%u,"
                "\"births\":%u,\"gen\":%u,\"adc\":%u,\"vbatt\":%.2f,\"px\":\"",
                (unsigned)(nPrey + nPred), (unsigned)nPrey, (unsigned)nPred, (unsigned)births,
                (unsigned)crittersMaxGen(), (unsigned)adcRaw, crittersVbatt());
  if (liveFb) {
    static const char *hex = "0123456789abcdef";
    for (int i = 0; i < VW * VH; i++) {
      uint8_t v = liveFb[i];
      *p++ = hex[v >> 4];
      *p++ = hex[v & 0xf];
    }
  }
  *p++ = '"';
  *p++ = '}';
  *p = 0;
  srv->send(200, "application/json", buf);
}

static int argI(const char *k, int d) {
  if (!srv->hasArg(k)) return d;
  return srv->arg(k).toInt();
}

void crittersHttpBegin(ESP8266WebServer &http) {
  srv = &http;
  http.on("/tank", HTTP_GET, sendState);
  http.on("/tank.json", HTTP_GET, sendState);
  http.on("/fb.json", HTTP_GET, sendBoardJson);
  http.on("/tank/feed", HTTP_POST, []() {
    focusLatch = true;
    crittersFeed(argI("x", random(4, 28)), argI("y", random(1, 8)));
    sendState();
  });
  http.on("/tank/scatter", HTTP_POST, []() {
    focusLatch = true;
    crittersFeedScatter();
    sendState();
  });
  http.on("/tank/shake", HTTP_POST, []() {
    focusLatch = true;
    crittersShake(srv->hasArg("amp") ? srv->arg("amp").toFloat() : 1.2f);
    sendState();
  });
  http.on("/tank/seed", HTTP_POST, []() {
    focusLatch = true;
    crittersSeed();
    sendState();
  });
  http.on("/tank/hunt", HTTP_POST, []() {
    focusLatch = true;
    crittersDropHunter();
    sendState();
  });
  http.on("/tank/follow", HTTP_POST, []() {
    crittersSetFollow(argI("on", 1) != 0);
    sendState();
  });
  http.on("/tank/focus", HTTP_POST, []() {
    focusLatch = true;
    sendState();
  });
  http.on("/tank/pan", HTTP_POST, []() {
    crittersNudgeCam(argI("dx", 0), argI("dy", 0));
    sendState();
  });
}

void crittersSetFb(uint8_t (*fb)[32]) { liveFb = &fb[0][0]; }
