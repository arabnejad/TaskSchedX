#ifndef TASKSCHEDX_TASKQUEUE_H
#define TASKSCHEDX_TASKQUEUE_H

#include <queue>
#include "Task.h"
namespace TaskSchedX {

// Comparator that delegates to Task::operator>() so that the priority queue’s top()
// returns the highest-priority task (lower numeric priority, earlier startTime on ties)
struct TaskPriorityCmp {
  bool operator()(const std::shared_ptr<Task> &a, const std::shared_ptr<Task> &b) const {
    return (*a) > (*b);
  }
};

/**
 * @class TaskQueue
 * @brief Priority queue of tasks ordered by priority then start time.
 *
 * *Thread-safety* : This container is not thread-safe. Callers
 * (e.g., `TaskScheduler`) must guard all operations with their own
 * mutex/condition variable to avoid missed wakeups.
 *
 * *Ordering* : The queue uses Task::operator> for ordering, which prioritizes tasks by:
 * 1. Priority value (lower numbers first)
 * 2. Start execution time (earlier times first) for equal priorities
 *
 */
class TaskQueue {
public:
  void push(const std::shared_ptr<Task> &task);

  std::shared_ptr<Task> pop();

  bool isEmpty() const;

  bool cancelTask(const std::string &taskId);

  size_t size() const noexcept;

  void clear() noexcept;

private:
  /** @brief  Priority queue storing tasks ordered by Task::operator> */
  std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task>>, TaskPriorityCmp> pq;
};
} // namespace TaskSchedX

#endif // TASKSCHEDX_TASKQUEUE_H