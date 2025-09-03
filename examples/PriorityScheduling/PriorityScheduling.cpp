#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include "TaskScheduler.h"

using namespace TaskSchedX;

int main() {
  std::cout << "=== Priority-Based Task Scheduling Example ===" << std::endl;
  std::cout << "This example demonstrates how task priorities affect execution order." << std::endl;
  std::cout << "Lower priority numbers indicate higher priority tasks." << std::endl;
  std::cout << "We'll schedule tasks with priorities 1, 3, 5, 10, and 15 all at the same time." << std::endl;
  std::cout << "Expected execution order: Priority 1, 3, 5, 10, 15\n" << std::endl;

  // Create scheduler with only 1 worker thread to clearly see priority ordering
  TaskScheduler scheduler(1);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Configure logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.enableConsoleLogging(true);

  // Vector to store task IDs for tracking
  std::vector<std::string> taskIds;

  // Schedule all tasks to execute at the same time but with different priorities
  auto startTime = std::chrono::system_clock::now() + std::chrono::seconds(2);

  std::cout << "🗓️ Scheduling tasks with different priorities (all at same execution time):" << std::endl;

  // === Priority 15 (lowest priority) - should execute last ===
  TaskConfig taskConfig;
  taskConfig.executeFn = []() {
    std::cout << "Task (PRIORITY 15) — Expected: #7 - Lowest" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to see ordering
  };
  taskConfig.startTime  = startTime;
  taskConfig.priority   = 15;    // Priority 15, lowest priority
  taskConfig.repeatable = false; // non-Repeatable

  std::string lowPriorityId = scheduler.scheduleTask(taskConfig);
  taskIds.push_back(lowPriorityId);
  std::cout << "🆗 Scheduled LOWEST priority task (15) with ID: " << lowPriorityId << std::endl;

  // === Priority 1 (highest priority) - should execute first ===
  taskConfig.executeFn = []() {
    std::cout << "Task (PRIORITY 1) — Expected: #2 - Highest priority" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to see ordering
  };
  taskConfig.startTime  = startTime;
  taskConfig.priority   = 1;     // Priority 1, highest priority
  taskConfig.repeatable = false; // non-Repeatable

  std::string highPriorityId = scheduler.scheduleTask(taskConfig);
  taskIds.push_back(highPriorityId);
  std::cout << "🆗 Scheduled HIGHEST priority task (1) with ID: " << highPriorityId << std::endl;

  // === Priority 10 (medium-low priority) ===
  taskConfig.executeFn = []() {
    std::cout << "Task (PRIORITY 10) — Expected: #6 - Medium-low" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to see ordering
  };
  taskConfig.startTime  = startTime;
  taskConfig.priority   = 10;    // Priority 10, second lowest priority
  taskConfig.repeatable = false; // non-Repeatable

  std::string mediumLowPriorityId = scheduler.scheduleTask(taskConfig);
  taskIds.push_back(mediumLowPriorityId);
  std::cout << "🆗 Scheduled MEDIUM-LOW priority task (10) with ID: " << mediumLowPriorityId << std::endl;

  // === Priority 5 (medium priority) ===
  taskConfig.executeFn = []() {
    std::cout << "Task (PRIORITY 5) — Expected: #5 - Medium priority" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Small delay to see ordering
  };
  taskConfig.startTime  = startTime;
  taskConfig.priority   = 5;     // Priority 5, second highest priority
  taskConfig.repeatable = false; // non-Repeatable

  std::string mediumPriorityId = scheduler.scheduleTask(taskConfig);
  taskIds.push_back(mediumPriorityId);
  std::cout << "🆗 Scheduled MEDIUM priority task (5) with ID: " << mediumPriorityId << std::endl;

  // Now schedule tasks with same priority but different execution times
  std::cout << "\n🗓️ Scheduling tasks with SAME priority but different execution times:" << std::endl;

  // === Task with priority 3 (earlier label) ===
  // This task has the same priority but is scheduled earlier than the next one
  taskConfig.executeFn  = []() { std::cout << "Task (PRIORITY 3-A) — Expected: #3" << std::endl; };
  taskConfig.startTime  = startTime;
  taskConfig.priority   = 3;     // Priority 3,
  taskConfig.repeatable = false; // non-Repeatable

  std::string earlierPriority3Id = scheduler.scheduleTask(taskConfig);
  taskIds.push_back(earlierPriority3Id);
  std::cout << "🆗 Scheduled task with priority 3-A with ID: " << earlierPriority3Id << std::endl;

  // === Task with priority 3 (later label) ===
  // This task has the same priority but is scheduled later than the previous one
  taskConfig.executeFn  = []() { std::cout << "Task (PRIORITY 3-B) — Expected: #4" << std::endl; };
  taskConfig.startTime  = startTime;
  taskConfig.priority   = 3;     // Priority 3,
  taskConfig.repeatable = false; // non-Repeatable

  std::string laterPriority3Id = scheduler.scheduleTask(taskConfig);
  taskIds.push_back(laterPriority3Id);
  std::cout << "🆗 Scheduled task with priority 3 (later time) with ID: " << laterPriority3Id << std::endl;

  // === Add some immediate high priority tasks ===
  // These tasks should execute immediately and have the highest priority
  std::cout << "\n🗓️ Adding immediate high priority tasks:" << std::endl;

  taskConfig.executeFn = []() { std::cout << "Task (PRIORITY 0) — Expected: #1 - Immediate execution" << std::endl; };
  taskConfig.startTime = std::chrono::system_clock::now(); // Immediate execution
  taskConfig.priority  = 0;                                // Priority 0, highest priority
  taskConfig.repeatable = false;                           // non-Repeatable

  std::string immediateId = scheduler.scheduleTask(taskConfig);
  taskIds.push_back(immediateId);
  std::cout << "🆗 Scheduled immediate high priority task (0) with ID: " << immediateId << std::endl;

  std::cout << "\n🚀 Starting scheduler..." << std::endl;
  std::cout << "Expected execution order:" << std::endl;
  std::cout << "1. Immediate high priority (0)" << std::endl;
  std::cout << "2. Highest priority (1)" << std::endl;
  std::cout << "3. Same priority - earlier time (3)" << std::endl;
  std::cout << "4. Same priority - later time (3)" << std::endl;
  std::cout << "5. Medium priority (5)" << std::endl;
  std::cout << "6. Medium-low priority (10)" << std::endl;
  std::cout << "7. Lowest priority (15)" << std::endl;
  std::cout << "\nActual execution order:" << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  // Wait for all tasks to complete
  std::this_thread::sleep_for(std::chrono::seconds(5));

  std::cout << "\n🏁 Stopping scheduler..." << std::endl;
  scheduler.stop();

  // Print task statuses
  std::cout << "\n=== Final Task Statuses ===" << std::endl;
  for (const auto &taskId : taskIds) {
    std::cout << "Task " << taskId << " status: " << Task::status_to_string(scheduler.getTaskStatus(taskId))
              << std::endl;
  }

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