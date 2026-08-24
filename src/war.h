#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>

// 48×18 bounded battlefield. 32×9 is a camera. Tank reel is untouched.

void warBegin();
void warSeed();     // fresh random genomes + new map
void warRematch();  // evolve winners, keep map bones
void warStep();
void warRender(uint8_t fb[][32], uint8_t bright);
void warHttpBegin(ESP8266WebServer &http);
void warSetFb(uint8_t (*fb)[32]);
void warDropWall(int viewX, int viewY);
void warReinforce(uint8_t team, bool arty);
bool warTakeFocus();
void warNudgeCam(int dx, int dy);
