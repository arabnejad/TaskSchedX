#include "Logger.h"

namespace TaskSchedX {

/**
 * @brief Gets the singleton Logger instance
 *
 * Returns a reference to the single Logger instance, creating it
 * if it doesn't exist. This method is thread-safe.
 *
 * @return Reference to the singleton Logger instance
 *
 * @note Use the LOGGER macro instead of calling this directly
 * @code
 * LOGGER.info("Application started");
 * LOGGER.error("Failed to open file: " + filename);
 * @endcode
 *
 */
Logger &Logger::getInstance() {
  static Logger instance;
  return instance;
}

/**
 * @brief Sets the minimum log level for output
 *
 * Only messages with levels greater than or equal to the specified
 * level will be output. Lower priority messages are ignored.
 * This allows runtime control of logging verbosity.
 *
 * @param level The minimum log level to output
 *
 * @note Setting to NONE disables all logging
 */
void Logger::setLevel(Level level) {
  // Acquire exclusive lock to ensure thread-safe modification
  // of the current log level setting
  std::lock_guard<std::mutex> lock(logMutex);
  // Update the minimum log level threshold
  // Messages below this level will be filtered out
  currentLevel = level;
}

/**
 * @brief Enables or disables console output for log messages
 *
 * Controls whether log messages are written to standard output.
 * Console output can be used independently of or in addition to file output.
 *
 * @param enable true to enable console output, false to disable
 *
 */
void Logger::enableConsoleOutput(bool enable) {
  // Acquire exclusive lock for thread-safe modification
  std::lock_guard<std::mutex> lock(logMutex);
  // Update the console output flag
  consoleOutput = enable;
}

/**
 * @brief Logs a message at DEBUG level
 *
 * Debug messages are typically used for detailed diagnostic information
 * during development and troubleshooting.
 *
 * @param message The message to log
 *
 * @note Only shows if current log level is DEBUG or lower
 */
void Logger::debug(const std::string &message) {
  log(Level::DEBUG, message);
}

/**
 * @brief Logs an informational message
 *
 * Shows a message at INFO level. Info messages are used for
 * general application flow and status information.
 *
 * @param message The message to log
 *
 * @note Only show if current log level is INFO or lower
 */
void Logger::info(const std::string &message) {
  log(Level::INFO, message);
}

/**
 * @brief Logs a warning message
 *
 * Shows a message at WARN level. Warning messages indicate
 * potentially problematic situations that don't prevent operation.
 *
 * @param message The message to log
 *
 * @note Only show if current log level is WARN or lower
 */
void Logger::warn(const std::string &message) {
  log(Level::WARN, message);
}

/**
 * @brief Logs an error message
 *
 * Shows a message at ERROR level. Error messages indicate
 * serious problems that may affect application functionality.
 *
 * @param message The message to log
 *
 * @note Only show if current log level is ERROR or lower
 */
void Logger::error(const std::string &message) {
  log(Level::ERROR, message);
}

/**
 * @brief Internal logging method that handles actual output
 *
 * Performs the actual logging work including level checking, timestamp
 * generation, message formatting, and output to console.
 *
 * @param level The log level of the message
 * @param message The message content to log
 *
 */
void Logger::log(Level level, const std::string &message) {
  // First check if this message should be output based on current log level
  // Messages with levels below the current threshold are ignored
  if (level < currentLevel) {
    return; // Skip logging this message
  }

  // Format the complete log message with timestamp and level
  // Format: "[TaskSchedX][YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] message"
  std::string logMessage = "[TaskSchedX][" + getCurrentTimestamp() + "][" + levelToString(level) + "] " + message;

  // Output to console if console output is enabled
  if (consoleOutput) {
    // Acquire exclusive lock for thread-safe logging operations
    // This ensures that log messages from different threads don't interleave
    std::lock_guard<std::mutex> lock(logMutex);

    std::cout << logMessage << std::endl;
  }
}

/**
 * @brief Generates a formatted timestamp string with millisecond precision
 *
 * Creates a timestamp string in the format "YYYY-MM-DD HH:MM:SS.mmm"
 * using the current system time with millisecond precision.
 *
 * @return Formatted timestamp string
 */
std::string Logger::getCurrentTimestamp() {
  // Get the current time point from the system clock
  auto now = std::chrono::system_clock::now();

  // Convert to time_t for standard time formatting functions
  auto time_t = std::chrono::system_clock::to_time_t(now);

  // Extract milliseconds component from the time point
  // This gives us sub-second precision for the timestamp
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

  // Format the date and time portion using standard library functions
  // This produces "YYYY-MM-DD HH:MM:SS" format
  std::ostringstream oss;
  oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");

  // Append the milliseconds with proper zero-padding
  // This produces ".mmm" format (e.g., ".001", ".123", ".999")
  oss << '.' << std::setfill('0') << std::setw(3) << ms.count();

  // Return the complete formatted timestamp string
  return oss.str();
}

/**
 * @brief Converts a log level enum to its string representation
 *
 * Returns a human-readable string representation of the log level
 * for use in formatted log messages.
 *
 * @param level The log level to convert
 * @return String representation of the level (e.g., "INFO", "ERROR")
 */
std::string Logger::levelToString(Level level) {
  switch (level) {
  case Level::DEBUG:
    return "DEBUG";
  case Level::INFO:
    return "INFO";
  case Level::WARN:
    return "WARN";
  case Level::ERROR:
    return "ERROR";
  case Level::NONE:
    return "NONE";
  default:
    return "UNKNOWN";
  }
}
} // namespace TaskSchedX
