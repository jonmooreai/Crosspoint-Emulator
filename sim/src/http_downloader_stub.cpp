#include "network/HttpDownloader.h"

#include "HalStorage.h"

#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace {
#ifdef _WIN32
std::wstring toWide(const std::string& value) {
  if (value.empty()) return std::wstring();
  const int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(len), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), len);
  if (!out.empty() && out.back() == L'\0') out.pop_back();
  return out;
}

std::string base64Encode(const std::string& input) {
  static constexpr char kBase64Table[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  int val = 0;
  int valb = -6;
  for (unsigned char c : input) {
    val = (val << 8) + c;
    valb += 8;
    while (valb >= 0) {
      out.push_back(kBase64Table[(val >> valb) & 0x3F]);
      valb -= 6;
    }
  }
  if (valb > -6) out.push_back(kBase64Table[((val << 8) >> (valb + 8)) & 0x3F]);
  while (out.size() % 4) out.push_back('=');
  return out;
}

bool downloadWithWinHttp(const std::string& url, const std::string& username, const std::string& password,
                         const std::function<bool(const uint8_t*, size_t, size_t, size_t)>& onChunk) {
  URL_COMPONENTS parts{};
  wchar_t host[256] = {};
  wchar_t path[2048] = {};
  parts.dwStructSize = sizeof(parts);
  parts.lpszHostName = host;
  parts.dwHostNameLength = static_cast<DWORD>(std::size(host));
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = static_cast<DWORD>(std::size(path));

  std::wstring wideUrl = toWide(url);
  if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &parts)) {
    return false;
  }

  const bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;
  HINTERNET session = WinHttpOpen(L"Crosspoint-Emulator/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) return false;

  HINTERNET connect = WinHttpConnect(session, std::wstring(parts.lpszHostName, parts.dwHostNameLength).c_str(),
                                     parts.nPort, 0);
  if (!connect) {
    WinHttpCloseHandle(session);
    return false;
  }

  HINTERNET request = WinHttpOpenRequest(connect, L"GET", std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength).c_str(),
                                         nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                         secure ? WINHTTP_FLAG_SECURE : 0);
  if (!request) {
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return false;
  }

  DWORD redirectPolicy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
  WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));

  std::wstring extraHeaders =
      L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
      L"Accept-Language: vi-VN,vi;q=0.9,en-US;q=0.8,en;q=0.7\r\n"
      L"Cache-Control: no-cache\r\n"
      L"Pragma: no-cache\r\n";
  if (!username.empty() || !password.empty()) {
    extraHeaders += toWide("Authorization: Basic " + base64Encode(username + ":" + password) + "\r\n");
  }

  const bool sent = WinHttpSendRequest(request,
                                       extraHeaders.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : extraHeaders.c_str(),
                                       extraHeaders.empty() ? 0 : static_cast<DWORD>(-1L), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                    WinHttpReceiveResponse(request, nullptr);
  if (!sent) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return false;
  }

  DWORD statusCode = 0;
  DWORD statusSize = sizeof(statusCode);
  if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                           &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX) ||
      statusCode < 200 || statusCode >= 300) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    return false;
  }

  unsigned long long contentLengthValue = 0;
  DWORD contentLengthSize = sizeof(contentLengthValue);
  size_t totalBytes = 0;
  if (WinHttpQueryHeaders(request,
                          WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX,
                          &contentLengthValue,
                          &contentLengthSize,
                          WINHTTP_NO_HEADER_INDEX)) {
    totalBytes = static_cast<size_t>(contentLengthValue);
  }

  std::vector<uint8_t> buffer(16 * 1024);
  size_t downloaded = 0;
  while (true) {
    DWORD available = 0;
    if (!WinHttpQueryDataAvailable(request, &available)) {
      WinHttpCloseHandle(request);
      WinHttpCloseHandle(connect);
      WinHttpCloseHandle(session);
      return false;
    }
    if (available == 0) break;

    DWORD toRead = std::min<DWORD>(available, static_cast<DWORD>(buffer.size()));
    DWORD read = 0;
    if (!WinHttpReadData(request, buffer.data(), toRead, &read)) {
      WinHttpCloseHandle(request);
      WinHttpCloseHandle(connect);
      WinHttpCloseHandle(session);
      return false;
    }
    if (read == 0) break;
    downloaded += read;
    if (!onChunk(buffer.data(), read, downloaded, totalBytes)) {
      WinHttpCloseHandle(request);
      WinHttpCloseHandle(connect);
      WinHttpCloseHandle(session);
      return false;
    }
  }

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return true;
}
#endif
}  // namespace

bool HttpDownloader::fetchUrl(const std::string& url, std::string& outContent, const std::string& username,
                              const std::string& password) {
#ifdef _WIN32
  outContent.clear();
  return downloadWithWinHttp(url, username, password,
                             [&outContent](const uint8_t* data, size_t size, size_t, size_t) {
                               outContent.append(reinterpret_cast<const char*>(data), size);
                               return true;
                             });
#else
  (void)url;
  (void)outContent;
  (void)username;
  (void)password;
  return false;
#endif
}

bool HttpDownloader::fetchUrl(const std::string& url, Stream& stream, const std::string& username,
                              const std::string& password) {
#ifdef _WIN32
  return downloadWithWinHttp(url, username, password,
                             [&stream](const uint8_t* data, size_t size, size_t, size_t) {
                               size_t written = 0;
                               while (written < size) {
                                 const size_t chunk = stream.write(data + written, size - written);
                                 if (chunk == 0) return false;
                                 written += chunk;
                               }
                               return true;
                             });
#else
  (void)url;
  (void)stream;
  (void)username;
  (void)password;
  return false;
#endif
}

HttpDownloader::DownloadError HttpDownloader::downloadToFile(const std::string& url, const std::string& destPath,
                                                             ProgressCallback progress, const std::string& username,
                                                             const std::string& password) {
#ifdef _WIN32
  auto& storage = HalStorage::getInstance();
  if (storage.exists(destPath.c_str())) {
    storage.remove(destPath.c_str());
  }

  FsFile file = storage.open(destPath.c_str(), O_CREAT | O_TRUNC | O_WRONLY);
  if (!file) {
    return FILE_ERROR;
  }

  const bool ok = downloadWithWinHttp(url, username, password,
                                      [&file, &progress](const uint8_t* data, size_t size, size_t downloaded,
                                                         size_t total) {
                                        if (file.write(data, size) != size) {
                                          return false;
                                        }
                                        if (progress) progress(downloaded, total);
                                        return true;
                                      });
  file.close();
  if (!ok) {
    storage.remove(destPath.c_str());
    return HTTP_ERROR;
  }
  return OK;
#else
  (void)url;
  (void)destPath;
  (void)progress;
  (void)username;
  (void)password;
  return HTTP_ERROR;
#endif
}
