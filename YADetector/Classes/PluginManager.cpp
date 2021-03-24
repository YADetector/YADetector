//
//  PluginManager.cpp
//  YAD
//

//#define LOG_NDEBUG 0
#define LOG_TAG "YADPM"
#import "LogMacros.h"

#include "PluginManager.h"
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sstream>
#include <stdexcept>
#include "Logger.h"
#ifdef WITH_YAD_TT
#include "YADetectorTT.h"
#endif

#define YAD_PLUGIN_PREFIX       "YADetector"
#define APPLE_FRAMEWORK_SUFFIX  ".framework"

static void logCallback(int level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args)
{
    YAD::Logger::getInstance().log((YAD::LogLevel)level, tag, file, line, function, format, args);
}

namespace YAD {

PluginManager::PluginManager()
{
    registerBuildInPlugins();
    registerExtendedPlugins();
}

PluginManager::~PluginManager()
{
    
}

Detector *PluginManager::createDetector(int maxFaceCount, YADPixelFormat pixFormat, YADDataType dataType)
{
    std::unique_lock<std::mutex> lock(mMutex);
    
    // 查询插件是否支持对应的参数，并且选择优化最好的插件
    Plugin *plugin = nullptr;
    float confidence = 0.0f;
    for (std::list<Plugin *>::iterator it = mPlugins.begin(); it != mPlugins.end(); ++it) {
        float newConfidence;
        if ((*it)->sniff(maxFaceCount, pixFormat, dataType, &newConfidence)) {
            if (newConfidence > confidence) {
                confidence = newConfidence;
                plugin = (*it);
            }
        }
    }
    // 无法找到符合要求的插件，返回空
    if (!plugin) {
        YLOGW("plugin not found, maxFaceCount: %d pixFormat:%d dataType: %d", maxFaceCount, pixFormat, dataType);
        return nullptr;
    }
    
    YLOGI("choose %s plugin, maxFaceCount: %d pixFormat: %d dataType: %d confidence: %f", plugin->getName(), maxFaceCount, pixFormat, dataType, confidence);
    
    // 否则调用插件创建detector
    return plugin->createDetector(maxFaceCount, pixFormat, dataType);
}

void PluginManager::registerBuildInPlugins()
{
#ifdef WITH_YAD_TT
    addPlugin(createYADetectorTTPlugin());
#endif
}

void PluginManager::registerExtendedPlugins()
{
    std::list<std::string> paths;
    
    // 搜索插件
    getAllLibPaths(paths);

    // 添加插件
    for (std::list<std::string>::iterator it = paths.begin(); it != paths.end(); ++it) {
        addPlugin(*it);
    }
}

void PluginManager::getAllLibPaths(std::list<std::string> &libPaths)
{
    // 查找标准库目录下动态库
    std::string libDir = getLibDir();
    
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir (libDir.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            std::string s0 = ent->d_name;
            std::string s1 = "";
            //YLOGD("%s\n", s0.c_str());
            if (isYADPlugin(s0, s1)) {
                YLOGI("found plugin: %s", s0.c_str());
                libPaths.push_back(libDir + "/" + s0 + "/" + s1);
            }
        }
        closedir(dir);
    } else {
        std::stringstream msg;
        msg << "opendir() err";
        throw std::runtime_error(msg.str());
    }
}

std::string PluginManager::getLibDir()
{
    CFBundleRef mainBundle;
    CFURLRef bundleUrl;
    CFStringRef sr;
    char path[FILENAME_MAX];

    if (!(mainBundle = CFBundleGetMainBundle())) {
        std::stringstream msg;
        msg << "CFBundleGetMainBundle() err";
        throw std::runtime_error(msg.str());
    }
    
    if (!(bundleUrl = CFBundleCopyBundleURL(mainBundle))) {
        std::stringstream msg;
        msg << "CFBundleCopyBundleURL() err";
        throw std::runtime_error(msg.str());
    }
    if (!(sr = CFURLCopyFileSystemPath(bundleUrl, kCFURLPOSIXPathStyle))) {
        CFRelease(bundleUrl);
        std::stringstream msg;
        msg << "CFURLCopyFileSystemPath() err";
        throw std::runtime_error(msg.str());
    }
    if (!CFStringGetCString(sr, path, FILENAME_MAX, kCFStringEncodingASCII)) {
        CFRelease(bundleUrl);
        CFRelease(sr);
        std::stringstream msg;
        msg << "CFStringGetCString() err";
        throw std::runtime_error(msg.str());
    }
    CFRelease(bundleUrl);
    CFRelease(sr);

    return std::string(path) + "/Frameworks";
}

// 举例动态库为libName=YADetectorXYZ.framework， 返回libBaseName=DetectorXYY
bool PluginManager::isYADPlugin(std::string &libName, std::string &libBaseName)
{
    // 搜索APPLE_FRAMEWORK_SUFFIX
    std::size_t pos = libName.find(APPLE_FRAMEWORK_SUFFIX);
    if (pos == std::string::npos) {
        return false;
    }
    // 去掉APPLE_FRAMEWORK_SUFFIX后缀的基本名
    libBaseName = libName.substr(0, pos);
    std::string prefix = YAD_PLUGIN_PREFIX;
    // 搜索以YAD_PLUGIN_PREFIX开头的Framework
    if (libBaseName.size() > prefix.size() && libBaseName.compare(0, prefix.size(), prefix) == 0) {
        return true;
    }
    return false;
}

void PluginManager::addPlugin(const std::string &libName)
{
    void *handle = dlopen(libName.c_str(), RTLD_NOW);
    if (!handle) {
        YLOGE("dlopen() failed, libName: %s", libName.c_str());
        return;
    }

    // XXX iOS CFBundleGetFunctionPointerForName
    typedef Plugin *(*CreateYADetectorPluginFunc)();
    CreateYADetectorPluginFunc createYADPlugin = (CreateYADetectorPluginFunc)dlsym(handle, "createYADetectorPlugin");
    if (createYADPlugin) {
        addPlugin((*createYADPlugin)());
    } else {
        YLOGE("dlsym() failed, create symbols not found, libName: %s", libName.c_str());
    }
}

void PluginManager::addPlugin(Plugin *plugin)
{
    std::unique_lock<std::mutex> lock(mMutex);

    if (!plugin) {
        return;
    }
    
    // 检查合法
    if (!(plugin->getName && plugin->sniff && plugin->createDetector)) {
        return;
    }
    
    // 检查重复
    for (std::list<Plugin *>::iterator it = mPlugins.begin(); it != mPlugins.end(); ++it) {
        if (*it == plugin) {
            return;
        }
    }
    
    // 注册日志
    plugin->setLog(logCallback);
    
    mPlugins.push_back(plugin);
    
    YLOGI("add %s plugin", plugin->getName());
}

YAD_SINGLETON_STATIC_INSTANCE(PluginManager)

} // namespace YAD
