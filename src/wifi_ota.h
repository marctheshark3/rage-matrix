#pragma once
#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <IPAddress.h>

void wifiOtaBegin();
void wifiOtaLoop();
void wifiOtaPrint();
bool wifiOtaUp();
IPAddress wifiOtaIp();
const char *wifiOtaHost();
ESP8266WebServer *wifiOtaServer();
