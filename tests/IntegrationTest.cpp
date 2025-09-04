#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <fstream>
#include <cstdio>
#include "TaskScheduler.h"

using namespace TaskSchedX;

class IntegrationTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Clean up any test files
  }

  void TearDown() override {
    // Clean up test files
  }
};

// Test complete workflow: scheduling, execution, monitoring, and cleanup
TEST_F(IntegrationTest, CompleteWorkflow) {
  TaskScheduler scheduler(4);
  // scheduler.setConsoleLoggingEnabled(false);
  // scheduler.setLogLevel(Logger::Level::INFO);

  std::atomic<int> completed{0}, failed{0}, cancelled{0};

  scheduler.setTaskCompletionCallback([&](const std::string &, Task::Status status) {
    if (status == Task::Status::COMPLETED)
      completed++;
    else if (status == Task::Status::FAILED)
      failed++;
    else if (status == Task::Status::CANCELLED)
      cancelled++;
  });

  std::vector<std::string> taskIds;

  // 1. Immediate successful tasks
  TaskConfig immediateConfig;
  for (int i = 0; i < 5; ++i) {
    immediateConfig.taskFn = [i]() {
      // std::cout << "Immediate task " << i << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    };
    immediateConfig.startTime  = std::chrono::system_clock::now() + std::chrono::milliseconds(100 * i);
    immediateConfig.priority   = i + 1;
    immediateConfig.isRepeatable = false;
    taskIds.push_back(scheduler.scheduleTask(immediateConfig));
  }

  // 2. Delayed tasks with varying priority
  TaskConfig delayedConfig;
  for (int i = 0; i < 3; ++i) {
    delayedConfig.taskFn = [i]() {
      // std::cout << "Delayed task " << i << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };
    delayedConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(2);
    delayedConfig.priority   = 3 - i;
    delayedConfig.isRepeatable = false;
    taskIds.push_back(scheduler.scheduleTask(delayedConfig));
  }

  // 3. Repeatable task
  TaskConfig repeatableConfig;
  repeatableConfig.taskFn = []() {
    // std::cout << "Repeatable task executing" << std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(100)); // do some work
  };
  repeatableConfig.startTime      = std::chrono::system_clock::now() + std::chrono::seconds(1);
  repeatableConfig.priority       = 5;
  repeatableConfig.isRepeatable     = true;
  repeatableConfig.repeatEvery = std::chrono::seconds(1);
  std::string repeatId            = scheduler.scheduleTask(repeatableConfig);
  taskIds.push_back(repeatId);

  // 4. Failing task
  TaskConfig failConfig;
  failConfig.taskFn = []() {
    // std::cout << "This task will fail" << std::endl;
    throw std::runtime_error("Intentional failure");
  };
  failConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  failConfig.priority   = 2;
  failConfig.isRepeatable = false;
  std::string failId    = scheduler.scheduleTask(failConfig);
  taskIds.push_back(failId);

  // 5. Cancellable task
  TaskConfig cancelConfig;
  cancelConfig.taskFn = []() {
    // std::cout << "This task should be cancelled" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5)); // Long sleep
  };
  cancelConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(3);
  cancelConfig.priority   = 1;
  cancelConfig.isRepeatable = false;
  std::string cancelId    = scheduler.scheduleTask(cancelConfig);
  taskIds.push_back(cancelId);

  scheduler.start();

  // Cancel after a short delay (during execution)
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  EXPECT_TRUE(scheduler.cancelTask(cancelId));

  std::this_thread::sleep_for(std::chrono::seconds(3));
  scheduler.cancelTask(repeatId);

  std::this_thread::sleep_for(std::chrono::seconds(1));
  scheduler.stop();

  // Verify results
  auto stats = scheduler.getStatistics();
  EXPECT_GT(stats.totalTasksScheduled, 0);
  EXPECT_GT(completed.load(), 5);
  EXPECT_EQ(failed.load(), 1);
  EXPECT_GE(cancelled.load(), 1);
}

// Test system under high load
TEST_F(IntegrationTest, HighLoadScenario) {
  const int NUM_TASKS = 100;

  TaskScheduler scheduler(4); // 4 worker threads for balanced throughput
  scheduler.setConsoleLoggingEnabled(false);
  scheduler.setLogLevel(Logger::Level::ERROR);

  std::atomic<int> completedTasks{0};

  scheduler.setTaskCompletionCallback([&](const std::string &, Task::Status status) {
    if (status == Task::Status::COMPLETED)
      completedTasks++;
  });

  // Schedule 100 lightweight tasks with short sleep
  TaskConfig config;
  config.taskFn = []() { std::this_thread::sleep_for(std::chrono::milliseconds(10)); };
  config.startTime = std::chrono::system_clock::now();

  for (int i = 0; i < NUM_TASKS; ++i) {
    scheduler.scheduleTask(config);
  }

  scheduler.start();

  // Wait for tasks to complete (with upper bound of 5s)
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (completedTasks.load() < NUM_TASKS && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  scheduler.stop();

  auto stats = scheduler.getStatistics();

  EXPECT_EQ(completedTasks.load(), NUM_TASKS);
  EXPECT_EQ(stats.totalTasksScheduled, NUM_TASKS);
  EXPECT_EQ(stats.tasksCompleted, NUM_TASKS);
}

// Test mixed repeated and one-time tasks
TEST_F(IntegrationTest, MixedTaskTypes) {
  TaskScheduler scheduler(3);

  std::atomic<int> oneTimeDone{0};
  std::atomic<int> repeatCount{0};

  // Schedule 3 one-time tasks
  TaskConfig oneTimeConfig;
  oneTimeConfig.taskFn  = [&]() { ++oneTimeDone; };
  oneTimeConfig.startTime  = std::chrono::system_clock::now();
  oneTimeConfig.priority   = 1;
  oneTimeConfig.isRepeatable = false;
  for (int i = 0; i < 3; ++i) {
    scheduler.scheduleTask(oneTimeConfig);
  }

  // Schedule 2 repeatable tasks
  TaskConfig repeatableConfig;
  repeatableConfig.taskFn      = [&]() { ++repeatCount; };
  repeatableConfig.startTime      = std::chrono::system_clock::now();
  repeatableConfig.priority       = 2;
  repeatableConfig.isRepeatable     = true;
  repeatableConfig.repeatEvery = std::chrono::seconds(1);

  std::vector<std::string> repeatablesTaskIds;
  for (int i = 0; i < 2; ++i) {
    repeatablesTaskIds.push_back(scheduler.scheduleTask(repeatableConfig));
  }

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(3)); // Let tasks run

  // Cancel repeatable tasks
  for (const auto &id : repeatablesTaskIds) {
    scheduler.cancelTask(id);
  }

  scheduler.stop();

  EXPECT_EQ(oneTimeDone.load(), 3); // All one-time tasks must complete
  EXPECT_GE(repeatCount.load(), 4); // Each repeatable should run a few times
}
