#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "TaskQueue.h"

using namespace TaskSchedX;

class TaskQueueTest : public ::testing::Test {
protected:
  TaskQueue queue;

  void SetUp() override {
    // Ensure queue is empty before each test
    queue.clear();
  }

  void TearDown() override {
    // Clean up after each test
    queue.clear();
  }

  std::shared_ptr<Task> createTestTask(int priority, int delaySeconds = 0) {
    auto startTime = std::chrono::system_clock::now() + std::chrono::seconds(delaySeconds);
    return std::make_shared<Task>([]() {}, startTime, priority);
  }
};

TEST_F(TaskQueueTest, BasicOperations) {
  EXPECT_TRUE(queue.isEmpty());
  EXPECT_EQ(queue.size(), 0);

  auto task = createTestTask(1);
  queue.push(task);

  EXPECT_FALSE(queue.isEmpty());
  EXPECT_EQ(queue.size(), 1);

  auto retrieved = queue.pop();

  EXPECT_TRUE(queue.isEmpty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(retrieved->priority, task->priority);
  EXPECT_EQ(retrieved->getId(), task->getId());
}

TEST_F(TaskQueueTest, PriorityOrder) {
  // Add tasks in reverse priority order
  queue.push(createTestTask(10)); // Lowest priority
  queue.push(createTestTask(1));  // Highest priority
  queue.push(createTestTask(5));  // Medium priority

  EXPECT_EQ(queue.size(), 3);

  // Should retrieve in priority order (1, 5, 10)
  auto first = queue.pop();
  EXPECT_EQ(first->priority, 1);

  auto second = queue.pop();
  EXPECT_EQ(second->priority, 5);

  auto third = queue.pop();
  EXPECT_EQ(third->priority, 10);

  EXPECT_TRUE(queue.isEmpty());
}

// Test time-based ordering for same priority
TEST_F(TaskQueueTest, TimeOrderSamePriority) {
  auto baseTime = std::chrono::system_clock::now();

  // Create tasks with same priority but different execution times
  auto laterTask   = std::make_shared<Task>([]() {}, baseTime + std::chrono::seconds(2), 5);
  auto earlierTask = std::make_shared<Task>([]() {}, baseTime + std::chrono::seconds(1), 5);

  // Add in reverse time order
  queue.push(laterTask);
  queue.push(earlierTask);

  // Should retrieve earlier task first
  auto first = queue.pop();
  EXPECT_EQ(first->startTime, earlierTask->startTime);

  auto second = queue.pop();
  EXPECT_EQ(second->startTime, laterTask->startTime);
}

TEST_F(TaskQueueTest, CancelById) {
  auto task1 = createTestTask(1);
  auto task2 = createTestTask(2);
  auto task3 = createTestTask(3);

  queue.push(task1);
  queue.push(task2);
  queue.push(task3);

  EXPECT_EQ(queue.size(), 3);

  // Cancel middle task
  bool cancelled = queue.cancelTask(task2->getId());

  EXPECT_TRUE(cancelled);
  EXPECT_EQ(queue.size(), 2);

  // Verify remaining tasks
  auto first  = queue.pop();
  auto second = queue.pop();

  EXPECT_TRUE((first->getId() == task1->getId() && second->getId() == task3->getId()) ||
              (first->getId() == task3->getId() && second->getId() == task1->getId()));
}

// Test cancellation of non-existent task
TEST_F(TaskQueueTest, CancelNonExistentTask) {
  auto task = createTestTask(1);
  queue.push(task);

  bool cancelled = queue.cancelTask("non_existent_id");

  EXPECT_FALSE(cancelled);
  EXPECT_EQ(queue.size(), 1); // Original task should still be there
}

TEST_F(TaskQueueTest, ClearQueue) {
  // Add multiple tasks
  for (int i = 1; i <= 5; ++i) {
    queue.push(createTestTask(i));
  }

  EXPECT_EQ(queue.size(), 5);
  EXPECT_FALSE(queue.isEmpty());

  queue.clear();

  EXPECT_EQ(queue.size(), 0);
  EXPECT_TRUE(queue.isEmpty());
}

TEST_F(TaskQueueTest, SizeTracking) {
  EXPECT_EQ(queue.size(), 0);

  // Add tasks one by one
  for (int i = 1; i <= 10; ++i) {
    queue.push(createTestTask(i));
    EXPECT_EQ(queue.size(), i);
  }

  // Remove tasks one by one
  for (int i = 10; i >= 1; --i) {
    EXPECT_EQ(queue.size(), i);
    auto task = queue.pop();
    EXPECT_EQ(queue.size(), i - 1);
  }

  EXPECT_EQ(queue.size(), 0);
  EXPECT_TRUE(queue.isEmpty());
}

TEST_F(TaskQueueTest, LargeScaleQueue) {
  const int LARGE_NUMBER = 1000;

  // Add many tasks
  for (int i = 0; i < LARGE_NUMBER; ++i) {
    queue.push(createTestTask(i % 10)); // Priorities 0-9
  }

  EXPECT_EQ(queue.size(), LARGE_NUMBER);

  // Retrieve all tasks and verify priority ordering
  int lastPriority = -1;
  for (int i = 0; i < LARGE_NUMBER; ++i) {
    auto task = queue.pop();
    EXPECT_GE(task->priority, lastPriority); // Should be in non-decreasing priority order
    lastPriority = task->priority;
  }

  EXPECT_TRUE(queue.isEmpty());
}