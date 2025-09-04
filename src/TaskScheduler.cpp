#include "TaskScheduler.h"
#include <future>
#include <sstream>

namespace TaskSchedX {

/**
 * @brief Constructs a TaskScheduler with the specified number of worker threads
 *
 * Initializes the task scheduler with a thread pool of the given size.
 * The scheduler is ready to accept tasks but does not start processing
 * until start() is called.
 *
 * @param numThreads Number of worker threads for the internal thread pool
 *
 * @note The scheduler must be started with start() before tasks will be executed
 * @note Worker threads are created immediately but remain idle until start() is called
 */
TaskScheduler::TaskScheduler(size_t numThreads) : threadPool(numThreads) {
  LOGGER.info("TaskScheduler initialized with " + std::to_string(numThreads) + " threads");
}

/**
 * @brief Destructor that ensures proper cleanup of the scheduler
 *
 * Automatically stops the scheduler if still running, calls stop() to join
 * threads and release resources.
 */
TaskScheduler::~TaskScheduler() {
  if (is_running.load(std::memory_order_acquire)) {
    stop();
  }
}

/**
 * @brief Schedules a task using a configuration structure
 *
 * Creates and schedules a new task using the provided TaskConfig structure.
 * This provides a more readable alternative to the positional parameter version.
 *
 * @param config TaskConfig structure containing all task parameters
 * @return Unique task identifier for tracking and management
 *
 */
std::string TaskScheduler::scheduleTask(const TaskConfig &config) {
  return scheduleTask(config.executeFn, config.startTime, config.priority, config.repeatable, config.repeatInterval,
                      config.executionTimeout);
}

/**
 * @brief Schedules a task for execution with comprehensive configuration options
 *
 * Creates and schedules a new task with specified parameters. The task is
 * assigned a unique ID and added to the priority queue for execution.
 *
 * @param executeFn The function to execute when the task runs
 * @param startTime The scheduled start execution time point
 * @param priority Task priority (lower values = higher priority)
 * @param repeatable Whether the task should repeat after completion
 * @param repeatInterval Time interval between repeated task executions (default: 10s)
 * @param executionTimeout Maximum allowed execution time before timeout (default: 60s)
 *
 * @return Unique task identifier for tracking and management
 *
 * @note Lower priority values indicate higher priority tasks
 * @note Repeated tasks are automatically rescheduled after successful completion
 * @note Tasks with executionTimeout <= 0 have no timeout limit
 * @note The returned ID can be used for cancellation and status queries
 */
std::string TaskScheduler::scheduleTask(std::function<void()>                 executeFn,
                                        std::chrono::system_clock::time_point startTime, int priority, bool repeatable,
                                        std::chrono::seconds repeatInterval, std::chrono::seconds executionTimeout) {

  auto task   = std::make_shared<Task>(executeFn, startTime, priority, repeatable, repeatInterval, executionTimeout);
  auto taskId = task->getId();

  // Add the task to the active tasks registry for tracking and management
  {
    std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
    activeTasks[taskId] = task;
  }

  // Add the task to the priority queue for scheduling
  {
    std::lock_guard<std::mutex> queue_lock(queueMutex);
    taskQueue.push(task);
  }

  // Update scheduling statistics in a thread-safe manner
  {
    std::lock_guard<std::mutex> stats_lock(statsMutex);
    stats.totalTasksScheduled++;
  }

  LOGGER.info("Task scheduled: " + task->getId() + " (priority: " + std::to_string(priority) +
              ", repeatable: " + (repeatable ? "yes" : "no") + ")");

  workAvailable_cv.notify_one();

  return taskId;
}

/**
 * @brief Cancels a task by its unique identifier
 *
 * Attempts to cancel the task with the specified ID. If the task is found
 * and not yet completed, it will be marked as cancelled, updates statistics,
 * and removed from the active task registry.
 *
 * @param taskId The unique identifier of the task to cancel
 * @return true if the task was found and cancelled, false otherwise
 *
 * @note Tasks that are currently running cannot be forcibly stopped
 * @note Cancelled tasks will not be rescheduled even if they were repeated tasks
 */
bool TaskScheduler::cancelTask(const std::string &taskId) {
  bool cancelled = false;

  // First, try to find and cancel the task in the active tasks registry
  {
    std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
    auto                        it = activeTasks.find(taskId);
    if (it != activeTasks.end()) {
      it->second->setStatus(Task::Status::CANCELLED);
      activeTasks.erase(it);
      cancelled = true;
      LOGGER.info("Task " + taskId + " cancelled from active tasks list");
    }
  }

  // not in active tasks list
  // try to find and canel if from the pending queue (not-yet-active tasks)
  if (!cancelled) {
    std::lock_guard<std::mutex> queue_lock(queueMutex);
    cancelled = taskQueue.cancelTask(taskId);
    if (cancelled) {
      LOGGER.info("Task " + taskId + " cancelled from task queue");
    }
  }

  // Update statistics and notify if the task was successfully cancelled
  if (cancelled) {
    updateStatistics(Task::Status::CANCELLED);
    notifyTaskCompletion(taskId, Task::Status::CANCELLED);
  } else {
    LOGGER.warn("Attempted to cancel non-existent task: " + taskId);
  }

  return cancelled;
}

/**
 * @brief Gets the current status of a task by its identifier
 *
 * This function first looks up the task in the active task registry.
 * If found, it returns the current status directly from the live task instance.
 *
 * If the task is not found in the active registry, it checks the
 * completedTasks map, which stores the final status of tasks that
 * have already finished execution (either COMPLETED, FAILED, CANCELLED, or TIMEOUT).
 *
 * This ensures that status can still be queried even after a task
 * has been removed from the activeTasks map.
 *
 * @param taskId The unique identifier of the task
 *
 * @return Current task status
 *
 * @note Returns COMPLETED for tasks that have finished and been removed from tracking
 * @note The status may change immediately after this call in multithreaded environments
 */
Task::Status TaskScheduler::getTaskStatus(const std::string &taskId) {

  std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);

  // Search for the task in the active tasks registry
  auto isActive = activeTasks.find(taskId);
  if (isActive != activeTasks.end()) {
    return isActive->second->getStatus();
  }

  // Check if the task has been completed and tracked
  auto doneTask = completedTasks.find(taskId);
  if (doneTask != completedTasks.end()) {
    return doneTask->second;
  }

  // If not found, assume the task completed successfully
  // TODO: maybe should throw an exception here instead?
  LOGGER.warn("Task status requested for non-existent task: " + taskId);
  return Task::Status::COMPLETED;
}

/**
 * @brief Launch the task scheduler in a background thread.
 *
 * Initializes and starts the task scheduler's main loop in a
 * dedicated thread, allowing the main application to remain
 * responsive.
 *
 * This method also ensures that the scheduler is not started multiple times.
 * Internally, it creates a thread that runs `runSchedulerLoop()` to process
 * scheduled tasks.
 *
 * @note The scheduler runs until `stop()` is called.
 * @note Calling this method multiple times has no effect after the first successful call.
 *
 */
void TaskScheduler::start() {
  if (is_running.exchange(true, std::memory_order_acq_rel)) {
    return; // Prevent double-start
  }
  schedulerThread = std::thread([this]() { runSchedulerLoop(); });
}

/**
 * @brief Starts the task scheduler's main processing loop
 *
 * Starts the main loop that continuously checks and runs scheduled tasks.
 * Tasks are executed based on their scheduled time and priority.
 * This function runs indefinitely until stop() is called.
 *
 */
void TaskScheduler::runSchedulerLoop() {
  LOGGER.info("TaskScheduler started");

  // Main scheduling loop - continues until stop() is called
  while (is_running.load(std::memory_order_acquire)) {

    std::unique_lock<std::mutex> queue_lock(queueMutex);

    // wait until (work available or stop requested)
    workAvailable_cv.wait(queue_lock,
                          [this] { return !is_running.load(std::memory_order_acquire) || !taskQueue.isEmpty(); });

    // Check if we should stop (stop() was called while waiting)
    if (!is_running.load(std::memory_order_acquire)) {
      break; // Exit the main scheduling loop
    }

    // Get the next highest priority task from the priority queue
    auto task = taskQueue.pop();

    // If it's not yet time to run the task, put it back and wait until its start time or stop.
    auto now = std::chrono::system_clock::now();
    if (task->startTime > now) {

      taskQueue.push(task);

      // Task is not ready yet - wait until:
      // - the specified time (task.startTime) is reached
      // OR
      // - stop() request was called
      workAvailable_cv.wait_until(queue_lock, task->startTime,
                                  [this] { return !is_running.load(std::memory_order_acquire); });

      if (!is_running.load(std::memory_order_acquire)) {
        break; // Exit the main scheduling loop
      }

      // continue getting tasks to re-evaluate queue head
      continue;
    }

    // Task is ready to execute - release the queue lock
    queue_lock.unlock();

    // Get the actual task object from the active tasks registry
    std::shared_ptr<Task> activeTask;
    {
      std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
      // Try to find the task by its ID in the activeTasks map
      auto it = activeTasks.find(task->getId());
      // If the task exists in the activeTasks map, save it to activeTask
      // if not exists, means task may have been cancelled
      if (it != activeTasks.end()) {
        activeTask = it->second;
      }
    }

    // Check if the task was cancelled or doesn't exist anymore
    if (!activeTask || activeTask->getStatus() == Task::Status::CANCELLED) {
      continue; // Skip this task and get the next one
    }

    // Mark the task as currently running
    activeTask->setStatus(Task::Status::RUNNING);
    // Dispatch task to thread pool
    threadPool.enqueueTask([this, activeTask]() { this->executeAndFinalizeTask(activeTask); });

    LOGGER.debug("Executing task: " + activeTask->getId() + " (priority: " + std::to_string(activeTask->priority) +
                 ")");
  }

  // Log the scheduler shutdown
  LOGGER.info("TaskScheduler stopped");
}

/**
 * @brief Post-processing logic for a completed or timed-out task.
 *
 * This function handles the finalization of a task after its execution.
 * It updates task status, statistics, and notifies registered completion callbacks.
 * Additionally, it handles rescheduling for repeatable tasks.
 *
 * @param activeTask The task that has just finished execution.
 * @param timedOut Indicates whether the task exceeded its timeout limit.
 *
 * @details
 * - If the task timed out or was cancelled, no further processing.
 * - If successful, the task is marked as COMPLETED and statistics are updated.
 * - And finally, completion callbacks are invoked.
 * - For repeatable tasks:
 *   - The task is rescheduled using its repeat interval.
 *   - Its status is reset to PENDING and re-added to the task queue.
 * - For non-repeatable tasks:
 *   - The task is removed from the active task registry.
 *
 */
void TaskScheduler::handleTaskPostExecution(std::shared_ptr<Task> executedTask, bool timedOut) {
  // Check if the task completed successfully (not timed out or cancelled)
  if (timedOut || executedTask->getStatus() == Task::Status::CANCELLED) {
    return;
  }

  LOGGER.info("Task completed: " + executedTask->getId());
  // Mark the task as completed
  executedTask->setStatus(Task::Status::COMPLETED);
  // Update completion statistics
  updateStatistics(Task::Status::COMPLETED);
  // Notify any registered completion callback
  notifyTaskCompletion(executedTask->getId(), Task::Status::COMPLETED);

  // Handle repeated task rescheduling
  if (executedTask->repeatable) {
    LOGGER.debug("Rescheduling repeatable task: " + executedTask->getId());
    // Reschedule the task for its next execution
    executedTask->reschedule();
    // Reset status to pending for the next execution
    executedTask->setStatus(Task::Status::PENDING);
    {
      // Add the rescheduled task back to the queue
      std::lock_guard<std::mutex> queue_lock(queueMutex);
      taskQueue.push(executedTask);
    }
    // notify the scheduler
    workAvailable_cv.notify_one();
  } else {
    // Non-repeatable task is done - remove from active tracking
    std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
    activeTasks.erase(executedTask->getId());
  }
}

/**
 * @brief Executes a task and finalizes its status upon completion or failure.
 *
 * This method is responsible for:
 * - Executing a task with optional timeout handling
 * - Handling task completion (including repeatable rescheduling)
 * - Catching and processing task failures (exceptions)
 * - Updating internal statistics and notifying completion callbacks
 *
 * @param activeTask The task to be executed and finalized.
 *
 * @note This function ensures that all post-execution logic is handled
 *       in a thread-safe manner, including both normal and error paths.
 */
void TaskScheduler::executeAndFinalizeTask(std::shared_ptr<Task> task) {
  try {

    LOGGER.info("Task started: " + task->getId() + " | Thread ID: " + Task::getCurrentThreadName());
    // Execute the task with optional timeout handling
    bool timedOut = this->executeTaskWithTimeout(*task);
    this->handleTaskPostExecution(task, timedOut);

    LOGGER.info("Task finished: " + task->getId() + " | Thread ID: " + Task::getCurrentThreadName());

  } catch (const std::exception &e) {

    LOGGER.error("Task failed: " + task->getId() + " - " + e.what());
    // Task execution failed with an exception
    task->setStatus(Task::Status::FAILED);
    // Update failure statistics
    updateStatistics(Task::Status::FAILED);
    // Notify completion callback of the failure
    notifyTaskCompletion(task->getId(), Task::Status::FAILED);
    // Remove the failed task from active tracking
    {
      std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
      activeTasks.erase(task->getId());
    }

  } catch (...) {

    LOGGER.error("Task failed: " + task->getId() + " - unknown exception");
    // Task execution failed with an exception
    task->setStatus(Task::Status::FAILED);
    // Update failure statistics
    updateStatistics(Task::Status::FAILED);
    // Notify completion callback of the failure
    notifyTaskCompletion(task->getId(), Task::Status::FAILED);
    // Remove the failed task from active tracking
    {
      std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
      activeTasks.erase(task->getId());
    }
  }
}

/**
 * @brief Stops the task scheduler and all worker threads
 *
 * Signals the scheduler to stop processing new tasks and shuts down
 * all worker threads. Pending tasks are dropped, but running tasks
 * are allowed to finish. No new task will be started.
 *
 * @note Remaining queued tasks will not be executed
 */
void TaskScheduler::stop() {
  LOGGER.info("TaskScheduler stop() requested");
  // Set the running flag to false to signal the scheduler loop to exit
  is_running.store(false, std::memory_order_release);
  // Wake the scheduler thread so it observes running=false
  workAvailable_cv.notify_all();
  // If the scheduler thread is running, join it to wait for its completion
  if (schedulerThread.joinable()) {
    schedulerThread.join();
  }
  threadPool.stop();
}

/**
 * @brief Registers a callback function for task completion notifications
 *
 * Sets a callback function that will be invoked whenever a task completes
 * (success, failure, timeout, or cancellation). Only *one* callback
 * can be registered at a time.
 *
 * @param callback Function to call on task completion, receives task ID and final status
 *
 * @note Keep callback execution time minimal to avoid blocking the scheduler
 * @note Setting a new callback replaces any previously registered callback
 */
void TaskScheduler::onTaskComplete(std::function<void(const std::string &, Task::Status)> callback) {
  // Only one callback can be registered at a time
  taskCompleteCallback = callback;
}

/**
 * @brief Configures the minimum log level for scheduler logging
 *
 * Sets the logging level for the integrated logger. Only messages at or
 * above this level will be output.
 *
 * @param level The minimum log level to output
 *
 */
void TaskScheduler::setLogLevel(Logger::Level level) {
  LOGGER.setLevel(level);
}

/**
 * @brief Enables or disables console logging output
 *
 * Controls whether log messages are written to standard output.
 *
 * @param enable true to enable console output, false to disable
 */
void TaskScheduler::enableConsoleLogging(bool enable) {
  LOGGER.enableConsoleOutput(enable);
}

/**
 * @brief Retrieves current scheduler statistics
 *
 * Returns a snapshot of the current scheduler performance statistics
 * including task counts by completion status.
 *
 * @return Current statistics structure
 *
 */
TaskScheduler::Statistics TaskScheduler::getStatistics() const {
  std::lock_guard<std::mutex> stats_lock(statsMutex);
  return stats;
}

/**
 * @brief Executes a task with optional timeout handling
 *
 * Executes the given task, optionally enforcing a timeout limit.
 * If the task exceeds its maximum execution time, it's marked as timed out.
 *
 * @param task Reference to the task to execute
 *
 * @return true if the task timed out, false if it completed normally
 *
 * @note Tasks with executionTimeout <= 0 have no timeout limit
 * @note Timed out tasks are marked with TIMEOUT status and removed from tracking
 */
bool TaskScheduler::executeTaskWithTimeout(Task &task) {
  // Check if timeout is disabled (executionTimeout <= 0)
  if (task.executionTimeout <= std::chrono::seconds(0)) {
    // No timeout limit - execute the task directly
    task.execute();
    return false; // Not TimeOut
  }

  // Execute the task
  const auto start = std::chrono::steady_clock::now();
  task.execute();
  const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start);

  if (elapsed > task.executionTimeout) {
    LOGGER.warn("Task timed out: " + task.getId());
    // Task exceeded its maximum execution time
    task.setStatus(Task::Status::TIMEOUT);
    // Update timeout statistics
    updateStatistics(Task::Status::TIMEOUT);
    // Notify completion callback of the timeout
    notifyTaskCompletion(task.getId(), Task::Status::TIMEOUT);
    // Remove the timed out task from active tracking
    {
      std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
      activeTasks.erase(task.getId());
    }
    return true; // Task TimeOut
  }

  // no TimeOut
  return false;
}

/**
 * @brief Updates scheduler statistics for a task
 *
 * Increments the appropriate counter in the statistics based on the
 * final status of a task.
 *
 * @param status The final status of the completed task
 *
 */
void TaskScheduler::updateStatistics(Task::Status status) {

  std::lock_guard<std::mutex> stats_lock(statsMutex);

  switch (status) {
  case Task::Status::COMPLETED:
    stats.tasksCompleted++;
    break;
  case Task::Status::FAILED:
    stats.tasksFailed++;
    break;
  case Task::Status::CANCELLED:
    stats.tasksCancelled++;
    break;
  case Task::Status::TIMEOUT:
    stats.tasksTimedOut++;
    break;
  default:
    // No statistics update for other statuses (PENDING, RUNNING)
    break;
  }
}

/**
 * @brief Invokes the registered task completion callback
 *
 * Calls the registered callback function (if any) with the task ID
 * and final status when a task completes.
 *
 * @param taskId The unique identifier of the completed task
 * @param status The final status of the completed task
 *
 * @note This method is safe to call even if no callback is registered
 */
void TaskScheduler::notifyTaskCompletion(const std::string &taskId, Task::Status status) {
  // Check if a callback has been registered
  if (taskCompleteCallback) {
    // Invoke the callback with the task ID and completion status
    // Note: Any exceptions thrown by the callback are not caught here
    // and may affect scheduler operation
    taskCompleteCallback(taskId, status);
  }
  // Track final task status for querying
  {
    std::lock_guard<std::mutex> activeTasks_lock(activeTasksMutex);
    completedTasks[taskId] = status;
  }
}
} // namespace TaskSchedX
