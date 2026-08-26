#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>

// Sealed 56x18 aquarium world. The 32x9 panel is a moving camera.
void aquariumBegin();
void aquariumStep();
void aquariumRender(uint8_t fb[][32], uint8_t bright);
void aquariumHttpBegin(ESP8266WebServer &http);
void aquariumSetFb(uint8_t (*fb)[32]);
bool aquariumTakeFocus();
void aquariumNudgeCam(int dx, int dy);
