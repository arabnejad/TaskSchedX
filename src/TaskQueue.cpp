#include "TaskQueue.h"

namespace TaskSchedX {

/**
 * @brief Adds a task to the priority queue
 *
 * Add a new task into the queue while maintaining priority order.
 *
 * @param task shared pointer to the task to enqueue
 *
 * @note Tasks are ordered by priority (lower values first), then by start execution time
 * @note Not thread-safe. Callers must synchronize externally
 */
void TaskQueue::push(const std::shared_ptr<Task> &task) {
  pq.push(task);
}

/**
 * @brief Retrieves and removes the highest priority task from the queue
 *
 * Returns the task with the highest priority (lowest priority value)
 * and removes it from the pq.
 *
 * @return the `std::shared_ptr<Task>` of the highest priority task
 *
 * @warning Calling this on an empty queue results in undefined behavior
 * @note Always check isEmpty() before calling this method
 */
std::shared_ptr<Task> TaskQueue::pop() {
  auto task = pq.top();
  pq.pop();
  return task;
}

/**
 * @brief Checks if the queue is empty
 *
 * Returns true if the queue contains no tasks.
 *
 * @return true if the queue is empty, false otherwise
 *
 * @note The queue state may change immediately after this call in multithreaded environments.
 */
bool TaskQueue::isEmpty() const {
  return pq.empty();
}

/**
 * @brief Cancels a task by its unique identifier (if present).
 *
 * Searches for a task with the specified ID, marks it as cancelled,
 * and removes it from the queue.
 *
 * @param taskId The unique identifier of the task to cancel
 *
 * @return true if the task was found and cancelled, false otherwise
 *
 * @note Not thread-safe; callers must synchronize.
 * @note This only affects tasks still in the queue, and not running tasks.
 */
bool TaskQueue::cancelTask(const std::string &taskId) {

  // Temporarily hold non-matching tasks
  std::vector<std::shared_ptr<Task>> tempTasks;

  // Flag to track if the target task was found
  bool found = false;

  // Extract all tasks from the queue while searching for the target
  while (!pq.empty()) {
    auto task = pq.top();
    pq.pop();

    // Check if this is the task we're looking for
    if (task->getId() == taskId) {
      // Mark the matching task it as cancelled, and drop it
      task->setStatus(Task::Status::CANCELLED);
      found = true;
    } else {
      // Keep non-matching tasks for reinsertion
      tempTasks.push_back(task);
    }
  }

  // Re-insert all non-matching tasks back into the queue
  for (auto &task : tempTasks) {
    pq.push(task);
  }

  // Return whether the target task was found and cancelled
  return found;
}

/**
 * @brief Gets the current number of tasks in the queue
 *
 * Returns the total number of tasks currently stored in the queue.
 *
 * @return Number of tasks in the queue
 *
 * @note The count may change immediately after this call in multithreaded
 * environments.
 */

size_t TaskQueue::size() const noexcept {
  return pq.size();
}

/**
 * @brief Removes all tasks from the queue
 *
 * Empties the queue by removing all stored tasks.
 *
 * @warning All pending tasks will be lost and cannot be recovered
 */
void TaskQueue::clear() noexcept {
  // Remove all elements from the priority queue
  // This is done by repeatedly popping until the queue is empty
  while (!pq.empty()) {
    pq.pop();
  }
}

} // namespace TaskSchedX
