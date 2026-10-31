#include "FreeRTOSStub.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <new>
#include <thread>
#include <unordered_map>

namespace {

struct TaskExit : std::exception {};

struct TaskInfo {
  std::thread thread;
  std::atomic<bool> cancelled{false};
};

// Backs every FreeRTOS semaphore flavour: a mutex starts with count 1, a binary
// semaphore with 0, and a recursive mutex also tracks its owner's nesting depth.
struct SimSemaphore {
  explicit SimSemaphore(uint32_t initialCount) : count(initialCount) {}
  std::mutex mtx;
  std::condition_variable cv;
  uint32_t count;
  TaskHandle_t owner = nullptr;
  uint32_t depth = 0;
};

std::unordered_map<TaskHandle_t, TaskInfo*> s_tasks;
std::mutex s_tasksMutex;

std::unordered_map<TaskHandle_t, uint32_t> s_notifyCounts;
std::mutex s_notifyMutex;
std::condition_variable s_notifyCv;

thread_local TaskHandle_t t_currentHandle = nullptr;
thread_local TaskInfo* t_currentInfo = nullptr;

const TaskHandle_t kMainPseudoTask = reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(1));

inline TaskHandle_t currentHandleOrMain() {
  return t_currentHandle ? t_currentHandle : kMainPseudoTask;
}

inline void checkCancelled() {
  if (t_currentInfo && t_currentInfo->cancelled.load()) throw TaskExit();
}

}  // namespace

void vTaskDelay(unsigned ms) {
  checkCancelled();
  constexpr unsigned kSliceMs = 5;
  unsigned remaining = ms;
  while (remaining > 0) {
    const unsigned sleepMs = remaining < kSliceMs ? remaining : kSliceMs;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    remaining -= sleepMs;
    checkCancelled();
  }
}

int xTaskCreate(void (*fn)(void*), const char*, unsigned, void* param, int, TaskHandle_t* handle) {
  auto h = reinterpret_cast<TaskHandle_t>(new uintptr_t(0));
  auto* info = new TaskInfo();
  {
    std::lock_guard<std::mutex> lock(s_tasksMutex);
    s_tasks[h] = info;
  }

  info->thread = std::thread([fn, param, h, info]() {
    t_currentHandle = h;
    t_currentInfo = info;
    try {
      fn(param);
    } catch (const TaskExit&) {
    }
    t_currentHandle = nullptr;
    t_currentInfo = nullptr;
  });

  if (handle) *handle = h;
  return pdPASS;
}

int xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                            TaskHandle_t* handle, BaseType_t) {
  return xTaskCreate(fn, name, stack, param, prio, handle);
}

void vTaskDelete(TaskHandle_t h) {
  if (!h) return;
  TaskInfo* info = nullptr;
  {
    std::lock_guard<std::mutex> lock(s_tasksMutex);
    const auto it = s_tasks.find(h);
    if (it != s_tasks.end()) {
      info = it->second;
      s_tasks.erase(it);
    }
  }
  if (!info) return;

  info->cancelled.store(true);
  {
    std::lock_guard<std::mutex> lock(s_notifyMutex);
    s_notifyCounts[h] = 1;
  }
  s_notifyCv.notify_all();

  if (info->thread.joinable()) info->thread.join();
  delete info;
  delete reinterpret_cast<uintptr_t*>(h);
}

TaskHandle_t xTaskGetCurrentTaskHandle() { return currentHandleOrMain(); }

BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, eNotifyAction action) {
  if (!task) return pdFALSE;
  std::lock_guard<std::mutex> lock(s_notifyMutex);
  uint32_t& count = s_notifyCounts[task];
  if (action == eIncrement) {
    count += value ? value : 1;
  } else {
    count += 1;
  }
  s_notifyCv.notify_all();
  return pdTRUE;
}

BaseType_t xTaskNotifyGive(TaskHandle_t task) { return xTaskNotify(task, 0, eIncrement); }

uint32_t ulTaskNotifyTake(BaseType_t clearCountOnExit, TickType_t ticksToWait) {
  const TaskHandle_t self = currentHandleOrMain();

  std::unique_lock<std::mutex> lock(s_notifyMutex);
  auto ready = [&]() { return s_notifyCounts[self] > 0; };

  if (ticksToWait == portMAX_DELAY) {
    while (!ready()) {
      lock.unlock();
      checkCancelled();
      lock.lock();
      s_notifyCv.wait_for(lock, std::chrono::milliseconds(5));
    }
  } else if (!ready()) {
    const auto timeout = std::chrono::milliseconds(ticksToWait * portTICK_PERIOD_MS);
    s_notifyCv.wait_for(lock, timeout, ready);
  }

  uint32_t& count = s_notifyCounts[self];
  if (count == 0) return 0;

  const uint32_t ret = count;
  if (clearCountOnExit == pdTRUE)
    count = 0;
  else
    count -= 1;
  return ret;
}

uint32_t ulTaskNotifyValueClear(TaskHandle_t task, uint32_t bitsToClear) {
  const TaskHandle_t handle = task ? task : currentHandleOrMain();
  std::lock_guard<std::mutex> lock(s_notifyMutex);
  uint32_t& value = s_notifyCounts[handle];
  const uint32_t previous = value;
  value &= ~bitsToClear;
  return previous;
}

SemaphoreHandle_t xSemaphoreCreateMutex() { return new SimSemaphore(1); }

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return new SimSemaphore(1); }

SemaphoreHandle_t xSemaphoreCreateBinary() { return new SimSemaphore(0); }

SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) {
  static_assert(sizeof(StaticSemaphore_t) >= sizeof(SimSemaphore), "StaticSemaphore_t too small");
  return new (storage) SimSemaphore(1);
}

static BaseType_t takeSemaphore(SemaphoreHandle_t m, TickType_t timeout, bool recursive) {
  auto* sem = static_cast<SimSemaphore*>(m);
  if (!sem) return pdFALSE;
  const TaskHandle_t self = currentHandleOrMain();

  std::unique_lock<std::mutex> lock(sem->mtx);
  if (recursive && sem->depth > 0 && sem->owner == self) {
    sem->depth++;
    return pdTRUE;
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
  while (sem->count == 0) {
    if (timeout == 0 || (timeout != portMAX_DELAY && std::chrono::steady_clock::now() >= deadline)) return pdFALSE;
    sem->cv.wait_for(lock, std::chrono::milliseconds(5));
    lock.unlock();
    checkCancelled();
    lock.lock();
  }
  sem->count--;
  sem->owner = self;
  sem->depth = 1;
  return pdTRUE;
}

static BaseType_t giveSemaphore(SemaphoreHandle_t m, bool recursive) {
  auto* sem = static_cast<SimSemaphore*>(m);
  if (!sem) return pdFALSE;
  {
    std::lock_guard<std::mutex> lock(sem->mtx);
    if (recursive && sem->depth > 1) {
      sem->depth--;
      return pdTRUE;
    }
    sem->owner = nullptr;
    sem->depth = 0;
    sem->count++;
  }
  sem->cv.notify_one();
  return pdTRUE;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t timeout) { return takeSemaphore(m, timeout, false); }

BaseType_t xSemaphoreGive(SemaphoreHandle_t m) { return giveSemaphore(m, false); }

BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t m, TickType_t timeout) { return takeSemaphore(m, timeout, true); }

BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t m) { return giveSemaphore(m, true); }

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t m, BaseType_t* higherPriorityTaskWoken) {
  if (higherPriorityTaskWoken) *higherPriorityTaskWoken = pdFALSE;
  return giveSemaphore(m, false);
}

TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t m) {
  auto* sem = static_cast<SimSemaphore*>(m);
  if (!sem) return nullptr;
  std::lock_guard<std::mutex> lock(sem->mtx);
  return sem->owner;
}

// Only used to probe whether a mutex is free without taking it.
BaseType_t xQueuePeek(SemaphoreHandle_t q, void*, TickType_t) {
  auto* sem = static_cast<SimSemaphore*>(q);
  if (!sem) return pdFALSE;
  std::lock_guard<std::mutex> lock(sem->mtx);
  return sem->count > 0 ? pdTRUE : pdFALSE;
}

void vSemaphoreDelete(SemaphoreHandle_t m) { delete static_cast<SimSemaphore*>(m); }

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 4096; }
