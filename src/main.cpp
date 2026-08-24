/*
 * Maker LEDDisplay2 v2.2 — variety reel
 * 32x9 greyscale (2x IS31FL3731, SDA=GPIO4 SCL=GPIO5)
 *
 * Auto-cycles modes. Serial @ 9600, newline:
 *   a           auto reel (default)
 *   0..9        lock a mode
 *   n           next
 *   bN          brightness 0-255 (default 90)
 *   t <text>    set scroll text + jump to text mode
 *   m           next Rage promo line + text mode
 *   k NAME      demo color name (this PCB is mono white)
 *   d           drop (wave)
 *   ?           help
 *
 * 0 wave  1 sine  2 text  3 clock  4 bars
 * 5 spark 6 rain  7 bounce 8 life  9 pulse
 */

#include <Arduino.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <IS31FL3731.h>
#include "wifi_ota.h"
#include "critters.h"
#include "war.h"
#include "card.h"

#define WAVE_W 32
#define WAVE_H 9

IS31FL3731 led(2);

enum Mode : uint8_t {
  MODE_WAVE = 0,
  MODE_SINE,
  MODE_TEXT,
  MODE_CLOCK,
  MODE_BARS,
  MODE_SPARK,
  MODE_RAIN,
  MODE_BOUNCE,
  MODE_LIFE,
  MODE_PULSE,
  MODE_PLASMA,
  MODE_TUNNEL,
  MODE_FIRE,
  MODE_LISSA,
  MODE_CA,
  MODE_STARS,
  MODE_XOR,
  MODE_RINGS,
  MODE_CYCLE,
  MODE_GRID,
  MODE_DISC,
  MODE_TANK,
  MODE_WAR,
  MODE_COUNT
};

static const char *MODE_NAME[MODE_COUNT] = {
    "wave", "sine", "text", "clock", "bars",
    "spark", "rain", "bounce", "life", "pulse",
    "plasma", "tunnel", "fire", "lissa", "ca",
    "stars", "xor", "rings", "cycle", "grid", "disc", "tank", "war"};

static const Mode VISUALS[] = {
    MODE_CYCLE, MODE_GRID, MODE_DISC, MODE_TANK, MODE_WAR, MODE_WAVE, MODE_PLASMA,
    MODE_TUNNEL, MODE_FIRE, MODE_LISSA, MODE_STARS, MODE_RINGS,
    MODE_SINE, MODE_LIFE, MODE_CA, MODE_XOR, MODE_PULSE,
    MODE_BARS, MODE_RAIN};
static const uint8_t VISUAL_N = sizeof(VISUALS) / sizeof(VISUALS[0]);
static Mode mode = MODE_TEXT;
static bool autoReel = true;
static uint8_t visualIdx = 0;
static bool autoWantText = true;

static void enterMode(Mode m); // fwd

static void autoNext() {
  if (autoWantText) {
    autoWantText = false;
    enterMode(MODE_TEXT);
  } else {
    autoWantText = true;
    enterMode(VISUALS[visualIdx]);
    visualIdx = (uint8_t)((visualIdx + 1) % VISUAL_N);
  }
}
static const char *MODE_TITLE[MODE_COUNT] = {
    "WAVE", "SINE", "TEXT", "CLOCK", "BARS",
    "SPARK", "RAIN", "BOUNCE", "LIFE", "PULSE",
    "PLASMA", "TUNNEL", "FIRE", "LISSA", "CA",
    "STARS", "XOR", "RINGS", "CYCLE", "GRID", "DISC", "TANK", "WAR ZONE"};

static void startTitle(Mode m);
static void renderTitle();
static bool titleLive();
static uint32_t modeStarted = 0;
static uint32_t reelMs = 8000;
static uint8_t brightScale = 90;
static const char *colorName = "white";
static uint32_t frame = 0;
static uint8_t fb[WAVE_H][WAVE_W];

static float u[WAVE_H][WAVE_W];
static float v[WAVE_H][WAVE_W];
static float c2 = 0.20f;
static float damp = 0.994f;
static uint32_t lastDropMs = 0;
static uint32_t dropEveryMs = 1400;
static const float kOff = 0.38f;

static const char *const PROMO[] = {
    "TRON  ",
    "I FIGHT FOR MARC  ",
    "ALWAYS ON  ",
    "SOVEREIGN LAYER  ",
    "ZERO MARGINAL COST  ",
    "THE CONTINUOUS THREAD  ",
};
static const uint8_t PROMO_N = sizeof(PROMO) / sizeof(PROMO[0]);
static uint8_t promoIdx = 0;
static bool usePromoBank = true;
static char scrollText[96] = "TRON  ";
static int16_t scrollX = WAVE_W;

static void loadPromo(uint8_t i) {
  promoIdx = (uint8_t)(i % PROMO_N);
  strncpy(scrollText, PROMO[promoIdx], sizeof(scrollText) - 1);
  scrollText[sizeof(scrollText) - 1] = 0;
  scrollX = WAVE_W;
  usePromoBank = true;
}

static uint8_t life[WAVE_H][WAVE_W];
static uint8_t life2[WAVE_H][WAVE_W];

static float ballX = 8, ballY = 4, ballVX = 0.35f, ballVY = 0.22f;

static uint8_t rainY[WAVE_W];
static uint8_t rainV[WAVE_W];

static float sparkHist[WAVE_W];
static uint8_t heat[WAVE_H][WAVE_W];
static uint8_t caRow[WAVE_W];
static float starX[12], starY[12], starZ[12];
static int8_t cycX[2], cycY[2], cycDX[2], cycDY[2];

static void fireSeed() {
  memset(heat, 0, sizeof(heat));
}

static void caSeed() {
  memset(caRow, 0, sizeof(caRow));
  caRow[WAVE_W / 2] = 1;
}

static void starsSeed() {
  for (int i = 0; i < 12; i++) {
    starX[i] = (float)(random(0, 200) - 100) / 50.0f;
    starY[i] = (float)(random(0, 200) - 100) / 80.0f;
    starZ[i] = 0.4f + random(0, 80) / 100.0f;
  }
}

static void cycleSeed() {
  memset(heat, 0, sizeof(heat));
  cycX[0] = 2; cycY[0] = 2; cycDX[0] = 1; cycDY[0] = 0;
  cycX[1] = 29; cycY[1] = 6; cycDX[1] = -1; cycDY[1] = 0;
}

static inline float clampf(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}

static void fbClear() { memset(fb, 0, sizeof(fb)); }

static void fbPlot(int x, int y, uint8_t c) {
  if ((unsigned)x >= WAVE_W || (unsigned)y >= WAVE_H) return;
  if (c > fb[y][x]) fb[y][x] = c;
}

static void fbShow() {
  for (int y = 0; y < WAVE_H; y++)
    for (int x = 0; x < WAVE_W; x++)
      led.drawPixel(x, y, fb[y][x]);
  led.display();
}

// 3x5 font, bit0 = top row. 5 rows, 3 cols.
static const uint8_t FONT3x5[][5] PROGMEM = {
    {0, 0, 0, 0, 0},          // space
    {2, 2, 2, 0, 2},          // !
    {5, 5, 0, 0, 0},          // "
    {5, 7, 5, 7, 5},          // #
    {2, 7, 6, 3, 7},          // $
    {5, 1, 2, 4, 5},          // %
    {2, 5, 2, 5, 3},          // &
    {2, 2, 0, 0, 0},          // '
    {1, 2, 2, 2, 1},          // (
    {4, 2, 2, 2, 4},          // )
    {0, 5, 2, 5, 0},          // *
    {0, 2, 7, 2, 0},          // +
    {0, 0, 0, 2, 4},          // ,
    {0, 0, 7, 0, 0},          // -
    {0, 0, 0, 0, 2},          // .
    {1, 1, 2, 4, 4},          // /
    {7, 5, 5, 5, 7},          // 0
    {2, 6, 2, 2, 7},          // 1
    {7, 1, 7, 4, 7},          // 2
    {7, 1, 7, 1, 7},          // 3
    {5, 5, 7, 1, 1},          // 4
    {7, 4, 7, 1, 7},          // 5
    {7, 4, 7, 5, 7},          // 6
    {7, 1, 1, 1, 1},          // 7
    {7, 5, 7, 5, 7},          // 8
    {7, 5, 7, 1, 7},          // 9
    {0, 2, 0, 2, 0},          // :
};

static void glyphBits(char ch, uint8_t out[5]) {
  if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
  int idx = -1;
  if (ch == ' ') idx = 0;
  else if (ch >= '!' && ch <= ':') idx = 1 + (ch - '!');
  else {
    // crude A-Z as 3x5
    static const uint8_t AZ[][5] PROGMEM = {
        {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3}, {6, 5, 5, 5, 6},
        {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4}, {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5},
        {7, 2, 2, 2, 7}, {1, 1, 1, 5, 2}, {5, 6, 4, 6, 5}, {4, 4, 4, 4, 7},
        {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {2, 5, 5, 5, 2}, {6, 5, 6, 4, 4},
        {2, 5, 5, 7, 3}, {6, 5, 6, 6, 5}, {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2},
        {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5},
        {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}};
    if (ch >= 'A' && ch <= 'Z') {
      memcpy_P(out, AZ[ch - 'A'], 5);
      return;
    }
    memset(out, 0, 5);
    return;
  }
  memcpy_P(out, FONT3x5[idx], 5);
}

static void drawChar(int x0, int y0, char ch, uint8_t c) {
  uint8_t g[5];
  glyphBits(ch, g);
  for (int r = 0; r < 5; r++) {
    uint8_t bits = g[r];
    for (int col = 0; col < 3; col++) {
      if (bits & (4 >> col)) fbPlot(x0 + col, y0 + r, c);
    }
  }
}

static void drawText(int x0, int y0, const char *s, uint8_t c) {
  int x = x0;
  while (*s) {
    drawChar(x, y0, *s++, c);
    x += 4;
  }
}

static void startTitle(Mode m) {
  if (m == MODE_TEXT) {
    cardClear();
    return;
  }
  cardShow(MODE_TITLE[m], (m == MODE_WAR || m == MODE_TANK) ? 2200 : 1600);
}

static void renderTitle() {
  fbClear();
  const char *s = cardText();
  int n = (int)strlen(s);
  if (!n) return;
  int w = n * 4 - 1;
  int x0 = (WAVE_W - w) / 2;
  uint8_t c = brightScale;
  // fade is handled by caller timeout; keep a short fade via leftover isn't here
  drawText(x0, 2, s, c ? c : 8);
  for (int x = x0; x < x0 + w && x < WAVE_W; x++)
    fbPlot(x, 8, c / 4);
}

static bool titleLive() { return cardLive(); }

static uint16_t sparsePx(float n) {
  if (n < kOff) return 0;
  float t = (n - kOff) / (1.0f - kOff);
  t = t * t;
  uint16_t c = (uint16_t)(t * brightScale);
  return c < 4 ? 0 : c;
}

static void waveClear() {
  memset(u, 0, sizeof(u));
  memset(v, 0, sizeof(v));
}

static void dropAt(int cx, int cy, float amp) {
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      float dx = (float)(x - cx);
      float dy = (float)(y - cy) * 1.6f;
      float r2 = dx * dx + dy * dy;
      if (r2 < 20.0f) {
        float bump = amp * expf(-r2 * 0.35f);
        u[y][x] += bump;
        v[y][x] += bump * 0.85f;
      }
    }
  }
}

static void waveStep() {
  float next[WAVE_H][WAVE_W];
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      float left = u[y][x > 0 ? x - 1 : x];
      float right = u[y][x < WAVE_W - 1 ? x + 1 : x];
      float up = u[y > 0 ? y - 1 : y][x];
      float down = u[y < WAVE_H - 1 ? y + 1 : y][x];
      float lap = (left + right + up + down) - 4.0f * u[y][x];
      float n = (2.0f * u[y][x] - v[y][x]) + c2 * lap;
      n *= damp;
      next[y][x] = clampf(n, -1.5f, 1.5f);
    }
  }
  memcpy(v, u, sizeof(u));
  memcpy(u, next, sizeof(u));
}

static void renderWave() {
  fbClear();
  float t = frame * 0.07f;
  for (int x = 0; x < WAVE_W; x++) {
    float e = 0;
    for (int y = 0; y < WAVE_H; y++) e += u[y][x];
    e /= (float)WAVE_H;
    float yf = 4.0f + e * 3.6f + 1.6f * sinf(x * 0.38f - t);
    int y0 = (int)floorf(yf);
    float frac = yf - y0;
    if (y0 >= 0 && y0 < WAVE_H)
      fbPlot(x, y0, (uint8_t)((1.0f - frac) * brightScale));
    if (y0 + 1 >= 0 && y0 + 1 < WAVE_H)
      fbPlot(x, y0 + 1, (uint8_t)(frac * brightScale * 0.55f));
  }
  for (int y = 0; y < WAVE_H; y++)
    for (int x = 0; x < WAVE_W; x++)
      fbPlot(x, y, (uint8_t)sparsePx(fabsf(u[y][x])));
}

static void renderSine() {
  fbClear();
  float t = frame * 0.08f;
  for (int x = 0; x < WAVE_W; x++) {
    float yf = sinf(x * 0.4f - t) * 3.1f + 4.0f;
    int y0 = (int)floorf(yf);
    float frac = yf - y0;
    if (y0 >= 0 && y0 < WAVE_H)
      fbPlot(x, y0, (uint8_t)((1.0f - frac) * brightScale));
    if (y0 + 1 >= 0 && y0 + 1 < WAVE_H)
      fbPlot(x, y0 + 1, (uint8_t)(frac * brightScale * 0.4f));
    int yi = (int)lroundf(sinf(x * 0.22f + t * 0.6f + 1.2f) * 2.2f + 4.0f);
    if (yi >= 0 && yi < WAVE_H) fbPlot(x, yi, brightScale / 3);
  }
}

static void renderText() {
  fbClear();
  drawText(scrollX, 2, scrollText, brightScale);
  if ((frame % 2) == 0) scrollX--;
  int w = (int)strlen(scrollText) * 4;
  if (scrollX < -w) {
    if (autoReel) {
      if (usePromoBank) promoIdx = (uint8_t)((promoIdx + 1) % PROMO_N);
      autoNext();
      return;
    }
    if (usePromoBank) loadPromo((uint8_t)(promoIdx + 1));
    else scrollX = WAVE_W;
  }
}

static void renderClock() {
  fbClear();
  uint32_t sec = millis() / 1000;
  uint32_t mm = (sec / 60) % 60;
  uint32_t ss = sec % 60;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)mm, (unsigned long)ss);
  drawText(2, 2, buf, brightScale);
  // ticking baseline
  int tick = (int)(sec % WAVE_W);
  fbPlot(tick, 8, brightScale / 2);
}

static void renderBars() {
  fbClear();
  float t = frame * 0.09f;
  for (int x = 0; x < WAVE_W; x++) {
    float n = 0.5f + 0.5f * sinf(x * 0.45f + t) * cosf(x * 0.17f - t * 0.7f);
    n = powf(clampf(n, 0, 1), 1.3f);
    int h = (int)lroundf(n * 8.0f);
    for (int y = 0; y < h; y++) {
      uint8_t c = (uint8_t)(brightScale * (0.35f + 0.65f * y / 8.0f));
      fbPlot(x, WAVE_H - 1 - y, c);
    }
  }
}

static void renderSpark() {
  fbClear();
  // walk a fake series
  memmove(sparkHist, sparkHist + 1, (WAVE_W - 1) * sizeof(float));
  sparkHist[WAVE_W - 1] =
      0.55f + 0.25f * sinf(frame * 0.11f) + 0.12f * sinf(frame * 0.03f) +
      (random(0, 20) - 10) * 0.008f;
  for (int x = 1; x < WAVE_W; x++) {
    int y0 = (int)lroundf(clampf(sparkHist[x - 1], 0, 1) * 8.0f);
    int y1 = (int)lroundf(clampf(sparkHist[x], 0, 1) * 8.0f);
    int y = y0;
    int dir = (y1 >= y0) ? 1 : -1;
    while (true) {
      fbPlot(x, 8 - y, brightScale);
      if (y == y1) break;
      y += dir;
    }
  }
}

static void rainInit() {
  for (int x = 0; x < WAVE_W; x++) {
    rainY[x] = random(0, WAVE_H + 6);
    rainV[x] = 1 + random(0, 2);
  }
}

static void renderRain() {
  fbClear();
  for (int x = 0; x < WAVE_W; x += 2) {
    int y = rainY[x];
    if (y >= 0 && y < WAVE_H) fbPlot(x, y, brightScale);
    if (y - 1 >= 0 && y - 1 < WAVE_H) fbPlot(x, y - 1, brightScale / 3);
    rainY[x] += rainV[x];
    if (rainY[x] > WAVE_H + 4) {
      rainY[x] = 0;
      rainV[x] = 1 + random(0, 2);
    }
  }
}

static void renderBounce() {
  fbClear();
  ballX += ballVX;
  ballY += ballVY;
  if (ballX < 0) { ballX = 0; ballVX = -ballVX; }
  if (ballX > WAVE_W - 1) { ballX = WAVE_W - 1; ballVX = -ballVX; }
  if (ballY < 0) { ballY = 0; ballVY = -ballVY; }
  if (ballY > WAVE_H - 1) { ballY = WAVE_H - 1; ballVY = -ballVY; }
  int x = (int)lroundf(ballX);
  int y = (int)lroundf(ballY);
  fbPlot(x, y, brightScale);
  fbPlot(x - 1, y, brightScale / 4);
  fbPlot(x + 1, y, brightScale / 4);
  fbPlot(x, y - 1, brightScale / 4);
  fbPlot(x, y + 1, brightScale / 4);
}

static void lifeSeed() {
  memset(life, 0, sizeof(life));
  for (int i = 0; i < 70; i++)
    life[random(0, WAVE_H)][random(0, WAVE_W)] = 1;
}

static int lifeN(int x, int y) {
  int n = 0;
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      if (!dx && !dy) continue;
      int xx = (x + dx + WAVE_W) % WAVE_W;
      int yy = (y + dy + WAVE_H) % WAVE_H;
      n += life[yy][xx];
    }
  return n;
}

static void renderLife() {
  fbClear();
  if ((frame % 4) == 0) {
    for (int y = 0; y < WAVE_H; y++) {
      for (int x = 0; x < WAVE_W; x++) {
        int n = lifeN(x, y);
        life2[y][x] = (life[y][x] ? (n == 2 || n == 3) : (n == 3));
      }
    }
    memcpy(life, life2, sizeof(life));
  }
  for (int y = 0; y < WAVE_H; y++)
    for (int x = 0; x < WAVE_W; x++)
      if (life[y][x]) fbPlot(x, y, brightScale);
}

static void renderPulse() {
  fbClear();
  float t = (frame % 40) / 40.0f;
  float beat = expf(-t * 6.0f) + 0.35f * expf(-fabsf(t - 0.22f) * 14.0f);
  int r = (int)lroundf(1.0f + beat * 5.0f);
  int cx = 16, cy = 4;
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy) * 2.4f);
      float n = clampf(1.0f - fabsf(d - r) * 0.9f, 0, 1);
      n = powf(n, 1.6f);
      if (n > 0.15f) fbPlot(x, y, (uint8_t)(n * brightScale));
    }
  }
}

static void renderPlasma() {
  fbClear();
  float t = frame * 0.07f;
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      float n = sinf(x * 0.33f + t);
      n += sinf(y * 0.71f - t * 1.3f);
      n += sinf((x + y) * 0.22f + t * 0.6f);
      n = n * 0.33f + 0.5f;
      n = powf(clampf(n, 0, 1), 1.8f);
      if (n > 0.22f) fbPlot(x, y, (uint8_t)(n * brightScale));
    }
  }
}

static void renderTunnel() {
  fbClear();
  float t = frame * 0.08f;
  int cx = 16, cy = 4;
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      float dx = (x - cx) * 0.55f;
      float dy = (y - cy);
      float r = sqrtf(dx * dx + dy * dy) + 0.2f;
      float a = atan2f(dy, dx);
      float n = sinf(r * 2.2f - t * 3.0f) * cosf(a * 3.0f + t);
      n = clampf(n, 0, 1);
      n = powf(n, 1.5f);
      if (n > 0.28f) fbPlot(x, y, (uint8_t)(n * brightScale));
    }
  }
}

static void renderFire() {
  fbClear();
  for (int x = 0; x < WAVE_W; x++) {
    int v = heat[WAVE_H - 1][x] + random(0, 90) - 30;
    heat[WAVE_H - 1][x] = (uint8_t)clampf((float)v, 0, 255);
    if (random(0, 18) == 0) heat[WAVE_H - 1][x] = 220;
  }
  for (int y = 0; y < WAVE_H - 1; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      int l = heat[y + 1][x > 0 ? x - 1 : x];
      int c = heat[y + 1][x];
      int r = heat[y + 1][x < WAVE_W - 1 ? x + 1 : x];
      int u = (y + 2 < WAVE_H) ? heat[y + 2][x] : c;
      int n = (l + c + r + u) / 4 - 12;
      heat[y][x] = (uint8_t)clampf((float)n, 0, 255);
    }
  }
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      float n = heat[y][x] / 255.0f;
      n = powf(n, 1.35f);
      if (n > 0.12f) fbPlot(x, y, (uint8_t)(n * brightScale));
    }
  }
}

static void renderLissa() {
  fbClear();
  float t = frame * 0.05f;
  for (int i = 0; i < 48; i++) {
    float p = t + i * 0.12f;
    float x = 15.5f + 14.0f * sinf(p * 3.0f);
    float y = 4.0f + 3.4f * sinf(p * 2.0f + 0.4f);
    uint8_t c = (uint8_t)(brightScale * (1.0f - i / 56.0f));
    fbPlot((int)lroundf(x), (int)lroundf(y), c);
  }
}

static void renderCa() {
  fbClear();
  if ((frame % 3) == 0) {
    uint8_t nxt[WAVE_W];
    for (int x = 0; x < WAVE_W; x++) {
      uint8_t l = caRow[(x + WAVE_W - 1) % WAVE_W];
      uint8_t c = caRow[x];
      uint8_t r = caRow[(x + 1) % WAVE_W];
      uint8_t rule = (uint8_t)((l << 2) | (c << 1) | r); // rule 30
      nxt[x] = (30 >> rule) & 1;
    }
    memcpy(caRow, nxt, sizeof(caRow));
    memmove(&heat[0][0], &heat[1][0], (WAVE_H - 1) * WAVE_W);
    memcpy(heat[WAVE_H - 1], caRow, WAVE_W);
  }
  for (int y = 0; y < WAVE_H; y++)
    for (int x = 0; x < WAVE_W; x++)
      if (heat[y][x]) fbPlot(x, y, brightScale);
}

static void renderStars() {
  fbClear();
  for (int i = 0; i < 12; i++) {
    starZ[i] -= 0.045f;
    if (starZ[i] <= 0.08f) {
      starX[i] = (float)(random(0, 200) - 100) / 50.0f;
      starY[i] = (float)(random(0, 200) - 100) / 80.0f;
      starZ[i] = 1.2f;
    }
    int x = (int)lroundf(16.0f + starX[i] / starZ[i] * 10.0f);
    int y = (int)lroundf(4.0f + starY[i] / starZ[i] * 4.0f);
    float b = clampf(1.2f - starZ[i], 0.15f, 1.0f);
    fbPlot(x, y, (uint8_t)(b * brightScale));
  }
}

static void renderXor() {
  fbClear();
  int t = frame;
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      int v = (x * 11 + t) ^ (y * 27 - t);
      float n = ((v & 0x3f) / 63.0f);
      n = powf(n, 2.2f);
      if (n > 0.28f) fbPlot(x, y, (uint8_t)(n * brightScale));
    }
  }
}

static void renderRings() {
  fbClear();
  float t = frame * 0.09f;
  for (int k = 0; k < 3; k++) {
    float cx = 8.0f + k * 8.0f + 3.0f * sinf(t * 0.7f + k);
    float cy = 4.0f + 2.2f * cosf(t * 0.9f + k * 1.3f);
    float rad = 1.2f + fmodf(t * 1.6f + k * 1.1f, 6.0f);
    for (int y = 0; y < WAVE_H; y++) {
      for (int x = 0; x < WAVE_W; x++) {
        float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy) * 2.2f);
        float n = clampf(1.0f - fabsf(d - rad) * 1.4f, 0, 1);
        n = powf(n, 1.7f);
        if (n > 0.2f) fbPlot(x, y, (uint8_t)(n * brightScale));
      }
    }
  }
}

static void renderCycle() {
  fbClear();
  for (int y = 0; y < WAVE_H; y++)
    for (int x = 0; x < WAVE_W; x++)
      if (heat[y][x]) {
        heat[y][x] = (heat[y][x] > 8) ? (uint8_t)(heat[y][x] - 8) : 0;
        fbPlot(x, y, heat[y][x]);
      }
  if ((frame % 2) == 0) {
    for (int i = 0; i < 2; i++) {
      if (random(0, 7) == 0) {
        if (random(0, 2)) { int t = cycDX[i]; cycDX[i] = -cycDY[i]; cycDY[i] = t; }
        else { int t = cycDX[i]; cycDX[i] = cycDY[i]; cycDY[i] = -t; }
      }
      int nx = cycX[i] + cycDX[i];
      int ny = cycY[i] + cycDY[i];
      if (nx < 0 || nx >= WAVE_W || ny < 0 || ny >= WAVE_H || heat[ny][nx] > 40) {
        cycDX[i] = -cycDX[i];
        cycDY[i] = -cycDY[i];
        nx = cycX[i] + cycDX[i];
        ny = cycY[i] + cycDY[i];
        if (nx < 0 || nx >= WAVE_W || ny < 0 || ny >= WAVE_H) {
          nx = (cycX[i] + WAVE_W) % WAVE_W;
          ny = (cycY[i] + WAVE_H) % WAVE_H;
        }
      }
      cycX[i] = (int8_t)nx;
      cycY[i] = (int8_t)ny;
      heat[cycY[i]][cycX[i]] = brightScale;
    }
  }
  fbPlot(cycX[0], cycY[0], brightScale);
  fbPlot(cycX[1], cycY[1], brightScale);
}

static void renderGrid() {
  fbClear();
  float t = frame * 0.06f;
  for (int i = 0; i < 6; i++) {
    float z = fmodf(t * 0.35f + i * 0.18f, 1.0f);
    int y = (int)lroundf(3.0f + z * 6.0f);
    uint8_t c = (uint8_t)(brightScale * (0.15f + 0.85f * z));
    if (y >= 0 && y < WAVE_H)
      for (int x = 0; x < WAVE_W; x++) fbPlot(x, y, c / 3);
  }
  for (int k = -4; k <= 4; k++) {
    for (int y = 3; y < WAVE_H; y++) {
      float p = (y - 2) / 7.0f;
      int x = (int)lroundf(16.0f + k * (2.0f + 6.0f * p) + sinf(t) * 0.4f);
      fbPlot(x, y, (uint8_t)(brightScale * (0.2f + 0.6f * p)));
    }
  }
  for (int x = 0; x < WAVE_W; x++) fbPlot(x, 3, brightScale / 5);
}

static void renderDisc() {
  fbClear();
  float t = frame * 0.08f;
  float cx = 16.0f + 10.0f * sinf(t);
  float cy = 4.0f + 2.4f * sinf(t * 2.0f);
  float rad = 1.4f + 0.35f * sinf(t * 3.0f);
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      float d = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy) * 2.1f);
      float n = clampf(1.0f - fabsf(d - rad) * 1.8f, 0, 1);
      n = powf(n, 1.4f);
      if (n > 0.2f) fbPlot(x, y, (uint8_t)(n * brightScale));
    }
  }
  fbPlot((int)lroundf(cx), (int)lroundf(cy), brightScale);
}

static void enterMode(Mode m) {
  mode = m;
  modeStarted = millis();
  frame = 0;
  reelMs = (m == MODE_TEXT) ? 14000 : ((m == MODE_TANK || m == MODE_WAR) ? 30000 : 8000);
  if (m == MODE_WAVE) {
    waveClear();
    dropAt(8, 4, 1.0f);
    dropAt(22, 3, 0.85f);
    dropAt(15, 6, 0.7f);
    lastDropMs = millis();
  } else if (m == MODE_TEXT) {
    if (usePromoBank) loadPromo(promoIdx);
    else scrollX = WAVE_W;
    reelMs = 7000;
  } else if (m == MODE_RAIN) {
    rainInit();
  } else if (m == MODE_LIFE) {
    lifeSeed();
  } else if (m == MODE_BOUNCE) {
    ballX = 6;
    ballY = 3;
    ballVX = 0.35f;
    ballVY = 0.22f;
  } else if (m == MODE_SPARK) {
    for (int i = 0; i < WAVE_W; i++) sparkHist[i] = 0.5f;
  } else if (m == MODE_FIRE) {
    fireSeed();
  } else if (m == MODE_CA) {
    caSeed();
    memset(heat, 0, sizeof(heat));
  } else if (m == MODE_STARS) {
    starsSeed();
  } else if (m == MODE_CYCLE) {
    cycleSeed();
  }
  if (!cardLive()) startTitle(m);
  Serial.print(F("mode="));
  Serial.println(MODE_NAME[m]);
}

static void printHelp() {
  Serial.println(F("RAGE INDUSTRIES — variety reel"));
  Serial.println(F("e=tank  r=war  f=feed  z=shake  a=auto  0-9=lock  n=next  bN"));
  Serial.println(F("POST /mode name=war|tank|wave|...  GET /mode.json"));
  Serial.println(F("0 wave 1 sine 2 text 3 clock 4 bars"));
  Serial.println(F("5 spark 6 rain 7 bounce 8 life 9 pulse"));
  Serial.println(F("this panel is cool-white; k only tags the demo reel"));
}

static void handleLine(char *line) {
  while (*line == ' ') line++;
  if (!*line) return;
  char cmd = line[0];
  char *arg = line + 1;
  while (*arg == ' ') arg++;

  if (cmd >= '0' && cmd <= '9') {
    autoReel = false;
    enterMode((Mode)(cmd - '0'));
    return;
  }
  switch (cmd) {
    case 'a':
    case 'A':
      autoReel = true;
      Serial.println(F("auto=on  text↔visual"));
      autoWantText = false;
      enterMode(MODE_TEXT);
      break;
    case 'n':
    case 'N':
      if (autoReel) autoNext();
      else enterMode((Mode)((mode + 1) % MODE_COUNT));
      break;
    case 't':
    case 'T':
      if (*arg) {
        strncpy(scrollText, arg, sizeof(scrollText) - 1);
        scrollText[sizeof(scrollText) - 1] = 0;
        usePromoBank = false;
      }
      autoReel = false;
      enterMode(MODE_TEXT);
      break;
    case 'm':
    case 'M':
      usePromoBank = true;
      loadPromo((uint8_t)(promoIdx + 1));
      autoReel = false;
      enterMode(MODE_TEXT);
      Serial.print(F("promo="));
      Serial.println(scrollText);
      break;
    case 'k':
    case 'K': {
      if (!*arg) arg = (char *)"white";
      if (!strcasecmp(arg, "amber") || !strcasecmp(arg, "gold"))
        colorName = "amber";
      else if (!strcasecmp(arg, "violet") || !strcasecmp(arg, "purple"))
        colorName = "violet";
      else if (!strcasecmp(arg, "cyan") || !strcasecmp(arg, "ice"))
        colorName = "cyan";
      else if (!strcasecmp(arg, "rage") || !strcasecmp(arg, "red"))
        colorName = "rage";
      else
        colorName = "white";
      Serial.print(F("color="));
      Serial.print(colorName);
      Serial.println(F(" (PCB is mono white — preview only)"));
      break;
    }
    case 'e':
    case 'E':
      autoReel = false;
      enterMode(MODE_TANK);
      break;
    case 'r':
    case 'R':
      autoReel = false;
      enterMode(MODE_WAR);
      break;
    case 'f':
    case 'F':
      crittersFeed(random(2, WAVE_W - 2), random(1, WAVE_H - 1));
      Serial.println(F("feed"));
      break;
    case 'z':
    case 'Z':
      crittersShake(1.3f);
      Serial.println(F("shake"));
      break;
    case 'b':
    case 'B': {
      int val = atoi(arg);
      if (val < 0) val = 0;
      if (val > 255) val = 255;
      brightScale = (uint8_t)val;
      Serial.print(F("brightness="));
      Serial.println(brightScale);
      break;
    }
    case 'd':
    case 'D':
      dropAt(random(2, WAVE_W - 2), random(1, WAVE_H - 1), 1.1f);
      Serial.println(F("drop"));
      break;
    case 'i':
    case 'I':
      wifiOtaPrint();
      break;
    case '?':
    case 'h':
    case 'H':
      printHelp();
      break;
    default:
      Serial.print(F("unknown "));
      Serial.println(line);
      printHelp();
      break;
  }
}

static char rxBuf[80];
static uint8_t rxLen = 0;

static void pollSerial() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      rxBuf[rxLen] = 0;
      if (rxLen) handleLine(rxBuf);
      rxLen = 0;
    } else if (rxLen < sizeof(rxBuf) - 1) {
      rxBuf[rxLen++] = c;
    }
  }
}

static bool parseModeName(const char *s, Mode *out) {
  if (!s || !out) return false;
  if (!strcasecmp(s, "warzone") || !strcasecmp(s, "army") || !strcasecmp(s, "war zone")) {
    *out = MODE_WAR;
    return true;
  }
  for (int i = 0; i < MODE_COUNT; i++) {
    if (!strcasecmp(s, MODE_NAME[i])) {
      *out = (Mode)i;
      return true;
    }
  }
  return false;
}

static void sendModeJson(ESP8266WebServer &http) {
  char buf[360];
  long left = (long)cardLeftMs();
  snprintf(buf, sizeof(buf),
           "{\"ok\":true,\"mode\":\"%s\",\"title\":\"%s\",\"title_ms\":%ld,\"auto\":%s,\"n\":%u}",
           MODE_NAME[mode], cardText(), left, autoReel ? "true" : "false", (unsigned)MODE_COUNT);
  http.send(200, "application/json", buf);
}

static void sendPanelJson(ESP8266WebServer &http) {
  static char buf[288 * 2 + 220];
  long left = (long)cardLeftMs();
  char *p = buf;
  p += snprintf(p, 200,
                "{\"ok\":true,\"sim\":\"%s\",\"mode\":\"%s\",\"title\":\"%s\","
                "\"title_ms\":%ld,\"w\":32,\"h\":9,\"px\":\"",
                MODE_NAME[mode], MODE_NAME[mode], cardText(), left);
  static const char *hex = "0123456789abcdef";
  for (int y = 0; y < WAVE_H; y++) {
    for (int x = 0; x < WAVE_W; x++) {
      uint8_t v = fb[y][x];
      *p++ = hex[v >> 4];
      *p++ = hex[v & 0xf];
    }
  }
  *p++ = '"';
  *p++ = '}';
  *p = 0;
  http.send(200, "application/json", buf);
}

static void modesHttpBegin(ESP8266WebServer &http) {
  http.on("/mode.json", HTTP_GET, [&http]() { sendModeJson(http); });
  http.on("/panel.json", HTTP_GET, [&http]() { sendPanelJson(http); });
  http.on("/modes.json", HTTP_GET, [&http]() {
    char buf[640];
    char *p = buf;
    p += sprintf(p, "{\"ok\":true,\"modes\":[");
    for (int i = 0; i < MODE_COUNT; i++) {
      p += sprintf(p, "%s\"%s\"", i ? "," : "", MODE_NAME[i]);
    }
    sprintf(p, "]}");
    http.send(200, "application/json", buf);
  });
  http.on("/mode", HTTP_POST, [&http]() {
    autoReel = false;
    if (http.hasArg("name")) {
      Mode m;
      if (parseModeName(http.arg("name").c_str(), &m)) enterMode(m);
    } else if (http.hasArg("next")) {
      enterMode((Mode)((mode + 1) % MODE_COUNT));
    }
    sendModeJson(http);
  });
  http.on("/mode/next", HTTP_POST, [&http]() {
    autoReel = false;
    enterMode((Mode)((mode + 1) % MODE_COUNT));
    sendModeJson(http);
  });
}

void setup() {
  Serial.begin(9600);
  delay(200);
  Serial.println();
  Serial.println(F("RAGE INDUSTRIES — variety reel"));
  printHelp();
  led.begin();
  led.clearDisplay();
  led.display();
  wifiOtaBegin();
  crittersBegin();
  warBegin();
  if (ESP8266WebServer *s = wifiOtaServer()) {
    crittersHttpBegin(*s);
    warHttpBegin(*s);
    modesHttpBegin(*s);
  }
  crittersSetFb(fb);
  warSetFb(fb);
  randomSeed(ESP.getChipId() ^ micros());
  autoReel = false;
  enterMode(MODE_TANK);
}

void loop() {
  wifiOtaLoop();
  pollSerial();
  if (crittersTakeFocus() && mode != MODE_TANK) {
    autoReel = false;
    enterMode(MODE_TANK);
  }
  if (warTakeFocus() && mode != MODE_WAR) {
    autoReel = false;
    enterMode(MODE_WAR);
  }

  if (autoReel && (millis() - modeStarted > reelMs)) {
    if (mode == MODE_TEXT && usePromoBank)
      promoIdx = (uint8_t)((promoIdx + 1) % PROMO_N);
    autoNext();
  }

  if (titleLive()) {
    renderTitle();
    fbShow();
    frame++;
    delay(8);
    return;
  }

  switch (mode) {
    case MODE_WAVE: {
      uint32_t now = millis();
      if (now - lastDropMs > dropEveryMs) {
        lastDropMs = now;
        dropEveryMs = 900 + random(0, 1600);
        dropAt(random(1, WAVE_W - 1), random(0, WAVE_H),
               0.7f + random(0, 50) / 100.0f);
      }
      waveStep();
      renderWave();
      break;
    }
    case MODE_SINE: renderSine(); break;
    case MODE_TEXT: renderText(); break;
    case MODE_CLOCK: renderClock(); break;
    case MODE_BARS: renderBars(); break;
    case MODE_SPARK: renderSpark(); break;
    case MODE_RAIN: renderRain(); break;
    case MODE_BOUNCE: renderBounce(); break;
    case MODE_LIFE: renderLife(); break;
    case MODE_PULSE: renderPulse(); break;
    case MODE_PLASMA: renderPlasma(); break;
    case MODE_TUNNEL: renderTunnel(); break;
    case MODE_FIRE: renderFire(); break;
    case MODE_LISSA: renderLissa(); break;
    case MODE_CA: renderCa(); break;
    case MODE_STARS: renderStars(); break;
    case MODE_XOR: renderXor(); break;
    case MODE_RINGS: renderRings(); break;
    case MODE_CYCLE: renderCycle(); break;
    case MODE_GRID: renderGrid(); break;
    default:
      if (mode == MODE_TANK) {
        crittersStep();
        crittersRender(fb, brightScale);
        crittersSetFb(fb);
      } else if (mode == MODE_WAR) {
        warStep();
        warRender(fb, brightScale);
        warSetFb(fb);
      } else {
        renderDisc();
      }
      break;
  }

  fbShow();
  frame++;
  delay(8);
}
