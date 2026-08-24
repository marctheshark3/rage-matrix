#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>

// Evolutionary petri dish. 64×18 torus; 32×9 is a camera, not the world.

void crittersBegin();
void crittersSeed();
void crittersStep();
void crittersRender(uint8_t fb[][32], uint8_t bright);
void crittersFeed(int viewX, int viewY);
void crittersFeedScatter();
void crittersShake(float amp);
void crittersDropHunter();
void crittersHttpBegin(ESP8266WebServer &http);
void crittersSetFb(uint8_t (*fb)[32]);

uint8_t crittersAlive();
uint16_t crittersBirths();
uint8_t crittersMaxGen();
uint16_t crittersAdc();
float crittersVbatt();
void crittersSetBrightAuto(bool on);
bool crittersBrightAuto();
void crittersSetFollow(bool on);
bool crittersFollow();
void crittersNudgeCam(int dx, int dy);
bool crittersTakeFocus(); // true once after a hub command
