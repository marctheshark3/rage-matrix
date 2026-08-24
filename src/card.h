#pragma once
#include <Arduino.h>

// Full-viewport 3×5 word. Firmware title cards + match score / NEXT / AGE.
void cardShow(const char *s, uint16_t ms);
bool cardLive();
const char *cardText();
uint32_t cardLeftMs();
void cardClear();
