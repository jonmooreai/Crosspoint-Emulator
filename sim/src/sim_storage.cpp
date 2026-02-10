#include "SDCardManager.h"
#include "ArduinoStub.h"
#include "SdFat.h"
#include "WString.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <dirent.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

std::string FsFile::s_rootPath = "./sdcard";

std::string FsFile::resolvePath(const char* path) {
  std::string p(path ? path : "");
  while (!p.empty() && p[0] == '/') p.erase(0, 1);
  if (p.empty()) return s_rootPath;
  return s_rootPath + "/" + p;
}

FsFile::FsFile(FsFile&& other) noexcept
    : fp_(other.fp_),
      dir_(other.dir_),
      isDir_(other.isDir_),
      dirPath_(std::move(other.dirPath_)),
      currentName_(std::move(other.currentName_)),
      filePath_(std::move(other.filePath_)) {
  other.fp_ = nullptr;
  other.dir_ = nullptr;
}

FsFile& FsFile::operator=(FsFile&& other) noexcept {
  close();
  fp_ = other.fp_;
  dir_ = other.dir_;
  isDir_ = other.isDir_;
  dirPath_ = std::move(other.dirPath_);
  currentName_ = std::move(other.currentName_);
  filePath_ = std::move(other.filePath_);
  other.fp_ = nullptr;
  other.dir_ = nullptr;
  return *this;
}

void FsFile::close() {
  if (fp_) {
    fclose(fp_);
    fp_ = nullptr;
  }
  if (dir_) {
    closedir(static_cast<DIR*>(dir_));
    dir_ = nullptr;
  }
  isDir_ = false;
  dirPath_.clear();
  currentName_.clear();
  filePath_.clear();
}

bool FsFile::openFullPath(const char* fullPath, oflag_t oflag) {
  close();
  if (!fullPath) return false;
  struct stat st;
  if (stat(fullPath, &st) != 0) {
    if ((oflag & O_CREAT) && (oflag & (O_WRONLY | O_RDWR))) {
      std::string p(fullPath);
      for (size_t i = 1; i < p.size(); i++) {
        if (p[i] == '/') {
          std::string part = p.substr(0, i);
          if (mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
        }
      }
    }
    fp_ = fopen(fullPath, (oflag & O_RDWR) ? "wb+" : "wb");
    if (fp_) filePath_ = fullPath;
    return fp_ != nullptr;
  }
  if (S_ISDIR(st.st_mode)) {
    DIR* d = opendir(fullPath);
    if (!d) return false;
    dir_ = d;
    isDir_ = true;
    dirPath_ = fullPath;
    return true;
  }
  const char* mode = (oflag & O_RDWR) ? "wb+" : ((oflag & O_WRONLY) ? "wb" : "rb");
  if ((oflag & O_CREAT) && (oflag & O_TRUNC)) mode = "wb";
  if ((oflag & O_RDWR) && (oflag & O_CREAT)) mode = "wb+";
  fp_ = fopen(fullPath, mode);
  if (fp_) filePath_ = fullPath;
  return fp_ != nullptr;
}

bool FsFile::open(const char* path, oflag_t oflag) {
  return openFullPath(resolvePath(path).c_str(), oflag);
}

int FsFile::read() {
  if (!fp_) return -1;
  int c = fgetc(fp_);
  return c;
}

int FsFile::peek() {
  if (!fp_) return -1;
  int c = fgetc(fp_);
  if (c != EOF) ungetc(c, fp_);
  return c;
}

int FsFile::read(uint8_t* buf, size_t size) {
  if (!fp_) return -1;
  return static_cast<int>(fread(buf, 1, size, fp_));
}

size_t FsFile::write(const uint8_t* buf, size_t size) {
  if (!fp_) return 0;
  return fwrite(buf, 1, size, fp_);
}

size_t FsFile::write(uint8_t c) {
  return fp_ && fputc(c, fp_) >= 0 ? 1 : 0;
}

bool FsFile::seek(uint32_t pos) {
  if (!fp_) return false;
  return fseek(fp_, static_cast<long>(pos), SEEK_SET) == 0;
}

bool FsFile::seekCur(int32_t offset) {
  if (!fp_) return false;
  return fseek(fp_, static_cast<long>(offset), SEEK_CUR) == 0;
}

uint32_t FsFile::position() const {
  if (!fp_) return 0;
  long p = ftell(fp_);
  return p >= 0 ? static_cast<uint32_t>(p) : 0;
}

size_t FsFile::print(const String& s) {
  if (!fp_) return 0;
  const char* c = s.c_str();
  size_t n = 0;
  while (*c) {
    if (fputc(static_cast<unsigned char>(*c++), fp_) >= 0) n++;
  }
  return n;
}

int FsFile::available() {
  if (!fp_) return 0;
  long pos = ftell(fp_);
  fseek(fp_, 0, SEEK_END);
  long end = ftell(fp_);
  fseek(fp_, pos, SEEK_SET);
  return static_cast<int>(end - pos);
}

void FsFile::rewindDirectory() {
  if (dir_) rewinddir(static_cast<DIR*>(dir_));
}

bool FsFile::getName(char* name, size_t size) const {
  if (!name || size == 0) return false;
  if (currentName_.empty()) return false;
  if (currentName_.size() >= size) {
    memcpy(name, currentName_.c_str(), size - 1);
    name[size - 1] = '\0';
  } else {
    strcpy(name, currentName_.c_str());
  }
  return true;
}

size_t FsFile::size() const {
  if (!fp_) return 0;
  long pos = ftell(fp_);
  fseek(fp_, 0, SEEK_END);
  long sz = ftell(fp_);
  fseek(fp_, pos, SEEK_SET);
  return sz >= 0 ? static_cast<size_t>(sz) : 0;
}

FsFile FsFile::openNextFile() {
  if (!dir_) return FsFile();
  struct dirent* ent = readdir(static_cast<DIR*>(dir_));
  while (ent && (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0))
    ent = readdir(static_cast<DIR*>(dir_));
  if (!ent) return FsFile();
  std::string fullPath = dirPath_ + "/" + ent->d_name;
  FsFile next;
  if (!next.openFullPath(fullPath.c_str(), O_RDONLY)) return FsFile();
  next.setCurrentName(ent->d_name);
  return next;
}

bool FsFile::rename(const char* newPath) {
  if (filePath_.empty() || !newPath) return false;
  const std::string dest = resolvePath(newPath);
  if (::rename(filePath_.c_str(), dest.c_str()) != 0) return false;
  filePath_ = dest;
  return true;
}

bool SdFat::begin(int, uint32_t) {
  return true;
}

FsFile SdFat::open(const char* path, oflag_t oflag) {
  FsFile f;
  f.open(path, oflag);
  return f;
}

bool SdFat::mkdir(const char* path, bool pFlag) {
  std::string full = FsFile::resolvePath(path);
  if (full.empty()) return false;
  if (pFlag) {
    // Create parent directories incrementally.
    // Example: /a/b/c -> mkdir(/a), mkdir(/a/b), mkdir(/a/b/c)
    const size_t start = (full[0] == '/') ? 1 : 0;
    for (size_t i = start; i <= full.size(); ++i) {
      if (i == full.size() || full[i] == '/') {
        if (i == 0) continue;
        const std::string part = full.substr(0, i);
        if (part.empty()) continue;
        if (::mkdir(part.c_str(), 0755) != 0 && errno != EEXIST) return false;
      }
    }
    return true;
  }
  return ::mkdir(full.c_str(), 0755) == 0 || errno == EEXIST;
}

bool SdFat::exists(const char* path) {
  std::string full = FsFile::resolvePath(path);
  struct stat st;
  return stat(full.c_str(), &st) == 0;
}

bool SdFat::remove(const char* path) {
  return ::remove(FsFile::resolvePath(path).c_str()) == 0;
}

bool SdFat::rmdir(const char* path) {
  return ::rmdir(FsFile::resolvePath(path).c_str()) == 0;
}

SDCardManager SDCardManager::instance;

SDCardManager::SDCardManager() = default;

bool SDCardManager::begin() {
  // Use absolute path so directory listing works regardless of process cwd (e.g. when run from build/)
  char resolved[PATH_MAX];
  if (realpath("./sdcard", resolved) != nullptr) {
    FsFile::setRootPath(resolved);
    Serial.printf("[%lu] [SD] Sim SD card %s\n", millis(), resolved);
  } else if (realpath("../sdcard", resolved) != nullptr) {
    FsFile::setRootPath(resolved);
    Serial.printf("[%lu] [SD] Sim SD card %s (from ../sdcard)\n", millis(), resolved);
  } else {
    FsFile::setRootPath("./sdcard");
    Serial.printf("[%lu] [SD] Sim SD card (./sdcard, realpath failed)\n", millis());
  }
  initialized = true;
  return initialized;
}

bool SDCardManager::ready() const {
  return initialized;
}

std::vector<String> SDCardManager::listFiles(const char* path, int maxFiles) {
  std::vector<String> ret;
  if (!initialized) return ret;
  FsFile root = sd.open(path, O_RDONLY);
  if (!root || !root.isDirectory()) return ret;
  int count = 0;
  char name[128];
  for (FsFile f = root.openNextFile(); f && count < maxFiles; f = root.openNextFile()) {
    if (f.isDirectory()) continue;
    f.getName(name, sizeof(name));
    ret.push_back(String(name));
    count++;
  }
  return ret;
}

String SDCardManager::readFile(const char* path) {
  if (!initialized) return String("");
  FsFile f;
  if (!openFileForRead("SD", path, f)) return String("");
  String content;
  constexpr size_t maxSize = 50000;
  size_t readSize = 0;
  while (f.available() && readSize < maxSize) {
    int c = f.read();
    if (c < 0) break;
    content += static_cast<char>(c);
    readSize++;
  }
  f.close();
  return content;
}

bool SDCardManager::readFileToStream(const char* path, Print& out, size_t chunkSize) {
  if (!initialized) return false;
  FsFile f;
  if (!openFileForRead("SD", path, f)) return false;
  uint8_t buf[256];
  size_t toRead = (chunkSize == 0 || chunkSize > sizeof(buf)) ? sizeof(buf) : chunkSize;
  while (f.available()) {
    int r = f.read(buf, toRead);
    if (r > 0) out.write(buf, static_cast<size_t>(r));
    else break;
  }
  f.close();
  return true;
}

size_t SDCardManager::readFileToBuffer(const char* path, char* buffer, size_t bufferSize,
                                       size_t maxBytes) {
  if (!buffer || bufferSize == 0) return 0;
  if (!initialized) {
    buffer[0] = '\0';
    return 0;
  }
  FsFile f;
  if (!openFileForRead("SD", path, f)) {
    buffer[0] = '\0';
    return 0;
  }
  size_t maxToRead = (maxBytes == 0) ? (bufferSize - 1) : (std::min)(maxBytes, bufferSize - 1);
  size_t total = 0;
  while (f.available() && total < maxToRead) {
    size_t want = maxToRead - total;
    size_t chunk = (want < 64) ? want : 64;
    int r = f.read(reinterpret_cast<uint8_t*>(buffer + total), chunk);
    if (r > 0) total += static_cast<size_t>(r);
    else break;
  }
  buffer[total] = '\0';
  f.close();
  return total;
}

bool SDCardManager::writeFile(const char* path, const String& content) {
  if (!initialized) return false;
  FsFile f;
  if (!openFileForWrite("SD", path, f)) return false;
  size_t n = f.print(content);
  f.close();
  return n == content.length();
}

bool SDCardManager::ensureDirectoryExists(const char* path) {
  if (!initialized) return false;
  if (sd.exists(path)) {
    FsFile dir = sd.open(path, O_RDONLY);
    if (dir && dir.isDirectory()) {
      dir.close();
      return true;
    }
    dir.close();
  }
  return sd.mkdir(path, true);
}

bool SDCardManager::openFileForRead(const char* moduleName, const char* path, FsFile& file) {
  if (!sd.exists(path)) {
    Serial.printf("[%lu] [%s] File does not exist: %s\n", millis(), moduleName, path);
    return false;
  }
  file = sd.open(path, O_RDONLY);
  if (!file) {
    Serial.printf("[%lu] [%s] Failed to open file for reading: %s\n", millis(), moduleName, path);
    return false;
  }
  return true;
}

bool SDCardManager::openFileForRead(const char* moduleName, const std::string& path, FsFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool SDCardManager::openFileForRead(const char* moduleName, const String& path, FsFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool SDCardManager::openFileForWrite(const char* moduleName, const char* path, FsFile& file) {
  // Ensure parent directory exists (e.g. /.crosspoint/epub_xxx/sections for section cache files)
  std::string p(path);
  size_t lastSlash = p.find_last_of('/');
  if (lastSlash != std::string::npos && lastSlash > 0) {
    std::string parent = p.substr(0, lastSlash);
    if (!parent.empty()) ensureDirectoryExists(parent.c_str());
  }
  file = sd.open(path, O_RDWR | O_CREAT | O_TRUNC);
  if (!file) {
    Serial.printf("[%lu] [%s] Failed to open file for writing: %s\n", millis(), moduleName, path);
    return false;
  }
  return true;
}

bool SDCardManager::openFileForWrite(const char* moduleName, const std::string& path, FsFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool SDCardManager::openFileForWrite(const char* moduleName, const String& path, FsFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool SDCardManager::removeDir(const char* path) {
  FsFile dir = sd.open(path, O_RDONLY);
  if (!dir || !dir.isDirectory()) return false;
  char name[128];
  for (FsFile file = dir.openNextFile(); file; file = dir.openNextFile()) {
    file.getName(name, sizeof(name));
    std::string filePath = path;
    if (!filePath.empty() && filePath.back() != '/') filePath += "/";
    filePath += name;
    if (file.isDirectory()) {
      if (!removeDir(filePath.c_str())) return false;
    } else {
      if (!sd.remove(filePath.c_str())) return false;
    }
  }
  return sd.rmdir(path);
}
