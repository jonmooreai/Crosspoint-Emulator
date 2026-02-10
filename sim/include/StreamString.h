#pragma once
#include "WString.h"
#include "Stream.h"

// Minimal stub: StreamString is a Stream that stores data in a String
class StreamString : public Stream {
 public:
  String readString() { return str_; }
  const String& getString() const { return str_; }
  void clear() { str_.str().clear(); }
  size_t write(uint8_t c) override {
    str_.str() += static_cast<char>(c);
    return 1;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }

 private:
  String str_;
};
