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
 * Automatically stops the scheduler if it's still running to ensure
 * proper cleanup of all resources including threads and active tasks.
 */
TaskScheduler::~TaskScheduler() {
  // Check if the scheduler is still running using atomic load
  if (running.load()) {
    // Stop the scheduler to clean up resources properly
    // This ensures threads are joined and resources are released
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
  // Create a new Task object with all specified parameters
  // The Task constructor will generate a unique ID and set initial status
  auto task = std::make_shared<Task>(executeFn, startTime, priority, repeatable, repeatInterval, executionTimeout);

  // Add the task to the active tasks registry for tracking and management
  {
    // Acquire exclusive lock for thread-safe access to active tasks map
    std::lock_guard<std::mutex> lock(activeTasksMutex);

    // Store the task using its unique ID as the key
    // This allows for efficient lookup, cancellation, and status queries
    activeTasks[task->getId()] = task;
  }

  // Add the task to the priority queue for scheduling
  // The queue will order tasks based on priority and execution time
  taskQueue.addTask(*task);

  // Update scheduling statistics in a thread-safe manner
  {
    // Acquire exclusive lock for statistics modification
    std::lock_guard<std::mutex> lock(statsMutex);

    // Increment the total number of tasks scheduled
    stats.totalTasksScheduled++;
  }

  // Log the successful task scheduling with relevant details
  LOGGER.info("Task scheduled: " + task->getId() + " (priority: " + std::to_string(priority) +
              ", repeatable: " + (repeatable ? "yes" : "no") + ")");

  // Notify the scheduler thread that a new task is available
  // This wakes up the scheduler if it's waiting for tasks
  condition.notify_one();

  // Return the unique task ID for client tracking and management
  return task->getId();
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
 * @note Tasks that are currently executing cannot be forcibly stopped
 * @note Cancelled tasks will not be rescheduled even if they were repeated tasks
 */
bool TaskScheduler::cancelTask(const std::string &taskId) {
  std::shared_ptr<Task> taskToCancel;
  {
    // Acquire exclusive lock for thread-safe access to active tasks
    std::lock_guard<std::mutex> lock(activeTasksMutex);

    // Search for the task in the active tasks registry
    auto it = activeTasks.find(taskId);
    if (it != activeTasks.end()) {
      // Mark as cancelled
      it->second->setStatus(Task::Status::CANCELLED);
      taskToCancel = it->second;
      activeTasks.erase(it);
      // Update statistics to reflect the cancellation
      updateStatistics(Task::Status::CANCELLED);
    }
  }

  if (taskToCancel) {
    // Notify completion callback of the cancellation
    notifyTaskCompletion(taskId, Task::Status::CANCELLED);
    LOGGER.info("Task " + taskId + " cancelled from active tasks list");
    return true;
  }

  // not in active tasks list
  // Cancel pending (not-yet-active) tasks from the queue
  if (taskQueue.cancelTask(taskId)) {
    updateStatistics(Task::Status::CANCELLED);
    notifyTaskCompletion(taskId, Task::Status::CANCELLED);
    LOGGER.info("Task " + taskId + " cancelled from task queue");
    return true;
  }

  // Not found in active or queued
  LOGGER.warn("Attempted to cancel non-existent task: " + taskId);
  return false;
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
  // Acquire shared lock for thread-safe access to active tasks
  std::lock_guard<std::mutex> lock(activeTasksMutex);

  // Search for the task in the active tasks registry
  auto isActive = activeTasks.find(taskId);
  if (isActive != activeTasks.end()) {
    // Task found - return its current status using thread-safe atomic load
    return isActive->second->getStatus();
  }

  // Check if the task has been completed and tracked
  auto doneTask = completedTasks.find(taskId);
  if (doneTask != completedTasks.end()) {
    // completedTasks is std::unordered_map<std::string, Task::Status>
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
  if (running.exchange(true))
    return; // Prevent double-start
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

  // Create a std::unique_lock named 'lock' for 'queueMutex' without locking it immediately.
  // The use of std::defer_lock defers locking, so the mutex starts unlocked and remains
  // unlocked until lock.lock() is called manually.
  // - 'taskQueue' will be updated for repeated task in handleTaskPostExecution using the same
  // mutex 'queueMutex'
  //
  // **** look at the first line in while(running){...} block ***
  // This allows us to control when the mutex is locked and unlocked,
  // which is important for efficient waiting and notification handling.
  // This is necessary to avoid deadlocks and ensure proper synchronization
  // between the scheduler thread and other threads that may modify the task queue.
  std::unique_lock<std::mutex> lock(queueMutex, std::defer_lock);

  // Main scheduling loop - continues until stop() is called
  while (running) {
    // Acquire the queue lock for safe access to the task queue
    lock.lock(); // Lock the mutex manually

    // Wait while the queue is empty and we're still running
    // This prevents busy-waiting and allows efficient thread blocking
    while (taskQueue.isEmpty() && running) {
      condition.wait(lock); // Releases lock and waits for notification
    }

    // Check if we should stop (stop() was called while waiting)
    if (!running) {
      lock.unlock(); // Release the lock before exiting
      break;         // Exit the main scheduling loop
    }

    // Get the next task from the priority queue
    // This is the highest priority task that's ready to run
    Task task = taskQueue.getTask();

    // Check if the task's execution time has arrived
    auto now = std::chrono::system_clock::now();

    if (task.startTime > now) {
      // Task is not ready yet - wait until:
      // - the specified time (task.startTime) is reached
      // OR
      // - the lambda check returns true, indicating an early
      //   wake-up condition (e.g., stop() was called)
      condition.wait_until(lock, task.startTime, [this] { return !running; });

      // Check if we were woken up by a stop request
      if (!running) {
        lock.unlock();
        break;
      }

      // Re-check the time: maybe it's now ready to execute
      auto current = std::chrono::system_clock::now();
      if (task.startTime > current) {
        // Still too early – requeue
        taskQueue.addTask(task);
        lock.unlock();
        continue;
      }
    }

    // Task is ready to execute - release the queue lock
    lock.unlock();

    // Get the actual task object from the active tasks registry
    // This ensures we're working with the current task state
    std::shared_ptr<Task> activeTask;
    {
      // Lock the mutex to safely access the shared activeTasks map
      std::lock_guard<std::mutex> activeLock(activeTasksMutex);
      // Try to find the task by its ID in the activeTasks map
      auto it = activeTasks.find(task.getId());
      // If the task exists in the activeTasks map, save it to activeTask
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

    // Log the task execution start
    LOGGER.debug("Executing task: " + activeTask->getId() + " (priority: " + std::to_string(activeTask->priority) +
                 ")");

    // Dispatch task to thread pool
    threadPool.enqueueTask([this, activeTask]() { this->executeAndFinalizeTask(activeTask); });
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
 * - If the task timed out or was cancelled, no further processing is done.
 * - If successful, the task is marked as COMPLETED and statistics are updated.
 * - Completion callbacks are invoked.
 * - For repeatable tasks:
 *   - The task is rescheduled using its repeat interval.
 *   - Its status is reset to PENDING and re-added to the task queue.
 * - For non-repeatable tasks:
 *   - The task is removed from the active task registry.
 *
 */
void TaskScheduler::handleTaskPostExecution(std::shared_ptr<Task> activeTask, bool timedOut) {
  // Check if the task completed successfully (not timed out or cancelled)
  if (timedOut || activeTask->getStatus() == Task::Status::CANCELLED) {
    return;
  }

  // Mark the task as completed
  activeTask->setStatus(Task::Status::COMPLETED);

  // Update completion statistics
  updateStatistics(Task::Status::COMPLETED);

  // Log successful completion
  LOGGER.info("Task completed: " + activeTask->getId());

  // Notify any registered completion callback
  notifyTaskCompletion(activeTask->getId(), Task::Status::COMPLETED);

  // Handle repeated task rescheduling
  if (activeTask->repeatable) {
    // Reschedule the task for its next execution
    activeTask->reschedule(); // Uses the configured repeated task interval

    // Reset status to pending for the next execution
    activeTask->setStatus(Task::Status::PENDING);
    // Log the rescheduling
    LOGGER.debug("Rescheduling repeatable task: " + activeTask->getId());

    {
      // Add the rescheduled task back to the queue
      std::lock_guard<std::mutex> lock(queueMutex);
      taskQueue.addTask(*activeTask);
      condition.notify_one();
    }
  } else {
    // Non-repeatable task is done - remove from active tracking
    std::lock_guard<std::mutex> lock(activeTasksMutex);
    activeTasks.erase(activeTask->getId());
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
void TaskScheduler::executeAndFinalizeTask(std::shared_ptr<Task> activeTask) {
  try {
    LOGGER.info("Task started: " + activeTask->getId() + " | Thread ID: " + Task::getCurrentThreadName());

    // Execute the task with optional timeout handling
    bool timedOut = this->executeTaskWithTimeout(*activeTask);
    this->handleTaskPostExecution(activeTask, timedOut);

    LOGGER.info("Task finished: " + activeTask->getId() + " | Thread ID: " + Task::getCurrentThreadName());
  } catch (const std::exception &e) {
    // Task execution failed with an exception
    activeTask->setStatus(Task::Status::FAILED);

    // Update failure statistics
    updateStatistics(Task::Status::FAILED);

    // Log the failure with exception details
    LOGGER.error("Task failed: " + activeTask->getId() + " - " + e.what());

    // Notify completion callback of the failure
    notifyTaskCompletion(activeTask->getId(), Task::Status::FAILED);

    {
      // Remove the failed task from active tracking
      std::lock_guard<std::mutex> activeLock(activeTasksMutex);
      activeTasks.erase(activeTask->getId());
    }
  } catch (...) {
    // Task execution failed with an exception
    activeTask->setStatus(Task::Status::FAILED);

    // Update failure statistics
    updateStatistics(Task::Status::FAILED);

    // Log the failure with exception details
    LOGGER.error("Task failed: " + activeTask->getId() + " - unknown exception");

    // Notify completion callback of the failure
    notifyTaskCompletion(activeTask->getId(), Task::Status::FAILED);

    {
      // Remove the failed task from active tracking
      std::lock_guard<std::mutex> activeLock(activeTasksMutex);
      activeTasks.erase(activeTask->getId());
    }
  }
}

/**
 * @brief Stops the task scheduler and all worker threads
 *
 * Signals the scheduler to stop processing new tasks and shuts down
 * all worker threads. Currently executing tasks will complete, but
 * no new tasks will be started.
 *
 * @note Remaining queued tasks will not be executed
 */
void TaskScheduler::stop() {
  // Set the running flag to false to signal the scheduler loop to exit
  running = false;

  // Wake up all threads waiting on the condition variable (using wait(), wait_for(), or wait_until() call)
  // This ensures the scheduler thread checks the running flag and exits
  condition.notify_all();

  // If the scheduler thread is running, join it to wait for its completion
  if (schedulerThread.joinable()) {
    schedulerThread.join();
  }
  // Stop and join all worker threads
  threadPool.stop();

  // Log the stop request
  LOGGER.info("TaskScheduler stop requested");
}

/**
 * @brief Registers a callback function for task completion notifications
 *
 * Sets a callback function that will be invoked whenever a task completes
 * (success, failure, timeout, or cancellation). Only one callback
 * can be registered at a time.
 *
 * @param callback Function to call on task completion, receives task ID and final status
 *
 * @note Keep callback execution time minimal to avoid blocking the scheduler
 * @note Setting a new callback replaces any previously registered callback
 */
void TaskScheduler::onTaskComplete(std::function<void(const std::string &, Task::Status)> callback) {
  // Store the callback function for later invocation
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
  // Use the LOGGER macro to access the singleton and set the level
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
  // Use the LOGGER macro to access the singleton and configure console output
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
  // Acquire shared lock for thread-safe access to statistics
  std::lock_guard<std::mutex> lock(statsMutex);

  // Return a copy of the current statistics structure
  // This provides a consistent snapshot at the time of the call
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
    return false; // Not timed out
  }

  // Execute the task asynchronously to enable timeout checking
  // std::launch::async ensures the task runs in a separate thread
  auto future = std::async(std::launch::async, [&task]() { task.execute(); });

  // Wait for the task to complete or timeout
  if (future.wait_for(task.executionTimeout) == std::future_status::timeout) {
    // Task exceeded its maximum execution time
    task.setStatus(Task::Status::TIMEOUT);

    // Update timeout statistics
    updateStatistics(Task::Status::TIMEOUT);

    // Log the timeout event
    LOGGER.warn("Task timed out: " + task.getId());

    // Notify completion callback of the timeout
    notifyTaskCompletion(task.getId(), Task::Status::TIMEOUT);

    // Remove the timed out task from active tracking
    {
      std::lock_guard<std::mutex> activeLock(activeTasksMutex);
      activeTasks.erase(task.getId());
    }

    return true; // Task timed out
  } else {
    // Task completed within the timeout limit
    // wait_for() == std::future_status::ready
    try {
      // Get the result to handle any exceptions thrown by the task
      future.get();
      return false; // Task completed successfully, not timed out
    } catch (...) {
      // Re-throw any exception for handling by the caller
      throw;
    }
  }
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
  // Acquire exclusive lock for thread-safe statistics modification
  std::lock_guard<std::mutex> lock(statsMutex);

  // Increment the appropriate counter based on task completion status
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
    std::lock_guard<std::mutex> lock(activeTasksMutex);
    completedTasks[taskId] = status;
  }
}
} // namespace TaskSchedX
