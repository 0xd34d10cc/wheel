#pragma once

#include <functional>
#include <string_view>
#include <format>
#include <iostream>
#include <chrono>


#if !defined(WH_LOG_MAX_ENTRY_LEN)
#define WH_LOG_MAX_ENTRY_LEN 4096
#endif

namespace wh {

using LogFn = std::function<void(std::string_view)>;

enum class LogLevel { Trace, Debug, Info, Warning, Error };

inline LogFn LOGGER = [](std::string_view entry) { std::cout << entry; };
inline std::atomic<LogLevel> LOG_LEVEL = LogLevel::Info;

// NOTE: must be thread-safe
void log_init(LogFn logger) { LOGGER = logger; }
void log_set_level(LogLevel level) { LOG_LEVEL = level; }

// TODO: remove template and use std::vformat_to & std::format_args instead
// TODO: format to lock free ring buffer
template <typename... Args>
void log_log(LogLevel level, const char* fmt, Args&&... args) {
  if (level < LOG_LEVEL.load()) {
    return;
  }

  char buffer[WH_LOG_MAX_ENTRY_LEN];
  static const std::string_view levels[] = {"TRACE", "DEBUG", "INFO", "WARN",
                                            "ERROR"};
  const auto now = std::chrono::utc_clock::now();

  // NOTE: chrono formatter is very slow for some reason
  auto [prefix_end, n] =
      std::format_to_n(buffer, std::size(buffer), "{:%F %T} [{}]: ", now,
                       levels[static_cast<int>(level)]);
  auto [end, _n] = std::format_to_n(prefix_end, std::size(buffer) - n - 2, fmt,
                                    std::forward<Args>(args)...);
  *end++ = '\n';
  *end = '\0';

  LOGGER(std::string_view(buffer, end));
}

}  // namespace wh

#define LOG_TRACE(...) ::wh::log_log(::wh::LogLevel::Trace, __VA_ARGS__)
#define LOG_DEBUG(...) ::wh::log_log(::wh::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...) ::wh::log_log(::wh::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...) ::wh::log_log(::wh::LogLevel::Warning, __VA_ARGS__)
#define LOG_ERROR(...) ::wh::log_log(::wh::LogLevel::Error, __VA_ARGS__)
