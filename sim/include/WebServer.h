#pragma once

#include "WString.h"
#include "WiFiClient.h"

#include <functional>
#include <memory>
#include <vector>

enum HTTPMethod {
  HTTP_ANY,
  HTTP_GET,
  HTTP_POST,
  HTTP_DELETE,
  HTTP_PUT,
  HTTP_PATCH,
  HTTP_HEAD,
  HTTP_OPTIONS,
  HTTP_PROPFIND,
  HTTP_MKCOL,
  HTTP_MOVE,
  HTTP_COPY,
  HTTP_LOCK,
  HTTP_UNLOCK
};

constexpr int CONTENT_LENGTH_UNKNOWN = -1;

enum { UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED };

struct HTTPRaw {
  uint8_t* buf = nullptr;
  size_t size = 0;
  size_t currentSize = 0;
  size_t totalSize = 0;
  bool isFirst = false;
  bool isFinal = false;
};

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

class RequestHandler {
 public:
  virtual ~RequestHandler() = default;
  virtual bool canHandle(class WebServer& server, HTTPMethod method, const String& uri) {
    (void)server;
    (void)method;
    (void)uri;
    return false;
  }
  virtual bool canRaw(class WebServer& server, const String& uri) {
    (void)server;
    (void)uri;
    return false;
  }
  virtual void raw(class WebServer& server, const String& uri, HTTPRaw& raw) {
    (void)server;
    (void)uri;
    (void)raw;
  }
  virtual bool handle(class WebServer& server, HTTPMethod method, const String& uri) {
    (void)server;
    (void)method;
    (void)uri;
    return false;
  }
};

class WebServer {
 public:
  explicit WebServer(uint16_t port) : port_(port) {}
  void on(const char* path, HTTPMethod method, std::function<void()> fn) { (void)path; (void)method; (void)fn; }
  void on(const char* path, HTTPMethod method, std::function<void()> uploadFn, std::function<void()> bodyFn) {
    (void)path; (void)method; (void)uploadFn; (void)bodyFn;
  }
  void onNotFound(std::function<void()> fn) { (void)fn; }
  void begin() {}
  void stop() {}
  void handleClient() {}
  void addHandler(RequestHandler* handler) { handlers_.emplace_back(handler); }
  void collectHeaders(const char*[], size_t) {}
  void send(int code, const char* type, const char* content) { (void)code; (void)type; (void)content; }
  void send(int code, const char* type, const String& content) { (void)code; (void)type; (void)content; }
  void send(int code) { (void)code; }
  void sendContent(const char* content) { (void)content; }
  void sendContent(const String& content) { (void)content; }
  void send_P(int code, const char* type, const char* content, size_t len) {
    (void)code; (void)type; (void)content; (void)len;
  }
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
  String header(const char* name) const { (void)name; return String(""); }
  HTTPMethod method() const { return HTTP_GET; }
  size_t clientContentLength() const { return 0; }
  const HTTPUpload& upload() const { return upload_; }

 private:
  uint16_t port_;
  HTTPUpload upload_;
  std::vector<std::unique_ptr<RequestHandler>> handlers_;
};
