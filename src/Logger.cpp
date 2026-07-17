#include "Logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ratelimiter {

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::info(const std::string& message) { log("INFO", message); }
void Logger::warn(const std::string& message) { log("WARN", message); }
void Logger::error(const std::string& message) { log("ERROR", message); }

void Logger::log(const char* level, const std::string& message) {
    using namespace std::chrono;

    const auto now = system_clock::now();
    const auto nowTimeT = system_clock::to_time_t(now);
    const auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

    std::tm localTime{};
#if defined(_WIN32)
    localtime_s(&localTime, &nowTimeT);
#else
    localtime_r(&nowTimeT, &localTime);
#endif

    std::ostringstream timestamp;
    timestamp << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S")
               << '.' << std::setfill('0') << std::setw(3) << ms.count();

    // Guard std::cout with a mutex since multiple worker threads log concurrently.
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << "[" << timestamp.str() << "] [" << level << "] " << message << std::endl;
}

}  // namespace ratelimiter
