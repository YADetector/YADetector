//
//  Logger.h
//  YAD
//

#ifndef YAD_LOGGER_H
#define YAD_LOGGER_H

#include "Singleton.h"
#include <stdarg.h>

namespace YAD {

typedef enum {
    LOG_LEVEL_VERBOSE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FAULT,
} LogLevel;

typedef void (*LogFunc)(LogLevel level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args);


class Logger : public Singleton<Logger>
{
public:
    Logger();
    ~Logger() {}
    
    void setHandler(LogFunc cb);
    void setLevel(LogLevel level);
    
    void log(LogLevel level, const char *tag, const char *file, int line, const char *function, const char *format, ...);
    void log(LogLevel level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args);
    
private:
    void logDefault(LogLevel level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args);
    // 去除文件目录名，只保留最后部分的文件名
    const char *getLastFilePathComponent(const char *file);
    
    LogFunc mLogHander;
    LogLevel mLogLevel; // 默认 LOG_LEVEL_VERBOSE
    
    Logger(const Logger &);
    Logger &operator=(const Logger &);
};

}  // namespace YAD

#endif /* YAD_LOGGER_H */
