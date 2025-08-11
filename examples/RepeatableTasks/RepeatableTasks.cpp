#include <iostream>
#include <thread>
#include <atomic>
#include "TaskScheduler.h"

using namespace TaskSchedX;

int main() {
  std::cout << "=== Repeatable Tasks Example ===" << std::endl;
  std::cout << "This example demonstrates scheduling multiple repeatable tasks with different intervals and priorities."
            << std::endl;
  std::cout << "We'll run 3 repeatable tasks:" << std::endl;
  std::cout << "- High priority task (priority 1) every 2 seconds" << std::endl;
  std::cout << "- Medium priority task (priority 5) every 3 seconds" << std::endl;
  std::cout << "- Low priority task (priority 10) every 5 seconds" << std::endl;
  std::cout << "Running for 15 seconds to observe multiple executions...\n" << std::endl;

  // Create scheduler with 4 worker threads
  TaskScheduler scheduler(4);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Configure logging for detailed output
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.enableConsoleLogging(true);

  // Counters to track task executions
  std::atomic<int> highPriorityCount(0);
  std::atomic<int> mediumPriorityCount(0);
  std::atomic<int> lowPriorityCount(0);

  TaskConfig taskConfig;

  // Schedule high priority repeatable task (every 2 seconds)
  taskConfig.executeFn = [&highPriorityCount]() {
    int count = highPriorityCount.fetch_add(1) + 1;
    std::cout << "HIGH PRIORITY Task executed (execution #" << count << ")" << std::endl;
  };
  taskConfig.startTime      = std::chrono::system_clock::now() + std::chrono::seconds(1); // Start in 1 second
  taskConfig.priority       = 1;                       // High priority (lower number = higher priority)
  taskConfig.repeatable     = true;                    // Repeatable task
  taskConfig.repeatInterval = std::chrono::seconds(2); // Every 2 seconds

  std::string highPriorityTaskId = scheduler.scheduleTask(taskConfig);
  std::cout << "🆗 Scheduled high priority repeatable task with ID: " << highPriorityTaskId << std::endl;

  // Schedule medium priority repeatable task (every 3 seconds)
  taskConfig.executeFn = [&mediumPriorityCount]() {
    int count = mediumPriorityCount.fetch_add(1) + 1;
    std::cout << "MEDIUM PRIORITY Task executed (execution #" << count << ")" << std::endl;
  };
  taskConfig.startTime      = std::chrono::system_clock::now() + std::chrono::seconds(1); // Start in 1 second
  taskConfig.priority       = 5;                                                          // Medium priority
  taskConfig.repeatable     = true;                                                       // Repeatable task
  taskConfig.repeatInterval = std::chrono::seconds(3);                                    // Every 3 seconds

  std::string mediumPriorityTaskId = scheduler.scheduleTask(taskConfig);
  std::cout << "🆗 Scheduled medium priority repeatable task with ID: " << mediumPriorityTaskId << std::endl;

  // Schedule low priority repeatable task (every 5 seconds)
  taskConfig.executeFn = [&lowPriorityCount]() {
    int count = lowPriorityCount.fetch_add(1) + 1;
    std::cout << "LOW PRIORITY Task executed (execution #" << count << ")" << std::endl;
  };
  taskConfig.startTime      = std::chrono::system_clock::now() + std::chrono::seconds(1); // Start in 1 second
  taskConfig.priority       = 10;                                                         // Low priority
  taskConfig.repeatable     = true;                                                       // Repeatable task
  taskConfig.repeatInterval = std::chrono::seconds(5);                                    // Every 5 seconds

  std::string lowPriorityTaskId = scheduler.scheduleTask(taskConfig);
  std::cout << "🆗 Scheduled low priority repeatable task with ID: " << lowPriorityTaskId << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  // Let tasks run for 15 seconds to see multiple executions
  std::this_thread::sleep_for(std::chrono::seconds(15));

  std::cout << "\n🏁 Stopping scheduler..." << std::endl;
  scheduler.stop();

  // Print execution summary
  std::cout << "\n=== Execution Summary ===" << std::endl;
  std::cout << "High priority task executions: " << highPriorityCount.load() << std::endl;
  std::cout << "Medium priority task executions: " << mediumPriorityCount.load() << std::endl;
  std::cout << "Low priority task executions: " << lowPriorityCount.load() << std::endl;

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