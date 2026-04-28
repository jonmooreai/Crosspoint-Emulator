#include "FreeRTOSStub.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace {

struct TaskExit : std::exception {};

struct TaskInfo {
  std::thread thread;
  std::atomic<bool> cancelled{false};
  std::mutex notifyMutex;
  std::condition_variable notifyCv;
  uint32_t notifyCount = 0;
};

struct MutexInfo {
  std::mutex mutex;
  std::atomic<TaskHandle_t> owner{nullptr};
};

std::unordered_map<TaskHandle_t, TaskInfo*> s_tasks;
std::mutex s_mutex;
uintptr_t s_mainTaskStorage = 0;
TaskHandle_t s_mainTaskHandle = reinterpret_cast<TaskHandle_t>(&s_mainTaskStorage);

thread_local TaskHandle_t t_currentHandle = nullptr;
thread_local TaskInfo* t_currentInfo = nullptr;

inline void checkCancelled() {
  if (t_currentInfo && t_currentInfo->cancelled.load()) {
    throw TaskExit();
  }
}

}  // namespace

void vTaskDelay(unsigned ms) {
  checkCancelled();
  constexpr unsigned SLICE_MS = 5;
  unsigned remaining = ms;
  while (remaining > 0) {
    const unsigned sleepMs = remaining < SLICE_MS ? remaining : SLICE_MS;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    remaining -= sleepMs;
    checkCancelled();
  }
}

int xTaskCreate(void (*fn)(void*), const char*, unsigned, void* param, int, TaskHandle_t* handle) {
  auto h = reinterpret_cast<TaskHandle_t>(new uintptr_t(0));
  auto* info = new TaskInfo();
  {
    std::lock_guard<std::mutex> lock(s_mutex);
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
  if (handle) {
    *handle = h;
  }
  return pdPASS;
}

void vTaskDelete(TaskHandle_t h) {
  if (!h) return;
  TaskInfo* info = nullptr;
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_tasks.find(h);
    if (it != s_tasks.end()) {
      info = it->second;
      s_tasks.erase(it);
    }
  }
  if (info) {
    info->cancelled.store(true);
    info->notifyCv.notify_all();
    if (info->thread.joinable()) {
      info->thread.join();
    }
    delete info;
  }
  delete reinterpret_cast<uintptr_t*>(h);
}

SemaphoreHandle_t xSemaphoreCreateMutex() {
  return reinterpret_cast<SemaphoreHandle_t>(new MutexInfo());
}

void xSemaphoreTake(SemaphoreHandle_t m, unsigned) {
  auto* mtx = m ? static_cast<MutexInfo*>(m) : nullptr;
  if (!mtx) return;
  const TaskHandle_t owner = t_currentHandle ? t_currentHandle : s_mainTaskHandle;

  if (!t_currentInfo) {
    mtx->mutex.lock();
    mtx->owner.store(owner);
    return;
  }

  while (!mtx->mutex.try_lock()) {
    checkCancelled();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  mtx->owner.store(owner);
}

void xSemaphoreGive(SemaphoreHandle_t m) {
  auto* mtx = m ? static_cast<MutexInfo*>(m) : nullptr;
  if (!mtx) return;
  mtx->owner.store(nullptr);
  mtx->mutex.unlock();
}

void vSemaphoreDelete(SemaphoreHandle_t m) {
  delete static_cast<MutexInfo*>(m);
}

TaskHandle_t xTaskGetCurrentTaskHandle() {
  return t_currentHandle ? t_currentHandle : s_mainTaskHandle;
}

TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t m) {
  auto* mtx = m ? static_cast<MutexInfo*>(m) : nullptr;
  return mtx ? mtx->owner.load() : nullptr;
}

int xTaskNotify(TaskHandle_t h, uint32_t value, int) {
  TaskInfo* info = nullptr;
  {
    std::lock_guard<std::mutex> lock(s_mutex);
    auto it = s_tasks.find(h);
    if (it != s_tasks.end()) {
      info = it->second;
    }
  }
  if (!info) return pdFALSE;
  {
    std::lock_guard<std::mutex> lock(info->notifyMutex);
    info->notifyCount += value;
  }
  info->notifyCv.notify_all();
  return pdPASS;
}

uint32_t ulTaskNotifyTake(int clearCountOnExit, unsigned timeout) {
  if (!t_currentInfo) return 0;

  std::unique_lock<std::mutex> lock(t_currentInfo->notifyMutex);
  auto hasNotification = [info = t_currentInfo]() { return info->notifyCount > 0 || info->cancelled.load(); };

  if (timeout == portMAX_DELAY) {
    t_currentInfo->notifyCv.wait(lock, hasNotification);
  } else {
    t_currentInfo->notifyCv.wait_for(lock, std::chrono::milliseconds(timeout), hasNotification);
  }

  if (t_currentInfo->cancelled.load()) {
    lock.unlock();
    checkCancelled();
    return 0;
  }

  const uint32_t current = t_currentInfo->notifyCount;
  if (current == 0) return 0;

  if (clearCountOnExit) {
    t_currentInfo->notifyCount = 0;
    return current;
  }

  t_currentInfo->notifyCount--;
  return current;
}

int xQueuePeek(SemaphoreHandle_t m, void*, unsigned) {
  auto* mtx = m ? static_cast<MutexInfo*>(m) : nullptr;
  if (!mtx) return pdFALSE;
  if (mtx->mutex.try_lock()) {
    mtx->mutex.unlock();
    return pdTRUE;
  }
  return pdFALSE;
}
