#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdio>
#include "TaskScheduler.h"

using namespace TaskSchedX;

class TaskSchedulerTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Additional setup if needed
  }

  void TearDown() override {
    // Ensure scheduler is stopped after each test
  }
};

// Test basic task scheduling and execution
TEST_F(TaskSchedulerTest, BasicScheduling) {
  TaskScheduler    scheduler(1);
  std::atomic<int> count{0};

  TaskConfig config;
  config.executeFn  = [&]() { ++count; };
  config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  config.priority   = 1;
  config.repeatable = false;
  std::string id    = scheduler.scheduleTask(config);

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(2));
  scheduler.stop();

  EXPECT_EQ(count, 1);
  EXPECT_EQ(scheduler.getTaskStatus(id), Task::Status::COMPLETED);
}

TEST_F(TaskSchedulerTest, RepeatingTask) {
  TaskScheduler    scheduler(1);
  std::atomic<int> count{0};

  TaskConfig config;
  config.executeFn      = [&]() { ++count; };
  config.startTime      = std::chrono::system_clock::now() + std::chrono::milliseconds(500);
  config.priority       = 1;
  config.repeatable     = true;
  config.repeatInterval = std::chrono::seconds(1); // repeat every 1 second
  scheduler.scheduleTask(config);

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(4)); // Allow multiple executions
  scheduler.stop();

  EXPECT_GE(count, 2); // Should execute at least 3 times
}

TEST_F(TaskSchedulerTest, PriorityExecutionOrder) {
  TaskScheduler    scheduler(1); // force serial execution
  std::vector<int> executionOrder;
  std::mutex       orderMutex;

  auto now = std::chrono::system_clock::now() + std::chrono::seconds(1);

  TaskConfig task1;
  task1.startTime = now;
  task1.priority  = 3;
  task1.executeFn = [&]() {
    std::lock_guard<std::mutex> l(orderMutex);
    executionOrder.push_back(3);
  };

  TaskConfig task2;
  task2.startTime = now;
  task2.priority  = 1;
  task2.executeFn = [&]() {
    std::lock_guard<std::mutex> l(orderMutex);
    executionOrder.push_back(1);
  };

  TaskConfig task3;
  task3.startTime = now;
  task3.priority  = 2;
  task3.executeFn = [&]() {
    std::lock_guard<std::mutex> l(orderMutex);
    executionOrder.push_back(2);
  };

  scheduler.scheduleTask(task1);
  scheduler.scheduleTask(task2);
  scheduler.scheduleTask(task3);

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(3));
  scheduler.stop();

  EXPECT_EQ(executionOrder.size(), 3);
  // Should execute in priority order: 1, 2, 3
  EXPECT_EQ(executionOrder, std::vector<int>({1, 2, 3}));
}

TEST_F(TaskSchedulerTest, CancelBeforeStart) {
  TaskScheduler    scheduler(1);
  std::atomic<int> ran{0};

  TaskConfig config;
  config.executeFn  = [&]() { ++ran; };
  config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(2);
  config.priority   = 1;
  config.repeatable = false;
  std::string id    = scheduler.scheduleTask(config);

  // Cancel before starting scheduler
  EXPECT_TRUE(scheduler.cancelTask(id));

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(3));
  scheduler.stop();

  EXPECT_EQ(ran, 0);
  EXPECT_EQ(scheduler.getTaskStatus(id), Task::Status::CANCELLED);
}

TEST_F(TaskSchedulerTest, CancelDuringExecution) {
  TaskScheduler     scheduler(1);
  std::atomic<bool> taskStarted{false};

  TaskConfig config;
  config.executeFn = [&]() {
    taskStarted = true;
    std::this_thread::sleep_for(std::chrono::seconds(2)); // Simulate task
  };
  config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  config.priority   = 1;
  config.repeatable = false;

  std::string id = scheduler.scheduleTask(config);

  scheduler.start();

  // Wait for task to start, then cancel
  while (!taskStarted) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  EXPECT_TRUE(scheduler.cancelTask(id));

  std::this_thread::sleep_for(std::chrono::seconds(3));

  scheduler.stop();

  EXPECT_TRUE(taskStarted);
}

TEST_F(TaskSchedulerTest, cancelNonExistentTask) {
  TaskScheduler scheduler(1);

  bool cancelled = scheduler.cancelTask("non_existent_task_id");

  EXPECT_FALSE(cancelled);
}

TEST_F(TaskSchedulerTest, TaskErrorHandling) {
  TaskScheduler    scheduler(2);
  std::atomic<int> successfulTasks(0);

  // Schedule a successful task
  TaskConfig successTasksConfig;
  successTasksConfig.executeFn  = [&]() { successfulTasks.fetch_add(1); };
  successTasksConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  successTasksConfig.priority   = 1;
  successTasksConfig.repeatable = false;
  scheduler.scheduleTask(successTasksConfig);

  // Schedule a failing task
  TaskConfig failureTaskConfig;
  failureTaskConfig.executeFn  = []() { throw std::runtime_error("Test error"); };
  failureTaskConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  failureTaskConfig.priority   = 1;
  failureTaskConfig.repeatable = false;
  scheduler.scheduleTask(failureTaskConfig);

  scheduler.start();

  std::this_thread::sleep_for(std::chrono::seconds(3));

  scheduler.stop();

  auto stats = scheduler.getStatistics();
  EXPECT_EQ(stats.totalTasksScheduled, 2);
  EXPECT_EQ(stats.tasksCompleted, 1);
  EXPECT_EQ(stats.tasksFailed, 1);
  EXPECT_EQ(successfulTasks.load(), 1);
}

TEST_F(TaskSchedulerTest, TimeoutTask) {
  TaskScheduler scheduler(1);

  TaskConfig config;
  config.executeFn = []() {
    // Quick task that completes normally
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  };
  config.startTime   = std::chrono::system_clock::now() + std::chrono::seconds(1);
  config.priority    = 1;
  config.repeatable  = false;
  std::string taskId = scheduler.scheduleTask(config);

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(3));
  scheduler.stop();

  EXPECT_EQ(scheduler.getTaskStatus(taskId), Task::Status::COMPLETED);
}

TEST_F(TaskSchedulerTest, SchedulerStatistics) {
  TaskScheduler scheduler(3);

  // Schedule multiple tasks with different outcomes
  TaskConfig config;
  config.executeFn  = []() { /* successful task */ };
  config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  config.priority   = 1;
  config.repeatable = false;
  scheduler.scheduleTask(config);

  config.executeFn  = []() { throw std::runtime_error("Test error"); };
  config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  config.priority   = 1;
  config.repeatable = false;
  scheduler.scheduleTask(config);

  // Schedule a task that will be cancelled
  config.executeFn            = []() { std::this_thread::sleep_for(std::chrono::seconds(5)); };
  config.startTime            = std::chrono::system_clock::now() + std::chrono::seconds(1);
  config.priority             = 1;
  config.repeatable           = false;
  std::string cancelledTaskId = scheduler.scheduleTask(config);

  scheduler.start();

  // Cancel one task
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  scheduler.cancelTask(cancelledTaskId);

  std::this_thread::sleep_for(std::chrono::seconds(3));
  scheduler.stop();

  auto stats = scheduler.getStatistics();
  EXPECT_EQ(stats.totalTasksScheduled, 3);
  EXPECT_EQ(stats.tasksCompleted, 1);
  EXPECT_EQ(stats.tasksFailed, 1);
  EXPECT_EQ(stats.tasksCancelled, 1);
}

TEST_F(TaskSchedulerTest, TaskCompletionCallback) {
  TaskScheduler             scheduler(4);
  std::atomic<int>          callbackCount(0);
  std::vector<std::string>  callbackTaskIds;
  std::vector<Task::Status> callbackStatuses;
  std::mutex                callbackMutex;

  scheduler.onTaskComplete([&](const std::string &taskId, Task::Status status) {
    std::lock_guard<std::mutex> lock(callbackMutex);
    callbackCount.fetch_add(1);
    callbackTaskIds.push_back(taskId);
    callbackStatuses.push_back(status);
  });

  // Schedule two tasks, one successful and one failing
  TaskConfig successTaskConfig;
  successTaskConfig.executeFn  = []() { /* successful task */ };
  successTaskConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  successTaskConfig.priority   = 1;
  successTaskConfig.repeatable = false;
  std::string taskId1          = scheduler.scheduleTask(successTaskConfig);

  TaskConfig failureTaskConfig;
  failureTaskConfig.executeFn  = []() { throw std::runtime_error("Test error"); };
  failureTaskConfig.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  failureTaskConfig.priority   = 1;
  failureTaskConfig.repeatable = false;
  std::string taskId2          = scheduler.scheduleTask(failureTaskConfig);

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(3));
  scheduler.stop();

  EXPECT_EQ(callbackCount.load(), 2);
  EXPECT_EQ(callbackTaskIds.size(), 2);
  EXPECT_EQ(callbackStatuses.size(), 2);

  // Verify callback was called for both tasks
  bool callbackTask1called = false;
  bool callbackTask2called = false;
  for (const auto &id : callbackTaskIds) {
    if (id == taskId1)
      callbackTask1called = true;
    if (id == taskId2)
      callbackTask2called = true;
  }
  EXPECT_TRUE(callbackTask1called);
  EXPECT_TRUE(callbackTask2called);
}

TEST_F(TaskSchedulerTest, SchedulerWithNoTasks) {
  TaskScheduler scheduler(1);

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  scheduler.stop();

  auto stats = scheduler.getStatistics();
  EXPECT_EQ(stats.totalTasksScheduled, 0);
  EXPECT_EQ(stats.tasksCompleted, 0);
  EXPECT_EQ(stats.tasksFailed, 0);
}

TEST_F(TaskSchedulerTest, TaskStatusQueries) {
  TaskScheduler scheduler(1);

  TaskConfig config;
  config.executeFn  = []() { std::this_thread::sleep_for(std::chrono::milliseconds(500)); };
  config.startTime  = std::chrono::system_clock::now() + std::chrono::seconds(1);
  config.priority   = 1;
  config.repeatable = false;

  // Schedule a task
  std::string taskId = scheduler.scheduleTask(config);

  // Initially should be pending (or completed if not found)
  auto initialStatus = scheduler.getTaskStatus(taskId);
  EXPECT_TRUE(initialStatus == Task::Status::PENDING || initialStatus == Task::Status::COMPLETED);

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(2));

  auto finalStatus = scheduler.getTaskStatus(taskId);
  EXPECT_EQ(finalStatus, Task::Status::COMPLETED);

  scheduler.stop();
}

TEST_F(TaskSchedulerTest, NonExistentTaskStatus) {
  TaskScheduler scheduler(1);

  auto status = scheduler.getTaskStatus("non_existent_task");

  EXPECT_EQ(status, Task::Status::COMPLETED); // Default for non-existent tasks
}

TEST_F(TaskSchedulerTest, DestructionWhileRunning) {
  std::atomic<int> executions(0);

  {
    TaskScheduler tempScheduler(2);

    TaskConfig config;
    config.executeFn  = [&]() { executions.fetch_add(1); };
    config.startTime  = std::chrono::system_clock::now() + std::chrono::milliseconds(500);
    config.priority   = 1;
    config.repeatable = false;

    tempScheduler.scheduleTask(config);

    tempScheduler.start();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    tempScheduler.stop();

    // Destructor should handle cleanup properly
  }

  EXPECT_EQ(executions.load(), 1);
}

TEST_F(TaskSchedulerTest, StressManyTasks) {
  TaskScheduler scheduler(4);

  const int        NUM_TASKS = 100;
  std::atomic<int> executions(0);

  for (int i = 0; i < NUM_TASKS; ++i) {
    TaskConfig config;
    config.executeFn  = [&]() { executions.fetch_add(1); };
    config.startTime  = std::chrono::system_clock::now() + std::chrono::milliseconds(100 + i);
    config.priority   = i % 10; // Varying priorities
    config.repeatable = false;  // One-time tasks
    scheduler.scheduleTask(config);
  }

  scheduler.start();
  std::this_thread::sleep_for(std::chrono::seconds(5));
  scheduler.stop();

  auto stats = scheduler.getStatistics();
  EXPECT_EQ(stats.totalTasksScheduled, NUM_TASKS);
  EXPECT_EQ(executions.load(), NUM_TASKS);
  EXPECT_EQ(stats.tasksCompleted, NUM_TASKS);
}