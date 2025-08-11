#include <iostream>
#include <thread>
#include "TaskScheduler.h"
using namespace TaskSchedX;

int main() {
  std::cout << "=== Task Completion Callback Mechanism Example ===" << std::endl;
  std::cout << "This example demonstrates how to use task completion callbacks:" << std::endl;
  std::cout << "1. Register a callback function to be notified when tasks complete" << std::endl;
  std::cout << "2. Receive task ID and completion status in the callback" << std::endl;
  std::cout << "3. Use callbacks for custom monitoring and logging" << std::endl;
  std::cout << "4. Handle different task completion statuses" << std::endl;
  std::cout << "\nThe callback will be triggered for each task that completes.\n" << std::endl;

  TaskScheduler scheduler(4);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Set up logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.enableConsoleLogging(true);

  // === Register a callback to be executed when any task is complete ===
  scheduler.onTaskComplete([](const std::string &taskId, Task::Status status) {
    std::cout << "[🛎️ CALLBACK 🛎️] Task completion callback triggered!" << std::endl;
    std::cout << "[🛎️ CALLBACK 🛎️] Task " << taskId << " finished with status: " << Task::status_to_string(status)
              << std::endl;
  });

  std::cout << "\n🗓️ Callback registered. Now scheduling a task..." << std::endl;

  // === Define a task ===
  TaskConfig taskConfig;
  taskConfig.executeFn  = []() { std::cout << "Task 1 executed" << std::endl; };
  taskConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1); // Execute in 1 second
  taskConfig.priority   = 1;                                                          // Priority 1
  taskConfig.repeatable = false;                                                      // non-Repeatable
  std::string taskId    = scheduler.scheduleTask(taskConfig);
  std::cout << "\n🆗 Scheduled task with ID: " << taskId << std::endl;

  std::cout << "\n🗓️ Starting scheduler and waiting for task completion..." << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(2)); // Allow time for the task and callback

  std::cout << "🏁 Stopping scheduler..." << std::endl;
  scheduler.stop();

  // Official scheduler statistics
  auto finalStats = scheduler.getStatistics();
  std::cout << "\n=== Official Scheduler Statistics ===" << std::endl;
  std::cout << "Total tasks scheduled: " << finalStats.totalTasksScheduled << std::endl;
  std::cout << "Tasks completed: " << finalStats.tasksCompleted << std::endl;
  std::cout << "Tasks failed: " << finalStats.tasksFailed << std::endl;
  std::cout << "Tasks cancelled: " << finalStats.tasksCancelled << std::endl;
  std::cout << "Tasks timed out: " << finalStats.tasksTimedOut << std::endl;

  return 0;
}