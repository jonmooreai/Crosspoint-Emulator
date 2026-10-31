#pragma once

#include <cstdint>

using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;
using QueueHandle_t = void*;
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
using portMUX_TYPE = int;

using StackType_t = uint8_t;

// Opaque task control block for xTaskCreateStatic callers sizing their storage.
struct StaticTask_t {
  alignas(16) unsigned char storage[512];
};

// Opaque storage for xSemaphoreCreateMutexStatic; sized for the sim's semaphore.
struct StaticSemaphore_t {
  alignas(16) unsigned char storage[256];
};

enum eNotifyAction {
  eNoAction = 0,
  eSetBits,
  eIncrement,
  eSetValueWithOverwrite,
  eSetValueWithoutOverwrite,
};

constexpr BaseType_t pdFALSE = 0;
constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdPASS = 1;
constexpr BaseType_t pdFAIL = 0;
constexpr TickType_t portMAX_DELAY = 0xFFFFFFFFu;
constexpr int portTICK_PERIOD_MS = 1;
constexpr portMUX_TYPE portMUX_INITIALIZER_UNLOCKED = 0;

#define pdMS_TO_TICKS(ms) (static_cast<TickType_t>(ms))
#define portYIELD_FROM_ISR(x) ((void)(x))
#define configASSERT(x) ((void)(x))

int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                TaskHandle_t* handle);
int xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                            TaskHandle_t* handle, BaseType_t core);
void vTaskDelete(TaskHandle_t h);
TaskHandle_t xTaskGetCurrentTaskHandle();
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t h);

BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, eNotifyAction action);
BaseType_t xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(BaseType_t clearCountOnExit, TickType_t ticksToWait);
uint32_t ulTaskNotifyValueClear(TaskHandle_t task, uint32_t bitsToClear);

SemaphoreHandle_t xSemaphoreCreateMutex();
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex();
SemaphoreHandle_t xSemaphoreCreateBinary();
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage);
BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t m);
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t m, TickType_t timeout);
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t m);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t m, BaseType_t* higherPriorityTaskWoken);
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t m);
BaseType_t xQueuePeek(SemaphoreHandle_t q, void* outItem, TickType_t timeout);
void vSemaphoreDelete(SemaphoreHandle_t m);

void vTaskDelay(unsigned ms);

#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))

#define taskENTER_CRITICAL(x) \
  do {                        \
    (void)(x);                \
  } while (0)

#define taskEXIT_CRITICAL(x) \
  do {                       \
    (void)(x);               \
  } while (0)
