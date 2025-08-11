#ifndef TASKSCHEDX_LOGGER_H
#define TASKSCHEDX_LOGGER_H

#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace TaskSchedX {

/**
 * @class Logger
 * @brief Thread-safe singleton logger with configurable output and log levels
 *
 * The Logger class provides a centralized logging system with support for multiple
 * log levels, file output, console output, and thread-safe operation. It implements
 * the singleton pattern to ensure consistent logging across the entire application.
 *
 * Key features:
 * - Singleton pattern for global access
 * - Multiple log levels (DEBUG, INFO, WARN, ERROR, NONE)
 * - Configurable console and file output
 * - Thread-safe logging operations
 * - Formatted log messages with level indicators
 *
 * Log levels hierarchy (from lowest to highest):
 * DEBUG < INFO < WARN < ERROR < NONE
 *
 * Messages are only logged if their level is >= the current log level.
 *
 */
class Logger {
public:
  /**
   * @enum Level
   * @brief Defines the available logging levels
   *
   * Log levels control which messages are actually output. Messages with
   * levels below the current threshold are ignored.
   */
  enum class Level {
    DEBUG, ///< Detailed debugging information (lowest priority)
    INFO,  ///< General informational messages
    WARN,  ///< Warning messages for potentially problematic situations
    ERROR, ///< Error messages for serious problems
    NONE   ///< Disables all logging output (highest priority)
  };

  static Logger &getInstance();

  void setLevel(Level level);

  void enableConsoleOutput(bool enable);

  void debug(const std::string &message);

  void info(const std::string &message);

  void warn(const std::string &message);

  void error(const std::string &message);

private:
  /**
   * @brief Constructor for singleton pattern
   *
   * Initializes the logger with default settings (NONE level, console disabled).
   */
  Logger() : currentLevel(Level::NONE), consoleOutput(false) {};

  /**
   * @brief Destructor
   *
   */
  ~Logger() {};

  void log(Level level, const std::string &message);

  std::string getCurrentTimestamp();

  std::string levelToString(Level level);

  /** @brief Current minimum log level */
  Level currentLevel;
  /** @brief Whether console output is enabled */
  bool consoleOutput;
  /** @brief  Mutex for thread-safe logging operations */
  std::mutex logMutex;
};

/**
 * @def LOGGER
 * @brief Convenient macro for accessing the Logger singleton instance
 *
 * This macro provides easy access to the Logger singleton without having
 * to call Logger::getInstance() repeatedly. Use this for all logging operations.
 *
 * @code
 * LOGGER.info("Application started");
 * LOGGER.error("Failed to open file: " + filename);
 * @endcode
 */
#define LOGGER Logger::getInstance()

} // namespace TaskSchedX

#endif // TASKSCHEDX_LOGGER_H