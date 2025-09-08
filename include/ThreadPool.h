#ifndef TASKSCHEDX_THREADPOOL_H
#define TASKSCHEDX_THREADPOOL_H

#include <vector>
#include <thread>
#include <functional>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include "Logger.h"
namespace TaskSchedX {

/**
 * @class ThreadPool
 * @brief Fixed-size thread pool for concurrent task execution
 *
 * The ThreadPool manages a fixed number of worker threads that wait on a
 * condition variable and execute enqueued tasks.
 *
 * @note All worker threads are created during construction and joined in stop()
 * @note Dispatch order is FIFO; completion order is naturally non-deterministic.
 */
class ThreadPool {
public:
  explicit ThreadPool(size_t numThreads, const std::string &namePrefix = "Thread");

  ~ThreadPool() noexcept;

  // Non-copyable and non-assignable.
  ThreadPool(const ThreadPool &)            = delete;
  ThreadPool &operator=(const ThreadPool &) = delete;

  void enqueueTask(std::function<void()> task);
  void stop() noexcept;

private:
  /** @brief  Collection of worker threads */
  std::vector<std::thread> workers;
  /** @brief  Pending tasks (dispatch order FIFO). */
  std::queue<std::function<void()>> taskQueue;
  /** @brief Protects taskQueue; paired with workReadyCv */
  std::mutex queueMutex;
  /** @brief  Condition variable to signal work available or stop requested */
  std::condition_variable workReadyCv;
  /** @brief  Atomic flag indicating shutdown request*/
  std::atomic<bool> stopFlag;

  /** @brief Optional name prefix for worker threads */
  std::string threadNamePrefix;
  /** @brief  Used to generate unique thread names*/
  std::atomic<size_t> threadCounter{0};

  void workerLoop();
};
} // namespace TaskSchedX

#endif // TASKSCHEDX_THREADPOOL_H