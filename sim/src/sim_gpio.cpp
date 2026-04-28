#include "HalGPIO.h"
#include "ArduinoStub.h"
#include "sim_gpio_control.h"
#include <activities/Activity.h>
#include <activities/ActivityManager.h>

#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct ScriptStep {
  enum class Type { ButtonPress, WaitActivity, WaitFile, WaitGlob };
  Type type = Type::ButtonPress;
  unsigned long atMs = 0;
  unsigned long releaseMs = 0;
  uint8_t button = 0xFF;
  std::string target;
  unsigned long timeoutMs = 0;
  bool pressed = false;
  bool logged = false;
  bool started = false;
  bool completed = false;
};

bool g_scriptLoaded = false;
uint8_t g_scriptState = 0;
std::vector<ScriptStep> g_scriptSteps;
size_t g_nextScriptStep = 0;
unsigned long g_scriptCursorMs = 0;
uint8_t g_controlState = 0;
std::mutex g_controlMutex;
std::vector<ScriptStep> g_controlTapSteps;

std::string trim(const std::string& value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string upper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::toupper(ch)); });
  return value;
}

uint8_t parseButtonName(const std::string& rawName) {
  const std::string name = upper(trim(rawName));
  if (name == "LEFT") return HalGPIO::BTN_LEFT;
  if (name == "RIGHT") return HalGPIO::BTN_RIGHT;
  if (name == "UP") return HalGPIO::BTN_UP;
  if (name == "DOWN") return HalGPIO::BTN_DOWN;
  if (name == "ENTER" || name == "CONFIRM") return HalGPIO::BTN_CONFIRM;
  if (name == "BACK" || name == "ESC") return HalGPIO::BTN_BACK;
  if (name == "POWER" || name == "P") return HalGPIO::BTN_POWER;
  return 0xFF;
}

const char* buttonName(uint8_t button) {
  switch (button) {
    case HalGPIO::BTN_LEFT: return "LEFT";
    case HalGPIO::BTN_RIGHT: return "RIGHT";
    case HalGPIO::BTN_UP: return "UP";
    case HalGPIO::BTN_DOWN: return "DOWN";
    case HalGPIO::BTN_CONFIRM: return "CONFIRM";
    case HalGPIO::BTN_BACK: return "BACK";
    case HalGPIO::BTN_POWER: return "POWER";
    default: return "UNKNOWN";
  }
}

void updateControlTapState(unsigned long nowMs) {
  std::lock_guard<std::mutex> lock(g_controlMutex);
  for (auto& step : g_controlTapSteps) {
    if (!step.pressed && nowMs >= step.atMs) {
      step.pressed = true;
      g_controlState = static_cast<uint8_t>(g_controlState | (1 << step.button));
      std::printf("[%lu] [SIM] Control press %s\n", nowMs, buttonName(step.button));
    }
    if (!step.logged && nowMs >= step.releaseMs) {
      step.logged = true;
      g_controlState = static_cast<uint8_t>(g_controlState & ~(1 << step.button));
      std::printf("[%lu] [SIM] Control release %s\n", nowMs, buttonName(step.button));
    }
  }
  g_controlTapSteps.erase(std::remove_if(g_controlTapSteps.begin(), g_controlTapSteps.end(),
                                         [](const ScriptStep& step) { return step.logged; }),
                          g_controlTapSteps.end());
}

std::filesystem::path resolveSdcardPath(const std::string& path) {
  std::string relative = path;
  while (!relative.empty() && (relative[0] == '/' || relative[0] == '\\')) {
    relative.erase(relative.begin());
  }
  return std::filesystem::path("sdcard") / std::filesystem::path(relative);
}

bool matchesSimpleGlob(const std::string& value, const std::string& pattern) {
  const size_t star = pattern.find('*');
  if (star == std::string::npos) return value == pattern;
  const std::string prefix = pattern.substr(0, star);
  const std::string suffix = pattern.substr(star + 1);
  if (value.size() < prefix.size() + suffix.size()) return false;
  return value.rfind(prefix, 0) == 0 && value.substr(value.size() - suffix.size()) == suffix;
}

bool globExistsUnderSdcard(const std::string& pattern) {
  namespace fs = std::filesystem;
  const fs::path root("sdcard");
  std::error_code ec;
  if (!fs::exists(root, ec)) return false;
  for (fs::recursive_directory_iterator it(root, ec), end; it != end && !ec; it.increment(ec)) {
    if (!it->is_regular_file(ec)) continue;
    const std::string relative = it->path().lexically_relative(root).generic_string();
    if (matchesSimpleGlob(relative, pattern)) {
      return true;
    }
  }
  return false;
}

void loadInputScript() {
  if (g_scriptLoaded) return;
  g_scriptLoaded = true;

  const char* scriptPath = std::getenv("SIM_INPUT_SCRIPT");
  if (!scriptPath || !*scriptPath) return;

  std::ifstream in(scriptPath);
  if (!in) {
    std::fprintf(stderr, "[SIM] Failed to open input script: %s\n", scriptPath);
    return;
  }

  std::string line;
  int lineNo = 0;
  while (std::getline(in, line)) {
    ++lineNo;
    const std::string text = trim(line);
    if (text.empty() || text[0] == '#') continue;

    std::istringstream iss(text);
    std::string firstToken;
    if (!(iss >> firstToken)) {
      continue;
    }

    const std::string firstUpper = upper(firstToken);
    if (firstUpper == "WAIT_ACTIVITY") {
      std::string activityName;
      unsigned long timeoutMs = 30000;
      if (!(iss >> activityName)) {
        std::fprintf(stderr, "[SIM] WAIT_ACTIVITY missing activity name at line %d\n", lineNo);
        continue;
      }
      if (iss >> timeoutMs) {
      }
      ScriptStep step;
      step.type = ScriptStep::Type::WaitActivity;
      step.target = activityName;
      step.timeoutMs = timeoutMs;
      g_scriptSteps.push_back(std::move(step));
      continue;
    }

    if (firstUpper == "WAIT_FILE") {
      std::string filePath;
      unsigned long timeoutMs = 30000;
      if (!(iss >> filePath)) {
        std::fprintf(stderr, "[SIM] WAIT_FILE missing path at line %d\n", lineNo);
        continue;
      }
      if (iss >> timeoutMs) {
      }
      ScriptStep step;
      step.type = ScriptStep::Type::WaitFile;
      step.target = filePath;
      step.timeoutMs = timeoutMs;
      g_scriptSteps.push_back(std::move(step));
      continue;
    }

    if (firstUpper == "WAIT_GLOB") {
      std::string globPattern;
      unsigned long timeoutMs = 30000;
      if (!(iss >> globPattern)) {
        std::fprintf(stderr, "[SIM] WAIT_GLOB missing pattern at line %d\n", lineNo);
        continue;
      }
      if (iss >> timeoutMs) {
      }
      ScriptStep step;
      step.type = ScriptStep::Type::WaitGlob;
      step.target = globPattern;
      step.timeoutMs = timeoutMs;
      g_scriptSteps.push_back(std::move(step));
      continue;
    }

    unsigned long atMs = 0;
    try {
      atMs = static_cast<unsigned long>(std::stoul(firstToken));
    } catch (...) {
      std::fprintf(stderr, "[SIM] Invalid script time at line %d: %s\n", lineNo, text.c_str());
      continue;
    }

    std::string key;
    unsigned long durationMs = 150;
    if (!(iss >> key)) {
      std::fprintf(stderr, "[SIM] Missing button name at line %d: %s\n", lineNo, text.c_str());
      continue;
    }
    if (iss >> durationMs) {
    }

    const uint8_t button = parseButtonName(key);
    if (button == 0xFF) {
      std::fprintf(stderr, "[SIM] Unknown button '%s' at line %d\n", key.c_str(), lineNo);
      continue;
    }

    ScriptStep step;
    step.type = ScriptStep::Type::ButtonPress;
    step.atMs = atMs;
    step.releaseMs = atMs + durationMs;
    step.button = button;
    g_scriptSteps.push_back(step);
  }

  std::printf("[SIM] Loaded %zu scripted input steps from %s\n", g_scriptSteps.size(), scriptPath);
}

uint8_t scriptedButtonState() {
  loadInputScript();
  const unsigned long nowMs = millis();
  updateControlTapState(nowMs);
  if (g_scriptSteps.empty()) {
    std::lock_guard<std::mutex> lock(g_controlMutex);
    return g_controlState;
  }
  while (g_nextScriptStep < g_scriptSteps.size()) {
    auto& step = g_scriptSteps[g_nextScriptStep];
    if (step.type == ScriptStep::Type::WaitActivity) {
      if (!step.started) {
        step.started = true;
        step.atMs = nowMs;
        std::printf("[%lu] [SIM] Script wait activity %s\n", nowMs, step.target.c_str());
      }
      if (activityManager.getCurrentActivityName() == step.target) {
        step.completed = true;
        g_scriptCursorMs = nowMs;
        std::printf("[%lu] [SIM] Script wait satisfied activity %s\n", nowMs, step.target.c_str());
        g_nextScriptStep++;
        continue;
      }
      if (step.timeoutMs > 0 && nowMs > step.atMs + step.timeoutMs) {
        std::fprintf(stderr, "[SIM] Script wait timed out for activity %s\n", step.target.c_str());
        std::exit(3);
      }
      break;
    }

    if (step.type == ScriptStep::Type::WaitFile) {
      if (!step.started) {
        step.started = true;
        step.atMs = nowMs;
        std::printf("[%lu] [SIM] Script wait file %s\n", nowMs, step.target.c_str());
      }
      if (std::filesystem::exists(resolveSdcardPath(step.target))) {
        step.completed = true;
        g_scriptCursorMs = nowMs;
        std::printf("[%lu] [SIM] Script wait satisfied file %s\n", nowMs, step.target.c_str());
        g_nextScriptStep++;
        continue;
      }
      if (step.timeoutMs > 0 && nowMs > step.atMs + step.timeoutMs) {
        std::fprintf(stderr, "[SIM] Script wait timed out for file %s\n", step.target.c_str());
        std::exit(3);
      }
      break;
    }

    if (step.type == ScriptStep::Type::WaitGlob) {
      if (!step.started) {
        step.started = true;
        step.atMs = nowMs;
        std::printf("[%lu] [SIM] Script wait glob %s\n", nowMs, step.target.c_str());
      }
      if (globExistsUnderSdcard(step.target)) {
        step.completed = true;
        g_scriptCursorMs = nowMs;
        std::printf("[%lu] [SIM] Script wait satisfied glob %s\n", nowMs, step.target.c_str());
        g_nextScriptStep++;
        continue;
      }
      if (step.timeoutMs > 0 && nowMs > step.atMs + step.timeoutMs) {
        std::fprintf(stderr, "[SIM] Script wait timed out for glob %s\n", step.target.c_str());
        std::exit(3);
      }
      break;
    }

    const uint8_t bit = static_cast<uint8_t>(1 << step.button);
    const unsigned long startAt = g_scriptCursorMs + step.atMs;
    const unsigned long releaseAt = g_scriptCursorMs + step.releaseMs;
    if (!step.pressed && nowMs >= startAt) {
      step.pressed = true;
      g_scriptState |= bit;
      std::printf("[%lu] [SIM] Script press %s\n", nowMs, buttonName(step.button));
    }
    if (step.pressed && !step.logged && nowMs >= releaseAt) {
      step.logged = true;
      step.completed = true;
      g_scriptState = static_cast<uint8_t>(g_scriptState & ~bit);
      std::printf("[%lu] [SIM] Script release %s\n", nowMs, buttonName(step.button));
      g_nextScriptStep++;
      continue;
    }
    break;
  }
  std::lock_guard<std::mutex> lock(g_controlMutex);
  return static_cast<uint8_t>(g_scriptState | g_controlState);
}
}  // namespace

static uint8_t s_keyToButton(SDL_Keycode key) {
  switch (key) {
    case SDLK_LEFT: return HalGPIO::BTN_LEFT;
    case SDLK_RIGHT: return HalGPIO::BTN_RIGHT;
    case SDLK_UP: return HalGPIO::BTN_UP;
    case SDLK_DOWN: return HalGPIO::BTN_DOWN;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return HalGPIO::BTN_CONFIRM;
    case SDLK_BACKSPACE:
    case SDLK_ESCAPE: return HalGPIO::BTN_BACK;
    case SDLK_p: return HalGPIO::BTN_POWER;
    default: return 0xFF;
  }
}

void HalGPIO::begin() {}

void HalGPIO::update() {
  // Pump SDL events so keyboard state is current when we read it.
  // SDL_PumpEvents is safe to call from the main thread and updates
  // the internal key state array used by SDL_GetKeyboardState.
  SDL_PumpEvents();

  prevState_ = lastState_;
  anyPressed_ = false;
  anyReleased_ = false;

  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  uint8_t state = 0;
  if (keys[SDL_SCANCODE_LEFT]) state |= (1 << BTN_LEFT);
  if (keys[SDL_SCANCODE_RIGHT]) state |= (1 << BTN_RIGHT);
  if (keys[SDL_SCANCODE_UP]) state |= (1 << BTN_UP);
  if (keys[SDL_SCANCODE_DOWN]) state |= (1 << BTN_DOWN);
  if (keys[SDL_SCANCODE_RETURN]) state |= (1 << BTN_CONFIRM);
  if (keys[SDL_SCANCODE_BACKSPACE] || keys[SDL_SCANCODE_ESCAPE]) state |= (1 << BTN_BACK);
  if (keys[SDL_SCANCODE_P]) state |= (1 << BTN_POWER);
  state |= scriptedButtonState();

  lastState_ = state;
  for (int i = 0; i <= 6; i++) {
    uint8_t bit = 1 << i;
    if ((state & bit) && !(prevState_ & bit)) anyPressed_ = true;
    if (!(state & bit) && (prevState_ & bit)) anyReleased_ = true;
  }
  if (state != 0) {
    if (prevState_ == 0) {
      pressStartMs_ = millis();
    } else if (!pressStartMs_) {
      pressStartMs_ = millis();
    }
  } else {
    pressStartMs_ = 0;
  }
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (lastState_ & (1 << buttonIndex)) != 0;
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (lastState_ & (1 << buttonIndex)) != 0 && (prevState_ & (1 << buttonIndex)) == 0;
}

bool HalGPIO::wasAnyPressed() const { return anyPressed_; }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (lastState_ & (1 << buttonIndex)) == 0 && (prevState_ & (1 << buttonIndex)) != 0;
}

bool HalGPIO::wasAnyReleased() const { return anyReleased_; }

unsigned long HalGPIO::getHeldTime() const {
  if (!pressStartMs_) return 0;
  return millis() - pressStartMs_;
}

void HalGPIO::startDeepSleep() {
  (void)0;
}

int HalGPIO::getBatteryPercentage() const { return 100; }

void sim_gpio_pump_events() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) std::exit(0);
  }
}

bool sim_gpio_try_parse_button(const std::string& name, uint8_t& outButton) {
  outButton = parseButtonName(name);
  return outButton != 0xFF;
}

const char* sim_gpio_button_name(uint8_t button) { return buttonName(button); }

void sim_gpio_press_button(uint8_t button) {
  std::lock_guard<std::mutex> lock(g_controlMutex);
  g_controlState = static_cast<uint8_t>(g_controlState | (1 << button));
}

void sim_gpio_release_button(uint8_t button) {
  std::lock_guard<std::mutex> lock(g_controlMutex);
  g_controlState = static_cast<uint8_t>(g_controlState & ~(1 << button));
  g_controlTapSteps.erase(std::remove_if(g_controlTapSteps.begin(), g_controlTapSteps.end(),
                                         [button](const ScriptStep& step) { return step.button == button; }),
                          g_controlTapSteps.end());
}

void sim_gpio_release_all_buttons() {
  std::lock_guard<std::mutex> lock(g_controlMutex);
  g_controlState = 0;
  g_controlTapSteps.clear();
}

void sim_gpio_tap_button(uint8_t button, unsigned long durationMs) {
  sim_gpio_queue_tap_button(button, durationMs, 0);
}

void sim_gpio_queue_tap_button(uint8_t button, unsigned long durationMs, unsigned long delayMs) {
  std::lock_guard<std::mutex> lock(g_controlMutex);
  const unsigned long nowMs = millis();
  ScriptStep step;
  step.type = ScriptStep::Type::ButtonPress;
  step.button = button;
  step.atMs = nowMs + delayMs;
  step.releaseMs = step.atMs + durationMs;
  g_controlTapSteps.push_back(step);
}
