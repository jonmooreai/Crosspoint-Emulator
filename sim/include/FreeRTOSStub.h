#pragma once

#include <cstdint>

using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;

constexpr int pdPASS = 1;
constexpr unsigned portMAX_DELAY = 0xFFFFFFFF;
constexpr int portTICK_PERIOD_MS = 1;

int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                TaskHandle_t* handle);
void vTaskDelete(TaskHandle_t h);
SemaphoreHandle_t xSemaphoreCreateMutex();
void xSemaphoreTake(SemaphoreHandle_t m, unsigned timeout);
void xSemaphoreGive(SemaphoreHandle_t m);
void vSemaphoreDelete(SemaphoreHandle_t m);
void vTaskDelay(unsigned ms);
