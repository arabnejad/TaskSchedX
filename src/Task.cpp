#include "Task.h"
#include <sstream>
#include <iomanip>
#include <thread>
#include <string>
#include <ostream>
#if defined(_WIN32) || defined(_WIN64) || defined(WIN32)
/* Windows headers */
#include <windows.h>
#include <processthreadsapi.h>
#elif defined(__APPLE__) || defined(__linux__)
/* POSIX headers */
#include <pthread.h>
#endif

namespace TaskSchedX {

/**
 * @brief Static atomic counter for generating unique task IDs
 * This counter is incremented each time a new Task is created,
 * ensuring that each task has a unique identifier.
 */
std::atomic<size_t> Task::nextId{1};

/**
 * @brief Constructs a new Task with specified parameters
 *
 * Initializes all task properties and generates a unique identifier.
 * The task starts in PENDING status and is ready to be scheduled.
 *
 * @param func The function to execute when the task runs
 * @param startTime The scheduled start execution time point
 * @param priority Task priority (lower values = higher priority)
 * @param repeatable Whether the task should repeat after completion
 * @param repeatInterval Time interval between repeated task executions
 * @param maxExecTime Maximum allowed execution time before timeout
 *
 * @note Priority comparison: tasks with lower priority values are executed first
 * @note Repeated tasks are automatically rescheduled after successful completion
 */
Task::Task(std::function<void()> func, std::chrono::system_clock::time_point startTime, int priority, bool repeatable,
           std::chrono::seconds repeatInterval, std::chrono::seconds maxExecTime)
    : executeFn(func),                // Store the function to execute
      startTime(startTime),           // Store the scheduled execution time
      priority(priority),             // Store the task priority (lower = higher priority)
      status(Status::PENDING),        // Initialize status as PENDING
      repeatable(repeatable),         // Store whether task should repeat
      repeatInterval(repeatInterval), // Store time between repeated task executions
      executionTimeout(maxExecTime),  // Store maximum allowed execution time
      taskId(generateTaskId())        // Generate and store unique task ID
{}

/**
 * @brief Copy constructor with proper atomic status handling
 *
 * Creates a copy of another task, handling the atomic status field correctly
 * by loading its current value and storing it in the new instance.
 *
 * @param other The task to copy from
 *
 * @note We load() the atomic status to get its current value for copying.
 * This ensures that the copied task has the same status as the original
 * at the time of copying, while maintaining thread safety.
 */
Task::Task(const Task &other)
    : executeFn(other.executeFn),           // Copy the function object
      startTime(other.startTime),           // Copy the execution time
      priority(other.priority),             // Copy the priority value
      status(other.status.load()),          // Load atomic status value (cannot move atomics) and store in new atomic
      repeatable(other.repeatable),         // Copy the repeatable flag
      repeatInterval(other.repeatInterval), // Copy the repeated task executions interval
      executionTimeout(other.executionTimeout), // Copy the timeout value
      taskId(other.taskId)                      // Copy the task ID
{}

/**
 * @brief Move constructor with atomic status handling
 *
 * Moves resources from another task. Since atomic types cannot be moved,
 * we load the value from the source atomic and store it in the new one.
 *
 * @param other The task to move from
 *
 */
Task::Task(Task &&other) noexcept
    : executeFn(std::move(other.executeFn)), // Move the function object
      startTime(other.startTime),            // Copy the execution time (trivial type)
      priority(other.priority),              // Copy the priority (trivial type)
      status(other.status.load()),           // Load atomic status value (cannot move atomics) and store in new atomic
      repeatable(other.repeatable),          // Copy the repeatable flag
      repeatInterval(other.repeatInterval),  // Copy the interval
      executionTimeout(other.executionTimeout), // Copy the timeout
      taskId(std::move(other.taskId))           // Move the task ID string
{}

/**
 * @brief Copy assignment operator with atomic status handling
 *
 * Assigns values from another task, properly handling the atomic status
 * field using store/load operations for thread safety.
 *
 * @param other The task to copy from
 * @return Reference to this task
 */
Task &Task::operator=(const Task &other) {
  if (this != &other) {                        // Protect against self-assignment
    executeFn = other.executeFn;               // Assign the function object
    startTime = other.startTime;               // Assign the execution time
    priority  = other.priority;                // Assign the priority
    status.store(other.status.load());         // Store the loaded atomic value
    repeatable       = other.repeatable;       // Assign the repeatable flag
    repeatInterval   = other.repeatInterval;   // Assign the interval
    executionTimeout = other.executionTimeout; // Assign the timeout
    taskId           = other.taskId;           // Assign the task ID
  }
  return *this; // Return reference to this object
}

/**
 * @brief Move assignment operator with atomic status handling
 *
 * Moves values from another task, handling the atomic status field
 * by loading its value (atomic types cannot be moved).
 *
 * @param other The task to move from
 * @return Reference to this task
 */
Task &Task::operator=(Task &&other) noexcept {
  if (this != &other) {                         // Protect against self-assignment
    executeFn = std::move(other.executeFn);     // Move the function object
    startTime = other.startTime;                // Copy the execution time
    priority  = other.priority;                 // Copy the priority
    status.store(other.status.load());          // Store the loaded atomic value
    repeatable       = other.repeatable;        // Copy the repeatable flag
    repeatInterval   = other.repeatInterval;    // Copy the interval
    executionTimeout = other.executionTimeout;  // Copy the timeout
    taskId           = std::move(other.taskId); // Move the task ID string
  }
  return *this; // Return reference to this object
}

/**
 * @brief Executes the task's function
 *
 * Invokes the stored function object. This is the core execution method
 * that performs the actual work of the task, and should only be called
 * by the task scheduler when the task is ready to run.
 * Any exceptions thrown by the function will propagate to the caller
 * and should be handled appropriately.
 *
 * @throws Any exception thrown by the task function
 * @note The caller is responsible for handling exceptions and updating task status
 */
void Task::execute() {
  executeFn();
}

/**
 * @brief Reschedules the task to a new execution time
 *
 * Updates the task's execution time to the specified time point.
 * Typically used for rescheduling failed tasks or manual task management.
 *
 * @param newTime The new execution time point
 */
void Task::reschedule(std::chrono::system_clock::time_point newTime) {
  startTime = newTime;
}

/**
 * @brief Reschedules the task using its configured repeat interval
 *
 * Automatically calculates the next execution time by adding the repeat
 * interval to the current time. Used for repeatable task management.
 *
 * This is useful for tasks that are meant to run periodically.
 * This method updates the startTime to ensure the task will be
 * executed at the correct interval in the future.
 *
 * @note Only meaningful for repeatable tasks
 */
void Task::reschedule() {
  startTime = std::chrono::system_clock::now() + repeatInterval;
}

/**
 * @brief Gets the unique task identifier
 *
 * Returns the automatically generated unique ID for this task.
 * Task IDs are in the format "task_XXXXXX" where X is a zero-padded number.
 *
 * @return Const reference to the task ID string
 */
const std::string &Task::getId() const {
  return taskId;
}

/**
 * @brief Gets the current task status (thread-safe)
 *
 * Returns the current execution status using atomic load operation
 * to ensure thread-safe access.
 *
 * @return Current task status
 */
Task::Status Task::getStatus() const {
  return status.load();
}

/**
 * @brief Sets the task status (thread-safe)
 *
 * Updates the task status using atomic store operation to ensure
 * thread-safe modification.
 *
 * @param newStatus The new status to set
 */
void Task::setStatus(Status newStatus) {
  status.store(newStatus);
}

/**
 * @brief Comparison operator for priority queue ordering
 *
 * Compares tasks based on priority first, then start execution time.
 *
 * @param other The task to compare against
 * @return true if this task has lower priority (should execute later)
 *
 * @note Lower priority values indicate higher priority tasks
 * @note For equal priorities, earlier start execution times have higher priority
 */
bool Task::operator>(const Task &other) const {
  // First compare by priority (lower number = higher priority)
  if (this->priority != other.priority) {
    return this->priority > other.priority;
  }
  // For equal priorities, compare by execution time (earlier = higher priority)
  return this->startTime > other.startTime;
}

/**
 * @brief Generates a unique task identifier
 *
 * Creates a unique ID string in the format "task_XXXXXX" using an
 * atomic counter to ensure thread-safe ID generation.
 *
 * @return Unique task ID string
 */
std::string Task::generateTaskId() {
  size_t id = nextId.fetch_add(1);

  // Format the ID as "task_" followed by a zero-padded 6-digit number
  // Example: "task_000001", "task_000042", "task_123456"
  std::ostringstream oss;
  oss << "task_" << std::setfill('0') << std::setw(6) << id;

  // Return the formatted ID string
  return oss.str();
}

/**
 * @brief Gets the current thread name
 *
 * Retrieves the name of the thread executing this task.
 * This is useful for debugging and logging purposes to identify which
 * thread is currently processing the task.
 *
 * @return Thread name string
 */
std::string Task::getCurrentThreadName() {
#if defined(__APPLE__) || defined(__linux__)
  char name[16] = {};
  pthread_getname_np(pthread_self(), name, sizeof(name));
  return std::string(name);
#else
  // Fallback: return hashed thread ID
  return std::to_string(std::hash<std::thread::id>()(std::this_thread::get_id()));
#endif
}

/**
 * @brief Converts Status enum to string for logging
 *
 * Provides a human-readable string representation of the task status.
 * Useful for logging and debugging purposes.
 */
std::ostream &operator<<(std::ostream &os, Task::Status status) {
  return os << Task::status_to_string(status);
}

} // namespace TaskSchedX
