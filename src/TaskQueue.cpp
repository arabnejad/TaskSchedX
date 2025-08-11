#include "TaskQueue.h"
#include <algorithm>

namespace TaskSchedX {

/**
 * @brief Adds a task to the priority queue
 *
 * Inserts a new task into the queue while maintaining priority order.
 * The queue uses Task::operator > for comparison, prioritizing tasks with
 * lower priority values and earlier start execution times.
 * The operation is thread-safe and will block other queue operations
 * during insertion.
 *
 * @param task The task to add to the queue (copied)
 *
 * @note Tasks are ordered by priority (lower values first), then by start execution time
 */
void TaskQueue::addTask(const Task &task) {
  // Acquire exclusive lock on the queue mutex to ensure thread safety
  // This prevents other threads from modifying the queue during insertion
  std::lock_guard<std::mutex> lock(queueMutex);
  // Insert the task into the priority queue
  // The queue automatically maintains ordering based on Task::operator >
  // Tasks with lower priority values will be at the top of the queue
  queue.push(task);
}

/**
 * @brief Retrieves and removes the highest priority task from the queue
 *
 * Returns the task with the highest priority (lowest priority value)
 * and removes it from the queue. This operation is thread-safe but **assumes**
 * the queue is not empty.
 *
 * @return Copy of the highest priority task
 *
 * @warning Calling this on an empty queue results in undefined behavior
 * @note Always check isEmpty() before calling this method
 */
Task TaskQueue::getTask() {
  // Acquire exclusive lock on the queue mutex for thread-safe access
  std::lock_guard<std::mutex> lock(queueMutex);

  // Get a copy of the highest priority task from the top of the queue
  // The priority queue ensures this is the task that should execute next
  Task task = queue.top();

  // Remove the task from the queue now that we have a copy
  // This prevents the same task from being retrieved multiple times
  queue.pop();

  // Return the copy of the task for execution
  return task;
}

/**
 * @brief Checks if the queue is empty
 *
 * Returns true if the queue contains no tasks. This provides a consistent
 * view of the queue state at the time of the call.
 *
 * @return true if the queue is empty, false otherwise
 *
 * @note The queue state may change immediately after this call in multithreaded environments
 */
bool TaskQueue::isEmpty() const {
  // Acquire shared lock on the queue mutex (const method uses mutable mutex)
  // This allows multiple threads to check emptiness simultaneously
  std::lock_guard<std::mutex> lock(queueMutex);

  // Return whether the underlying priority queue is empty
  return queue.empty();
}

/**
 * @brief Cancels a task by its unique identifier
 *
 * Searches for a task with the specified ID, marks it as cancelled,
 * and removes it from the queue. This operation requires scanning
 * the entire queue.
 *
 * @param taskId The unique identifier of the task to cancel
 *
 * @return true if the task was found and cancelled, false otherwise
 *
 * @note The cancelled task is removed from the queue entirely
 */
bool TaskQueue::cancelTask(const std::string &taskId) {
  // Acquire exclusive lock for the entire cancellation operation
  std::lock_guard<std::mutex> lock(queueMutex);

  // Temporary vector to hold tasks while searching
  std::vector<Task> tempTasks;
  // Flag to track if the target task was found
  bool found = false;

  // Extract all tasks from the queue while searching for the target
  while (!queue.empty()) {
    Task task = queue.top(); // Get the top task
    queue.pop();             // Remove it from the queue

    // Check if this is the task we're looking for
    if (task.getId() == taskId) {
      // Found the target task - mark it as cancelled
      task.setStatus(Task::Status::CANCELLED);
      found = true;
      // no need to added the cancelled task back to tempTasks
    } else {
      // This is not the target task - keep it for re-insertion
      tempTasks.push_back(task);
    }
  }

  // Re-insert all non-cancelled tasks back into the queue
  // This restores the queue with all tasks except the cancelled one
  for (const auto &task : tempTasks) {
    queue.push(task);
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
 * @note The count may change immediately after this call in multithreaded environments
 */

size_t TaskQueue::size() const {
  // Acquire shared lock for thread-safe access to queue size
  std::lock_guard<std::mutex> lock(queueMutex);

  // Return the current size of the underlying priority queue
  return queue.size();
}

/**
 * @brief Removes all tasks from the queue
 *
 * Empties the queue by removing all stored tasks.
 *
 * @warning All pending tasks will be lost and cannot be recovered
 */
void TaskQueue::clear() {
  // Acquire exclusive lock to prevent other operations during clearing
  std::lock_guard<std::mutex> lock(queueMutex);

  // Remove all elements from the priority queue
  // This is done by repeatedly popping until the queue is empty
  while (!queue.empty()) {
    queue.pop();
  }
}

} // namespace TaskSchedX
