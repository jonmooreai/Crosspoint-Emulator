#pragma once

#include "WString.h"
#include <cstdint>

class IPAddress {
 public:
  IPAddress() : a_(0), b_(0), c_(0), d_(0) {}
  IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : a_(a), b_(b), c_(c), d_(d) {}
  uint8_t operator[](int i) const {
    if (i == 0) return a_;
    if (i == 1) return b_;
    if (i == 2) return c_;
    if (i == 3) return d_;
    return 0;
  }
  String toString() const {
    char buf[16];
    snprintf(buf, sizeof(buf), "%u.%u.%u.%u", a_, b_, c_, d_);
    return String(buf);
  }
  bool operator!=(const IPAddress& o) const {
    return a_ != o.a_ || b_ != o.b_ || c_ != o.c_ || d_ != o.d_;
  }

 private:
  uint8_t a_, b_, c_, d_;
};

enum wl_status_t {
  WL_IDLE_STATUS = 0,
  WL_NO_SSID_AVAIL,
  WL_SCAN_COMPLETED,
  WL_CONNECTED,
  WL_CONNECT_FAILED,
  WL_CONNECTION_LOST,
  WL_DISCONNECTED
};

enum { WIFI_OFF, WIFI_STA, WIFI_AP };
enum wifi_mode_t { WIFI_MODE_NULL = 0, WIFI_MODE_STA = 1, WIFI_MODE_AP = 2, WIFI_MODE_APSTA = 3 };
enum { WIFI_SCAN_RUNNING = -1, WIFI_SCAN_FAILED = -2 };
enum { WIFI_AUTH_OPEN = 0 };

class WiFiClass {
 public:
  void mode(int) {}
  wl_status_t status() const { return WL_DISCONNECTED; }
  void disconnect(bool = true) {}
  void softAPdisconnect(bool = true) {}
  bool softAP(const char* ssid, const char* pass = nullptr, int = 1, bool = false, int = 4) {
    (void)ssid;
    (void)pass;
    return false;  // no-op in sim
  }
  IPAddress softAPIP() const { return IPAddress(192, 168, 4, 1); }
  IPAddress localIP() const { return IPAddress(0, 0, 0, 0); }
  String SSID() const { return String(""); }
  String SSID(int i) const {
    (void)i;
    return String("");
  }
  wifi_mode_t getMode() const { return WIFI_MODE_NULL; }
  String getHostname() const { return String(""); }
  void setSleep(bool) {}
  int softAPgetStationNum() const { return 0; }
  void macAddress(uint8_t* mac) {
    for (int i = 0; i < 6; i++) mac[i] = 0;
  }
  void scanDelete() {}
  int16_t scanNetworks(bool = false) { return 0; }
  int16_t scanComplete() { return 0; }
  int32_t RSSI() const { return 0; }
  int32_t RSSI(int i) const {
    (void)i;
    return 0;
  }
  int encryptionType(int i) const {
    (void)i;
    return WIFI_AUTH_OPEN;
  }
  bool begin(const char* ssid, const char* pass = nullptr) {
    (void)ssid;
    (void)pass;
    return false;
  }
};

extern WiFiClass WiFi;
