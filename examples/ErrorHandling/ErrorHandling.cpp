#include <iostream>
#include <thread>
#include "TaskScheduler.h"

using namespace TaskSchedX;

int main() {
  std::cout << "=== Error Handling and Exception Management Example ===" << std::endl;
  std::cout << "This example demonstrates how the TaskScheduler handles task failures:" << std::endl;
  std::cout << "1. Tasks that throw exceptions are caught and marked as FAILED" << std::endl;
  std::cout << "2. Failed tasks don't crash the scheduler or affect other tasks" << std::endl;
  std::cout << "3. Error information is logged for debugging purposes" << std::endl;
  std::cout << "4. Failure statistics are tracked and can be monitored" << std::endl;

  TaskScheduler scheduler(4);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Set up logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.enableConsoleLogging(true);

  std::cout << "\n🗓️ Scheduling an error-prone task that will throw an exception..." << std::endl;

  TaskConfig taskConfig;
  taskConfig.executeFn = []() {
    std::cout << "Error Handling: Task started execution" << std::endl;
    std::cout << "About to simulate an error..." << std::endl;
    throw std::runtime_error("Simulated error in task - this is intentional for demonstration");
  };
  taskConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(2); // schedule to run in 2 seconds
  taskConfig.priority   = 1;                                                          // Priority 1
  taskConfig.repeatable = false;                                                      // non-Repeatable

  std::string taskId = scheduler.scheduleTask(taskConfig);
  std::cout << "🆗 Scheduled error-prone task with ID: " << taskId << std::endl;

  std::cout << "\n🚀 Starting scheduler. The task will fail, but the scheduler should continue running..." << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(4)); // Allow time for task execution

  std::cout << "\n🏁 Stopping scheduler..." << std::endl;
  scheduler.stop();

  // Check final task status
  std::cout << "\n=== Task Status Check ===" << std::endl;
  std::cout << "Final task status: " << Task::status_to_string(scheduler.getTaskStatus(taskId)) << std::endl;

  // Official scheduler statistics
  auto finalStats = scheduler.getStatistics();
  std::cout << "\n=== Official Scheduler Statistics ===" << std::endl;
  std::cout << "Total tasks scheduled: " << finalStats.totalTasksScheduled << std::endl;
  std::cout << "Tasks completed: " << finalStats.tasksCompleted << std::endl;
  std::cout << "Tasks failed: " << finalStats.tasksFailed << std::endl;
  std::cout << "Tasks cancelled: " << finalStats.tasksCancelled << std::endl;
  std::cout << "Tasks timed out: " << finalStats.tasksTimedOut << std::endl;

  if (finalStats.totalTasksScheduled > 0) {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Failure rate: " << (100.0 * finalStats.tasksFailed / finalStats.totalTasksScheduled) << "%"
              << std::endl;
  }

  return 0;
}