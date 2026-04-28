// Emulator entry point: init SDL/sim, then run Crosspoint setup() and loop().
// setup() and loop() are defined in Crosspoint src/main.cpp.
// Behavior matches the real device: single core (prewarm on main thread with yields),
// shared SPI (display and SD serialized).

#include <Epub.h>
#include <HardwareSerial.h>
#include <SDCardManager.h>
#include <SdFat.h>
#include <activities/Activity.h>
#include <activities/ActivityManager.h>
#include <activities/util/KeyboardEntryActivity.h>
#include <util/ScreenDebugState.h>
#include <sim_gpio_control.h>
#include "sim_display.h"

#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// Crosspoint app entry points (from main.cpp)
extern void setup();
extern void loop();

namespace {
constexpr int kLibraryThumbHeight = 100;

struct SimCliOptions {
  std::string inputScriptPath;
  std::string screenshotPath;
  long autoShotDelayMs = -1;
  long exitAfterMs = -1;
  std::string stateJsonPath;
  long stateJsonIntervalMs = 100;
  std::string waitForActivity;
  long waitTimeoutMs = -1;
  bool controlStdio = false;
  bool resetSession = false;
  long delayCapMs = -1;
};

struct ControlCommand {
  enum class Type {
    GetState,
    GetScreen,
    TypeText,
    WaitMs,
    Press,
    Release,
    ReleaseAll,
    Hold,
    Tap,
    GoBack,
    Screenshot,
    WaitActivity,
    WaitHeader,
    WaitBody,
    WaitListItem,
    WaitMenuItem,
    WaitSelectedListItem,
    WaitSelectedMenuItem,
    WaitPopup,
    ActivateVisibleListItem,
    ActivateVisibleMenuItem,
    Quit,
    Invalid
  };
  Type type = Type::Invalid;
  uint8_t button = 0xFF;
  unsigned long durationMs = 150;
  unsigned long timeoutMs = 30000;
  std::string arg;
};

struct PendingControlWait {
  enum class Kind { Activity, Header, Body, ListItem, MenuItem, SelectedListItem, SelectedMenuItem, Popup, Delay };
  Kind kind = Kind::Activity;
  std::string value;
  unsigned long startedAtMs = 0;
  unsigned long timeoutMs = 30000;
  unsigned long lastProgressAtMs = 0;
};

struct PendingControlAction {
  enum class Kind { ActivateVisibleListItem, ActivateVisibleMenuItem, GoBack, HoldButton };
  Kind kind = Kind::ActivateVisibleListItem;
  std::string value;
  uint8_t button = 0xFF;
  std::string sourceActivityName;
  std::string sourceHeaderTitle;
  std::string sourceBodyPrimaryText;
  std::string sourceBodySecondaryText;
  std::string sourcePopupMessage;
  std::string sourceSelectedLabel;
  std::string sourceSelectedTitle;
  int sourceMenuItemCount = 0;
  int sourceListItemCount = 0;
  unsigned long startedAtMs = 0;
  unsigned long timeoutMs = 10000;
  unsigned long nextStepAtMs = 0;
  bool confirmQueued = false;
};

bool endsWithEpub(const std::string& name) {
  if (name.size() < 5) return false;
  std::string ext = name.substr(name.size() - 5);
  for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".epub";
}

std::filesystem::path resolveSimSdcardRoot() {
  namespace fs = std::filesystem;
  std::error_code ec;
  fs::path resolved = fs::absolute("./sdcard", ec);
  if (!ec && fs::exists(resolved, ec)) {
    return resolved.lexically_normal();
  }

  ec.clear();
  resolved = fs::absolute("../sdcard", ec);
  if (!ec && fs::exists(resolved, ec)) {
    return resolved.lexically_normal();
  }

  return fs::absolute("./sdcard").lexically_normal();
}

void resetEmulatorSessionState() {
  namespace fs = std::filesystem;
  const fs::path sdRoot = resolveSimSdcardRoot();
  std::error_code ec;
  fs::create_directories(sdRoot, ec);

  const fs::path crosspointRoot = sdRoot / ".crosspoint";
  const fs::path recentJsonPath = crosspointRoot / "recent.json";
  const fs::path stateJsonPath = crosspointRoot / "state.json";

  const fs::path filesToRemove[] = {recentJsonPath, stateJsonPath};
  for (const auto& filePath : filesToRemove) {
    ec.clear();
    fs::remove(filePath, ec);
  }

  if (fs::exists(crosspointRoot, ec)) {
    for (fs::directory_iterator it(crosspointRoot, ec), end; it != end && !ec; it.increment(ec)) {
      const fs::path entryPath = it->path();
      const std::string name = entryPath.filename().string();
      if (!it->is_directory(ec)) continue;
      if (name == "plugins") continue;
      std::error_code removeEc;
      fs::remove_all(entryPath, removeEc);
    }
  }
}

std::atomic<bool> g_prewarmDone{false};
bool g_autoScreenshotDone = false;
bool g_autoExitDone = false;
bool g_waitSatisfied = false;
bool g_controlReadyEmitted = false;
std::string g_lastStateJson;
unsigned long g_lastStateWriteMs = 0;
SimCliOptions g_cliOptions;
std::mutex g_controlMutex;
std::queue<ControlCommand> g_controlCommands;
std::optional<PendingControlWait> g_pendingControlWait;
std::optional<PendingControlAction> g_pendingControlAction;
std::thread g_controlReaderThread;
std::mutex g_stdoutMutex;

void setEnvVar(const char* key, const std::string& value) {
#ifdef _MSC_VER
  _putenv_s(key, value.c_str());
#else
  setenv(key, value.c_str(), 1);
#endif
}

std::string jsonEscape(const std::string& input) {
  std::string out;
  out.reserve(input.size() + 16);
  for (char ch : input) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (static_cast<unsigned char>(ch) < 0x20) {
          char buf[7];
          std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
          out += buf;
        } else {
          out.push_back(ch);
        }
        break;
    }
  }
  return out;
}

std::string toUpperAscii(std::string value) {
  for (char& ch : value) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
  return value;
}

std::string buildCompactStateJson();
void emitControlJson(const std::string& payload);

bool tryParseUnsignedLong(const std::string& raw, unsigned long& out) {
  if (raw.empty()) return false;
  char* end = nullptr;
  const auto parsed = std::strtoul(raw.c_str(), &end, 10);
  if (end == raw.c_str() || *end != '\0') return false;
  out = parsed;
  return true;
}

std::string trimAscii(const std::string& value) {
  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) ++start;
  size_t end = value.size();
  while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;
  return value.substr(start, end - start);
}

bool parseWaitTextAndTimeout(std::istringstream& iss, const std::string& commandName, std::string& text,
                             unsigned long& timeoutMs, std::string& error) {
  std::string remainder;
  std::getline(iss, remainder);
  remainder = trimAscii(remainder);
  if (remainder.empty()) {
    error = commandName + " requires text to match";
    return false;
  }

  const auto lastSpace = remainder.find_last_of(" \t");
  if (lastSpace != std::string::npos) {
    const auto maybeTimeout = trimAscii(remainder.substr(lastSpace + 1));
    unsigned long parsedTimeout = 0;
    if (tryParseUnsignedLong(maybeTimeout, parsedTimeout)) {
      const auto maybeText = trimAscii(remainder.substr(0, lastSpace));
      if (!maybeText.empty()) {
        text = maybeText;
        timeoutMs = parsedTimeout;
        return true;
      }
    }
  }

  text = remainder;
  return true;
}

std::string screenSnapshotJson() {
  const auto screen = SCREEN_DEBUG.getSnapshot();
  std::ostringstream out;
  out << "{";
  out << "\"activityName\":\"" << jsonEscape(screen.activityName) << "\"";
  out << ",\"headerTitle\":\"" << jsonEscape(screen.headerTitle) << "\"";
  out << ",\"headerSubtitle\":\"" << jsonEscape(screen.headerSubtitle) << "\"";
  out << ",\"subHeaderLabel\":\"" << jsonEscape(screen.subHeaderLabel) << "\"";
  out << ",\"subHeaderRightLabel\":\"" << jsonEscape(screen.subHeaderRightLabel) << "\"";
  out << ",\"bodyPrimaryText\":\"" << jsonEscape(screen.bodyPrimaryText) << "\"";
  out << ",\"bodySecondaryText\":\"" << jsonEscape(screen.bodySecondaryText) << "\"";
  out << ",\"bodyTertiaryText\":\"" << jsonEscape(screen.bodyTertiaryText) << "\"";
  out << ",\"popupMessage\":\"" << jsonEscape(screen.popupMessage) << "\"";
  out << ",\"diagnostics\":{";
  out << "\"trackedSeriesLoaded\":" << (screen.diagnostics.trackedSeriesLoaded ? "true" : "false");
  out << ",\"trackedSeriesCount\":" << screen.diagnostics.trackedSeriesCount;
  out << ",\"totalJobCount\":" << screen.diagnostics.totalJobCount;
  out << ",\"activeJobCount\":" << screen.diagnostics.activeJobCount;
  out << ",\"coverCacheFileCount\":" << screen.diagnostics.coverCacheFileCount;
  out << ",\"summary\":\"" << jsonEscape(screen.diagnostics.summary) << "\"}";
  out << ",\"buttonHints\":{";
  out << "\"btn1\":\"" << jsonEscape(screen.buttonHints.btn1) << "\"";
  out << ",\"btn2\":\"" << jsonEscape(screen.buttonHints.btn2) << "\"";
  out << ",\"btn3\":\"" << jsonEscape(screen.buttonHints.btn3) << "\"";
  out << ",\"btn4\":\"" << jsonEscape(screen.buttonHints.btn4) << "\"}";
  out << ",\"buttonMenu\":{";
  out << "\"selectedIndex\":" << screen.buttonMenu.selectedIndex;
  out << ",\"selectedLabel\":\"" << jsonEscape(screen.buttonMenu.selectedLabel) << "\"";
  out << ",\"labels\":[";
  for (size_t i = 0; i < screen.buttonMenu.labels.size(); ++i) {
    if (i > 0) out << ",";
    out << "\"" << jsonEscape(screen.buttonMenu.labels[i]) << "\"";
  }
  out << "]}";
  out << ",\"list\":{";
  out << "\"itemCount\":" << screen.list.itemCount;
  out << ",\"selectedIndex\":" << screen.list.selectedIndex;
  out << ",\"selectedVisibleIndex\":" << screen.list.selectedVisibleIndex;
  out << ",\"selectedTitle\":\"" << jsonEscape(screen.list.selectedTitle) << "\"";
  out << ",\"selectedSubtitle\":\"" << jsonEscape(screen.list.selectedSubtitle) << "\"";
  out << ",\"selectedValue\":\"" << jsonEscape(screen.list.selectedValue) << "\"";
  out << ",\"visibleItems\":[";
  for (size_t i = 0; i < screen.list.visibleItems.size(); ++i) {
    if (i > 0) out << ",";
    out << "{";
    out << "\"title\":\"" << jsonEscape(screen.list.visibleItems[i].title) << "\"";
    out << ",\"subtitle\":\"" << jsonEscape(screen.list.visibleItems[i].subtitle) << "\"";
    out << ",\"value\":\"" << jsonEscape(screen.list.visibleItems[i].value) << "\"";
    out << "}";
  }
  out << "]}}";
  return out.str();
}

const char* controlWaitKindName(PendingControlWait::Kind kind) {
  switch (kind) {
    case PendingControlWait::Kind::Activity: return "activity";
    case PendingControlWait::Kind::Header: return "header";
    case PendingControlWait::Kind::Body: return "body";
    case PendingControlWait::Kind::ListItem: return "list_item";
    case PendingControlWait::Kind::MenuItem: return "menu_item";
    case PendingControlWait::Kind::SelectedListItem: return "selected_list_item";
    case PendingControlWait::Kind::SelectedMenuItem: return "selected_menu_item";
    case PendingControlWait::Kind::Popup: return "popup";
    case PendingControlWait::Kind::Delay: return "delay";
  }
  return "unknown";
}

bool containsAsciiCaseInsensitive(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) return true;
  const auto upperHaystack = toUpperAscii(haystack);
  const auto upperNeedle = toUpperAscii(needle);
  return upperHaystack.find(upperNeedle) != std::string::npos;
}

bool matchesListItemText(const ScreenDebugListItem& item, const std::string& value, std::string& matchedText) {
  if (containsAsciiCaseInsensitive(item.title, value)) {
    matchedText = item.title;
    return true;
  }
  if (containsAsciiCaseInsensitive(item.subtitle, value)) {
    matchedText = item.subtitle;
    return true;
  }
  if (containsAsciiCaseInsensitive(item.value, value)) {
    matchedText = item.value;
    return true;
  }
  return false;
}

bool screenBodyMatches(const ScreenDebugSnapshot& screen, const std::string& value, std::string& matchedText);

bool screenMatchesWait(const PendingControlWait& wait, std::string& matchedText) {
  const auto screen = SCREEN_DEBUG.getSnapshot();

  switch (wait.kind) {
    case PendingControlWait::Kind::Activity:
      if (activityManager.getCurrentActivityName() == wait.value) {
        matchedText = activityManager.getCurrentActivityName();
        return true;
      }
      return false;
    case PendingControlWait::Kind::Header:
      if (containsAsciiCaseInsensitive(screen.headerTitle, wait.value)) {
        matchedText = screen.headerTitle;
        return true;
      }
      if (containsAsciiCaseInsensitive(screen.headerSubtitle, wait.value)) {
        matchedText = screen.headerSubtitle;
        return true;
      }
      return false;
    case PendingControlWait::Kind::Body: return screenBodyMatches(screen, wait.value, matchedText);
    case PendingControlWait::Kind::ListItem:
      for (const auto& item : screen.list.visibleItems) {
        if (matchesListItemText(item, wait.value, matchedText)) return true;
      }
      return false;
    case PendingControlWait::Kind::MenuItem:
      for (const auto& label : screen.buttonMenu.labels) {
        if (containsAsciiCaseInsensitive(label, wait.value)) {
          matchedText = label;
          return true;
        }
      }
      return false;
    case PendingControlWait::Kind::SelectedListItem: {
      ScreenDebugListItem selectedItem{screen.list.selectedTitle, screen.list.selectedSubtitle, screen.list.selectedValue};
      return matchesListItemText(selectedItem, wait.value, matchedText);
    }
    case PendingControlWait::Kind::SelectedMenuItem:
      if (containsAsciiCaseInsensitive(screen.buttonMenu.selectedLabel, wait.value)) {
        matchedText = screen.buttonMenu.selectedLabel;
        return true;
      }
      return false;
    case PendingControlWait::Kind::Popup:
      if (containsAsciiCaseInsensitive(screen.popupMessage, wait.value)) {
        matchedText = screen.popupMessage;
        return true;
      }
      return false;
    case PendingControlWait::Kind::Delay:
      if (millis() >= wait.startedAtMs + wait.timeoutMs) {
        matchedText = std::to_string(wait.timeoutMs) + "ms";
        return true;
      }
      return false;
  }
  return false;
}

bool screenBodyMatches(const ScreenDebugSnapshot& screen, const std::string& value, std::string& matchedText) {
  if (containsAsciiCaseInsensitive(screen.bodyPrimaryText, value)) {
    matchedText = screen.bodyPrimaryText;
    return true;
  }
  if (containsAsciiCaseInsensitive(screen.bodySecondaryText, value)) {
    matchedText = screen.bodySecondaryText;
    return true;
  }
  if (containsAsciiCaseInsensitive(screen.bodyTertiaryText, value)) {
    matchedText = screen.bodyTertiaryText;
    return true;
  }
  return false;
}

bool visibleListContains(const ScreenDebugSnapshot& screen, const std::string& value, int& targetVisibleIndex) {
  targetVisibleIndex = -1;
  std::string ignored;
  for (size_t i = 0; i < screen.list.visibleItems.size(); ++i) {
    if (matchesListItemText(screen.list.visibleItems[i], value, ignored)) {
      targetVisibleIndex = static_cast<int>(i);
      return true;
    }
  }
  return false;
}

bool visibleMenuContains(const ScreenDebugSnapshot& screen, const std::string& value, int& targetIndex) {
  targetIndex = -1;
  for (size_t i = 0; i < screen.buttonMenu.labels.size(); ++i) {
    if (containsAsciiCaseInsensitive(screen.buttonMenu.labels[i], value)) {
      targetIndex = static_cast<int>(i);
      return true;
    }
  }
  return false;
}

bool startPendingActivation(PendingControlAction::Kind kind, const std::string& value, std::string& message) {
  const auto screen = SCREEN_DEBUG.getSnapshot();
  int targetIndex = -1;
  const bool found = kind == PendingControlAction::Kind::ActivateVisibleListItem
                         ? visibleListContains(screen, value, targetIndex)
                         : visibleMenuContains(screen, value, targetIndex);
  if (!found) {
    message = std::string(kind == PendingControlAction::Kind::ActivateVisibleListItem ? "Visible list item not found: "
                                                                                       : "Visible menu item not found: ") +
              value;
    return false;
  }

  const unsigned long nowMs = millis();
  g_pendingControlAction = PendingControlAction{kind,
                                                value,
                                                0xFF,
                                                screen.activityName,
                                                screen.headerTitle,
                                                screen.bodyPrimaryText,
                                                screen.bodySecondaryText,
                                                screen.popupMessage,
                                                screen.buttonMenu.selectedLabel,
                                                screen.list.selectedTitle,
                                                static_cast<int>(screen.buttonMenu.labels.size()),
                                                screen.list.itemCount,
                                                nowMs,
                                                12000,
                                                nowMs + 120,
                                                false};
  return true;
}

bool startPendingGoBack(unsigned long timeoutMs, std::string& message) {
  const auto screen = SCREEN_DEBUG.getSnapshot();
  const auto stackActivities = activityManager.getStackActivityNames();
  if (stackActivities.empty() && activityManager.getCurrentActivityName() == "Home") {
    message = "Already at root screen";
    return false;
  }

  const unsigned long nowMs = millis();
  g_pendingControlAction = PendingControlAction{PendingControlAction::Kind::GoBack,
                                                "",
                                                0xFF,
                                                screen.activityName,
                                                screen.headerTitle,
                                                screen.bodyPrimaryText,
                                                screen.bodySecondaryText,
                                                screen.popupMessage,
                                                screen.buttonMenu.selectedLabel,
                                                screen.list.selectedTitle,
                                                static_cast<int>(screen.buttonMenu.labels.size()),
                                                screen.list.itemCount,
                                                nowMs,
                                                timeoutMs == 0 ? 10000UL : timeoutMs,
                                                nowMs + 120,
                                                false};
  return true;
}

bool isReadOnlyControlCommand(const ControlCommand::Type type) {
  return type == ControlCommand::Type::GetState || type == ControlCommand::Type::GetScreen ||
         type == ControlCommand::Type::Screenshot;
}

bool canBypassBarrier(const ControlCommand::Type type) {
  return type == ControlCommand::Type::ReleaseAll || type == ControlCommand::Type::Quit ||
         isReadOnlyControlCommand(type);
}

bool canRunBeforeControlReady(const ControlCommand::Type type) {
  switch (type) {
    case ControlCommand::Type::GetState:
    case ControlCommand::Type::GetScreen:
    case ControlCommand::Type::WaitMs:
    case ControlCommand::Type::WaitActivity:
    case ControlCommand::Type::WaitHeader:
    case ControlCommand::Type::WaitBody:
    case ControlCommand::Type::WaitListItem:
    case ControlCommand::Type::WaitMenuItem:
    case ControlCommand::Type::WaitSelectedListItem:
    case ControlCommand::Type::WaitSelectedMenuItem:
    case ControlCommand::Type::WaitPopup:
    case ControlCommand::Type::ReleaseAll:
    case ControlCommand::Type::Screenshot:
    case ControlCommand::Type::Quit:
      return true;
    default:
      return false;
  }
}

bool controlChannelReady() {
  if (!g_cliOptions.controlStdio) return false;
  if (g_controlReadyEmitted) return true;
  if (!activityManager.getPendingActivityName().empty()) return false;
  if (activityManager.getPendingActionName() != "none") return false;

  const auto screen = SCREEN_DEBUG.getSnapshot();
  const bool hasInteractiveMenu = !screen.buttonMenu.labels.empty();
  const bool hasInteractiveList = screen.list.itemCount > 0;
  const bool hasVisibleActivity = !activityManager.getCurrentActivityName().empty();
  return hasVisibleActivity && (hasInteractiveMenu || hasInteractiveList);
}

void maybeEmitControlReady() {
  if (!g_cliOptions.controlStdio || g_controlReadyEmitted || !controlChannelReady()) return;
  g_controlReadyEmitted = true;
  emitControlJson(std::string("{\"ok\":true,\"event\":\"ready\",\"state\":") + buildCompactStateJson() + "}");
}

void printHelp() {
  std::puts("Crosspoint Emulator CLI");
  std::puts("  --input-script <path>");
  std::puts("  --screenshot-path <path>");
  std::puts("  --autoshot-delay-ms <ms>");
  std::puts("  --exit-after-ms <ms>");
  std::puts("  --state-json <path>");
  std::puts("  --state-json-interval-ms <ms>");
  std::puts("  --wait-for-activity <name>");
  std::puts("  --wait-timeout-ms <ms>");
  std::puts("  --reset-session");
  std::puts("  --delay-cap-ms <ms>");
  std::puts("  --control-stdio");
  std::puts("  --help");
}

bool parseLongArg(const char* raw, long& out) {
  if (!raw || !*raw) return false;
  char* end = nullptr;
  out = std::strtol(raw, &end, 10);
  return end != raw && *end == '\0';
}

bool parseArgs(int argc, char** argv, SimCliOptions& options) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto requireValue = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "Missing value for %s\n", name);
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "--help" || arg == "-h") {
      printHelp();
      std::exit(0);
    } else if (arg == "--input-script") {
      const char* value = requireValue("--input-script");
      if (!value) return false;
      options.inputScriptPath = value;
    } else if (arg == "--screenshot-path") {
      const char* value = requireValue("--screenshot-path");
      if (!value) return false;
      options.screenshotPath = value;
    } else if (arg == "--autoshot-delay-ms") {
      const char* value = requireValue("--autoshot-delay-ms");
      if (!value || !parseLongArg(value, options.autoShotDelayMs)) return false;
    } else if (arg == "--exit-after-ms") {
      const char* value = requireValue("--exit-after-ms");
      if (!value || !parseLongArg(value, options.exitAfterMs)) return false;
    } else if (arg == "--state-json") {
      const char* value = requireValue("--state-json");
      if (!value) return false;
      options.stateJsonPath = value;
    } else if (arg == "--state-json-interval-ms") {
      const char* value = requireValue("--state-json-interval-ms");
      if (!value || !parseLongArg(value, options.stateJsonIntervalMs)) return false;
    } else if (arg == "--wait-for-activity") {
      const char* value = requireValue("--wait-for-activity");
      if (!value) return false;
      options.waitForActivity = value;
    } else if (arg == "--wait-timeout-ms") {
      const char* value = requireValue("--wait-timeout-ms");
      if (!value || !parseLongArg(value, options.waitTimeoutMs)) return false;
    } else if (arg == "--reset-session") {
      options.resetSession = true;
    } else if (arg == "--delay-cap-ms") {
      const char* value = requireValue("--delay-cap-ms");
      if (!value || !parseLongArg(value, options.delayCapMs)) return false;
    } else if (arg == "--control-stdio") {
      options.controlStdio = true;
    } else {
      std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
      return false;
    }
  }
  return true;
}

void normalizeCliPaths(SimCliOptions& options) {
  namespace fs = std::filesystem;

  auto normalizePath = [](std::string& path) {
    if (path.empty()) return;
    std::error_code ec;
    fs::path resolved = fs::absolute(fs::path(path), ec);
    if (!ec) {
      path = resolved.lexically_normal().string();
    }
  };

  normalizePath(options.inputScriptPath);
  normalizePath(options.screenshotPath);
  normalizePath(options.stateJsonPath);
}

void applyCliOptions(const SimCliOptions& options) {
  if (!options.inputScriptPath.empty()) setEnvVar("SIM_INPUT_SCRIPT", options.inputScriptPath);
  if (!options.screenshotPath.empty()) setEnvVar("SIM_SCREENSHOT_PATH", options.screenshotPath);
  if (options.autoShotDelayMs >= 0) setEnvVar("SIM_AUTOSHOT_DELAY_MS", std::to_string(options.autoShotDelayMs));
  if (options.exitAfterMs >= 0) setEnvVar("SIM_EXIT_AFTER_MS", std::to_string(options.exitAfterMs));
  if (options.delayCapMs >= 0) setEnvVar("SIM_DELAY_CAP_MS", std::to_string(options.delayCapMs));
}

std::string buildStateJson() {
  const auto screen = SCREEN_DEBUG.getSnapshot();
  std::ostringstream out;
  const auto stack = activityManager.getStackActivityNames();
  out << "{\n";
  out << "  \"millis\": " << millis() << ",\n";
  out << "  \"currentActivity\": \"" << jsonEscape(activityManager.getCurrentActivityName()) << "\",\n";
  out << "  \"pendingAction\": \"" << jsonEscape(activityManager.getPendingActionName()) << "\",\n";
  out << "  \"pendingActivity\": \"" << jsonEscape(activityManager.getPendingActivityName()) << "\",\n";
  out << "  \"stackActivities\": [";
  for (size_t i = 0; i < stack.size(); ++i) {
    if (i > 0) out << ", ";
    out << "\"" << jsonEscape(stack[i]) << "\"";
  }
  out << "],\n";
  out << "  \"prewarmDone\": " << (g_prewarmDone.load() ? "true" : "false") << ",\n";
  out << "  \"autoScreenshotDone\": " << (g_autoScreenshotDone ? "true" : "false") << ",\n";
  out << "  \"autoExitDone\": " << (g_autoExitDone ? "true" : "false") << ",\n";
  out << "  \"waitSatisfied\": " << (g_waitSatisfied ? "true" : "false") << ",\n";
  out << "  \"screen\": " << screenSnapshotJson() << ",\n";
  out << "  \"diagnostics\": {\n";
  out << "    \"trackedSeriesLoaded\": " << (screen.diagnostics.trackedSeriesLoaded ? "true" : "false") << ",\n";
  out << "    \"trackedSeriesCount\": " << screen.diagnostics.trackedSeriesCount << ",\n";
  out << "    \"totalJobCount\": " << screen.diagnostics.totalJobCount << ",\n";
  out << "    \"activeJobCount\": " << screen.diagnostics.activeJobCount << ",\n";
  out << "    \"coverCacheFileCount\": " << screen.diagnostics.coverCacheFileCount << ",\n";
  out << "    \"summary\": \"" << jsonEscape(screen.diagnostics.summary) << "\"\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

std::string buildCompactStateJson() {
  const auto stack = activityManager.getStackActivityNames();
  std::ostringstream out;
  out << "{\"millis\":" << millis();
  out << ",\"currentActivity\":\"" << jsonEscape(activityManager.getCurrentActivityName()) << "\"";
  out << ",\"pendingAction\":\"" << jsonEscape(activityManager.getPendingActionName()) << "\"";
  out << ",\"pendingActivity\":\"" << jsonEscape(activityManager.getPendingActivityName()) << "\"";
  out << ",\"stackActivities\":[";
  for (size_t i = 0; i < stack.size(); ++i) {
    if (i > 0) out << ",";
    out << "\"" << jsonEscape(stack[i]) << "\"";
  }
  out << "]";
  out << ",\"screen\":" << screenSnapshotJson();
  out << "}";
  return out.str();
}

void emitControlJson(const std::string& payload) {
  std::lock_guard<std::mutex> lock(g_stdoutMutex);
  std::printf("[SIMCTL] %s\n", payload.c_str());
  std::fflush(stdout);
}

void emitControlError(const std::string& message) {
  emitControlJson(std::string("{\"ok\":false,\"error\":\"") + jsonEscape(message) + "\"}");
}

std::optional<ControlCommand> parseControlCommandLine(const std::string& line) {
  std::istringstream iss(line);
  std::string name;
  if (!(iss >> name)) return std::nullopt;

  ControlCommand cmd;
  std::string upperName = toUpperAscii(name);

  if (upperName == "GET_STATE") {
    cmd.type = ControlCommand::Type::GetState;
    return cmd;
  }
  if (upperName == "GET_SCREEN") {
    cmd.type = ControlCommand::Type::GetScreen;
    return cmd;
  }
  if (upperName == "TYPE_TEXT") {
    std::getline(iss, cmd.arg);
    cmd.arg = trimAscii(cmd.arg);
    cmd.type = cmd.arg.empty() ? ControlCommand::Type::Invalid : ControlCommand::Type::TypeText;
    if (cmd.arg.empty()) cmd.arg = "TYPE_TEXT requires text";
    return cmd;
  }
  if (upperName == "WAIT_MS") {
    if (!(iss >> cmd.timeoutMs)) {
      cmd.type = ControlCommand::Type::Invalid;
      cmd.arg = "WAIT_MS requires a duration in milliseconds";
      return cmd;
    }
    cmd.type = ControlCommand::Type::WaitMs;
    return cmd;
  }
  if (upperName == "QUIT") {
    cmd.type = ControlCommand::Type::Quit;
    return cmd;
  }
  if (upperName == "GO_BACK") {
    if (iss >> cmd.timeoutMs) {
    }
    cmd.type = ControlCommand::Type::GoBack;
    return cmd;
  }
  if (upperName == "RELEASE_ALL") {
    cmd.type = ControlCommand::Type::ReleaseAll;
    return cmd;
  }
  if (upperName == "SCREENSHOT") {
    std::getline(iss, cmd.arg);
    if (!cmd.arg.empty() && cmd.arg[0] == ' ') cmd.arg.erase(cmd.arg.begin());
    cmd.type = cmd.arg.empty() ? ControlCommand::Type::Invalid : ControlCommand::Type::Screenshot;
    if (cmd.arg.empty()) cmd.arg = "SCREENSHOT requires a path";
    return cmd;
  }
  if (upperName == "WAIT_ACTIVITY") {
    if (!(iss >> cmd.arg)) {
      cmd.type = ControlCommand::Type::Invalid;
      cmd.arg = "WAIT_ACTIVITY requires an activity name";
      return cmd;
    }
    if (iss >> cmd.timeoutMs) {
    }
    cmd.type = ControlCommand::Type::WaitActivity;
    return cmd;
  }
  if (upperName == "WAIT_HEADER" || upperName == "WAIT_BODY" || upperName == "WAIT_LIST_ITEM" ||
      upperName == "WAIT_MENU_ITEM" || upperName == "WAIT_SELECTED_LIST_ITEM" ||
      upperName == "WAIT_SELECTED_MENU_ITEM" || upperName == "WAIT_POPUP") {
    if (!parseWaitTextAndTimeout(iss, upperName, cmd.arg, cmd.timeoutMs, cmd.arg)) {
      cmd.type = ControlCommand::Type::Invalid;
      return cmd;
    }
    if (upperName == "WAIT_HEADER") {
      cmd.type = ControlCommand::Type::WaitHeader;
    } else if (upperName == "WAIT_BODY") {
      cmd.type = ControlCommand::Type::WaitBody;
    } else if (upperName == "WAIT_LIST_ITEM") {
      cmd.type = ControlCommand::Type::WaitListItem;
    } else if (upperName == "WAIT_MENU_ITEM") {
      cmd.type = ControlCommand::Type::WaitMenuItem;
    } else if (upperName == "WAIT_SELECTED_LIST_ITEM") {
      cmd.type = ControlCommand::Type::WaitSelectedListItem;
    } else if (upperName == "WAIT_SELECTED_MENU_ITEM") {
      cmd.type = ControlCommand::Type::WaitSelectedMenuItem;
    } else {
      cmd.type = ControlCommand::Type::WaitPopup;
    }
    return cmd;
  }
  if (upperName == "ACTIVATE_VISIBLE_LIST_ITEM" || upperName == "ACTIVATE_VISIBLE_MENU_ITEM") {
    std::getline(iss, cmd.arg);
    cmd.arg = trimAscii(cmd.arg);
    if (cmd.arg.empty()) {
      cmd.type = ControlCommand::Type::Invalid;
      cmd.arg = upperName + " requires text to match";
      return cmd;
    }
    cmd.type = upperName == "ACTIVATE_VISIBLE_LIST_ITEM" ? ControlCommand::Type::ActivateVisibleListItem
                                                          : ControlCommand::Type::ActivateVisibleMenuItem;
    return cmd;
  }
  if (upperName == "PRESS" || upperName == "RELEASE" || upperName == "TAP" || upperName == "HOLD") {
    std::string buttonName;
    if (!(iss >> buttonName)) {
      cmd.type = ControlCommand::Type::Invalid;
      cmd.arg = upperName + " requires a button name";
      return cmd;
    }
    if (!sim_gpio_try_parse_button(buttonName, cmd.button)) {
      cmd.type = ControlCommand::Type::Invalid;
      cmd.arg = "Unknown button: " + buttonName;
      return cmd;
    }
    if ((upperName == "TAP" || upperName == "HOLD") && (iss >> cmd.durationMs)) {
    }
    cmd.type = upperName == "PRESS"   ? ControlCommand::Type::Press
               : upperName == "RELEASE" ? ControlCommand::Type::Release
               : upperName == "HOLD"    ? ControlCommand::Type::Hold
                                        : ControlCommand::Type::Tap;
    return cmd;
  }

  cmd.type = ControlCommand::Type::Invalid;
  cmd.arg = "Unknown control command: " + name;
  return cmd;
}

void startControlReader() {
  g_controlReaderThread = std::thread([] {
    std::string line;
    while (std::getline(std::cin, line)) {
      auto parsed = parseControlCommandLine(line);
      if (!parsed.has_value()) continue;
      std::lock_guard<std::mutex> lock(g_controlMutex);
      g_controlCommands.push(*parsed);
    }
  });
  g_controlReaderThread.detach();
}

void drainControlCommands() {
  while (true) {
    ControlCommand cmd;
    {
      std::lock_guard<std::mutex> lock(g_controlMutex);
      if (g_controlCommands.empty()) break;
      cmd = g_controlCommands.front();
      if (!g_controlReadyEmitted && !canRunBeforeControlReady(cmd.type)) {
        break;
      }
      const bool blockedByBarrier = g_pendingControlWait.has_value() || g_pendingControlAction.has_value();
      if (blockedByBarrier && !canBypassBarrier(cmd.type)) {
        break;
      }
      g_controlCommands.pop();
    }

    switch (cmd.type) {
      case ControlCommand::Type::GetState:
        emitControlJson(std::string("{\"ok\":true,\"event\":\"state\",\"state\":") + buildCompactStateJson() + "}");
        break;
      case ControlCommand::Type::GetScreen:
        emitControlJson(std::string("{\"ok\":true,\"event\":\"screen\",\"screen\":") + screenSnapshotJson() + "}");
        break;
      case ControlCommand::Type::TypeText:
        if (activityManager.injectAutomationText(cmd.arg)) {
          emitControlJson(std::string("{\"ok\":true,\"event\":\"text_typed\",\"text\":\"") + jsonEscape(cmd.arg) + "\"}");
        } else {
          emitControlError("Current activity does not accept automation text input");
        }
        break;
      case ControlCommand::Type::WaitMs:
        g_pendingControlWait = PendingControlWait{PendingControlWait::Kind::Delay, std::to_string(cmd.timeoutMs) + "ms",
                                                  millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"delay\",\"value\":\"") +
                        std::to_string(cmd.timeoutMs) + "ms\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::Press:
        sim_gpio_press_button(cmd.button);
        emitControlJson(std::string("{\"ok\":true,\"event\":\"pressed\",\"button\":\"") +
                        jsonEscape(sim_gpio_button_name(cmd.button)) + "\"}");
        break;
      case ControlCommand::Type::Release:
        sim_gpio_release_button(cmd.button);
        emitControlJson(std::string("{\"ok\":true,\"event\":\"released\",\"button\":\"") +
                        jsonEscape(sim_gpio_button_name(cmd.button)) + "\"}");
        break;
      case ControlCommand::Type::ReleaseAll:
        sim_gpio_release_all_buttons();
        if (g_pendingControlAction.has_value() && g_pendingControlAction->kind == PendingControlAction::Kind::HoldButton) {
          g_pendingControlAction.reset();
        }
        emitControlJson("{\"ok\":true,\"event\":\"released_all\"}");
        break;
      case ControlCommand::Type::Hold: {
        const unsigned long nowMs = millis();
        g_pendingControlAction = PendingControlAction{PendingControlAction::Kind::HoldButton,
                                                      "",
                                                      cmd.button,
                                                      activityManager.getCurrentActivityName(),
                                                      "",
                                                      "",
                                                      "",
                                                      "",
                                                      "",
                                                      "",
                                                      0,
                                                      0,
                                                      nowMs,
                                                      cmd.durationMs == 0 ? 150UL : cmd.durationMs,
                                                      nowMs,
                                                      false};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"action_started\",\"action\":\"hold\",\"button\":\"") +
                        jsonEscape(sim_gpio_button_name(cmd.button)) + "\",\"durationMs\":" +
                        std::to_string(cmd.durationMs == 0 ? 150UL : cmd.durationMs) + "}");
        return;
      }
      case ControlCommand::Type::Tap:
        sim_gpio_tap_button(cmd.button, cmd.durationMs);
        emitControlJson(std::string("{\"ok\":true,\"event\":\"tapped\",\"button\":\"") +
                        jsonEscape(sim_gpio_button_name(cmd.button)) + "\",\"durationMs\":" +
                        std::to_string(cmd.durationMs) + "}");
        break;
      case ControlCommand::Type::GoBack: {
        std::string message;
        if (startPendingGoBack(cmd.timeoutMs, message)) {
          emitControlJson(std::string("{\"ok\":true,\"event\":\"action_started\",\"action\":\"go_back\",\"timeoutMs\":") +
                          std::to_string(cmd.timeoutMs == 0 ? 10000UL : cmd.timeoutMs) + "}");
          return;
        }
        emitControlError(message);
        break;
      }
      case ControlCommand::Type::Screenshot:
        if (sim_display_save_screenshot(cmd.arg.c_str())) {
          emitControlJson(std::string("{\"ok\":true,\"event\":\"screenshot\",\"path\":\"") + jsonEscape(cmd.arg) + "\"}");
        } else {
          emitControlError("Failed to save screenshot");
        }
        break;
      case ControlCommand::Type::WaitActivity:
        g_pendingControlWait = PendingControlWait{PendingControlWait::Kind::Activity, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"activity\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::WaitHeader:
        g_pendingControlWait = PendingControlWait{PendingControlWait::Kind::Header, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"header\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::WaitBody:
        g_pendingControlWait = PendingControlWait{PendingControlWait::Kind::Body, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"body\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::WaitListItem:
        g_pendingControlWait = PendingControlWait{PendingControlWait::Kind::ListItem, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"list_item\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::WaitMenuItem:
        g_pendingControlWait = PendingControlWait{PendingControlWait::Kind::MenuItem, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"menu_item\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::WaitSelectedListItem:
        g_pendingControlWait =
            PendingControlWait{PendingControlWait::Kind::SelectedListItem, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string(
                            "{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"selected_list_item\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::WaitSelectedMenuItem:
        g_pendingControlWait =
            PendingControlWait{PendingControlWait::Kind::SelectedMenuItem, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string(
                            "{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"selected_menu_item\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::WaitPopup:
        g_pendingControlWait = PendingControlWait{PendingControlWait::Kind::Popup, cmd.arg, millis(), cmd.timeoutMs, 0};
        emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_started\",\"waitType\":\"popup\",\"value\":\"") +
                        jsonEscape(cmd.arg) + "\",\"timeoutMs\":" + std::to_string(cmd.timeoutMs) + "}");
        return;
      case ControlCommand::Type::ActivateVisibleListItem: {
        std::string message;
        if (startPendingActivation(PendingControlAction::Kind::ActivateVisibleListItem, cmd.arg, message)) {
          emitControlJson(std::string("{\"ok\":true,\"event\":\"action_started\",\"action\":\"activate_visible_list_item\",\"value\":\"") +
                          jsonEscape(cmd.arg) + "\"}");
          return;
        } else {
          emitControlError(message);
        }
        break;
      }
      case ControlCommand::Type::ActivateVisibleMenuItem: {
        std::string message;
        if (startPendingActivation(PendingControlAction::Kind::ActivateVisibleMenuItem, cmd.arg, message)) {
          emitControlJson(std::string("{\"ok\":true,\"event\":\"action_started\",\"action\":\"activate_visible_menu_item\",\"value\":\"") +
                          jsonEscape(cmd.arg) + "\"}");
          return;
        } else {
          emitControlError(message);
        }
        break;
      }
      case ControlCommand::Type::Quit:
        emitControlJson("{\"ok\":true,\"event\":\"quitting\"}");
        std::exit(0);
      case ControlCommand::Type::Invalid:
        emitControlError(cmd.arg);
        break;
    }
  }
}

void updateControlWait() {
  if (!g_pendingControlWait.has_value()) return;
  const auto nowMs = millis();
  auto& wait = *g_pendingControlWait;
  std::string matchedText;
  if (screenMatchesWait(wait, matchedText)) {
    emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_satisfied\",\"waitType\":\"") +
                    controlWaitKindName(wait.kind) + "\",\"value\":\"" + jsonEscape(wait.value) +
                    "\",\"matched\":\"" + jsonEscape(matchedText) + "\",\"state\":" + buildCompactStateJson() + "}");
    g_pendingControlWait.reset();
    return;
  }
  if (nowMs >= wait.lastProgressAtMs + 1000) {
    wait.lastProgressAtMs = nowMs;
    emitControlJson(std::string("{\"ok\":true,\"event\":\"wait_progress\",\"waitType\":\"") +
                    controlWaitKindName(wait.kind) + "\",\"value\":\"" + jsonEscape(wait.value) +
                    "\",\"elapsedMs\":" + std::to_string(nowMs - wait.startedAtMs) + ",\"timeoutMs\":" +
                    std::to_string(wait.timeoutMs) + ",\"state\":" + buildCompactStateJson() + "}");
  }
  if (wait.timeoutMs > 0 && nowMs > wait.startedAtMs + wait.timeoutMs) {
    emitControlJson(std::string("{\"ok\":false,\"event\":\"wait_timeout\",\"waitType\":\"") +
                    controlWaitKindName(wait.kind) + "\",\"value\":\"" + jsonEscape(wait.value) +
                    "\",\"state\":" + buildCompactStateJson() + "}");
    g_pendingControlWait.reset();
  }
}

const char* controlActionKindName(PendingControlAction::Kind kind) {
  switch (kind) {
    case PendingControlAction::Kind::ActivateVisibleListItem: return "activate_visible_list_item";
    case PendingControlAction::Kind::ActivateVisibleMenuItem: return "activate_visible_menu_item";
    case PendingControlAction::Kind::GoBack: return "go_back";
    case PendingControlAction::Kind::HoldButton: return "hold";
  }
  return "unknown";
}

void finishControlActionSuccess(const PendingControlAction& action, const std::string& matched) {
  emitControlJson(std::string("{\"ok\":true,\"event\":\"action_completed\",\"action\":\"") +
                  controlActionKindName(action.kind) + "\",\"value\":\"" + jsonEscape(action.value) +
                  "\",\"matched\":\"" + jsonEscape(matched) + "\",\"state\":" + buildCompactStateJson() + "}");
  g_pendingControlAction.reset();
}

void finishControlActionError(const PendingControlAction& action, const std::string& error) {
  emitControlJson(std::string("{\"ok\":false,\"event\":\"action_failed\",\"action\":\"") +
                  controlActionKindName(action.kind) + "\",\"value\":\"" + jsonEscape(action.value) +
                  "\",\"error\":\"" + jsonEscape(error) + "\",\"state\":" + buildCompactStateJson() + "}");
  g_pendingControlAction.reset();
}

bool hasSameScreenActionCompleted(const PendingControlAction& action, const ScreenDebugSnapshot& screen) {
  const bool menuStateChanged = screen.buttonMenu.selectedLabel != action.sourceSelectedLabel ||
                                static_cast<int>(screen.buttonMenu.labels.size()) != action.sourceMenuItemCount;
  const bool listStateChanged =
      screen.list.selectedTitle != action.sourceSelectedTitle || screen.list.itemCount != action.sourceListItemCount;
  const bool bodyChanged = screen.bodyPrimaryText != action.sourceBodyPrimaryText ||
                           screen.bodySecondaryText != action.sourceBodySecondaryText;
  const bool popupChanged = screen.popupMessage != action.sourcePopupMessage;
  return menuStateChanged || listStateChanged || bodyChanged || popupChanged;
}

void updateControlAction() {
  if (!g_pendingControlAction.has_value()) return;

  const unsigned long nowMs = millis();
  auto& action = *g_pendingControlAction;
  if (action.kind == PendingControlAction::Kind::HoldButton) {
    if (!action.confirmQueued) {
      sim_gpio_press_button(action.button);
      action.confirmQueued = true;
      action.nextStepAtMs = nowMs + action.timeoutMs;
      return;
    }

    if (nowMs < action.nextStepAtMs) {
      return;
    }

    sim_gpio_release_button(action.button);
    finishControlActionSuccess(action, sim_gpio_button_name(action.button));
    return;
  }

  if (action.timeoutMs > 0 && nowMs > action.startedAtMs + action.timeoutMs) {
    const auto screen = SCREEN_DEBUG.getSnapshot();
    std::string message =
        action.kind == PendingControlAction::Kind::GoBack
            ? "Timed out waiting for back navigation from " +
                  (action.sourceHeaderTitle.empty() ? action.sourceActivityName : action.sourceHeaderTitle)
            : "Timed out while waiting for activation to complete";
    if (!screen.headerTitle.empty() || !screen.activityName.empty()) {
      message += " (still on " + (screen.headerTitle.empty() ? screen.activityName : screen.headerTitle) + ")";
    }
    finishControlActionError(action, message);
    return;
  }
  if (nowMs < action.nextStepAtMs) {
    return;
  }

  const auto screen = SCREEN_DEBUG.getSnapshot();
  if (action.kind == PendingControlAction::Kind::GoBack) {
    const bool transitioned =
        screen.activityName != action.sourceActivityName || screen.headerTitle != action.sourceHeaderTitle;
    if (transitioned && activityManager.getPendingActivityName().empty() && activityManager.getPendingActionName() == "none") {
      finishControlActionSuccess(action, screen.headerTitle.empty() ? screen.activityName : screen.headerTitle);
      return;
    }

    sim_gpio_tap_button(HalGPIO::BTN_BACK, 180);
    action.nextStepAtMs = nowMs + 420;
    return;
  }

  if (action.kind == PendingControlAction::Kind::ActivateVisibleListItem) {
    if (action.confirmQueued) {
      const bool transitioned =
          screen.activityName != action.sourceActivityName || screen.headerTitle != action.sourceHeaderTitle;
      const bool sameScreenActionCompleted = hasSameScreenActionCompleted(action, screen);
      if ((transitioned || sameScreenActionCompleted) && activityManager.getPendingActivityName().empty() &&
          activityManager.getPendingActionName() == "none") {
        finishControlActionSuccess(action, screen.headerTitle.empty() ? screen.activityName : screen.headerTitle);
      } else {
        action.nextStepAtMs = nowMs + 100;
      }
      return;
    }

    int targetVisibleIndex = -1;
    if (!visibleListContains(screen, action.value, targetVisibleIndex)) {
      finishControlActionError(action, "Visible list item disappeared before activation");
      return;
    }

    const int currentVisibleIndex = screen.list.selectedVisibleIndex;
    if (currentVisibleIndex == targetVisibleIndex) {
      sim_gpio_tap_button(HalGPIO::BTN_CONFIRM, 180);
      action.confirmQueued = true;
      action.nextStepAtMs = nowMs + 450;
      return;
    }

    const bool selectionMissing = currentVisibleIndex < 0;
    const uint8_t navButton =
        selectionMissing || targetVisibleIndex > currentVisibleIndex ? HalGPIO::BTN_DOWN : HalGPIO::BTN_UP;
    sim_gpio_tap_button(navButton, 160);
    action.nextStepAtMs = nowMs + 320;
    return;
  }

  if (action.confirmQueued) {
    const bool transitioned =
        screen.activityName != action.sourceActivityName || screen.headerTitle != action.sourceHeaderTitle;
    const bool sameScreenActionCompleted = hasSameScreenActionCompleted(action, screen);
    if ((transitioned || sameScreenActionCompleted) && activityManager.getPendingActivityName().empty() &&
        activityManager.getPendingActionName() == "none") {
      finishControlActionSuccess(action, screen.headerTitle.empty() ? screen.activityName : screen.headerTitle);
    } else {
      action.nextStepAtMs = nowMs + 100;
    }
    return;
  }

  int targetIndex = -1;
  if (!visibleMenuContains(screen, action.value, targetIndex)) {
    finishControlActionError(action, "Visible menu item disappeared before activation");
    return;
  }

  const int currentIndex = screen.buttonMenu.selectedIndex;
  if (currentIndex == targetIndex) {
    sim_gpio_tap_button(HalGPIO::BTN_CONFIRM, 180);
    action.confirmQueued = true;
    action.nextStepAtMs = nowMs + 450;
    return;
  }

  const bool selectionMissing = currentIndex < 0;
  const uint8_t navButton = selectionMissing || targetIndex > currentIndex ? HalGPIO::BTN_DOWN : HalGPIO::BTN_UP;
  sim_gpio_tap_button(navButton, 160);
  action.nextStepAtMs = nowMs + 320;
}

void maybeWriteStateJson(bool force = false) {
  if (g_cliOptions.stateJsonPath.empty()) return;

  const unsigned long nowMs = millis();
  if (!force && g_cliOptions.stateJsonIntervalMs > 0 &&
      nowMs < g_lastStateWriteMs + static_cast<unsigned long>(g_cliOptions.stateJsonIntervalMs)) {
    return;
  }

  const std::string stateJson = buildStateJson();
  if (!force && stateJson == g_lastStateJson) return;

  std::ofstream out(g_cliOptions.stateJsonPath, std::ios::binary | std::ios::trunc);
  if (!out) {
    std::fprintf(stderr, "[SIM] Failed to write state json: %s\n", g_cliOptions.stateJsonPath.c_str());
    return;
  }
  out << stateJson;
  g_lastStateJson = stateJson;
  g_lastStateWriteMs = nowMs;
}

void maybeSatisfyWaitCondition() {
  if (g_waitSatisfied || g_cliOptions.waitForActivity.empty()) return;
  if (activityManager.getCurrentActivityName() != g_cliOptions.waitForActivity) return;

  g_waitSatisfied = true;
  std::printf("[%lu] [SIM] Wait condition satisfied: activity=%s\n", millis(), g_cliOptions.waitForActivity.c_str());
  maybeWriteStateJson(true);
}

void maybeFailWaitTimeout() {
  if (g_waitSatisfied || g_cliOptions.waitForActivity.empty() || g_cliOptions.waitTimeoutMs < 0) return;
  if (millis() < static_cast<unsigned long>(g_cliOptions.waitTimeoutMs)) return;

  std::fprintf(stderr, "[SIM] Timed out waiting for activity: %s\n", g_cliOptions.waitForActivity.c_str());
  maybeWriteStateJson(true);
  std::exit(2);
}

// Prewarm one EPUB per main-loop iteration so UI gets control between thumbnails
// (matches device: single core, yields in image generation).
static bool g_prewarmRootOpen = false;
static FsFile g_prewarmRoot;

void prewarmStep() {
  if (g_prewarmDone.load()) return;

  if (!g_prewarmRootOpen) {
    g_prewarmRoot = SdMan.open("/", O_RDONLY);
    if (!g_prewarmRoot || !g_prewarmRoot.isDirectory()) {
      std::printf("[%lu] [SIM] Could not open SD root for thumb prewarm\n", millis());
      g_prewarmDone.store(true);
      return;
    }
    g_prewarmRootOpen = true;
    return;
  }

  FsFile file = g_prewarmRoot.openNextFile();
  if (!file) {
    g_prewarmRoot.close();
    g_prewarmRootOpen = false;
    g_prewarmDone.store(true);
    std::printf("[%lu] [SIM] Thumb prewarm complete\n", millis());
    return;
  }
  if (file.isDirectory()) {
    file.close();
    return;
  }
  char name[512];
  if (!file.getName(name, sizeof(name))) {
    file.close();
    return;
  }
  file.close();

  std::string filename(name);
  if (!endsWithEpub(filename)) {
    return;
  }

  const std::string path = "/" + filename;
  Epub epub(path, "/.crosspoint");
  if (!epub.load(true, true)) {
    std::printf("[%lu] [SIM] Failed to load EPUB for thumb prewarm: %s\n", millis(), path.c_str());
    return;
  }
  if (!epub.generateThumbBmp(kLibraryThumbHeight)) {
    std::printf("[%lu] [SIM] Failed to prewarm thumb: %s\n", millis(), path.c_str());
  } else {
    std::printf("[%lu] [SIM] Prewarmed thumb: %s\n", millis(), path.c_str());
  }
}

void maybeAutoScreenshot() {
  if (g_autoScreenshotDone) return;
  const char* path = std::getenv("SIM_SCREENSHOT_PATH");
  const char* delayMsStr = std::getenv("SIM_AUTOSHOT_DELAY_MS");
  if (!path || !*path || !delayMsStr || !*delayMsStr) return;

  const long delayMs = std::strtol(delayMsStr, nullptr, 10);
  if (delayMs <= 0 || millis() < static_cast<unsigned long>(delayMs)) return;

  if (sim_display_save_screenshot(path)) {
    g_autoScreenshotDone = true;
    printf("[SIM] Screenshot saved to %s\n", path);
  }
}

void maybeAutoExit() {
  if (g_autoExitDone) return;
  const char* exitAfterMsStr = std::getenv("SIM_EXIT_AFTER_MS");
  if (!exitAfterMsStr || !*exitAfterMsStr) return;

  const long exitAfterMs = std::strtol(exitAfterMsStr, nullptr, 10);
  if (exitAfterMs <= 0 || millis() < static_cast<unsigned long>(exitAfterMs)) return;

  g_autoExitDone = true;
  printf("[SIM] Auto exit after %ld ms\n", exitAfterMs);
  std::exit(0);
}
}  // namespace

int main(int argc, char** argv) {
  if (!parseArgs(argc, argv, g_cliOptions)) {
    printHelp();
    return 2;
  }
  normalizeCliPaths(g_cliOptions);
  if (g_cliOptions.resetSession) {
    resetEmulatorSessionState();
  }
  applyCliOptions(g_cliOptions);

  // Ensure ./sdcard is findable: if run from build/, chdir to project root
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists("./sdcard", ec) && fs::exists("../sdcard", ec)) {
    fs::current_path("..", ec);
    if (ec) {
      fprintf(stderr, "Could not chdir to project root (../sdcard)\n");
    }
  }

  if (!sim_display_init()) {
    fprintf(stderr, "sim_display_init failed\n");
    return 1;
  }

  printf("Crosspoint emulator: running setup() then loop(). Close window to exit.\n");
  setup();
  maybeWriteStateJson(true);
  if (g_cliOptions.controlStdio) {
    startControlReader();
  }

  // Single main thread: one prewarm step per frame, then events and loop (matches device).
  while (true) {
    prewarmStep();
    maybeAutoScreenshot();
    maybeAutoExit();
    maybeFailWaitTimeout();
    if (!sim_display_pump_events()) {
      break;
    }
    if (g_cliOptions.controlStdio) {
      maybeEmitControlReady();
    }
    if (g_cliOptions.controlStdio) {
      drainControlCommands();
    }
    loop();
    maybeSatisfyWaitCondition();
    maybeWriteStateJson();
    if (g_cliOptions.controlStdio) {
      maybeEmitControlReady();
      updateControlAction();
      updateControlWait();
    }
  }

  maybeWriteStateJson(true);
  sim_display_shutdown();
  return 0;
}
