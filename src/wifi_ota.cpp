#include "wifi_ota.h"

#include <ArduinoOTA.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "secrets.h"

static bool started = false;
static ESP8266WebServer http(80);
static ESP8266HTTPUpdateServer httpUpdater;

void wifiOtaBegin() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname(OTA_HOSTNAME);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("wifi joining "));
  Serial.println(WIFI_SSID);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(200);
    yield();
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("wifi FAIL — demo stays offline, serial flash still works"));
    return;
  }

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPort(8266);
#ifdef OTA_PASSWORD
  if (OTA_PASSWORD[0]) ArduinoOTA.setPassword(OTA_PASSWORD);
#endif
  ArduinoOTA.onStart([]() { Serial.println(F("ota start")); });
  ArduinoOTA.onEnd([]() { Serial.println(F("ota end")); });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.print(F("ota err "));
    Serial.println((int)e);
  });
  ArduinoOTA.begin(true);

  httpUpdater.setup(&http, "/update");
  http.on("/", []() {
    http.send(200, "text/plain",
              "RAGE INDUSTRIES matrix\n"
              "GET /update  POST firmware\n"
              "GET /mode.json  POST /mode name=war|tank|wave\n"
              "GET /panel.json  GET /tank.json  GET /fb.json  GET /war.json\n"
              "POST /tank/feed|scatter|shake|seed|follow|pan\n");
  });
  http.begin();

  started = true;
  wifiOtaPrint();
}

void wifiOtaLoop() {
  if (!started) return;
  ArduinoOTA.handle();
  http.handleClient();
  MDNS.update();
}

void wifiOtaPrint() {
  Serial.print(F("wifi "));
  Serial.print(WiFi.status() == WL_CONNECTED ? F("up ") : F("down "));
  Serial.print(WiFi.localIP());
  Serial.print(F("  http://"));
  Serial.print(WiFi.localIP());
  Serial.print(F("/update  ota "));
  Serial.print(OTA_HOSTNAME);
  Serial.println(F(".local:8266"));
}

bool wifiOtaUp() { return WiFi.status() == WL_CONNECTED; }

IPAddress wifiOtaIp() { return WiFi.localIP(); }

const char *wifiOtaHost() { return OTA_HOSTNAME; }

ESP8266WebServer *wifiOtaServer() { return started ? &http : nullptr; }
