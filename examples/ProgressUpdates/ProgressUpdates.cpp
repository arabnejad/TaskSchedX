#include <iostream>
#include <thread>
#include "TaskScheduler.h"

using namespace TaskSchedX;

int main() {
  std::cout << "=== Progress Updates and Long-Running Tasks Example ===" << std::endl;
  std::cout << "This example demonstrates how to handle long-running tasks with progress reporting:" << std::endl;
  std::cout << "1. Schedule a task that runs for several seconds" << std::endl;
  std::cout << "2. The task reports its progress at regular intervals" << std::endl;
  std::cout << "3. Progress updates are logged and can be monitored in real-time" << std::endl;
  std::cout << "4. Demonstrate how the scheduler handles tasks of varying duration" << std::endl;
  std::cout << "\nThe task will simulate processing work and report progress from 0% to 100%.\n" << std::endl;

  TaskScheduler scheduler(4);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Set up logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.setConsoleLoggingEnabled(true);

  std::cout << "\n🗓️ Scheduling a long-running task with progress reporting..." << std::endl;

  TaskConfig taskConfig;
  taskConfig.taskFn = []() {
    std::cout << "=== Long-running task started ===" << std::endl;
    std::cout << "Beginning data processing simulation..." << std::endl;
    // Simulate a long-running process with progress updates
    for (int i = 0; i <= 100; i += 10) {
      std::cout << "Progress: " << i << "% - Processing data batch " << i / 10 + 1 << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Simulate work
    }

    std::cout << "=== Long-running task completed successfully ===" << std::endl;
  };
  taskConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  taskConfig.priority   = 1;     // Priority 1
  taskConfig.isRepeatable = false; // non-Repeatable

  std::string taskId = scheduler.scheduleTask(taskConfig);

  std::cout << "🆗 Scheduled progress reporting task with ID: " << taskId << std::endl;

  std::cout << "🗓️ Starting scheduler. Watch for progress updates in the logs below..." << std::endl;
  std::cout << "🗓️ Expected: Progress updates from 0% to 100% over approximately 2-3 seconds.\n" << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  std::this_thread::sleep_for(std::chrono::seconds(5)); // Allow time for task execution

  std::cout << "\n🏁 Stopping scheduler..." << std::endl;
  scheduler.stop();

  // Check final task status
  std::cout << "\n=== Task Execution Summary ===" << std::endl;
  std::cout << "Final task status: " << Task::status_to_string(scheduler.getTaskStatus(taskId)) << std::endl;

  // Official scheduler statistics
  auto finalStats = scheduler.getStatistics();
  std::cout << "\n=== Official Scheduler Statistics ===" << std::endl;
  std::cout << "Total tasks scheduled: " << finalStats.totalTasksScheduled << std::endl;
  std::cout << "Tasks completed: " << finalStats.tasksCompleted << std::endl;
  std::cout << "Tasks failed: " << finalStats.tasksFailed << std::endl;
  std::cout << "Tasks cancelled: " << finalStats.tasksCancelled << std::endl;
  std::cout << "Tasks timed out: " << finalStats.tasksTimeout << std::endl;
  if (finalStats.totalTasksScheduled > 0) {
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "Failure rate: " << (100.0 * finalStats.tasksFailed / finalStats.totalTasksScheduled) << "%"
              << std::endl;
  }

  return 0;
}