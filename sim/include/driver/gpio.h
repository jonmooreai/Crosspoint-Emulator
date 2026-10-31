#pragma once

typedef int gpio_num_t;

typedef enum { GPIO_MODE_INPUT, GPIO_MODE_OUTPUT } gpio_mode_t;
typedef enum { GPIO_INTR_DISABLE, GPIO_INTR_LOW_LEVEL, GPIO_INTR_HIGH_LEVEL } gpio_int_type_t;

#define GPIO_NUM_13 13

inline int gpio_hold_en(gpio_num_t) { return 0; }
inline int gpio_hold_dis(gpio_num_t) { return 0; }
inline void gpio_deep_sleep_hold_en() {}
inline void gpio_deep_sleep_hold_dis() {}
inline int gpio_set_direction(gpio_num_t, gpio_mode_t) { return 0; }
inline int gpio_set_level(gpio_num_t, unsigned) { return 0; }
inline int gpio_wakeup_enable(gpio_num_t, gpio_int_type_t) { return 0; }
inline int gpio_wakeup_disable(gpio_num_t) { return 0; }
inline int gpio_set_intr_type(gpio_num_t, gpio_int_type_t) { return 0; }
