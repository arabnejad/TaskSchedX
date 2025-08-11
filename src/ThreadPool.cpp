#include "ThreadPool.h"
#include <iostream>

namespace TaskSchedX {

/**
 * @brief Constructs a thread pool with the specified number of worker threads
 *
 * Creates and starts the specified number of worker threads immediately.
 * Each thread will wait for tasks to be enqueued and execute them as
 * they become available.
 *
 * @param numThreads Number of worker threads to create
 * @param namePrefix Optional prefix for thread names (for debugging)
 *
 * @note All threads are started immediately and begin waiting for tasks
 * @note The number of threads cannot be changed after construction
 */
ThreadPool::ThreadPool(size_t numThreads, const std::string &namePrefix) : threadNamePrefix(namePrefix) {
  // Initialize the stopFlag to false - threads should start in running state
  // This atomic flag will be used to signal threads to stop when needed
  stopFlag = false;

  // Create the specified number of worker threads
  for (size_t i = 0; i < numThreads; ++i) {
    workers.emplace_back([this]() {
      // this is not perfect. but I could not make it better
      size_t             index = threadCounter.fetch_add(1);
      std::ostringstream oss;
      oss << threadNamePrefix << "-" << index + 1; // Generate unique thread name

// Set name from inside the thread, using platform-specific call
#ifdef __APPLE__
      pthread_setname_np(oss.str().c_str());
#elif defined(__linux__)
      pthread_setname_np(pthread_self(), oss.str().substr(0, 15).c_str()); // max 16 bytes incl. null
#elif defined(_WIN32)
      SetThreadDescription(GetCurrentThread(), std::wstring(oss.str().begin(), oss.str().end()).c_str());
#endif

      worker(); // actual task processing loop
    });
  }

  // At this point, all worker threads are running and waiting for tasks
  // The threads will remain active until stop() is called
}

/**
 * @brief Destructor that ensures proper cleanup of all threads
 *
 * Automatically calls stop() to ensure all worker threads are properly
 * joined before the ThreadPool is destroyed. This prevents resource leaks
 * and ensures graceful shutdown.
 *
 */
ThreadPool::~ThreadPool() {
  // Call stop() to ensure all threads are properly shut down
  // This is safe to call even if stop() has already been called
  stop();
}

/**
 * @brief Enqueues a task for execution by worker threads
 *
 * Adds a task to the internal queue and notifies one waiting worker thread
 * to process it. The task will be executed as soon as a worker thread
 * becomes available. Tasks are processed in FIFO (first-in, first-out) order
 *
 * @param task Function object to execute (must be callable with no parameters)
 *
 * @note Tasks are executed in FIFO order
 * @note The task is copied into the queue
 */
void ThreadPool::enqueueTask(const std::function<void()> &task) {
  {
    // Acquire exclusive lock on the task queue for thread-safe insertion
    std::lock_guard<std::mutex> lock(queueMutex);

    // Add the task to the end of the FIFO queue
    // The task is copied into the queue
    taskQueue.push(task);
  }

  // Notify one waiting worker thread that a new task is available
  // If multiple threads are waiting, only one will be awakened
  // If no threads are waiting, the notification is ignored
  condition.notify_one();
}

/**
 * @brief Stops all worker threads and waits for them to complete
 *
 * Sets the stop flag, notifies all worker threads to wake up and exit,
 * then waits for all threads to complete their current tasks and terminate.
 *
 * @note Any remaining tasks in the queue will not be executed
 * @note This method is safe to call multiple times
 */
void ThreadPool::stop() {
  // Set the atomic stop flag to true
  // This signals all worker threads that they should exit their loops
  stopFlag = true;

  // Wake up all worker threads that might be waiting for tasks
  // This ensures that threads check the stop flag and can exit
  condition.notify_all();

  // Wait for each worker thread to complete and join it
  for (auto &worker : workers) {
    // Check if the thread is joinable (hasn't been joined already)
    if (worker.joinable()) {
      // Wait for the thread to complete its current task and exit
      // This blocks until the thread's worker() function returns
      worker.join();
    }
  }

  // At this point, all worker threads have been properly shut down
  // and their resources have been cleaned up
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
void ThreadPool::worker() {
  // Continue processing tasks until stop is requested
  while (!stopFlag) {
    std::function<void()> task; // Task to be executed by the worker

    {
      std::unique_lock<std::mutex> lock(queueMutex);

      // Wait until either a task is available or stop is requested
      // The lambda function is the predicate - wait returns when it's true
      // This prevents spurious wakeups from causing issues
      condition.wait(lock, [this] { return !taskQueue.empty() || stopFlag; });

      // Check if we should stop (stop flag set and possibly no tasks)
      if (stopFlag)
        break; // Exit the worker loop to terminate the thread

      // At this point, we know there's at least one task in the queue
      // Get the next task from the front of the FIFO queue
      task = taskQueue.front();

      // Remove the task from the queue now that we have a copy
      taskQueue.pop();
    }

    // Execute the task outside of the lock to allow other threads
    // to access the queue while this task is running
    // Any exceptions thrown by the task are the task's responsibility
    task();
  }

  // Worker thread exits here when stop flag is set
  // The thread will be joined by the stop() method
}
} // namespace TaskSchedX
