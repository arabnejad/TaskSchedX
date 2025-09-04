#include "TaskScheduler.h"
#include <iostream>
#include <thread>
#include <atomic>

using namespace TaskSchedX;

int main() {

  std::cout << "=== Statistics and Monitoring Example ===" << std::endl;
  std::cout << "This example demonstrates comprehensive task monitoring and statistics tracking." << std::endl;
  std::cout << "We'll create various types of tasks and monitor their execution in real-time:" << std::endl;
  std::cout << "- Successful tasks" << std::endl;
  std::cout << "- Failing tasks" << std::endl;
  std::cout << "- Timeout tasks" << std::endl;
  std::cout << "- Cancelled tasks" << std::endl;
  std::cout << "- Repeatable tasks\n\n" << std::endl;

  // Create scheduler with 3 worker threads
  TaskScheduler scheduler(3);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Configure logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.setConsoleLoggingEnabled(true);

  std::atomic<int> repeatCount(0);

  std::cout << "🗓️ Setting up various task types for monitoring..." << std::endl;

  // === Successful Tasks ===
  TaskConfig success;
  success.taskFn = []() { std::cout << "✅ Success task executed" << std::endl; };
  success.startTime = std::chrono::system_clock::now() + std::chrono::milliseconds(200);

  std::string taskIdSuccess = scheduler.scheduleTask(success);
  std::cout << "🆗 Scheduled successful task with ID: " << taskIdSuccess << std::endl;

  // === Failing Tasks ===
  TaskConfig fail;
  fail.taskFn = []() {
    std::cout << "❌ Failure task running" << std::endl;
    throw std::runtime_error("Intentional failure");
  };
  fail.startTime = std::chrono::system_clock::now() + std::chrono::milliseconds(400);

  std::string taskIdFail = scheduler.scheduleTask(fail);
  std::cout << "🆗 Scheduled failing task with ID: " << taskIdFail << std::endl;

  // === Timeout Tasks ===
  TaskConfig timeout;
  timeout.taskFn = []() {
    std::cout << "⏳ Timeout task running" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
  };
  timeout.startTime        = std::chrono::system_clock::now() + std::chrono::milliseconds(600);
  timeout.timeout = std::chrono::seconds(1);

  std::string taskIdTimeout = scheduler.scheduleTask(timeout);
  std::cout << "🆗 Scheduled timeout task with ID: " << taskIdTimeout << " (1s timeout)" << std::endl;

  // === Repeatable Task (cancel later) ===
  TaskConfig repeatable;
  repeatable.taskFn = [&repeatCount]() {
    int count = repeatCount.fetch_add(1) + 1;
    std::cout << "🔁 Repeatable task run #" << count << std::endl;
  };
  repeatable.isRepeatable     = true;
  repeatable.repeatEvery = std::chrono::seconds(1);
  repeatable.startTime      = std::chrono::system_clock::now() + std::chrono::milliseconds(800);

  std::string taskIdRepeatable = scheduler.scheduleTask(repeatable);
  std::cout << "🆗 Scheduled repeatable task with ID: " << taskIdRepeatable << std::endl;

  // === Tasks to Cancel ===
  TaskConfig cancellable;
  cancellable.taskFn = []() { std::cout << "🚫 This should not run!" << std::endl; };
  cancellable.startTime = std::chrono::system_clock::now() + std::chrono::seconds(5);

  std::string taskIdCancellable = scheduler.scheduleTask(cancellable);
  std::cout << "🆗 Scheduled task for cancellation with ID: " << taskIdCancellable << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  // Cancel tasks after a short wait
  std::this_thread::sleep_for(std::chrono::seconds(2));
  bool cancelled = scheduler.cancelTask(taskIdCancellable);
  std::cout << "\n[🛠️ ACTION 🛠️] Cancellable task with ID " << taskIdCancellable
            << " cancellation: " << (cancelled ? "SUCCESS" : "FAILED") << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(3));
  cancelled = scheduler.cancelTask(taskIdRepeatable);
  std::cout << "\n[🛠️ ACTION 🛠️] Repeatable task with ID " << taskIdRepeatable
            << " cancellation: " << (cancelled ? "SUCCESS (may run for final time)" : "FAILED") << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(1));

  std::cout << "\n🏁 Stopping scheduler..." << std::endl;
  scheduler.stop();

  // Official scheduler statistics
  auto finalStats = scheduler.getStatistics();
  std::cout << "\n=== Official Scheduler Statistics ===" << std::endl;
  std::cout << "Total tasks scheduled: " << finalStats.totalTasksScheduled << std::endl;
  std::cout << "Tasks completed: " << finalStats.tasksCompleted << std::endl;
  std::cout << "Tasks failed: " << finalStats.tasksFailed << std::endl;
  std::cout << "Tasks cancelled: " << finalStats.tasksCancelled << std::endl;
  std::cout << "Tasks timed out: " << finalStats.tasksTimeout << std::endl;

  // Calculate and display performance metrics
  size_t totalProcessed =
      finalStats.tasksCompleted + finalStats.tasksFailed + finalStats.tasksCancelled + finalStats.tasksTimeout;

  std::cout << "\n=== Performance Metrics ===" << std::endl;
  std::cout << "Total tasks processed: " << totalProcessed << std::endl;
  std::cout << "Repeatable task executions: " << repeatCount.load() << std::endl;
  std::cout << std::fixed << std::setprecision(1);
  std::cout << "Success rate: " << (100.0 * finalStats.tasksCompleted / totalProcessed) << "%" << std::endl;
  std::cout << "Failure rate: " << (100.0 * finalStats.tasksFailed / totalProcessed) << "%" << std::endl;
  std::cout << "Cancellation rate: " << (100.0 * finalStats.tasksCancelled / totalProcessed) << "%" << std::endl;
  std::cout << "Timeout rate: " << (100.0 * finalStats.tasksTimeout / totalProcessed) << "%" << std::endl;

  return 0;
}
