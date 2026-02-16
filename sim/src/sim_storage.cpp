#include "SDCardManager.h"
#include "ArduinoStub.h"
#include "SdFat.h"
#include "sim_spi_bus.h"
#include "WString.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstring>
#include <filesystem>
#include <stdlib.h>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace {
struct DirIteratorState {
  fs::directory_iterator it;
  fs::directory_iterator end;
};
}  // namespace

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
  SpiBusGuard guard;
  if (fp_) {
    fclose(fp_);
    fp_ = nullptr;
  }
  if (dir_) {
    delete static_cast<DirIteratorState*>(dir_);
    dir_ = nullptr;
  }
  isDir_ = false;
  dirPath_.clear();
  currentName_.clear();
  filePath_.clear();
}

bool FsFile::openFullPath(const char* fullPath, oflag_t oflag) {
  SpiBusGuard guard;
  close();
  if (!fullPath) return false;
  std::error_code ec;
  if (!fs::exists(fs::path(fullPath), ec) || ec) {
    if ((oflag & O_CREAT) && (oflag & (O_WRONLY | O_RDWR))) {
      const fs::path parent = fs::path(fullPath).parent_path();
      if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) return false;
      }
    }
    fp_ = fopen(fullPath, (oflag & O_RDWR) ? "wb+" : "wb");
    if (fp_) filePath_ = fullPath;
    return fp_ != nullptr;
  }
  if (fs::is_directory(fs::path(fullPath), ec) && !ec) {
    auto* d = new DirIteratorState{fs::directory_iterator(fs::path(fullPath), ec), fs::directory_iterator()};
    if (ec) {
      delete d;
      return false;
    }
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
  SpiBusGuard guard;
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
  SpiBusGuard guard;
  if (!fp_) return -1;
  return static_cast<int>(fread(buf, 1, size, fp_));
}

size_t FsFile::write(const uint8_t* buf, size_t size) {
  SpiBusGuard guard;
  if (!fp_) return 0;
  return fwrite(buf, 1, size, fp_);
}

size_t FsFile::write(uint8_t c) {
  SpiBusGuard guard;
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
  if (!dir_) return;
  auto* d = static_cast<DirIteratorState*>(dir_);
  std::error_code ec;
  d->it = fs::directory_iterator(fs::path(dirPath_), ec);
  if (ec) d->it = d->end;
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
  auto* d = static_cast<DirIteratorState*>(dir_);
  while (d->it != d->end) {
    const fs::path entryPath = d->it->path();
    ++d->it;
    const std::string name = entryPath.filename().string();
    if (name == "." || name == "..") continue;

    FsFile next;
    if (!next.openFullPath(entryPath.string().c_str(), O_RDONLY)) return FsFile();
    next.setCurrentName(name);
    return next;
  }
  return FsFile();
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
  FsFile next;
  next.open(path, oflag);
  return next;
}

bool SdFat::mkdir(const char* path, bool pFlag) {
  const fs::path full(FsFile::resolvePath(path));
  if (full.empty()) return false;
  std::error_code ec;
  if (pFlag) {
    fs::create_directories(full, ec);
    return !ec;
  }
  return fs::create_directory(full, ec) || fs::exists(full);
}

bool SdFat::exists(const char* path) {
  std::error_code ec;
  return fs::exists(fs::path(FsFile::resolvePath(path)), ec) && !ec;
}

bool SdFat::remove(const char* path) {
  std::error_code ec;
  return fs::remove(fs::path(FsFile::resolvePath(path)), ec) && !ec;
}

bool SdFat::rmdir(const char* path) {
  std::error_code ec;
  return fs::remove(fs::path(FsFile::resolvePath(path)), ec) && !ec;
}

SDCardManager SDCardManager::instance;

SDCardManager::SDCardManager() = default;

bool SDCardManager::begin() {
  // Resolve SD root in a deterministic order independent of process cwd.
  // Prefer project-root sdcard path from compile-time CMake source dir.
#ifdef CROSSPOINT_EMULATOR_ROOT
  const fs::path projectSd = fs::path(CROSSPOINT_EMULATOR_ROOT) / "sdcard";
#else
  const fs::path projectSd = fs::path("sdcard");
#endif

  const fs::path candidates[] = {
      projectSd,
      fs::path("sdcard"),
      fs::path("../sdcard"),
      fs::path("../../sdcard"),
      fs::path("../../../sdcard"),
  };

  std::error_code ec;
  fs::path resolved;
  for (const auto& candidate : candidates) {
    if (fs::exists(candidate, ec) && !ec && fs::is_directory(candidate, ec) && !ec) {
      resolved = fs::absolute(candidate, ec);
      if (!ec) break;
    }
    ec.clear();
  }

  // If nothing exists yet, create project-root sdcard so filesystem APIs keep working.
  if (resolved.empty()) {
    fs::create_directories(projectSd, ec);
    if (!ec) {
      resolved = fs::absolute(projectSd, ec);
      if (!ec) {
        const std::string root = resolved.string();
        FsFile::setRootPath(root);
        Serial.printf("[%lu] [SD] Created sim SD card %s\n", millis(), root.c_str());
        initialized = true;
        return initialized;
      }
    }
    FsFile::setRootPath("./sdcard");
    Serial.printf("[%lu] [SD] Sim SD card fallback to ./sdcard (path resolve failed)\n", millis());
    initialized = true;
    return initialized;
  }

  const std::string root = resolved.string();
  FsFile::setRootPath(root);
  Serial.printf("[%lu] [SD] Sim SD card %s\n", millis(), root.c_str());
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
