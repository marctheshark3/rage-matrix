#include "card.h"
#include <string.h>

static char buf[12] = "";
static uint32_t until = 0;

void cardShow(const char *s, uint16_t ms) {
  strncpy(buf, s ? s : "", sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  until = millis() + ms;
}

bool cardLive() { return until && (int32_t)(millis() - until) < 0; }

const char *cardText() { return buf; }

uint32_t cardLeftMs() {
  if (!cardLive()) return 0;
  return until - millis();
}

void cardClear() {
  until = 0;
  buf[0] = 0;
}
