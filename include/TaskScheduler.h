#ifndef TASKSCHEDX_TASKSCHEDULER_H
#define TASKSCHEDX_TASKSCHEDULER_H

#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <future>
#include "TaskQueue.h"
#include "ThreadPool.h"
#include "Logger.h"
namespace TaskSchedX {

/**
 * @struct TaskConfig
 * @brief Configuration structure for scheduling tasks with named parameters
 *
 * Provides a clear, readable way to specify task parameters using named fields
 * instead of positional arguments. This improves code readability and reduces
 * the chance of parameter ordering errors.
 */
struct TaskConfig {
  std::function<void()>                 taskFn;                  ///< The function to execute
  std::chrono::system_clock::time_point startTime;                  ///< Scheduled start execution time
  int                                   priority;                   ///< Task priority (lower = higher priority)
  bool                                  isRepeatable = false;       ///< Whether task should repeat
  std::chrono::seconds repeatEvery   = std::chrono::seconds(10); ///< Interval between repeated task executions
  std::chrono::seconds timeout = std::chrono::seconds(60); ///< Maximum execution time before timeout

  TaskConfig() : taskFn(nullptr), startTime(std::chrono::system_clock::now()) {}

  TaskConfig(std::function<void()> func, std::chrono::system_clock::time_point st, int prio = 0,
             bool isRepeatable = false, std::chrono::seconds interval = std::chrono::seconds(10),
             std::chrono::seconds maxTime = std::chrono::seconds(60))
      : taskFn(std::move(func)), startTime(st), priority(prio), isRepeatable(isRepeatable), repeatEvery(interval),
        timeout(maxTime) {}

  TaskConfig(const TaskConfig &other)                = default; ///< Copy constructor
  TaskConfig(TaskConfig &&other) noexcept            = default; ///< Move constructor
  TaskConfig &operator=(const TaskConfig &other)     = default; ///< Copy assignment
  TaskConfig &operator=(TaskConfig &&other) noexcept = default; ///< Move assignment
  ~TaskConfig()                                      = default; ///< Destructor
  /**
   * @brief Converts the configuration to a string representation for logging
   *
   * Provides a human-readable format of the task configuration, useful for debugging.
   */
  std::string toString() const {
    std::ostringstream oss;
    oss << "TaskConfig(taskFn=" << (taskFn ? "set" : "null")
        << ", startTime=" << std::chrono::system_clock::to_time_t(startTime) << ", priority=" << priority
        << ", isRepeatable=" << (isRepeatable ? "true" : "false") << ", repeatEvery=" << repeatEvery.count()
        << "s, timeout=" << timeout.count() << "s)";
    return oss.str();
  };
};

/**
 * @class TaskScheduler
 * @brief Advanced task scheduler with support for priorities, timeouts, repeatable tasks, and dependencies
 *
 * The TaskScheduler class is the main orchestrator for task execution, combining a priority-based
 * task queue with a thread pool for concurrent execution. It provides comprehensive task management
 * including scheduling, cancellation, status tracking, and statistics collection.
 *
 */
class TaskScheduler {
public:
  TaskScheduler(size_t numThreads);

  ~TaskScheduler();

  std::string scheduleTask(std::function<void()> taskFn, std::chrono::system_clock::time_point startTime, int priority,
                           bool isRepeatable = false, std::chrono::seconds repeatEvery = std::chrono::seconds(10),
                           std::chrono::seconds timeout = std::chrono::seconds(60));

  std::string scheduleTask(const TaskConfig &config);

  bool cancelTask(const std::string &taskId);

  Task::Status getTaskStatus(const std::string &taskId);

  void start();

  void stop();

  void setTaskCompletionCallback(std::function<void(const std::string &, Task::Status)> callback);

  void setLogLevel(Logger::Level level);

  void setConsoleLoggingEnabled(bool enable);

  /**
   * @struct Statistics
   * @brief Container for scheduler performance and execution statistics
   *
   * Provides comprehensive statistics about task scheduling and execution
   * performance, useful for monitoring and debugging.
   */
  struct Statistics {
    size_t totalTasksScheduled = 0; ///< Total number of tasks scheduled since startup
    size_t tasksCompleted      = 0; ///< Number of tasks completed successfully
    size_t tasksFailed         = 0; ///< Number of tasks that failed with exceptions
    size_t tasksCancelled      = 0; ///< Number of tasks cancelled before or during execution
    size_t tasksTimeout       = 0; ///< Number of tasks that exceeded their timeout limit
  };

  Statistics getStatistics() const;

private:
  /** @brief Mutex for protecting queue operations */
  std::mutex queueMutex;
  /** @brief Condition variable for scheduler synchronization */
  std::condition_variable workAvailable_cv;
  /** @brief Atomic flag indicating scheduler running state */
  std::atomic<bool> isRunning{false};

  /** @brief Internal thread pool for parallel task execution */
  ThreadPool threadPool;
  /** @brief Internal thread running the scheduling loop */
  std::thread schedulerThread;

  /** @brief Priority queue for managing scheduled tasks */
  TaskQueue taskQueue;

  /** @brief Task completion callback */
  std::function<void(const std::string &, Task::Status)> completionCallback;

  /** @brief Registry of active tasks by ID */
  std::unordered_map<std::string, std::shared_ptr<Task>> activeTasks;
  /** @brief Mutex for protecting active tasks registry */
  std::mutex activeTasksMutex;

  /** @brief Registry of completed tasks by ID */
  std::unordered_map<std::string, Task::Status> completedTasks;

  /** @brief Mutex for protecting statistics updates */
  mutable std::mutex statsMutex;
  /** @brief Current scheduler statistics */
  Statistics stats;

  void runSchedulerLoop();

  void finalizeCompletedTask(std::shared_ptr<Task> activeTask, bool timedOut);

  void executeAndFinalizeTask(std::shared_ptr<Task> activeTask);

  bool runTaskWithTimeout(Task &task);

  void updateStatistics(Task::Status status);

  void notifyTaskCompletion(const std::string &taskId, Task::Status status);
};
} // namespace TaskSchedX

#endif // TASKSCHEDX_TASKSCHEDULER_H