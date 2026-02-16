#pragma once

// Sim stub for device hal/HalStorage.h. On device this is the storage HAL;
// in the emulator we forward to SDCardManager so the same app code compiles.

#include "SDCardManager.h"
#include "SdFat.h"

class HalStorage {
 public:
  bool begin() { return SdMan.begin(); }
  bool ready() const { return SdMan.ready(); }
  std::vector<String> listFiles(const char* path = "/", int maxFiles = 200) {
    return SdMan.listFiles(path, maxFiles);
  }
  String readFile(const char* path) { return SdMan.readFile(path); }
  bool readFileToStream(const char* path, Print& out, size_t chunkSize = 256) {
    return SdMan.readFileToStream(path, out, chunkSize);
  }
  size_t readFileToBuffer(const char* path, char* buffer, size_t bufferSize, size_t maxBytes = 0) {
    return SdMan.readFileToBuffer(path, buffer, bufferSize, maxBytes);
  }
  bool writeFile(const char* path, const String& content) { return SdMan.writeFile(path, content); }
  bool ensureDirectoryExists(const char* path) { return SdMan.ensureDirectoryExists(path); }
  FsFile open(const char* path, oflag_t oflag = O_RDONLY) { return SdMan.open(path, oflag); }
  bool exists(const char* path) { return SdMan.exists(path); }
  bool mkdir(const char* path, bool pFlag = true) { return SdMan.mkdir(path, pFlag); }
  bool remove(const char* path) { return SdMan.remove(path); }
  bool rmdir(const char* path) { return SdMan.rmdir(path); }
  bool openFileForRead(const char* moduleName, const char* path, FsFile& file) {
    return SdMan.openFileForRead(moduleName, path, file);
  }
  bool openFileForRead(const char* moduleName, const std::string& path, FsFile& file) {
    return SdMan.openFileForRead(moduleName, path, file);
  }
  bool openFileForRead(const char* moduleName, const String& path, FsFile& file) {
    return SdMan.openFileForRead(moduleName, path, file);
  }
  bool openFileForWrite(const char* moduleName, const char* path, FsFile& file) {
    return SdMan.openFileForWrite(moduleName, path, file);
  }
  bool openFileForWrite(const char* moduleName, const std::string& path, FsFile& file) {
    return SdMan.openFileForWrite(moduleName, path, file);
  }
  bool openFileForWrite(const char* moduleName, const String& path, FsFile& file) {
    return SdMan.openFileForWrite(moduleName, path, file);
  }
  bool removeDir(const char* path) { return SdMan.removeDir(path); }

  static HalStorage& getInstance() {
    static HalStorage s;
    return s;
  }
};

#define Storage HalStorage::getInstance()

// Downstream code must use Storage instead of SdMan
#ifdef SdMan
#undef SdMan
#endif
