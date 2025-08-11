#include "Logger.h"
#include <gtest/gtest.h>
#include <sstream>
#include <iostream>
#include <regex>

using namespace TaskSchedX;

// Helper function to capture console output
std::string captureConsoleOutput(const std::function<void()> &func) {
  std::ostringstream buffer;
  std::streambuf    *original = std::cout.rdbuf();
  std::cout.rdbuf(buffer.rdbuf());

  func();

  std::cout.rdbuf(original);
  return buffer.str();
}

class LoggerTest : public ::testing::Test {
protected:
  void SetUp() override {
    LOGGER.setLevel(Logger::Level::DEBUG);
    LOGGER.enableConsoleOutput(true);
  }
  void TearDown() override {
    LOGGER.setLevel(Logger::Level::NONE);
    LOGGER.enableConsoleOutput(false);
  }
};

TEST_F(LoggerTest, LoggerSingleton) {
  Logger &logger1 = LOGGER;
  Logger &logger2 = LOGGER;
  EXPECT_EQ(&logger1, &logger2);
}

TEST_F(LoggerTest, LogLevelFiltering) {
  LOGGER.setLevel(Logger::Level::DEBUG);
  std::string output = captureConsoleOutput([]() {
    LOGGER.debug("Debug message");
    LOGGER.info("Info message");
    LOGGER.warn("Warn message");
    LOGGER.error("Error message");
  });

  EXPECT_NE(output.find("Debug message"), std::string::npos);
  EXPECT_NE(output.find("Info message"), std::string::npos);
  EXPECT_NE(output.find("Warn message"), std::string::npos);
  EXPECT_NE(output.find("Error message"), std::string::npos);

  LOGGER.setLevel(Logger::Level::WARN);
  output = captureConsoleOutput([]() {
    LOGGER.debug("Hidden debug");
    LOGGER.info("Hidden info");
    LOGGER.warn("Visible warn");
    LOGGER.error("Visible error");
  });

  EXPECT_EQ(output.find("Hidden debug"), std::string::npos);
  EXPECT_EQ(output.find("Hidden info"), std::string::npos);
  EXPECT_NE(output.find("Visible warn"), std::string::npos);
  EXPECT_NE(output.find("Visible error"), std::string::npos);
}

TEST_F(LoggerTest, ConsoleLoggingToggle) {
  LOGGER.setLevel(Logger::Level::INFO);

  LOGGER.enableConsoleOutput(true);
  std::string out1 = captureConsoleOutput([]() { LOGGER.info("Visible message"); });
  EXPECT_NE(out1.find("Visible message"), std::string::npos);

  LOGGER.enableConsoleOutput(false);
  std::string out2 = captureConsoleOutput([]() { LOGGER.info("Invisible message"); });
  EXPECT_EQ(out2.find("Invisible message"), std::string::npos);
}

TEST_F(LoggerTest, LogMessageFormatting) {
  LOGGER.setLevel(Logger::Level::INFO);
  std::string output = captureConsoleOutput([]() { LOGGER.info("Format test message"); });

  std::regex pattern(R"(\[INFO\].*Format test message)");

  EXPECT_TRUE(std::regex_search(output, pattern));
}