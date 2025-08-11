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

  Task createTestTask(int priority, int delaySeconds = 0) {
    auto startTime = std::chrono::system_clock::now() + std::chrono::seconds(delaySeconds);
    return Task([]() {}, startTime, priority);
  }
};

TEST_F(TaskQueueTest, BasicOperations) {
  EXPECT_TRUE(queue.isEmpty());
  EXPECT_EQ(queue.size(), 0);

  Task task = createTestTask(1);
  queue.addTask(task);

  EXPECT_FALSE(queue.isEmpty());
  EXPECT_EQ(queue.size(), 1);

  Task retrieved = queue.getTask();

  EXPECT_TRUE(queue.isEmpty());
  EXPECT_EQ(queue.size(), 0);
  EXPECT_EQ(retrieved.priority, task.priority);
  EXPECT_EQ(retrieved.getId(), task.getId());
}

TEST_F(TaskQueueTest, PriorityOrder) {
  // Add tasks in reverse priority order
  queue.addTask(createTestTask(10)); // Lowest priority
  queue.addTask(createTestTask(1));  // Highest priority
  queue.addTask(createTestTask(5));  // Medium priority

  EXPECT_EQ(queue.size(), 3);

  // Should retrieve in priority order (1, 5, 10)
  Task first = queue.getTask();
  EXPECT_EQ(first.priority, 1);

  Task second = queue.getTask();
  EXPECT_EQ(second.priority, 5);

  Task third = queue.getTask();
  EXPECT_EQ(third.priority, 10);

  EXPECT_TRUE(queue.isEmpty());
}

// Test time-based ordering for same priority
TEST_F(TaskQueueTest, TimeOrderSamePriority) {
  auto baseTime = std::chrono::system_clock::now();

  // Create tasks with same priority but different execution times
  Task laterTask([]() {}, baseTime + std::chrono::seconds(2), 5);
  Task earlierTask([]() {}, baseTime + std::chrono::seconds(1), 5);

  // Add in reverse time order
  queue.addTask(laterTask);
  queue.addTask(earlierTask);

  // Should retrieve earlier task first
  Task first = queue.getTask();
  EXPECT_EQ(first.startTime, earlierTask.startTime);

  Task second = queue.getTask();
  EXPECT_EQ(second.startTime, laterTask.startTime);
}

TEST_F(TaskQueueTest, CancelById) {
  Task task1 = createTestTask(1);
  Task task2 = createTestTask(2);
  Task task3 = createTestTask(3);

  queue.addTask(task1);
  queue.addTask(task2);
  queue.addTask(task3);

  EXPECT_EQ(queue.size(), 3);

  // Cancel middle task
  bool cancelled = queue.cancelTask(task2.getId());

  EXPECT_TRUE(cancelled);
  EXPECT_EQ(queue.size(), 2);

  // Verify remaining tasks
  Task first  = queue.getTask();
  Task second = queue.getTask();

  EXPECT_TRUE((first.getId() == task1.getId() && second.getId() == task3.getId()) ||
              (first.getId() == task3.getId() && second.getId() == task1.getId()));
}

// Test cancellation of non-existent task
TEST_F(TaskQueueTest, CancelNonExistentTask) {
  Task task = createTestTask(1);
  queue.addTask(task);

  bool cancelled = queue.cancelTask("non_existent_id");

  EXPECT_FALSE(cancelled);
  EXPECT_EQ(queue.size(), 1); // Original task should still be there
}

TEST_F(TaskQueueTest, ClearQueue) {
  // Add multiple tasks
  for (int i = 1; i <= 5; ++i) {
    queue.addTask(createTestTask(i));
  }

  EXPECT_EQ(queue.size(), 5);
  EXPECT_FALSE(queue.isEmpty());

  queue.clear();

  EXPECT_EQ(queue.size(), 0);
  EXPECT_TRUE(queue.isEmpty());
}

TEST_F(TaskQueueTest, ThreadSafety) {
  constexpr int NUM_THREADS      = 4;
  constexpr int TASKS_PER_THREAD = 25;

  std::atomic<int> tasksAdded(0);
  std::atomic<int> tasksRetrieved(0);

  // Producer threads
  std::vector<std::thread> producers;
  for (int t = 0; t < NUM_THREADS; ++t) {
    producers.emplace_back([&]() {
      for (int i = 0; i < TASKS_PER_THREAD; ++i) {
        queue.addTask(createTestTask(i));
        tasksAdded.fetch_add(1);
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // do some work
      }
    });
  }

  // Wait for all producers to finish
  for (auto &t : producers)
    t.join();

  EXPECT_EQ(tasksAdded.load(), NUM_THREADS * TASKS_PER_THREAD);

  // Consumer threads
  std::vector<std::thread> consumers;
  for (int t = 0; t < NUM_THREADS; ++t) {
    consumers.emplace_back([&]() {
      while (tasksRetrieved.load() < NUM_THREADS * TASKS_PER_THREAD) {
        if (!queue.isEmpty()) {
          auto task = queue.getTask();
          tasksRetrieved.fetch_add(1);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(100)); // do some work
      }
    });
  }

  // Wait for all consumers to finish
  for (auto &t : consumers)
    t.join();

  EXPECT_EQ(tasksRetrieved.load(), NUM_THREADS * TASKS_PER_THREAD);
  EXPECT_TRUE(queue.isEmpty());
}

TEST_F(TaskQueueTest, SizeTracking) {
  EXPECT_EQ(queue.size(), 0);

  // Add tasks one by one
  for (int i = 1; i <= 10; ++i) {
    queue.addTask(createTestTask(i));
    EXPECT_EQ(queue.size(), i);
  }

  // Remove tasks one by one
  for (int i = 10; i >= 1; --i) {
    EXPECT_EQ(queue.size(), i);
    Task task = queue.getTask();
    EXPECT_EQ(queue.size(), i - 1);
  }

  EXPECT_EQ(queue.size(), 0);
  EXPECT_TRUE(queue.isEmpty());
}

TEST_F(TaskQueueTest, LargeScaleQueue) {
  const int LARGE_NUMBER = 1000;

  // Add many tasks
  for (int i = 0; i < LARGE_NUMBER; ++i) {
    queue.addTask(createTestTask(i % 10)); // Priorities 0-9
  }

  EXPECT_EQ(queue.size(), LARGE_NUMBER);

  // Retrieve all tasks and verify priority ordering
  int lastPriority = -1;
  for (int i = 0; i < LARGE_NUMBER; ++i) {
    Task task = queue.getTask();
    EXPECT_GE(task.priority, lastPriority); // Should be in non-decreasing priority order
    lastPriority = task.priority;
  }

  EXPECT_TRUE(queue.isEmpty());
}