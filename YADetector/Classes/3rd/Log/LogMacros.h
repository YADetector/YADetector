#include "Logger.h"

// ---------------------------------------------------------------------

/*
 * Normally we strip YLOGV (VERBOSE messages) from release builds.
 * You can modify this (for example with "#define LOG_NDEBUG 0"
 * at the top of your source file) to change that behavior.
 */
#ifndef LOG_NDEBUG
#ifdef DEBUG
#define LOG_NDEBUG 0
#else
#define LOG_NDEBUG 1
#endif
#endif

/*
* This is the local tag used for the following simplified
* logging macros.  You can change this preprocessor definition
* before using the other macros to change the tag.
*/
#ifndef LOG_TAG
#define LOG_TAG "Global"
#endif

// ---------------------------------------------------------------------

#ifndef YLOGV
#if LOG_NDEBUG
#define YLOGV(fmt, ...) ((void)0)
#else
#define YLOGV(fmt, ...) yad::Logger::getInstance().log(yad::LOG_LEVEL_VERBOSE,  LOG_TAG, "", 0, "", fmt, ##__VA_ARGS__);
#endif
#endif

#define YLOGD(fmt, ...) yad::Logger::getInstance().log(yad::LOG_LEVEL_DEBUG,   LOG_TAG, "", 0, "", fmt, ##__VA_ARGS__);
#define YLOGI(fmt, ...) yad::Logger::getInstance().log(yad::LOG_LEVEL_INFO,    LOG_TAG, "", 0, "", fmt, ##__VA_ARGS__);
#define YLOGW(fmt, ...) yad::Logger::getInstance().log(yad::LOG_LEVEL_WARN,    LOG_TAG, "", 0, "", fmt, ##__VA_ARGS__);
#define YLOGE(fmt, ...) yad::Logger::getInstance().log(yad::LOG_LEVEL_ERROR,   LOG_TAG, "", 0, "", fmt, ##__VA_ARGS__);
#define YLOGF(fmt, ...) yad::Logger::getInstance().log(yad::LOG_LEVEL_FAULT,   LOG_TAG, "", 0, "", fmt, ##__VA_ARGS__);
