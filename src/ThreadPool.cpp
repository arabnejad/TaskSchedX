#include "ThreadPool.h"
#include <iostream>

#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
/* Windows thread naming APIs */
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(__APPLE__) || defined(__linux__)
/* POSIX thread naming APIs */
#include <pthread.h>
#endif

namespace TaskSchedX {

/**
 * @brief Create a pool with numThreads workers
 *
 * Workers start immediately, wait for tasks, and execute them as they arrive.
 *
 * @param numThreads Number of worker threads to create
 * @param namePrefix Optional prefix for thread names (for debugging)
 *
 * @note All threads are started immediately and begin waiting for tasks
 */
ThreadPool::ThreadPool(size_t numThreads, const std::string &namePrefix)
    : stopFlag(false), threadNamePrefix(namePrefix) {

  // Create the specified number of worker threads
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this]() {
      // this is not perfect. but I could not make it better
      size_t             index = threadCounter.fetch_add(1);
      std::ostringstream oss;
      oss << threadNamePrefix << "-" << index + 1; // Generate unique thread name

// Set name from inside the thread, using platform-specific call
#ifdef __APPLE__
      // macOS: one-arg pthread_setname_np; failures are ignored
      (void)pthread_setname_np(oss.str().c_str());
#elif defined(__linux__)
      // Linux: two-arg pthread_setname_np; truncate to 15 so it fits the 16-byte (incl NUL) limit
      std::string name = oss.str().substr(0, 15);
      (void)pthread_setname_np(pthread_self(), name.c_str());
#elif (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
      // Windows: ignore any HRESULT failures
      std::wstring w = std::wstring(oss.str().begin(), oss.str().end());
      (void)SetThreadDescription(GetCurrentThread(), w.c_str());
#endif

      workerLoop(); // actual task processing loop
    });
  }
  // At this point, all worker threads are running and waiting for tasks
  // The threads will remain active until stop() is called
}

/**
 * @brief Ensures a graceful shutdown by delegating to stop()
 *
 * Automatically calls stop() to ensure all worker threads are properly
 * joined before the ThreadPool is destroyed. This prevents resource leaks
 * and ensures graceful shutdown.
 *
 */
ThreadPool::~ThreadPool() noexcept {
  stop();
}

/**
 * @brief Enqueue a task and notify one waiting worker
 *
 * Adds a task to the internal queue and notifies one waiting worker thread
 * to process it.
 *
 * @param task Function object to execute (must be callable with no parameters)
 *
 * @note Tasks are dispatched in FIFO order (completion is not
 * guaranteed FIFO)
 */
void ThreadPool::enqueueTask(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    taskQueue.push(std::move(task));
  }

  workReadyCv.notify_one();
}

/**
 * @brief Stops all worker threads and waits for them to complete
 *
 * Sets the stop flag, notifies all worker threads to wake up and exit,
 * then waits for all threads to complete their current tasks and terminate.
 *
 * @note Any remaining tasks in the queue will not be executed
 */
void ThreadPool::stop() noexcept {
  // Publish stop (release); workers observe it on acquire-loads
  stopFlag.store(true, std::memory_order_release);
  workReadyCv.notify_all();

  for (auto &worker : workers) {
    if (worker.joinable()) {
      worker.join();
    }
  }
  // Discard any pending tasks after joining workers
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    while (!taskQueue.empty())
      taskQueue.pop();
  }
}

/**
 * @brief Main worker thread function that processes tasks from the queue
 *
 * This is the function executed by each worker thread. It continuously:
 * 1. Waits for tasks to become available or stop signal
 * 2. Retrieves a task from the queue when available
 * 3. Executes the task
 * 4. Repeats until stop is requested
 *
 */
void ThreadPool::workerLoop() {
  // Continue processing tasks until stop is requested
  for (;;) {
    std::function<void()> task; // Task to be executed by the worker

    {
      std::unique_lock<std::mutex> lock(queueMutex);
      workReadyCv.wait(lock, [this] {
        // Wake when a task is queued or when stopFlag is set.
        return !taskQueue.empty() || stopFlag.load(std::memory_order_acquire);
      });

      // Exit if shutdown was requested
      if (stopFlag.load(std::memory_order_acquire))
        break;

      task = std::move(taskQueue.front());
      taskQueue.pop();
    }

    // Execute the task
    try {
      task();
    } catch (const std::exception &e) {
      std::cerr << "[ThreadPool] task exception: " << e.what() << std::endl;
    } catch (...) {
      std::cerr << "[ThreadPool] task threw non-std exception" << std::endl;
    }
  }
}
} // namespace TaskSchedX
