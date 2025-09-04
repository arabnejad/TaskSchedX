#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <set>
#include <vector>
#include "Task.h"

using namespace TaskSchedX;

class TaskTest : public ::testing::Test {
protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(TaskTest, DefaultConstruction) {
  auto                  startTime = std::chrono::system_clock::now() + std::chrono::seconds(1);
  bool                  executed  = false;
  int                   priority  = 5;
  std::function<void()> taskFn = [&executed]() { executed = true; };

  Task task(taskFn, startTime, priority);

  EXPECT_EQ(task.priority, priority);
  EXPECT_EQ(task.startTime, startTime);
  EXPECT_EQ(task.getStatus(), Task::Status::PENDING);
  EXPECT_FALSE(task.isRepeatable);
  EXPECT_EQ(task.repeatEvery, std::chrono::seconds(10));   // Default
  EXPECT_EQ(task.timeout, std::chrono::seconds(60)); // Default
  EXPECT_FALSE(executed);
  EXPECT_FALSE(task.getId().empty());
}

TEST_F(TaskTest, FullConstruction) {
  auto                  startTime        = std::chrono::system_clock::now() + std::chrono::seconds(2);
  bool                  executed         = false;
  std::function<void()> taskFn        = [&executed]() { executed = true; };
  int                   priority         = 3;
  bool                  isRepeatable     = true;
  std::chrono::seconds  repeatEvery   = std::chrono::seconds(5);
  std::chrono::seconds  timeout = std::chrono::seconds(30);

  Task task(taskFn, startTime, priority, isRepeatable, repeatEvery, timeout);

  EXPECT_EQ(task.priority, priority);
  EXPECT_EQ(task.startTime, startTime);
  EXPECT_EQ(task.getStatus(), Task::Status::PENDING);
  EXPECT_TRUE(task.isRepeatable);
  EXPECT_EQ(task.repeatEvery, repeatEvery);
  EXPECT_EQ(task.timeout, timeout);
  EXPECT_FALSE(executed);
}

TEST_F(TaskTest, ExecuteTask) {
  std::atomic<int>      counter(0);
  auto                  startTime = std::chrono::system_clock::now();
  std::function<void()> taskFn = [&counter]() { counter.fetch_add(1); };
  int                   priority  = 1;

  Task task(taskFn, startTime, priority);

  EXPECT_EQ(counter.load(), 0);

  task.execute();

  EXPECT_EQ(counter.load(), 1);
}

TEST_F(TaskTest, ExecuteThrows) {
  auto                  startTime = std::chrono::system_clock::now();
  std::function<void()> taskFn = []() { throw std::runtime_error("Test exception"); };
  int                   priority  = 1;

  Task task(taskFn, startTime, priority);

  EXPECT_THROW(task.execute(), std::runtime_error);
}

TEST_F(TaskTest, StatusTransitions) {
  auto                  startTime = std::chrono::system_clock::now();
  std::function<void()> taskFn = []() {};
  int                   priority  = 1;

  Task task(taskFn, startTime, priority);

  EXPECT_EQ(task.getStatus(), Task::Status::PENDING);

  task.setStatus(Task::Status::RUNNING);
  EXPECT_EQ(task.getStatus(), Task::Status::RUNNING);

  task.setStatus(Task::Status::COMPLETED);
  EXPECT_EQ(task.getStatus(), Task::Status::COMPLETED);

  task.setStatus(Task::Status::FAILED);
  EXPECT_EQ(task.getStatus(), Task::Status::FAILED);

  task.setStatus(Task::Status::CANCELLED);
  EXPECT_EQ(task.getStatus(), Task::Status::CANCELLED);

  task.setStatus(Task::Status::TIMEOUT);
  EXPECT_EQ(task.getStatus(), Task::Status::TIMEOUT);
}

TEST_F(TaskTest, RescheduleWithTime) {
  std::function<void()> taskFn = []() {};
  int                   priority  = 1;
  auto                  startTime = std::chrono::system_clock::now();
  auto                  newTime   = startTime + std::chrono::seconds(5);

  Task task(taskFn, startTime, priority);

  EXPECT_EQ(task.startTime, startTime);

  task.reschedule(newTime);

  EXPECT_EQ(task.startTime, newTime);
}

TEST_F(TaskTest, RescheduleRepeatable) {
  std::function<void()> taskFn      = []() {};
  int                   priority       = 1;
  auto                  startTime      = std::chrono::system_clock::now();
  bool                  isRepeatable   = true;
  std::chrono::seconds  repeatEvery = std::chrono::seconds(3);

  Task task(taskFn, startTime, priority, isRepeatable, repeatEvery);

  auto originalTime = task.startTime;

  // Wait a bit to ensure time difference
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  task.reschedule(); // Should reschedule to now + 3 seconds

  EXPECT_GT(task.startTime, originalTime);
  // The new time should be approximately now + 3 seconds
  auto expectedTime = std::chrono::system_clock::now() + repeatEvery;
  auto timeDiff     = std::chrono::duration_cast<std::chrono::milliseconds>(task.startTime - expectedTime).count();
  EXPECT_LT(std::abs(timeDiff), 100); // Within 100ms tolerance
}

TEST_F(TaskTest, CopyConstructor) {
  std::atomic<int>      counter(0);
  std::function<void()> taskFn        = [&counter]() { counter.fetch_add(1); };
  int                   priority         = 5;
  auto                  startTime        = std::chrono::system_clock::now();
  bool                  isRepeatable     = true;
  std::chrono::seconds  repeatEvery   = std::chrono::seconds(7);
  std::chrono::seconds  timeout = std::chrono::seconds(25);

  Task original(taskFn, startTime, priority, isRepeatable, repeatEvery, timeout);

  original.setStatus(Task::Status::RUNNING);

  Task copy(original);

  EXPECT_EQ(copy.priority, original.priority);
  EXPECT_EQ(copy.startTime, original.startTime);
  EXPECT_EQ(copy.getStatus(), original.getStatus());
  EXPECT_EQ(copy.isRepeatable, original.isRepeatable);
  EXPECT_EQ(copy.repeatEvery, original.repeatEvery);
  EXPECT_EQ(copy.timeout, original.timeout);
  EXPECT_EQ(copy.getId(), original.getId());

  // Test that both can execute
  copy.execute();
  original.execute();
  EXPECT_EQ(counter.load(), 2);
}

TEST_F(TaskTest, MoveConstructor) {
  std::atomic<int>      counter(0);
  std::function<void()> taskFn        = [&counter]() { counter.fetch_add(1); };
  auto                  startTime        = std::chrono::system_clock::now();
  int                   priority         = 3;
  bool                  isRepeatable     = true;
  std::chrono::seconds  repeatEvery   = std::chrono::seconds(4);
  std::chrono::seconds  timeout = std::chrono::seconds(20);

  Task original(taskFn, startTime, priority, isRepeatable, repeatEvery, timeout);

  original.setStatus(Task::Status::COMPLETED);

  std::string originalId = original.getId();

  Task moved(std::move(original));

  EXPECT_EQ(moved.priority, priority);
  EXPECT_EQ(moved.startTime, startTime);
  EXPECT_EQ(moved.getStatus(), Task::Status::COMPLETED);
  EXPECT_EQ(moved.isRepeatable, isRepeatable);
  EXPECT_EQ(moved.repeatEvery, repeatEvery);
  EXPECT_EQ(moved.timeout, timeout);
  EXPECT_EQ(moved.getId(), originalId);

  moved.execute();
  EXPECT_EQ(counter.load(), 1);
}

TEST_F(TaskTest, CopyAssignment) {
  auto startTime1 = std::chrono::system_clock::now();
  auto startTime2 = startTime1 + std::chrono::seconds(1);

  Task task1([]() {}, startTime1, 1);
  Task task2([]() {}, startTime2, 2);
  task1.setStatus(Task::Status::RUNNING);

  task2 = task1;

  EXPECT_EQ(task2.priority, task1.priority);
  EXPECT_EQ(task2.startTime, task1.startTime);
  EXPECT_EQ(task2.getStatus(), task1.getStatus());
  EXPECT_EQ(task2.getId(), task1.getId());
}

TEST_F(TaskTest, MoveAssignment) {
  auto startTime1 = std::chrono::system_clock::now();
  auto startTime2 = startTime1 + std::chrono::seconds(1);

  Task task1([]() {}, startTime1, 1);
  Task task2([]() {}, startTime2, 2);

  task1.setStatus(Task::Status::FAILED);
  std::string originalId = task1.getId();

  task2 = std::move(task1);

  EXPECT_EQ(task2.priority, 1);
  EXPECT_EQ(task2.startTime, startTime1);
  EXPECT_EQ(task2.getStatus(), Task::Status::FAILED);
  EXPECT_EQ(task2.getId(), originalId);
}

TEST_F(TaskTest, ComparisonOperator) {
  auto now = std::chrono::system_clock::now();

  // Test priority comparison
  Task highPriority([]() {}, now, 1);
  Task lowPriority([]() {}, now, 5);

  EXPECT_TRUE(lowPriority > highPriority); // Lower priority number = higher priority
  EXPECT_FALSE(highPriority > lowPriority);

  // Test time comparison with same priority
  Task earlierTime([]() {}, now, 3);
  Task laterTime([]() {}, now + std::chrono::seconds(1), 3);

  EXPECT_TRUE(laterTime > earlierTime); // Later time = lower priority
  EXPECT_FALSE(earlierTime > laterTime);
}

TEST_F(TaskTest, UniqueId) {
  std::vector<Task>     tasks;
  std::set<std::string> uniqueIds;

  // Create multiple tasks and verify unique IDs
  for (int i = 0; i < 10; ++i) {
    tasks.emplace_back([]() {}, std::chrono::system_clock::now(), i);
    uniqueIds.insert(tasks.back().getId());
  }

  // All IDs should be unique
  EXPECT_EQ(uniqueIds.size(), 10);

  // IDs should follow the expected format
  for (const auto &id : uniqueIds) {
    EXPECT_TRUE(id.find("task_") == 0);
    // Task IDs are in the format "task_XXXXXX" ("task_" + 6 digits)
    // where X is a zero-padded number
    EXPECT_EQ(id.length(), 11);
  }
}

TEST_F(TaskTest, StatusThreadSafety) {
  auto startTime = std::chrono::system_clock::now();
  Task task([]() {}, startTime, 1);

  std::atomic<bool> testComplete(false);
  std::atomic<int>  statusChanges(0);

  // Thread 1: Continuously change status
  std::thread writer([&task, &testComplete, &statusChanges]() {
    std::vector<Task::Status> statuses = {Task::Status::PENDING, Task::Status::RUNNING,   Task::Status::COMPLETED,
                                          Task::Status::FAILED,  Task::Status::CANCELLED, Task::Status::TIMEOUT};

    int count = 0;
    while (!testComplete.load()) {
      task.setStatus(statuses[count % statuses.size()]);
      statusChanges.fetch_add(1);
      count++;
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  });

  // Thread 2: Continuously read status
  std::thread reader([&task, &testComplete]() {
    while (!testComplete.load()) {
      volatile auto status = task.getStatus(); // Read status
      (void)status;                            // Suppress unused variable warning
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    }
  });

  // Let threads run for a short time
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  testComplete = true;

  writer.join();
  reader.join();

  // Should have performed many status changes without crashing
  EXPECT_GT(statusChanges.load(), 0);
}