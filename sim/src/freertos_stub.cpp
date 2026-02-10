#include "FreeRTOSStub.h"

#include <thread>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Sim FreeRTOS stub
//
// Key challenge: on real FreeRTOS, vTaskDelete() kills a task immediately,
// even if it is blocked on a semaphore. std::thread has no such mechanism,
// so we must make every blocking point *cooperative* — the task periodically
// checks whether it has been cancelled and throws TaskExit to unwind.
//
// Deadlock scenario this solves:
//   1. Main thread: xSemaphoreTake(renderingMutex)     — holds the mutex
//   2. Main thread: vTaskDelete(displayTaskHandle)      — sets cancelled, join()s
//   3. Display task: xSemaphoreTake(renderingMutex)     — would block forever
//
// Fix: xSemaphoreTake uses try_lock() in a loop, checking cancelled between
// attempts. When cancelled, it throws TaskExit so the thread exits and
// join() returns.
// ---------------------------------------------------------------------------

namespace {

struct TaskExit : std::exception {};

struct TaskInfo {
  std::thread thread;
  std::atomic<bool> cancelled{false};
};
std::unordered_map<TaskHandle_t, TaskInfo*> s_tasks;
std::mutex s_mutex;

thread_local TaskHandle_t t_currentHandle = nullptr;
thread_local TaskInfo* t_currentInfo = nullptr;

// Check if the current task has been cancelled and throw if so.
inline void checkCancelled() {
  if (t_currentInfo && t_currentInfo->cancelled.load())
    throw TaskExit();
}

}  // namespace

void vTaskDelay(unsigned ms) {
  checkCancelled();
  // Sleep in small increments so cancellation is noticed quickly.
  constexpr unsigned SLICE_MS = 5;
  unsigned remaining = ms;
  while (remaining > 0) {
    unsigned sleepMs = remaining < SLICE_MS ? remaining : SLICE_MS;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    remaining -= sleepMs;
    checkCancelled();
  }
}

int xTaskCreate(void (*fn)(void*), const char*, unsigned, void* param, int,
                TaskHandle_t* handle) {
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
    } catch (const TaskExit&) {}
    t_currentHandle = nullptr;
    t_currentInfo = nullptr;
  });
  if (handle) *handle = h;
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
    // Signal cancellation — the task will see this in vTaskDelay or xSemaphoreTake.
    info->cancelled.store(true);
    if (info->thread.joinable())
      info->thread.join();
    delete info;
  }
  delete reinterpret_cast<uintptr_t*>(h);
}

SemaphoreHandle_t xSemaphoreCreateMutex() {
  return reinterpret_cast<SemaphoreHandle_t>(new std::mutex);
}

void xSemaphoreTake(SemaphoreHandle_t m, unsigned) {
  auto* mtx = m ? static_cast<std::mutex*>(m) : nullptr;
  if (!mtx) return;

  // Main thread (no task context) — just lock normally.
  if (!t_currentInfo) {
    mtx->lock();
    return;
  }

  // Task thread — spin on try_lock with cancellation checks so we never
  // block permanently on a mutex held by the thread that is join()ing us.
  while (!mtx->try_lock()) {
    checkCancelled();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void xSemaphoreGive(SemaphoreHandle_t m) {
  if (m) static_cast<std::mutex*>(m)->unlock();
}

void vSemaphoreDelete(SemaphoreHandle_t m) {
  delete static_cast<std::mutex*>(m);
}
