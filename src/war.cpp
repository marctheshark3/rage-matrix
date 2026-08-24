#include "war.h"
#include "card.h"

#include <math.h>
#include <string.h>

#define VW 32
#define VH 9
#define WW 48
#define WH 18
#define MAX_U 16
#define MAX_S 14
#define TEAM_W 0
#define TEAM_E 1

struct Unit {
  float x, y, ox, oy, a, hp;
  uint8_t g[8];
  uint8_t team, gen, cd, alive, role; // role 0=inf 1=arty
  uint8_t kills;
  uint16_t dmg, born;
};

struct Shot {
  float x, y, dx, dy;
  uint8_t team, dmg, splash, on, life, arty;
};

static Unit us[MAX_U];
static Shot ss[MAX_S];
static uint8_t wall[WH][WW];
static uint8_t elite[2][4][8];
static uint8_t eliteN[2];
static float camX = 8.0f, camY = 4.5f;
static uint16_t matchN = 0, tick = 0, matchTick = 0, lastKillTick = 0;
static uint8_t nW = 0, nE = 0, maxGen = 0;
static uint16_t killW = 0, killE = 0;
static bool focusLatch = false;
static uint8_t *liveFb = nullptr;
static ESP8266WebServer *srv = nullptr;
static uint8_t boom[VH][VW];
static bool civOn = true;
static bool autoNext = true;
static uint8_t pace = 1; // 0 brawl · 1 long · 2 epic
static uint8_t age = 0;  // 0 camp · 1 melee · 2 guns · 3 arty
static uint16_t sciW = 0, sciE = 0;
static uint8_t seq = 0; // 0 live · 1 score · 2 next · 3 hold
static bool harvested = false;
static uint16_t turnN = 0;
static uint32_t playUntil = 0;
static bool autoTurns = true;
static const char *AGE_NAME[] = {"CAMP", "MELEE", "GUNS", "ARTY"};
static const float CITY_X[4] = {5.0f, 5.0f, 43.0f, 43.0f};
static const float CITY_Y[4] = {5.0f, 13.0f, 5.0f, 13.0f};

static uint16_t playMs() {
  if (pace == 0) return 1400;
  if (pace == 2) return 2800;
  return 2200;
}
static uint16_t turnCap() {
  if (pace == 0) return 16;
  if (pace == 2) return 36;
  return 24;
}

static uint16_t matchCap() {
  if (pace == 0) return 1600;
  if (pace == 2) return 5600;
  return 3600;
}
static uint16_t stallCap() {
  if (pace == 0) return 520;
  if (pace == 2) return 1600;
  return 980;
}
static uint16_t ageNeed(uint8_t a) {
  uint16_t base = (a == 0) ? 28 : (a == 1 ? 72 : 140);
  if (pace == 0) return (uint16_t)(base * 0.7f);
  if (pace == 2) return (uint16_t)(base * 1.35f);
  return base;
}

bool warTakeFocus() {
  if (!focusLatch) return false;
  focusLatch = false;
  return true;
}

static float frand() { return random(0, 10000) / 10000.0f; }
static float clampf(float x, float a, float b) {
  if (x < a) return a;
  if (x > b) return b;
  return x;
}
static int clampi(int v, int a, int b) {
  if (v < a) return a;
  if (v > b) return b;
  return v;
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

static void tally() {
  nW = nE = 0;
  maxGen = 0;
  for (int i = 0; i < MAX_U; i++) {
    if (!us[i].alive) continue;
    if (us[i].team == TEAM_W) nW++;
    else nE++;
    if (us[i].gen > maxGen) maxGen = us[i].gen;
  }
}

static bool blockedCell(int x, int y) {
  if (x < 0 || y < 0 || x >= WW || y >= WH) return true;
  return wall[y][x] != 0;
}

// Integer grid DDA. Arty skips one berm (1), never a bunker (2). No sqrt.
static bool los(float x0, float y0, float x1, float y1, bool arty) {
  int x = (int)floorf(x0), y = (int)floorf(y0);
  int x1i = (int)floorf(x1), y1i = (int)floorf(y1);
  if (x == x1i && y == y1i) return true;
  int dx = abs(x1i - x), dy = abs(y1i - y);
  int sx = x1i > x ? 1 : -1;
  int sy = y1i > y ? 1 : -1;
  int err = dx - dy;
  bool skipped = false;
  for (;;) {
    int e2 = err * 2;
    if (e2 > -dy) { err -= dy; x += sx; }
    if (e2 < dx) { err += dx; y += sy; }
    if (x == x1i && y == y1i) return true;
    uint8_t cell = (x < 0 || y < 0 || x >= WW || y >= WH) ? 1 : wall[y][x];
    if (cell == 0) continue;
    if (cell == 2) return false;
    if (arty && !skipped) { skipped = true; continue; }
    return false;
  }
}

static void clearShots() { memset(ss, 0, sizeof(ss)); }

static int spawnUnit(float x, float y, uint8_t team, uint8_t role, const uint8_t *src, uint8_t gen) {
  for (int i = 0; i < MAX_U; i++) {
    if (us[i].alive) continue;
    us[i].alive = 1;
    us[i].team = team;
    us[i].role = role;
    us[i].x = clampf(x, 1, WW - 2);
    us[i].y = clampf(y, 1, WH - 2);
    us[i].ox = us[i].x;
    us[i].oy = us[i].y;
    us[i].a = (team == TEAM_W) ? 0.0f : 3.1416f;
    us[i].hp = role ? 1.35f : 1.0f;
    us[i].gen = gen;
    us[i].cd = (uint8_t)random(0, 12);
    us[i].kills = 0;
    us[i].dmg = 0;
    us[i].born = turnN;
    if (src) {
      memcpy(us[i].g, src, 8);
      mutate(us[i].g);
    } else {
      for (int k = 0; k < 8; k++) us[i].g[k] = (uint8_t)random(40, 220);
    }
    if (!civOn && us[i].g[6] > 200) us[i].role = 1;
    return i;
  }
  return -1;
}

static void fireFrom(Unit &u, float tx, float ty) {
  int slot = -1;
  for (int i = 0; i < MAX_S; i++)
    if (!ss[i].on) {
      slot = i;
      break;
    }
  if (slot < 0) return;
  float dx = tx - u.x, dy = ty - u.y;
  float d = sqrtf(dx * dx + dy * dy);
  if (d < 0.2f) return;
  float spd = u.role ? 0.55f : 0.70f;
  ss[slot].on = 1;
  ss[slot].x = u.x;
  ss[slot].y = u.y;
  ss[slot].dx = dx / d * spd;
  ss[slot].dy = dy / d * spd;
  ss[slot].team = u.team;
  ss[slot].arty = u.role;
  ss[slot].dmg = u.role ? 22 : 14;
  ss[slot].splash = u.role ? 2 : 0;
  ss[slot].life = 28;
  uint8_t rof = u.g[3];
  u.cd = u.role ? (18 + (255 - rof) / 14) : (8 + (255 - rof) / 20);
}

static void buildMap() {
  memset(wall, 0, sizeof(wall));
  for (int x = 0; x < WW; x++) {
    wall[0][x] = 1;
    wall[WH - 1][x] = 1;
  }
  for (int y = 0; y < WH; y++) {
    wall[y][0] = 1;
    wall[y][WW - 1] = 1;
  }
  // berms + one bunker in the midfield (keep lanes open or nobody dies)
  for (int k = 0; k < 3; k++) {
    int x = 16 + random(0, 16);
    int y = 2 + random(0, WH - 5);
    int len = 3 + random(0, 5);
    bool vert = random(0, 2);
    for (int i = 0; i < len; i++) {
      int xx = vert ? x : x + i;
      int yy = vert ? y + i : y;
      if (xx > 2 && xx < WW - 3 && yy > 1 && yy < WH - 2) wall[yy][xx] = 1;
    }
  }
  for (int k = 0; k < 1; k++) {
    int x = 18 + random(0, 12);
    int y = 4 + random(0, 8);
    for (int dy = 0; dy < 2; dy++)
      for (int dx = 0; dx < 3; dx++)
        if (y + dy < WH - 2 && x + dx < WW - 3) wall[y + dy][x + dx] = 2; // bunker (arty cannot skip 2)
  }
}

static void deploy(bool evolve) {
  memset(us, 0, sizeof(us));
  clearShots();
  memset(boom, 0, sizeof(boom));
  matchTick = 0;
  lastKillTick = 0;
  turnN = 0;
  playUntil = 0;
  matchN++;
  for (uint8_t t = 0; t < 2; t++) {
    for (int c = 0; c < 2; c++) {
      int ci = t * 2 + c;
      for (int k = 0; k < 2; k++) {
        bool arty = (!civOn || age >= 3) && (k == 0 && c == 0);
        float x = CITY_X[ci] + (frand() - 0.5f) * 3.2f;
        float y = CITY_Y[ci] + (frand() - 0.5f) * 2.4f;
        const uint8_t *src = nullptr;
        uint8_t gen = 0;
        if (evolve && eliteN[t] > 0) {
          src = elite[t][random(0, eliteN[t])];
          gen = maxGen;
        }
        spawnUnit(x, y, t, arty ? 1 : 0, src, gen);
      }
    }
  }
  camX = 8.0f;
  camY = 4.5f;
  tally();
}

static void harvestElites() {
  // keep best 4 genomes per team (kills*8 + dmg + hp leftover)
  int score[MAX_U];
  for (int i = 0; i < MAX_U; i++) {
    if (!us[i].alive && us[i].kills == 0 && us[i].dmg == 0) {
      score[i] = -1;
      continue;
    }
    score[i] = us[i].kills * 80 + us[i].dmg + (int)(us[i].hp * 20);
  }
  for (uint8_t t = 0; t < 2; t++) {
    eliteN[t] = 0;
    for (int slot = 0; slot < 4; slot++) {
      int best = -1, bs = -1;
      for (int i = 0; i < MAX_U; i++) {
        if (us[i].team != t || score[i] < 0) continue;
        if (score[i] > bs) {
          bs = score[i];
          best = i;
        }
      }
      if (best < 0) break;
      memcpy(elite[t][slot], us[best].g, 8);
      eliteN[t]++;
      score[best] = -1;
    }
  }
  if (maxGen < 250) maxGen++;
}

static void finishToNext() {
  if (!harvested) harvestElites();
  harvested = false;
  seq = 0;
  deploy(true);
}

static void requestEnd() {
  if (seq) return;
  harvestElites();
  harvested = true;
  char s[12];
  snprintf(s, sizeof(s), "W%d-E%d", (int)(killW % 100), (int)(killE % 100));
  cardShow(s, 2200);
  seq = 1;
}

static void pumpCards() {
  if (seq == 1 && !cardLive()) {
    cardShow("NEXT", 1600);
    seq = 2;
    return;
  }
  if (seq == 2 && !cardLive()) {
    if (autoNext) finishToNext();
    else seq = 3;
    return;
  }
}

static void tryAgeUp() {
  if (!civOn || age >= 3) return;
  if (turnN >= 220) age = 3;
  else if (turnN >= 130) age = 2;
  else if (turnN >= 50) age = 1;
}

void warRematch() {
  if (civOn) return;
  if (seq == 0) requestEnd();
  else finishToNext();
}

void warSeed() {
  memset(elite, 0, sizeof(elite));
  eliteN[0] = eliteN[1] = 0;
  maxGen = 0;
  matchN = 0;
  killW = killE = 0;
  tick = 0;
  matchTick = 0;
  lastKillTick = 0;
  sciW = sciE = 0;
  age = civOn ? 0 : 2;
  seq = 0;
  harvested = false;
  turnN = 0;
  playUntil = 0;
  autoTurns = true;
  buildMap();
  deploy(false);
  cardClear();
}

void warBegin() { warSeed(); }

static int nearestEnemy(const Unit &u, float *odx, float *ody, float *od2) {
  int best = -1;
  float bd = 1e9f;
  for (int i = 0; i < MAX_U; i++) {
    if (!us[i].alive || us[i].team == u.team) continue;
    float dx = us[i].x - u.x, dy = us[i].y - u.y;
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

static void steer(Unit &u, float dx, float dy, float turn) {
  float want = atan2f(dy, dx);
  float da = want - u.a;
  while (da > 3.1416f) da -= 6.2832f;
  while (da < -3.1416f) da += 6.2832f;
  if (da > turn) da = turn;
  if (da < -turn) da = -turn;
  u.a += da;
}

static void tryMove(Unit &u, float nx, float ny) {
  int gx = (int)floorf(nx), gy = (int)floorf(ny);
  if (!blockedCell(gx, gy)) {
    u.x = clampf(nx, 1.1f, WW - 2.1f);
    u.y = clampf(ny, 1.1f, WH - 2.1f);
    return;
  }
  float sx = u.x + (u.team == TEAM_W ? 0.20f : -0.20f);
  if (!blockedCell((int)floorf(sx), (int)floorf(u.y))) {
    u.x = clampf(sx, 1.1f, WW - 2.1f);
    return;
  }
  float sy = u.y + ((tick & 1) ? 0.22f : -0.22f);
  if (!blockedCell((int)floorf(u.x), (int)floorf(sy))) {
    u.y = clampf(sy, 1.1f, WH - 2.1f);
    return;
  }
  u.a += 1.15f;
}

static void applyKill(int victim, uint8_t team) {
  us[victim].alive = 0;
  if (team == TEAM_W) killW++;
  else killE++;
  lastKillTick = matchTick;
  for (int k = 0; k < MAX_U; k++)
    if (us[k].alive && us[k].team == team) {
      us[k].kills++;
      break;
    }
}

static void resolveTurn() {
  if (seq && !civOn) return;
  seq = 0;
  for (int i = 0; i < MAX_U; i++) {
    us[i].ox = us[i].x;
    us[i].oy = us[i].y;
  }
  clearShots();
  turnN++;
  tick = turnN;
  matchTick = turnN;
  sciW = (uint16_t)(sciW + 4 + nW);
  sciE = (uint16_t)(sciE + 4 + nE);
  tryAgeUp();

  // Cities slowly replace the dead. World does not reset.
  if ((turnN % 6) == 0) {
    for (uint8_t t = 0; t < 2; t++) {
      uint8_t n = (t == TEAM_W) ? nW : nE;
      if (n >= 6) continue;
      int ci = t * 2 + (turnN & 1);
      spawnUnit(CITY_X[ci] + (frand() - 0.5f) * 1.6f, CITY_Y[ci] + (frand() - 0.5f) * 1.2f, t,
                (age >= 3 && n == 0) ? 1 : 0, eliteN[t] ? elite[t][0] : nullptr, maxGen);
    }
    tally();
  }

  for (int i = 0; i < MAX_U; i++) {
    Unit &u = us[i];
    if (!u.alive) continue;
    uint16_t life = (turnN > u.born) ? (uint16_t)(turnN - u.born) : 0;
    bool scout = u.g[7] > 140;
    bool pack = u.g[1] > 110;
    float edx = 0, edy = 0, ed2 = 1e9f;
    int ei = nearestEnemy(u, &edx, &edy, &ed2);
    int home = (u.team == TEAM_W ? 0 : 2) + (u.y > WH * 0.5f);
    float tx = CITY_X[home], ty = CITY_Y[home];
    float step = 0.28f + (life > 20 ? 0.18f : 0) + (life > 60 ? 0.12f : 0);

    // Friend pull — they bunch and move as a group.
    float fx = 0, fy = 0;
    int fn = 0;
    for (int j = 0; j < MAX_U; j++) {
      if (j == i || !us[j].alive || us[j].team != u.team) continue;
      float ddx = us[j].x - u.x, ddy = us[j].y - u.y;
      float d2 = ddx * ddx + ddy * ddy;
      if (d2 < 36.0f && d2 > 0.4f) {
        fx += ddx;
        fy += ddy;
        fn++;
      }
    }
    if (age == 0) {
      tx += (frand() - 0.5f) * 3.0f + (u.team == TEAM_W ? 1.4f : -1.4f);
      ty += (frand() - 0.5f) * 2.4f;
      step *= 0.55f;
    } else if (!scout && age < 2) {
      tx += (u.team == TEAM_W ? 4.0f : -4.0f);
      ty += (frand() - 0.5f) * 3.0f;
    } else if (ei >= 0 && (age >= 2 || scout)) {
      tx = us[ei].x;
      ty = us[ei].y;
      step += 0.15f;
    }
    if (pack && fn) {
      tx = u.x + fx / fn * 0.55f + (tx - u.x) * 0.55f;
      ty = u.y + fy / fn * 0.55f + (ty - u.y) * 0.55f;
    }
    float dx = tx - u.x, dy = ty - u.y;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > 0.15f) tryMove(u, u.x + dx / d * step, u.y + dy / d * step);

    if (ei < 0 || life < 8) continue; // grow up before they fight
    bool see = los(u.x, u.y, us[ei].x, us[ei].y, u.role != 0);
    if (age == 1 && ed2 < 2.8f) {
      us[ei].hp -= 0.22f;
      if (us[ei].hp <= 0) applyKill(ei, u.team);
    } else if (age == 2 && !u.role && see && ed2 < 64.0f && (turnN % 2) == (i & 1)) {
      fireFrom(u, us[ei].x, us[ei].y);
      us[ei].hp -= 0.28f;
      if (us[ei].hp <= 0) applyKill(ei, u.team);
    } else if (age >= 3 && see && ed2 < (u.role ? 180.0f : 64.0f) && (turnN % 2) == (i & 1)) {
      fireFrom(u, us[ei].x, us[ei].y);
      us[ei].hp -= u.role ? 0.45f : 0.28f;
      if (us[ei].hp <= 0) applyKill(ei, u.team);
    }
  }
  tally();
  playUntil = millis() + playMs();
}

void warStepTurn() {
  autoTurns = false;
  if (!cardLive() && seq == 0) resolveTurn();
}

static void civTick() {
  seq = 0;
  if (cardLive()) cardClear();
  for (int y = 0; y < VH; y++)
    for (int x = 0; x < VW; x++)
      if (boom[y][x] > 8) boom[y][x] -= 8;
      else boom[y][x] = 0;
  for (int i = 0; i < MAX_S; i++) {
    Shot &s = ss[i];
    if (!s.on) continue;
    s.x += s.dx;
    s.y += s.dy;
    if (s.life) s.life--;
    int gx = (int)floorf(s.x), gy = (int)floorf(s.y);
    if (s.life == 0 || s.x < 0 || s.y < 0 || s.x >= WW || s.y >= WH ||
        (blockedCell(gx, gy) && !(s.arty && wall[clampi(gy, 0, WH - 1)][clampi(gx, 0, WW - 1)] == 1))) {
      int vx = (int)lroundf(s.x - camX), vy = (int)lroundf(s.y - camY);
      if ((unsigned)vx < VW && (unsigned)vy < VH) boom[vy][vx] = 80;
      s.on = 0;
    }
  }
  float mx = 24.0f, my = 9.0f, best = 1e9f;
  for (int i = 0; i < MAX_U; i++) {
    if (!us[i].alive || us[i].team != TEAM_W) continue;
    for (int j = 0; j < MAX_U; j++) {
      if (!us[j].alive || us[j].team != TEAM_E) continue;
      float dx = us[j].x - us[i].x, dy = us[j].y - us[i].y;
      float d2 = dx * dx + dy * dy;
      if (d2 < best) {
        best = d2;
        mx = (us[i].x + us[j].x) * 0.5f;
        my = (us[i].y + us[j].y) * 0.5f;
      }
    }
  }
  camX += (clampf(mx - VW * 0.5f, 0, WW - VW) - camX) * 0.12f;
  camY += (clampf(my - VH * 0.5f, 0, WH - VH) - camY) * 0.12f;
  if ((int32_t)(millis() - playUntil) >= 0 && autoTurns) resolveTurn();
}

void warStep() {
  if (civOn) {
    civTick();
    return;
  }
  pumpCards();
  if (seq) return;

  tick++;
  matchTick++;
  if ((tick % 12) == 0) {
    sciW = (uint16_t)(sciW + nW);
    sciE = (uint16_t)(sciE + nE);
    tryAgeUp();
  }
  for (int y = 0; y < VH; y++)
    for (int x = 0; x < VW; x++)
      if (boom[y][x] > 8) boom[y][x] -= 8;
      else boom[y][x] = 0;

  for (int i = 0; i < MAX_U; i++) {
    Unit &u = us[i];
    if (!u.alive) continue;
    if (u.cd) u.cd--;
    float speed = (u.role ? 0.14f : 0.30f) + (u.g[0] / 255.0f) * (u.role ? 0.12f : 0.28f);
    float turn = 0.18f + (u.g[1] / 255.0f) * 0.50f;
    float range = (u.role ? 12.0f : 8.0f) + (u.g[2] / 255.0f) * (u.role ? 14.0f : 8.0f);
    float acc = 0.72f + (u.g[4] / 255.0f) * 0.26f;
    if (civOn && age == 0) {
      speed *= 0.18f;
      range = 0;
    } else if (civOn && age == 1) {
      speed *= 0.55f;
      range = 2.2f;
    } else if (civOn && age == 2 && u.role) {
      range = 0; // guns age: infantry only
    }

    float edx = 0, edy = 0, ed2 = 1e9f;
    int ei = nearestEnemy(u, &edx, &edy, &ed2);
    if (ei < 0) {
      yield();
      continue;
    }

    bool canSee = los(u.x, u.y, us[ei].x, us[ei].y, u.role != 0);
    bool stallPush = (matchTick - lastKillTick) > (civOn ? 220 : 90);
    float holdX = (u.team == TEAM_W) ? 11.0f : (WW - 12.0f);
    if (civOn && age == 0) {
      float hx = (u.team == TEAM_W) ? 6.0f : (WW - 7.0f);
      steer(u, hx - u.x, (WH * 0.5f) - u.y + (frand() - 0.5f) * 4.0f, turn);
      tryMove(u, u.x + cosf(u.a) * speed, u.y + sinf(u.a) * speed);
    } else if (u.role) {
      // artillery holds the back line, walks only to unmask
      float tx = holdX, ty = us[ei].y;
      steer(u, tx - u.x, ty - u.y, turn * 0.6f);
      if (fabsf(u.x - holdX) > 1.2f || !canSee) tryMove(u, u.x + cosf(u.a) * speed, u.y + sinf(u.a) * speed);
    } else {
      float push = (u.team == TEAM_W) ? speed * 1.15f : -speed * 1.15f;
      steer(u, edx, edy, turn);
      tryMove(u, u.x + push + cosf(u.a) * speed * 0.15f, u.y + sinf(u.a) * speed);
    }

    bool mayShoot = range > 0.5f && u.cd == 0 && (canSee || stallPush) && ed2 < range * range;
    if (civOn && u.role && age < 3) mayShoot = false;
    if (mayShoot) {
      float j = (1.0f - acc) * 2.2f;
      fireFrom(u, us[ei].x + (frand() - 0.5f) * j, us[ei].y + (frand() - 0.5f) * j);
    }
  }

  for (int i = 0; i < MAX_S; i++) {
    Shot &s = ss[i];
    if (!s.on) continue;
    s.x += s.dx;
    s.y += s.dy;
    if (s.life) s.life--;
    int gx = (int)floorf(s.x), gy = (int)floorf(s.y);
    bool hitWall = blockedCell(gx, gy);
    if ((hitWall && !s.arty) || s.life == 0 || s.x < 0 || s.y < 0 || s.x >= WW || s.y >= WH) {
      if (hitWall && s.arty && wall[clampi(gy, 0, WH - 1)][clampi(gx, 0, WW - 1)] == 2) {
        // bunker eats arty
      }
      s.on = 0;
      continue;
    }
    if (hitWall && s.arty && wall[gy][gx] == 1) continue; // skip berm

    for (int j = 0; j < MAX_U; j++) {
      if (!us[j].alive || us[j].team == s.team) continue;
      float dx = us[j].x - s.x, dy = us[j].y - s.y;
      float rad = s.splash ? 2.1f : 0.95f;
      if (dx * dx + dy * dy > rad * rad) continue;
      float armor = 0.55f + (us[j].g[5] / 255.0f) * 0.40f;
      float take = (s.dmg / 100.0f) * (1.4f - armor);
      us[j].hp -= take;
      for (int k = 0; k < MAX_U; k++)
        if (us[k].alive && us[k].team == s.team) us[k].dmg += (uint16_t)(take * 40);
      int vx = (int)lroundf(s.x - camX);
      int vy = (int)lroundf(s.y - camY);
      if ((unsigned)vx < VW && (unsigned)vy < VH) boom[vy][vx] = 90;
      if (us[j].hp <= 0) {
        us[j].alive = 0;
        if (s.team == TEAM_W) killW++;
        else killE++;
        lastKillTick = matchTick;
        for (int k = 0; k < MAX_U; k++)
          if (us[k].alive && us[k].team == s.team) {
            us[k].kills++;
            break;
          }
      }
      s.on = 0;
      break;
    }
  }

  tally();
  // Camera on the closest opposing pair — COM parked the view on the west wall.
  float best = 1e9f, mx = 24.0f, my = 9.0f;
  for (int i = 0; i < MAX_U; i++) {
    if (!us[i].alive || us[i].team != TEAM_W) continue;
    for (int j = 0; j < MAX_U; j++) {
      if (!us[j].alive || us[j].team != TEAM_E) continue;
      float dx = us[j].x - us[i].x, dy = us[j].y - us[i].y;
      float d2 = dx * dx + dy * dy;
      if (d2 < best) {
        best = d2;
        mx = (us[i].x + us[j].x) * 0.5f;
        my = (us[i].y + us[j].y) * 0.5f;
      }
    }
  }
  float tx = clampf(mx - VW * 0.5f, 0, WW - VW);
  float ty = clampf(my - VH * 0.5f, 0, WH - VH);
  camX += (tx - camX) * 0.42f;
  camY += (ty - camY) * 0.42f;

  // Stalemate: punch a lane so LOS exists; rematch if still dead.
  uint16_t stall = matchTick - lastKillTick;
  if (stall == 220 || stall == 440 || stall == 700) {
    int lx = clampi((int)mx, 8, WW - 10);
    for (int y = 1; y < WH - 1; y++) {
      wall[y][lx] = 0;
      wall[y][lx + 1] = 0;
      wall[y][lx + 2] = 0;
    }
  }
  if ((nW == 0 || nE == 0 || matchTick > matchCap() || stall > stallCap()) && matchTick > 80)
    requestEnd();
}

static void plot(uint8_t fb[][32], int x, int y, uint8_t c) {
  if ((unsigned)x >= VW || (unsigned)y >= VH) return;
  if (c > fb[y][x]) fb[y][x] = c;
}

void warRender(uint8_t fb[][32], uint8_t bright) {
  memset(fb, 0, VW * VH);
  uint8_t b = bright;
  float u = 1.0f;
  if (civOn && playUntil && (int32_t)(playUntil - millis()) > 0) {
    u = 1.0f - (float)(playUntil - millis()) / (float)playMs();
    if (u < 0) u = 0;
    if (u > 1) u = 1;
  }
  for (int y = 0; y < VH; y++) {
    for (int x = 0; x < VW; x++) {
      int gx = (int)floorf(camX) + x;
      int gy = (int)floorf(camY) + y;
      if (gx < 0 || gy < 0 || gx >= WW || gy >= WH) continue;
      if (wall[gy][gx] == 1) plot(fb, x, y, (uint8_t)(b * 0.28f));
      else if (wall[gy][gx] == 2) plot(fb, x, y, (uint8_t)(b * 0.48f));
    }
  }
  if (civOn) {
    for (int c = 0; c < 4; c++) {
      int x = (int)lroundf(CITY_X[c] - camX);
      int y = (int)lroundf(CITY_Y[c] - camY);
      uint8_t c0 = (uint8_t)(b * 0.72f);
      plot(fb, x, y, c0);
      plot(fb, x + 1, y, c0 / 2);
      plot(fb, x, y + 1, c0 / 2);
      plot(fb, x - 1, y, c0 / 3);
    }
  }
  for (int i = 0; i < MAX_S; i++) {
    if (!ss[i].on) continue;
    int x = (int)lroundf(ss[i].x - camX);
    int y = (int)lroundf(ss[i].y - camY);
    plot(fb, x, y, b);
    if (ss[i].arty) plot(fb, x, y - 1, b / 2);
  }
  for (int i = 0; i < MAX_U; i++) {
    if (!us[i].alive) continue;
    float fx = us[i].ox + (us[i].x - us[i].ox) * u;
    float fy = us[i].oy + (us[i].y - us[i].oy) * u;
    int x = (int)lroundf(fx - camX);
    int y = (int)lroundf(fy - camY);
    uint16_t life = (turnN > us[i].born) ? (uint16_t)(turnN - us[i].born) : 0;
    float grow = life < 8 ? 0.28f : (life < 30 ? 0.55f : (life < 80 ? 0.85f : 1.0f));
    uint8_t core = (uint8_t)clampf(b * grow * (0.45f + 0.50f * clampf(us[i].hp, 0, 1.2f)), 8, b);
    if (us[i].role) {
      plot(fb, x, y, core);
      plot(fb, x + 1, y, core / 2);
      plot(fb, x - 1, y, core / 2);
      plot(fb, x, y + 1, core / 2);
      plot(fb, x, y - 1, core / 2);
    } else if (life < 8) {
      plot(fb, x, y, core); // pup — one pixel
    } else if (us[i].team == TEAM_W) {
      plot(fb, x, y, core);
      plot(fb, x + 1, y, core / 3);
    } else {
      plot(fb, x, y, core);
      plot(fb, x - 1, y, core / 3);
    }
  }
  for (int y = 0; y < VH; y++)
    for (int x = 0; x < VW; x++)
      if (boom[y][x] > fb[y][x]) fb[y][x] = boom[y][x] > b ? b : boom[y][x];
}

void warDropWall(int viewX, int viewY) {
  int gx = clampi((int)floorf(camX) + viewX, 2, WW - 3);
  int gy = clampi((int)floorf(camY) + viewY, 2, WH - 3);
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = 0; dx <= 2; dx++) wall[gy + dy][gx + dx] = (dx == 1 && dy == 0) ? 2 : 1;
}

void warReinforce(uint8_t team, bool arty) {
  float x = team == TEAM_W ? 3.5f : WW - 4.0f;
  const uint8_t *src = (eliteN[team] ? elite[team][0] : nullptr);
  spawnUnit(x, 3 + frand() * (WH - 6), team, arty ? 1 : 0, src, maxGen);
  tally();
}

void warNudgeCam(int dx, int dy) {
  camX = clampf(camX + dx, 0, WW - VW);
  camY = clampf(camY + dy, 0, WH - VH);
}

void warSetRules(bool civ, bool autoN, uint8_t p) {
  civOn = civ;
  autoNext = autoN;
  autoTurns = autoN;
  pace = p > 2 ? 2 : p;
}

void warAdvanceAge() {
  if (age < 3) age++;
}

bool warHolding() { return seq != 0; }

void warSetFb(uint8_t (*fb)[32]) { liveFb = &fb[0][0]; }

static void sendState() {
  tally();
  char buf[900];
  snprintf(buf, sizeof(buf),
           "{\"ok\":true,\"sim\":\"war\",\"w\":32,\"h\":9,\"west\":%u,\"east\":%u,"
           "\"match\":%u,\"gen\":%u,\"kw\":%u,\"ke\":%u,\"tick\":%u,"
           "\"civ\":%s,\"auto\":%s,\"pace\":%u,\"age\":%u,\"age_name\":\"%s\","
           "\"sciw\":%u,\"scie\":%u,\"turn\":%u,\"hold\":%u,\"card\":\"%s\","
           "\"cam\":[%.1f,%.1f],\"imu\":false,\"ldr\":false,"
           "\"note\":\"turn civ: resolve then playback; CAMP>MELEE>GUNS>ARTY\"}",
           (unsigned)nW, (unsigned)nE, (unsigned)matchN, (unsigned)maxGen, (unsigned)killW,
           (unsigned)killE, (unsigned)matchTick, civOn ? "true" : "false",
           autoTurns ? "true" : "false", (unsigned)pace, (unsigned)age, AGE_NAME[age],
           (unsigned)sciW, (unsigned)sciE, (unsigned)turnN, (unsigned)seq, cardText(), camX, camY);
  srv->send(200, "application/json", buf);
}

static void sendBoardJson() {
  tally();
  static char buf[288 * 2 + 280];
  char *p = buf;
  p += snprintf(p, 240,
                "{\"ok\":true,\"sim\":\"war\",\"w\":32,\"h\":9,\"west\":%u,\"east\":%u,"
                "\"match\":%u,\"gen\":%u,\"kw\":%u,\"ke\":%u,\"px\":\"",
                (unsigned)nW, (unsigned)nE, (unsigned)matchN, (unsigned)maxGen, (unsigned)killW,
                (unsigned)killE);
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

void warHttpBegin(ESP8266WebServer &http) {
  srv = &http;
  http.on("/war", HTTP_GET, sendState);
  http.on("/war.json", HTTP_GET, sendState);
  http.on("/war/fb.json", HTTP_GET, sendBoardJson);
  http.on("/war/seed", HTTP_POST, []() {
    focusLatch = true;
    warSeed();
    sendState();
  });
  http.on("/war/next", HTTP_POST, []() {
    focusLatch = true;
    warRematch();
    sendState();
  });
  http.on("/war/wall", HTTP_POST, []() {
    focusLatch = true;
    warDropWall(argI("x", 16), argI("y", 4));
    sendState();
  });
  http.on("/war/west", HTTP_POST, []() {
    focusLatch = true;
    warReinforce(TEAM_W, argI("arty", 0) != 0);
    sendState();
  });
  http.on("/war/east", HTTP_POST, []() {
    focusLatch = true;
    warReinforce(TEAM_E, argI("arty", 0) != 0);
    sendState();
  });
  http.on("/war/focus", HTTP_POST, []() {
    focusLatch = true;
    sendState();
  });
  http.on("/war/pan", HTTP_POST, []() {
    warNudgeCam(argI("dx", 0), argI("dy", 0));
    sendState();
  });
  http.on("/war/opts", HTTP_POST, []() {
    focusLatch = true;
    bool civ = srv->hasArg("civ") ? srv->arg("civ").toInt() != 0 : civOn;
    bool an = srv->hasArg("auto") ? srv->arg("auto").toInt() != 0 : autoNext;
    uint8_t p = srv->hasArg("pace") ? (uint8_t)srv->arg("pace").toInt() : pace;
    warSetRules(civ, an, p);
    sendState();
  });
  http.on("/war/civ", HTTP_POST, []() {
    focusLatch = true;
    warSetRules(true, true, pace == 0 ? 1 : pace);
    warSeed();
    sendState();
  });
  http.on("/war/brawl", HTTP_POST, []() {
    focusLatch = true;
    warSetRules(false, true, 0);
    warSeed();
    sendState();
  });
  http.on("/war/epic", HTTP_POST, []() {
    focusLatch = true;
    warSetRules(true, autoNext, 2);
    sendState();
  });
  http.on("/war/age", HTTP_POST, []() {
    focusLatch = true;
    warAdvanceAge();
    sendState();
  });
  http.on("/war/turn", HTTP_POST, []() {
    focusLatch = true;
    warStepTurn();
    sendState();
  });
  http.on("/war/play", HTTP_POST, []() {
    focusLatch = true;
    autoTurns = true;
    autoNext = true;
    if (!cardLive() && seq == 0 && (int32_t)(millis() - playUntil) >= 0) resolveTurn();
    sendState();
  });
}
