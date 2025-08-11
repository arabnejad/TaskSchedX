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
namespace TaskSchedX {

/**
 * @class ThreadPool
 * @brief Thread pool implementation for concurrent task execution
 *
 * The ThreadPool class manages a fixed number of worker threads that can
 * execute tasks concurrently. It provides a thread-safe task queue and
 * automatic work distribution among available threads.
 *
 * @note All worker threads are created during construction and destroyed during shutdown
 * @note Tasks are executed in FIFO order (first enqueued, first executed)
 */
class ThreadPool {
public:
  explicit ThreadPool(size_t numThreads, const std::string &namePrefix = "Thread");

  ~ThreadPool();

  void enqueueTask(const std::function<void()> &task);

  void stop();

private:
  /** @brief  Collection of worker threads */
  std::vector<std::thread> workers;
  /** @brief  FIFO queue of pending tasks */
  std::queue<std::function<void()>> taskQueue;
  /** @brief Mutex protecting the task queue */
  std::mutex queueMutex;
  /** @brief  Condition variable for thread synchronization */
  std::condition_variable condition;
  /** @brief  Atomic flag indicating shutdown request*/
  std::atomic<bool> stopFlag;

  /** @brief Optional name prefix for worker threads */
  std::string threadNamePrefix;
  /** @brief  Used to generate unique thread names*/
  std::atomic<size_t> threadCounter{0};

  void worker();
};
} // namespace TaskSchedX

#endif // TASKSCHEDX_THREADPOOL_H