#include "TaskScheduler.h"
#include <iostream>
#include <thread>
using namespace TaskSchedX;

int main() {
  std::cout << "=== Custom Scheduling Policies Example ===" << std::endl;
  std::cout << "This example demonstrates advanced scheduling policies and patterns:" << std::endl;
  std::cout << "1. Priority-based scheduling with different task types" << std::endl;
  std::cout << "2. Time-based scheduling with staggered execution" << std::endl;
  std::cout << "3. Mixed scheduling policies in a single scheduler" << std::endl;
  std::cout << "4. Custom task execution patterns and monitoring" << std::endl;
  std::cout << "\nWe'll create tasks with different scheduling characteristics to show flexibility.\n" << std::endl;

  TaskScheduler scheduler(4);
  std::cout << "✅ TaskScheduler created successfully" << std::endl;

  // Set up logging
  scheduler.setLogLevel(Logger::Level::INFO);
  scheduler.enableConsoleLogging(true);

  // === Immediate tasks: Prioritized to run ASAP ===
  // These tasks will run immediately, demonstrating priority scheduling
  std::cout << "\n🚀 Scheduling immediate priority tasks..." << std::endl;
  for (int i = 1; i <= 3; ++i) {
    TaskConfig config;
    config.executeFn  = [i]() { std::cout << "[Immediate] Task " << i << " executed." << std::endl; };
    config.priority   = i;                                // Priority 1, 2, 3
    config.startTime  = std::chrono::system_clock::now(); // Execute immediately
    config.repeatable = false;                            // non-Repeatable

    std::string taskId = scheduler.scheduleTask(config);
    std::cout << "🆗 Scheduled immediate task " << i << " (priority " << i << ") with ID: " << taskId << std::endl;
  }

  // === Staggered tasks: Scheduled with time gaps ===
  // These tasks will run at staggered intervals, demonstrating time-based scheduling
  std::cout << "\n📅 Scheduling staggered tasks (delayed)..." << std::endl;
  for (int i = 1; i <= 3; ++i) {
    TaskConfig config;
    config.executeFn  = [i]() { std::cout << "[Staggered] Task " << i << " executed." << std::endl; };
    config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(i * 2); // 2s, 4s, 6s, 8s
    config.priority   = 5;     // Priority 5, Same priority for all
    config.repeatable = false; // non-Repeatable

    std::string taskId = scheduler.scheduleTask(config);
    std::cout << "🆗 Scheduled staggered task " << i << " (execute at +" << (2 * i) << "s) with ID: " << taskId
              << std::endl;
  }

  // === Batch tasks: Same time, different priorities ===
  // These tasks will run at the same time but with different priorities
  std::cout << "\n📦 Scheduling batch tasks..." << std::endl;
  auto batchTime = std::chrono::system_clock::now() + std::chrono::seconds(8);
  for (int i = 1; i <= 3; ++i) {
    TaskConfig config;
    config.executeFn  = [i]() { std::cout << "[Batch] Task " << i << " running..." << std::endl; };
    config.startTime  = batchTime; // All execute at the same time
    config.priority   = 10 - i;    // Reverse priority (5=highest, 1=lowest)
    config.repeatable = false;     // non-Repeatable

    std::string taskId = scheduler.scheduleTask(config);
    std::cout << "🆗 Scheduled batch task " << i << " (priority " << (10 - i) << ") with ID: " << taskId << std::endl;
  }

  // === Maintenance tasks (repeatable) ===
  // These tasks will run periodically, demonstrating repeatable scheduling
  std::cout << "\n🔄 Scheduling periodic maintenance tasks..." << std::endl;

  // Quick maintenance (every 3 seconds)
  TaskConfig quick;
  quick.executeFn      = []() { std::cout << "[Maintenance] Quick check." << std::endl; };
  quick.startTime      = std::chrono::system_clock::now() + std::chrono::seconds(3); // Execute in 3 seconds
  quick.priority       = 20;                                                         // Lower priority than other tasks
  quick.repeatable     = true;                                                       // Repeatable task
  quick.repeatInterval = std::chrono::seconds(4);                                    // Repeat every 4 seconds

  std::string quickMaintenanceId = scheduler.scheduleTask(quick);
  std::cout << "🆗 Scheduled quick maintenance (every 3s) with ID: " << quickMaintenanceId << std::endl;

  // Slow maintenance (every 6 seconds)
  TaskConfig slow;
  slow.executeFn      = []() { std::cout << "[Maintenance] Slow cleanup." << std::endl; };
  slow.startTime      = std::chrono::system_clock::now() + std::chrono::seconds(5); // Execute in 5 seconds
  slow.priority       = 25;                                                         // lowest priority than other tasks
  slow.repeatable     = true;                                                       // Repeatable task
  slow.repeatInterval = std::chrono::seconds(6);                                    // Repeat every 6 seconds

  std::string slowMaintenanceId = scheduler.scheduleTask(slow);
  std::cout << "🆗 Scheduled slow maintenance (every 6s) with ID: " << slowMaintenanceId << std::endl;

  std::cout << "\n🚀 Starting scheduler with custom policies..." << std::endl;
  std::cout << "Monitoring execution for 20 seconds...\n" << std::endl;

  // Start the scheduler : Internally runs tasks in background
  scheduler.start();
  std::cout << "🏁 Scheduler started. Tasks will execute according to their schedule." << std::endl;

  for (int t = 0; t <= 20; ++t) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    if (t % 5 == 0 && t > 0) {
      auto stats = scheduler.getStatistics();
      std::cout << "[👀 MONITOR 👀] Time: " << t << "s - Completed: " << stats.tasksCompleted
                << ", Total scheduled: " << stats.totalTasksScheduled << std::endl;
    }
  }

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
