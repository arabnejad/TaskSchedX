#ifndef TASKSCHEDX_TASK_H
#define TASKSCHEDX_TASK_H

#include <functional>
#include <chrono>
#include <memory>
#include <atomic>
#include <string>

namespace TaskSchedX {

/**
 * @class Task
 * @brief Represents a schedulable unit of work with execution time, priority, and status tracking
 *
 * The Task class encapsulates a function to be executed along with its scheduling metadata
 * such as execution time, priority, repeatable behavior, and timeout settings. It provides
 * thread-safe status tracking and supports task dependencies.
 *
 */
class Task {
public:
  /**
   * @enum Status
   * @brief Represents the current execution status of a task
   */
  enum class Status {
    PENDING,   ///< Task is waiting to be executed
    RUNNING,   ///< Task is currently being executed
    COMPLETED, ///< Task has completed successfully
    FAILED,    ///< Task execution failed with an exception
    CANCELLED, ///< Task was cancelled before or during execution
    TIMEOUT    ///< Task execution exceeded the maximum allowed time
  };

  /**
   * @brief Converts Status enum to string for logging
   *
   * Returns a human-readable string representation of the tasks status
   * level for use in formatted log messages.
   *
   * @param status The status level to convert
   * @return String representation of the level (e.g., "PENDING", "CANCELLED")
   */
  static const char *status_to_string(Status status) {
    switch (status) {
    case Status::PENDING:
      return "PENDING";
    case Status::RUNNING:
      return "RUNNING";
    case Status::COMPLETED:
      return "COMPLETED";
    case Status::FAILED:
      return "FAILED";
    case Status::CANCELLED:
      return "CANCELLED";
    case Status::TIMEOUT:
      return "TIMEOUT";
    default:
      return "UNKNOWN";
    }
  }

  Task(std::function<void()> func, std::chrono::system_clock::time_point startTime, int priority,
       bool repeatable = false, std::chrono::seconds repeatInterval = std::chrono::seconds(10),
       std::chrono::seconds maxExecTime = std::chrono::seconds(60));

  // Copy constructor
  Task(const Task &other);

  // Move constructor
  Task(Task &&other) noexcept;

  // Copy assignment
  Task &operator=(const Task &other);

  // Move assignment
  Task &operator=(Task &&other) noexcept;

  void execute();

  void reschedule(std::chrono::system_clock::time_point newTime);

  void reschedule();

  const std::string &getId() const;

  Status getStatus() const;

  void setStatus(Status newStatus);

  bool operator>(const Task &other) const;

  std::string generateTaskId();

  static std::string getCurrentThreadName();

  /** @brief The function to execute */
  std::function<void()> executeFn;
  /** @brief  Scheduled start execution time*/
  std::chrono::system_clock::time_point startTime;
  /** @brief Task priority (lower = higher priority) */
  int priority;
  /** @brief  Current execution status (thread-safe) */
  std::atomic<Status> status;
  /** @brief  Whether task repeats after completion */
  bool repeatable;
  /** @brief  Time between repeated task executions */
  std::chrono::seconds repeatInterval;
  /** @brief Maximum allowed execution time */
  std::chrono::seconds executionTimeout;

private:
  /** @brief Unique task identifier */
  std::string taskId;
  /** @brief  Global counter for generating unique IDs */
  static std::atomic<size_t> nextId;
};

std::ostream &operator<<(std::ostream &os, Task::Status status);
} // namespace TaskSchedX

#endif // TASKSCHEDX_TASK_H