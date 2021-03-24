//
//  Logger.cpp
//  YAD
//

#include "Logger.h"
#include <string>
#include <stdio.h>
#ifdef __APPLE__
#include "Log_IOS.h"
#endif

#define MSG_BUF_SIZE 1024

static const char *kLogLevels[] = { "V", "D", "I", "W", "E", "F", NULL };

namespace YAD {

Logger::Logger() :
    mLogHander(NULL),
    mLogLevel(LOG_LEVEL_VERBOSE)
{
    
}

void Logger::setHandler(LogFunc cb)
{
    mLogHander = cb;
}

void Logger::setLevel(LogLevel level)
{
    mLogLevel = level;
}

void Logger::log(LogLevel level, const char *tag, const char *file, int line, const char *function, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    log(level, tag, file, line, function, format, args);
    va_end(args);
}

void Logger::log(LogLevel level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args)
{
    if (level < LOG_LEVEL_VERBOSE || level > LOG_LEVEL_FAULT) {
        return;
    }
    
    if (level < mLogLevel) {
        return;
    }
    
    if (mLogHander) {
        mLogHander(level, tag, file, line, function, format, args);
    } else {
        logDefault(level, tag, file, line, function, format, args);
    }
}

void Logger::logDefault(LogLevel level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args)
{
    // 日志级别和TAG
    std::string message;
    message.append(std::string(kLogLevels[level]) + "/" + std::string(tag) + ": ");

    // 模仿Android的输出
    file = getLastFilePathComponent(file);
    std::string file_name = std::string(file);
    std::string function_name = std::string(function);
    if (!file_name.empty() || !function_name.empty()) {
        if (!file_name.empty()) {
            message.append(file_name + ":" + std::to_string(line) + ":");
        }
        if (!function_name.empty()) {
            message.append(function_name + ":");
        }
        message.append(" "); // 最后加个空格
    }
    
    char s[MSG_BUF_SIZE];
    if (format) {
        vsnprintf(s, MSG_BUF_SIZE, format, args);
    }
    message.append(s);
    
#ifdef __APPLE__
    // 便于Logger可以跨平台，需要单独封装NSLog
    YAD::logImpl(message.c_str());
#else
    fprintf(stderr, "%s\n", message.c_str());
    fflush(stderr);
#endif
}

const char *Logger::getLastFilePathComponent(const char *file)
{
    const char *ptr = file;
    for (ptr += strlen(file) - 1; *ptr != '/' && ptr > file; --ptr);
    ++ptr;
    return ptr;
}

YAD_SINGLETON_STATIC_INSTANCE(Logger)

} // namespace YAD
