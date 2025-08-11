#ifndef TASKSCHEDX_TASKQUEUE_H
#define TASKSCHEDX_TASKQUEUE_H

#include <queue>
#include <mutex>
#include <functional>
#include <chrono>
#include <condition_variable>
#include "Task.h"
namespace TaskSchedX {

/**
 * @class TaskQueue
 * @brief Thread-safe priority queue for managing scheduled tasks
 *
 * The TaskQueue class provides a thread-safe wrapper around std::priority_queue
 * for managing Task objects. It ensures proper synchronization for concurrent
 * access by multiple threads while maintaining task priority ordering.
 *
 * The queue uses Task::operator> for ordering, which prioritizes tasks by:
 * 1. Priority value (lower numbers first)
 * 2. Start execution time (earlier times first) for equal priorities
 *
 */
class TaskQueue {
public:
  void addTask(const Task &task);

  Task getTask();

  bool isEmpty() const;

  bool cancelTask(const std::string &taskId);

  size_t size() const;

  void clear();

private:
  /** @brief  Priority queue storing tasks ordered by Task::operator> */
  std::priority_queue<Task, std::vector<Task>, std::greater<Task>> queue;

  /** @brief  Mutex for protecting queue operations from concurrent access */
  mutable std::mutex queueMutex;
};
} // namespace TaskSchedX

#endif // TASKSCHEDX_TASKQUEUE_H