#pragma once

#include "ArduinoStub.h"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

#include "common/FsApiConstants.h"

class FsFile : public Stream {
 public:
  FsFile() : fp_(nullptr), dir_(nullptr), isDir_(false), dirPath_(), currentName_() {}
  ~FsFile() { close(); }

  FsFile(FsFile&& other) noexcept;
  FsFile& operator=(FsFile&& other) noexcept;

  bool close();
  void flush() override;
  bool truncate(uint64_t length);
  bool getModifyDateTime(uint16_t* date, uint16_t* time) const;
  bool isOpen() const { return fp_ != nullptr || dir_ != nullptr; }
  bool open(const char* path, oflag_t oflag = O_RDONLY);
  bool openFullPath(const char* fullPath, oflag_t oflag);
  int read() override;
  int read(uint8_t* buf, size_t size);
  int read(void* buf, size_t size) { return read(static_cast<uint8_t*>(buf), size); }
  int read(char* buf, size_t size) { return read(reinterpret_cast<uint8_t*>(buf), size); }
  size_t write(const void* buf, size_t size) { return write(static_cast<const uint8_t*>(buf), size); }
  int peek() override;
  size_t write(uint8_t c) override;
  size_t write(const uint8_t* buf, size_t size) override;
  size_t print(const class String& s);
  int available() override;
  bool isDirectory() const { return isDir_; }
  void rewindDirectory();
  bool getName(char* name, size_t size) const;
  size_t size() const;
  bool seek(uint32_t pos);
  bool seekSet(uint32_t pos) { return seek(pos); }
  bool seekCur(int32_t offset);
  uint32_t position() const;
  size_t fileSize() const { return size(); }
  void setCurrentName(const std::string& name) { currentName_ = name; }
  FsFile openNextFile();
  bool rename(const char* newPath);

  operator bool() const { return fp_ != nullptr || dir_ != nullptr; }

  static void setRootPath(const std::string& root) { s_rootPath = root; }
  static std::string resolvePath(const char* path);

 private:
  FILE* fp_;
  void* dir_;  // DIR* from dirent.h when isDir_
  bool isDir_;
  std::string dirPath_;
  std::string currentName_;
  std::string filePath_;  // full path when open as file (for rename)

  static std::string s_rootPath;
};

class SdFat {
 public:
  SdFat() = default;
  bool begin(int csPin = -1, uint32_t maxSck = 0);
  FsFile open(const char* path, oflag_t oflag = O_RDONLY);
  bool mkdir(const char* path, bool pFlag = true);
  bool exists(const char* path);
  bool remove(const char* path);
  bool rmdir(const char* path);
  bool rename(const char* oldPath, const char* newPath);
};
