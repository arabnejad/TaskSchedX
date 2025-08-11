#include "TaskScheduler.h"
#include <iostream>
#include <thread>

using namespace TaskSchedX;

int main() {
  std::cout << "=== Simple Task Timeout Demo ===\n"
            << "This demo shows tasks that complete, timeout, or run without limits.\n"
            << std::endl;

  TaskScheduler scheduler(3);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Set up logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.enableConsoleLogging(true);

  // === Quick task (completes before timeout) ===

  TaskConfig quick;
  quick.executeFn = []() {
    std::cout << "✅ Quick task started" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "✅ Quick task completed" << std::endl;
  };
  quick.executionTimeout  = std::chrono::seconds(3);
  std::string taskIdQuick = scheduler.scheduleTask(quick);
  std::cout << "🆗 Scheduled quick task with ID: " << taskIdQuick << std::endl;

  // === Slow task (exceeds timeout) ===
  TaskConfig slow;
  slow.executeFn = []() {
    std::cout << "⏳ Slow task started" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(4));
    std::cout << "⛔ Slow task finished (but marked as TIMEOUT)" << std::endl;
  };
  slow.executionTimeout  = std::chrono::seconds(2);
  std::string taskIdSlow = scheduler.scheduleTask(slow);
  std::cout << "🆗 Scheduled slow task with ID: " << taskIdSlow << std::endl;

  // === Unlimited task (no timeout) ===
  TaskConfig unlimited;
  unlimited.executeFn = []() {
    std::cout << "🔄 Unlimited task running" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "✅ Unlimited task completed" << std::endl;
  };
  unlimited.executionTimeout  = std::chrono::seconds(0); // No timeout
  std::string taskIdUnlimited = scheduler.scheduleTask(unlimited);
  std::cout << "🆗 Scheduled unlimited task with ID: " << taskIdUnlimited << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(6));

  std::cout << "\n🏁 Stopping scheduler..." << std::endl;
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
