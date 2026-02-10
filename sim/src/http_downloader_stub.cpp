// Stub implementation of HttpDownloader for emulator (no HTTP in sim).
#include "network/HttpDownloader.h"

bool HttpDownloader::fetchUrl(const std::string& url, std::string& outContent) {
  (void)url;
  (void)outContent;
  return false;
}
bool HttpDownloader::fetchUrl(const std::string& url, Stream& stream) {
  (void)url;
  (void)stream;
  return false;
}
HttpDownloader::DownloadError HttpDownloader::downloadToFile(const std::string& url,
                                                             const std::string& destPath,
                                                             ProgressCallback progress) {
  (void)url;
  (void)destPath;
  (void)progress;
  return HttpDownloader::HTTP_ERROR;
}
