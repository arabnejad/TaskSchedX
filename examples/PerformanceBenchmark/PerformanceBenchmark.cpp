#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <mutex>
#include <map>
#include <sstream>
#include <sys/resource.h>
#include <pthread.h>

#include "TaskScheduler.h"

using namespace TaskSchedX;

long getCpuTimeMs() {
  struct rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  long user = usage.ru_utime.tv_sec * 1000 + usage.ru_utime.tv_usec / 1000;
  long sys  = usage.ru_stime.tv_sec * 1000 + usage.ru_stime.tv_usec / 1000;
  return user + sys;
}

class Benchmark {
public:
  void run(const std::string &label, int threads, int numTasks, std::function<void()> taskBody) {
    std::cout << "\n🔧 Running [" << label << "] with " << threads << " thread(s) and " << numTasks << " tasks..."
              << std::endl;

    TaskScheduler scheduler(threads);
    std::cout << "✅ TaskScheduler created successfully" << std::endl;
    scheduler.enableConsoleLogging(false);

    std::atomic<int> completed{0};
    auto             startAt  = std::chrono::steady_clock::now();
    long             cpuStart = getCpuTimeMs();

    for (int i = 0; i < numTasks; ++i) {
      TaskConfig taskConfig;
      taskConfig.executeFn = [&]() {
        taskBody();
        completed++;
      };
      taskConfig.startTime  = std::chrono::system_clock::now(); // Immediate execution
      taskConfig.priority   = 1;                                // Same priority for all
      taskConfig.repeatable = false;                            // non-Repeatable

      scheduler.scheduleTask(taskConfig);
    }

    scheduler.start();
    while (completed.load() < numTasks) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    scheduler.stop();

    auto endAt  = std::chrono::steady_clock::now();
    long cpuEnd = getCpuTimeMs();

    auto   wallTime   = std::chrono::duration_cast<std::chrono::milliseconds>(endAt - startAt).count();
    long   cpuUsed    = cpuEnd - cpuStart;
    double throughput = (1000.0 * numTasks) / wallTime;

    // Official scheduler statistics
    auto finalStats = scheduler.getStatistics();
    std::cout << "\n=== Official Scheduler Statistics ===" << std::endl;
    std::cout << "Total tasks scheduled: " << finalStats.totalTasksScheduled << std::endl;
    std::cout << "Tasks completed: " << finalStats.tasksCompleted << std::endl;
    std::cout << "Tasks failed: " << finalStats.tasksFailed << std::endl;
    std::cout << "Tasks cancelled: " << finalStats.tasksCancelled << std::endl;
    std::cout << "Tasks timed out: " << finalStats.tasksTimedOut << std::endl;

    std::cout << "\nTotalnumTasks = " << numTasks << " | ✅ Completed: " << completed.load()
              << " | 🕒 Wall Time: " << wallTime << " ms"
              << " | 🧠 CPU Time: " << cpuUsed << " ms"
              << " | ⚡ Throughput: " << std::fixed << std::setprecision(2) << throughput << " tasks/sec" << std::endl;
  }
};

int main() {
  const int        NUM_TASKS    = 100;
  std::vector<int> threadCounts = {1, 2, 4, 8};

  Benchmark bench;

  auto cpuTask = [] {
    volatile long long sum = 0;
    for (int i = 0; i < 50000000; ++i)
      sum += i * i;
  };

  auto ioTask = [] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); };

  auto mixedTask = [] {
    volatile long sum = 0;
    for (int i = 0; i < 50000; ++i) // cpu
      sum += i;
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); // I/O
  };

  std::cout << "=== Performance Benchmark ===" << std::endl;
  std::cout << "This example demonstrates how to measure and analyze the performance of the TaskScheduler:"
            << std::endl;
  std::cout << "1. Benchmark execution speed with varying thread pool sizes" << std::endl;
  std::cout << "2. Evaluate different workload types: CPU-bound, I/O-bound, and mixed" << std::endl;
  std::cout << "3. Measure task throughput, CPU utilization, and execution time" << std::endl;

  std::cout << "\n🔥 === CPU-BOUND TASKS === 🔥\n";
  for (int threads : threadCounts) {
    bench.run("CPU-Bound", threads, NUM_TASKS, cpuTask);
  }

  std::cout << "\n💾 === I/O-BOUND TASKS === 💾\n";
  for (int threads : threadCounts) {
    bench.run("I/O-Bound", threads, NUM_TASKS, ioTask);
  }

  std::cout << "\n🔥+💾 === MIXED CPU + I/O TASKS === 💾+🔥\n";
  for (int threads : threadCounts) {
    bench.run("Mixed Workload", threads, NUM_TASKS, mixedTask);
  }

  return 0;
}
