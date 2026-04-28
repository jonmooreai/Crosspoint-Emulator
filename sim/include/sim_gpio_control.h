#pragma once

#include <cstdint>
#include <string>

bool sim_gpio_try_parse_button(const std::string& name, uint8_t& outButton);
const char* sim_gpio_button_name(uint8_t button);
void sim_gpio_press_button(uint8_t button);
void sim_gpio_release_button(uint8_t button);
void sim_gpio_release_all_buttons();
void sim_gpio_tap_button(uint8_t button, unsigned long durationMs);
void sim_gpio_queue_tap_button(uint8_t button, unsigned long durationMs, unsigned long delayMs);
