#include "TaskScheduler.h"
#include <iostream>
#include <thread>
#include <atomic>

using namespace TaskSchedX;

int main() {
  std::cout << "=== Task Cancellation Example ===" << std::endl;
  std::cout << "Scenarios:" << std::endl;
  std::cout << "1. Cancel before execution" << std::endl;
  std::cout << "2. Cancel during execution" << std::endl;
  std::cout << "3. Cancel repeatable task" << std::endl;
  std::cout << "4. Cancel non-existent task" << std::endl;

  TaskScheduler scheduler(3);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;
  // Configure logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.setConsoleLoggingEnabled(true);

  std::atomic<int> longTaskCount(0);
  std::atomic<int> repeatCount(0);

  // === Cancel before execution ===
  TaskConfig preCancel;
  preCancel.taskFn         = []() { std::cout << "🚨 Should not run 🚨\n"; };
  preCancel.startTime         = std::chrono::system_clock::now() + std::chrono::seconds(3);
  std::string taskIdPreCancel = scheduler.scheduleTask(preCancel);
  std::cout << "🆗 Scheduled task (pre-cancel) with ID: " << taskIdPreCancel << std::endl;
  bool cancelled = scheduler.cancelTask(taskIdPreCancel);
  std::cout << "[🛠️ ACTION 🛠️] preCancel task with ID " << taskIdPreCancel
            << " cancellation: " << (cancelled ? "SUCCESS" : "FAILED") << std::endl;

  // === Cancel during long-running execution ===
  TaskConfig longTask;
  longTask.taskFn = [&]() {
    longTaskCount++;
    std::cout << "🕐 Long task started" << std::endl;
    for (int i = 1; i <= 10; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(300));
      std::cout << "🕐 Long task progress: " << i * 10 << "%" << std::endl;
    }
    std::cout << "✅ Long task completed (even though cancellation was requested)" << std::endl;
  };
  longTask.startTime         = std::chrono::system_clock::now() + std::chrono::seconds(1);
  std::string taskIdLongTask = scheduler.scheduleTask(longTask);
  std::cout << "🆗 Scheduled long task with ID: " << taskIdLongTask << std::endl;

  // === Cancel repeatable task after 3 runs ===
  TaskConfig repeatTask;
  repeatTask.taskFn = [&]() {
    int c = repeatCount.fetch_add(1) + 1;
    std::cout << "🔁 Repeat #" << c << std::endl;
  };
  repeatTask.startTime         = std::chrono::system_clock::now() + std::chrono::seconds(1);
  repeatTask.isRepeatable        = true;
  repeatTask.repeatEvery    = std::chrono::seconds(1);
  std::string taskIdRepeatTask = scheduler.scheduleTask(repeatTask);
  std::cout << "🆗 Scheduled repeatable task with ID: " << taskIdRepeatTask << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  // Cancel long task mid-way
  std::this_thread::sleep_for(std::chrono::seconds(2));
  std::cout << "\n[⛔] Cancelling long-running task...\n";
  scheduler.cancelTask(taskIdLongTask);

  // Cancel repeatable after some runs
  std::this_thread::sleep_for(std::chrono::seconds(4));
  std::cout << "\n[⛔] Cancelling repeatable task after " << repeatCount << " executions...\n";
  scheduler.cancelTask(taskIdRepeatTask);

  // 4️⃣ Attempt to cancel non-existent task
  std::cout << "\n[❓] Cancelling fake task...\n";
  if (!scheduler.cancelTask("invalid_id")) {
    std::cout << "[⚠️] Non-existent task could not be cancelled.\n";
  }

  std::this_thread::sleep_for(std::chrono::seconds(2));

  std::cout << "\n🏁 Stopping scheduler..." << std::endl;
  scheduler.stop();

  // Summary
  std::cout << "\n=== Summary ===\n";
  std::cout << "Long task ran: " << longTaskCount << " time(s)\n";
  std::cout << "Repeat ran:    " << repeatCount << " time(s)\n";

  // Official scheduler statistics
  auto finalStats = scheduler.getStatistics();
  std::cout << "\n=== Official Scheduler Statistics ===" << std::endl;
  std::cout << "Total tasks scheduled: " << finalStats.totalTasksScheduled << std::endl;
  std::cout << "Tasks completed: " << finalStats.tasksCompleted << std::endl;
  std::cout << "Tasks failed: " << finalStats.tasksFailed << std::endl;
  std::cout << "Tasks cancelled: " << finalStats.tasksCancelled << std::endl;
  std::cout << "Tasks timed out: " << finalStats.tasksTimeout << std::endl;
  std::cout << "\nNote: 'Tasks completed' includes each run of repeatable tasks.\n";

  return 0;
}
