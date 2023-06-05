//
//  Logger.cpp
//  YAD
//

#include "Logger.h"
#ifdef __APPLE__
#include "Logger_iOS.h"
#endif

#include <stdio.h>
#include <mutex>
#include <memory>
#include <string>

static const char *kLogLevels[] = { "V", "D", "I", "W", "E", "F", NULL };

namespace yad {

// 根据单例释放顺序对称原则(构造: A->B，析构: B->A)，建议创建Logger单例要比创建其它单例要更早，
// 否则别的单例析构函数一旦调用Logger函数，会导致崩溃.
Logger &Logger::getInstance()
{
    static Logger instance;
    return instance;
}

Logger::Logger() :
    log_hander_(NULL),
    log_level_(LOG_LEVEL_VERBOSE)
{
    
}

Logger::~Logger()
{
    
}

void Logger::setHandler(LogFunc cb)
{
    log_hander_ = cb;
}

void Logger::setLevel(LogLevel level)
{
    log_level_ = level;
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
    
    if (level < log_level_) {
        return;
    }
    
    if (log_hander_) {
        log_hander_(level, tag, file, line, function, format, args);
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
    
    char *buffer;
    int bufferSize = vasprintf(&buffer, format, args);
    if (bufferSize < 0) {
        return;
    }
    message.append(buffer);
    
#ifdef __APPLE__
    // 便于Logger可以跨平台，需要单独封装NSLog
    yad::logImpl(message.c_str());
#else
    fprintf(stderr, "%s\n", message.c_str());
    fflush(stderr);
#endif
    
    free(buffer);
    buffer = NULL;
}

const char *Logger::getLastFilePathComponent(const char *file)
{
    const char *ptr = file;
    for (ptr += strlen(file) - 1; *ptr != '/' && ptr > file; --ptr);
    ++ptr;
    return ptr;
}

}; // namespace yad
