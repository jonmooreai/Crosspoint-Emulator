#include "HalGPIO.h"
#include "ArduinoStub.h"

#include <SDL.h>
#include <cstring>

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

  lastState_ = state;
  for (int i = 0; i <= 6; i++) {
    uint8_t bit = 1 << i;
    if ((state & bit) && !(prevState_ & bit)) anyPressed_ = true;
    if (!(state & bit) && (prevState_ & bit)) anyReleased_ = true;
  }
  if (state & ((1 << BTN_CONFIRM) | (1 << BTN_POWER)))
    pressStartMs_ = pressStartMs_ ? pressStartMs_ : millis();
  else
    pressStartMs_ = 0;
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
