#include <iostream>
#include <thread>
#include "TaskScheduler.h"

using namespace TaskSchedX;

int main() {
  std::cout << "=== Basic TaskScheduler Usage Example ===" << std::endl;
  std::cout << "This example demonstrates the fundamental usage of TaskScheduler:" << std::endl;
  std::cout << "1. Create a TaskScheduler instance with worker threads" << std::endl;
  std::cout << "2. Schedule multiple simple tasks with different start execution times" << std::endl;
  std::cout << "3. Start the scheduler to begin task execution" << std::endl;
  std::cout << "4. Monitor task execution and completion" << std::endl;
  std::cout << "5. Stop the scheduler and view results" << std::endl;

  std::cout << "Step 1: Creating TaskScheduler with 2 worker threads..." << std::endl;

  // Create a TaskScheduler with 3 worker threads
  TaskScheduler scheduler(3);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Configure basic logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.setConsoleLoggingEnabled(true);

  std::cout << "\n🗓️ Step 2: Scheduling simple tasks..." << std::endl;

  // === Schedule a simple immediate task ===
  TaskConfig task1Config;
  task1Config.taskFn  = []() { std::cout << "Hello from Task 1! This is a simple scheduled task." << std::endl; };
  task1Config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1); // Execute in 1 second
  task1Config.priority   = 1;                                                          // Priority 1
  task1Config.isRepeatable = false;                                                      // non-Repeatable
  std::string task1Id    = scheduler.scheduleTask(task1Config);
  std::cout << "🆗 Scheduled Task 1 (immediate) with ID: " << task1Id << std::endl;

  // === Schedule a delayed task ===
  TaskConfig task2Config;
  task2Config.taskFn = []() {
    std::cout << "Hello from Task 2! This task was scheduled for later execution." << std::endl;
  };
  task2Config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(3); // Execute in 3 seconds
  task2Config.priority   = 2;                                                          // Priority 2
  task2Config.isRepeatable = false;                                                      // non-Repeatable
  std::string task2Id    = scheduler.scheduleTask(task2Config);
  std::cout << "🆗 Scheduled Task 2 (delayed) with ID: " << task2Id << std::endl;

  // === Schedule a task with some work to do ===
  TaskConfig task3Config;
  task3Config.taskFn = []() {
    std::cout << "Task 3 started - simulating some work..." << std::endl;
    for (int i = 1; i <= 5; ++i) {
      // This task simulates some work by logging progress
      std::this_thread::sleep_for(std::chrono::milliseconds(200));
      std::cout << "Task 3 progress: step " << i << "/5" << std::endl;
    }
    std::cout << "Task 3 completed all work!" << std::endl;
  };
  task3Config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(2); // Execute in 2 seconds
  task3Config.priority   = 1;                                                          // Priority 1
  task3Config.isRepeatable = false;                                                      // non-Repeatable
  std::string task3Id    = scheduler.scheduleTask(task3Config);
  std::cout << "🆗 Scheduled Task 3 (with work) with ID: " << task3Id << std::endl;

  std::cout << "\n🗓️ Step 3: Starting the scheduler..." << std::endl;
  std::cout << "The scheduler will now begin executing tasks at their scheduled times." << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  // Wait for tasks to complete
  std::cout << "\n🗓️ Step 4: Monitoring task execution..." << std::endl;
  for (int i = 1; i <= 5; ++i) {
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Check task statuses
    auto status1 = scheduler.getTaskStatus(task1Id);
    auto status2 = scheduler.getTaskStatus(task2Id);
    auto status3 = scheduler.getTaskStatus(task3Id);

    std::cout << "🧐 MONITOR " << i << "s: Task statuses - " << "Task1: " << Task::status_to_string(status1) << ", "
              << "Task2: " << Task::status_to_string(status2) << ", " << "Task3: " << Task::status_to_string(status3)
              << std::endl;
  }

  scheduler.stop();
  std::cout << "\n🏁 Scheduler stopped successfully" << std::endl;

  // Print final results
  std::cout << "\n=== Final Results ===" << std::endl;
  std::cout << "Task 1 final status: " << Task::status_to_string(scheduler.getTaskStatus(task1Id)) << std::endl;
  std::cout << "Task 2 final status: " << Task::status_to_string(scheduler.getTaskStatus(task2Id)) << std::endl;
  std::cout << "Task 3 final status: " << Task::status_to_string(scheduler.getTaskStatus(task3Id)) << std::endl;

  // Official scheduler statistics
  auto finalStats = scheduler.getStatistics();
  std::cout << "\n=== Official Scheduler Statistics ===" << std::endl;
  std::cout << "Total tasks scheduled: " << finalStats.totalTasksScheduled << std::endl;
  std::cout << "Tasks completed: " << finalStats.tasksCompleted << std::endl;
  std::cout << "Tasks failed: " << finalStats.tasksFailed << std::endl;
  std::cout << "Tasks cancelled: " << finalStats.tasksCancelled << std::endl;
  std::cout << "Tasks timed out: " << finalStats.tasksTimeout << std::endl;

  return 0;
}