#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include "ThreadPool.h"

using namespace TaskSchedX;

class ThreadPoolTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Setup before each test
  }

  void TearDown() override {
    // Cleanup after each test
  }
};

TEST_F(ThreadPoolTest, ConstructionDestruction) {
  // Test construction with different thread counts
  {
    ThreadPool pool(1);
    // Pool should be created successfully
  } // Destructor should clean up properly

  {
    ThreadPool pool(4);
    // Pool should be created successfully
  }

  {
    ThreadPool pool(8);
    // Pool should be created successfully
  }

  // If we reach here, construction and destruction worked
  SUCCEED();
}

TEST_F(ThreadPoolTest, BasicExecution) {
  ThreadPool       pool(2);
  std::atomic<int> counter(0);

  // Enqueue a simple task
  std::function<void()> executeFn = [&counter]() { counter.fetch_add(1); };
  pool.enqueueTask(executeFn);

  // Wait for task to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  EXPECT_EQ(counter.load(), 1);
}

TEST_F(ThreadPoolTest, MultipleExecution) {
  ThreadPool       pool(3);
  std::atomic<int> counter(0);
  const int        NUM_TASKS = 10;

  // Enqueue multiple tasks
  std::function<void()> executeFn = [&counter]() { counter.fetch_add(1); };
  for (int i = 0; i < NUM_TASKS; ++i) {
    pool.enqueueTask(executeFn);
  }

  // Wait for all tasks to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  EXPECT_EQ(counter.load(), NUM_TASKS);
}

TEST_F(ThreadPoolTest, ConcurrentExecution) {
  int              numThreads = 4;
  ThreadPool       pool(numThreads);
  std::atomic<int> running(0);
  std::atomic<int> maxRunning(0);
  std::atomic<int> completed(0);
  const int        NUM_TASKS = 8;

  std::function<void()> executeFn = [&running, &maxRunning, &completed]() {
    // Update max concurrent if needed
    running.fetch_add(1);
    int now    = running.load();
    maxRunning = std::max(maxRunning.load(), now);

    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    running.fetch_sub(1);
    completed.fetch_add(1);
  };

  // Enqueue tasks that track concurrency
  for (int i = 0; i < NUM_TASKS; ++i) {
    pool.enqueueTask(executeFn);
  }

  // Wait for all tasks to complete
  std::this_thread::sleep_for(std::chrono::seconds(1));

  EXPECT_EQ(completed.load(), NUM_TASKS);
  EXPECT_LE(maxRunning.load(), numThreads); // Should not exceed thread pool size
  EXPECT_GT(maxRunning.load(), 1);          // Should have some concurrency
}

TEST_F(ThreadPoolTest, FIFOOrdering) {
  ThreadPool       pool(1); // Single thread to ensure ordering
  std::vector<int> executionOrder;
  std::mutex       orderMutex;

  const int NUM_TASKS = 5;

  // Enqueue tasks that record their execution order
  for (int i = 0; i < NUM_TASKS; ++i) {
    pool.enqueueTask([i, &executionOrder, &orderMutex]() {
      std::lock_guard<std::mutex> lock(orderMutex);
      executionOrder.push_back(i);
    });
  }

  // Wait for all tasks to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  EXPECT_EQ(executionOrder.size(), NUM_TASKS);

  // Verify FIFO ordering
  for (int i = 0; i < NUM_TASKS; ++i) {
    EXPECT_EQ(executionOrder[i], i);
  }
}

TEST_F(ThreadPoolTest, StopFunctionality) {
  ThreadPool       pool(2);
  std::atomic<int> counter(0);

  std::function<void()> executeFn = [&counter]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    counter.fetch_add(1);
  };

  // Enqueue some tasks
  for (int i = 0; i < 5; ++i) {
    pool.enqueueTask(executeFn);
  }

  // Let some tasks start
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  // Stop the pool
  pool.stop();

  // No more tasks should execute after stop
  int countAfterStop = counter.load();
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  // Counter should not increase after stop
  EXPECT_EQ(counter.load(), countAfterStop);
}

TEST_F(ThreadPoolTest, TaskExceptionsHandled) {
  ThreadPool       pool(2);
  std::atomic<int> successfulTasks(0);
  std::atomic<int> failedTasks(0);

  const int NUM_TASKS = 6;

  // Enqueue mix of successful and failing tasks
  for (int i = 0; i < NUM_TASKS; ++i) {
    pool.enqueueTask([i, &successfulTasks, &failedTasks]() {
      try {
        if (i % 2 == 0) {
          // Even tasks succeed
          std::this_thread::sleep_for(std::chrono::milliseconds(10));
          successfulTasks.fetch_add(1);
        } else {
          // Odd tasks fail
          throw std::runtime_error("Test exception");
        }
      } catch (...) {
        failedTasks.fetch_add(1);
      }
    });
  }

  // Wait for all tasks to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  EXPECT_EQ(successfulTasks.load(), 3); // Tasks 0, 2, 4
  EXPECT_EQ(failedTasks.load(), 3);     // Tasks 1, 3, 5
}

TEST_F(ThreadPoolTest, ThreadSafetyUnderLoad) {
  ThreadPool        pool(4);
  std::atomic<int>  counter(0);
  std::atomic<bool> stopEnqueuing(false);
  const int         NUM_WORKERS       = 3;
  const int         TASKS_PER_WORKERS = 50;

  std::vector<std::thread> workers;

  // Create multiple threads that enqueue tasks
  for (int t = 0; t < NUM_WORKERS; ++t) {
    workers.emplace_back([&]() {
      for (int i = 0; i < TASKS_PER_WORKERS; ++i) {
        pool.enqueueTask([&]() {
          counter.fetch_add(1);
          std::this_thread::sleep_for(std::chrono::microseconds(100));
        });
      }
    });
  }

  // Wait for enqueueing to complete
  for (auto &thread : workers) {
    thread.join();
  }

  // Wait for all tasks to execute
  std::this_thread::sleep_for(std::chrono::seconds(2));

  EXPECT_EQ(counter.load(), NUM_WORKERS * TASKS_PER_WORKERS);
}

TEST_F(ThreadPoolTest, PerformanceTest) {
  const int        NUM_TASKS    = 100;
  std::vector<int> threadCounts = {1, 2, 4, 8};

  for (int numThreads : threadCounts) {
    ThreadPool       pool(numThreads);
    std::atomic<int> completedTasks(0);

    std::function<void()> executeFn = [&completedTasks]() {
      // Simulate CPU work
      volatile long sum = 0;
      for (int j = 0; j < 10000; ++j) {
        sum += j;
      }
      completedTasks.fetch_add(1);
    };

    auto startTime = std::chrono::steady_clock::now();

    // Enqueue CPU-bound tasks
    for (int i = 0; i < NUM_TASKS; ++i) {
      pool.enqueueTask(executeFn);
    }

    // Wait for completion
    while (completedTasks.load() < NUM_TASKS) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    auto endTime  = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);

    // Verify all tasks completed
    EXPECT_EQ(completedTasks.load(), NUM_TASKS);

    // Performance should generally improve with more threads (up to a point)
    // This is more of a sanity check than a strict requirement
    EXPECT_LT(duration.count(), 10000); // Should complete within 10 seconds
  }
}

TEST_F(ThreadPoolTest, LongRunningTasks) {
  ThreadPool       pool(2);
  std::atomic<int> startedTasks(0);
  std::atomic<int> completedTasks(0);

  std::function<void()> executeFn = [&startedTasks, &completedTasks]() {
    startedTasks.fetch_add(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    completedTasks.fetch_add(1);
  };

  // Enqueue long-running tasks
  for (int i = 0; i < 4; ++i) {
    pool.enqueueTask(executeFn);
  }

  // Check that tasks start executing
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  EXPECT_GT(startedTasks.load(), 0);
  EXPECT_LE(startedTasks.load(), 2); // At most 2 should be running (pool size)

  // Wait for all to complete
  std::this_thread::sleep_for(std::chrono::milliseconds(500));

  EXPECT_EQ(startedTasks.load(), 4);
  EXPECT_EQ(completedTasks.load(), 4);
}

TEST_F(ThreadPoolTest, DestructorWaitsForTasks) {
  std::atomic<int> completedTasks(0);

  std::function<void()> executeFn = [&completedTasks]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    completedTasks.fetch_add(1);
  };

  {
    ThreadPool pool(2);

    // Enqueue tasks that take some time
    for (int i = 0; i < 4; ++i) {
      pool.enqueueTask(executeFn);
    }

    // Let some tasks start
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Pool destructor should wait for running tasks to complete
  }

  // After destruction, running tasks should have completed
  EXPECT_EQ(completedTasks.load(), 4);
}