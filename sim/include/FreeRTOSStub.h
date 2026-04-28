#pragma once

#include <cstdint>

using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;

constexpr int pdPASS = 1;
constexpr int pdTRUE = 1;
constexpr int pdFALSE = 0;
constexpr unsigned portMAX_DELAY = 0xFFFFFFFF;
constexpr int portTICK_PERIOD_MS = 1;
constexpr int eIncrement = 0;

int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                TaskHandle_t* handle);
void vTaskDelete(TaskHandle_t h);
SemaphoreHandle_t xSemaphoreCreateMutex();
void xSemaphoreTake(SemaphoreHandle_t m, unsigned timeout);
void xSemaphoreGive(SemaphoreHandle_t m);
void vSemaphoreDelete(SemaphoreHandle_t m);
void vTaskDelay(unsigned ms);
TaskHandle_t xTaskGetCurrentTaskHandle();
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t m);
int xTaskNotify(TaskHandle_t h, uint32_t value, int action);
uint32_t ulTaskNotifyTake(int clearCountOnExit, unsigned timeout);
int xQueuePeek(SemaphoreHandle_t m, void* out, unsigned timeout);
inline void taskENTER_CRITICAL(void*) {}
inline void taskEXIT_CRITICAL(void*) {}
