#pragma once

#include "WString.h"
#include "WiFiClient.h"

#include <functional>

enum { HTTP_GET, HTTP_POST };
constexpr int CONTENT_LENGTH_UNKNOWN = -1;

enum { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };

struct HTTPUpload {
  int status = UPLOAD_FILE_ABORTED;
  String filename;
  String name;
  String type;
  size_t size = 0;
  size_t totalSize = 0;
  size_t currentSize = 0;
  const uint8_t* buf = nullptr;
};

class WebServer {
 public:
  explicit WebServer(uint16_t port) : port_(port) {}
  void on(const char* path, int method, std::function<void()> fn) { (void)path; (void)method; (void)fn; }
  void on(const char* path, int method, std::function<void()> uploadFn, std::function<void()> bodyFn) {
    (void)path; (void)method; (void)uploadFn; (void)bodyFn;
  }
  void onNotFound(std::function<void()> fn) { (void)fn; }
  void begin() {}
  void stop() {}
  void handleClient() {}
  void send(int code, const char* type, const char* content) { (void)code; (void)type; (void)content; }
  void send(int code, const char* type, const String& content) { (void)code; (void)type; (void)content; }
  void sendContent(const char* content) { (void)content; }
  void sendContent(const String& content) { (void)content; }
  void sendHeader(const char* name, const char* value, bool first = false) {
    (void)name;
    (void)value;
    (void)first;
  }
  void sendHeader(const String& name, const String& value, bool first = false) {
    (void)name;
    (void)value;
    (void)first;
  }
  void setContentLength(size_t len) { (void)len; }
  WiFiClient client() { return WiFiClient(); }
  bool hasArg(const char* name) const { (void)name; return false; }
  String arg(const char* name) const { (void)name; return String(""); }
  String uri() const { return String(""); }
  const HTTPUpload& upload() const { return upload_; }

 private:
  uint16_t port_;
  HTTPUpload upload_;
};
