//
//  PluginManager.cpp
//  YAD
//

//#define LOG_NDEBUG 0
#define LOG_TAG "YADPM"
#include "LogMacros.h"

#include "PluginManager.h"
#include <CoreFoundation/CoreFoundation.h>
#include "Logger.h"
#ifdef WITH_YAD_TT
#include "YADetectorTT.h"
#endif

#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <mutex>
#include <memory>
#include <sstream>
#include <regex>
#include <stdexcept>

#define YAD_PLUGIN_DIRS_KEY     "YAD_PLUGIN_DIRS"

static void logCallback(int level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args)
{
    yad::Logger::getInstance().log((yad::LogLevel)level, tag, file, line, function, format, args);
}

namespace yad {

PluginManager &PluginManager::getInstance()
{
    static PluginManager instance;
    return instance;
}

PluginManager::PluginManager()
{
    YLOGV("ctor");
    
    registerBuildInPlugins();
    registerExtendedPlugins();
}

PluginManager::~PluginManager()
{
    YLOGV("dtor");
}

size_t PluginManager::getPluginCount()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    return plugins_.size();
}
    
Detector *PluginManager::createDetector(YADConfig &config)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    int maxFaceCount = std::stoi(config[kYADMaxFaceCount]);
    YADPixelFormat pixFormat = (YADPixelFormat)std::stoi(config[kYADPixFormat]);
    YADDataType dataType = (YADDataType)std::stoi(config[kYADDataType]);
    
    // 查询插件是否支持对应的参数，并且选择优化最好的插件
    Plugin *plugin = nullptr;
    float confidence = 0.0f;
    for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
        float newConfidence;
        if ((*it)->sniff(config, &newConfidence)) {
            if (newConfidence > confidence) {
                confidence = newConfidence;
                plugin = (*it);
            }
        }
    }
    // 无法找到符合要求的插件，返回空
    if (!plugin) {
        YLOGW("plugin not found, maxFaceCount: %d pixFormat:%d dataType: %d",
              maxFaceCount, pixFormat, dataType);
        return nullptr;
    }
    
    YLOGI("select %s plugin, maxFaceCount: %d pixFormat: %d dataType: %d confidence: %f",
          plugin->getName(), maxFaceCount, pixFormat, dataType, confidence);
    
    // 否则调用插件创建detector
    return plugin->createDetector(config);
}

void PluginManager::registerBuildInPlugins()
{
#ifdef WITH_YAD_TT
    addPlugin(createYADetectorTTPlugin());
#endif
}

void PluginManager::registerExtendedPlugins()
{
    std::list<std::string> pluginDirectories;
    
    getPluginDirectories(pluginDirectories);
    for (std::list<std::string>::iterator it = pluginDirectories.begin(); it != pluginDirectories.end(); ++it) {
        registerPlugins(*it);
    }
}

// 获取所有插件目录，插件注册优先级根据目录添加顺序决定
void PluginManager::getPluginDirectories(std::list<std::string> &pluginDirectories)
{
    // 优先添加应用程序的目录
    pluginDirectories.push_back(getAppLibDirectory());
    
    // 接着添加用户指定的目录，目录可以多个，以","隔开
    char *dirs = getenv(YAD_PLUGIN_DIRS_KEY);
    if (dirs) {
        char *brkt;
        for (char *dir = strtok_r(dirs, ",", &brkt); dir; dir = strtok_r(NULL, ",", &brkt)) {
            pluginDirectories.push_back(dir);
        }
    }
}

void PluginManager::registerPlugins(std::string libDirectory)
{
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(libDirectory.c_str())) != NULL) {
        while ((ent = readdir (dir)) != NULL) {
            std::string fileName = ent->d_name;
            // YLOGD("%s\n", fileName.c_str());
            std::string relativeLibPath = getRelativePluginPath(fileName);
            if (!relativeLibPath.empty()) {
                YLOGI("found plugin: %s", fileName.c_str());
                // 构造全路径
                std::string fullLibPath = libDirectory + "/" + relativeLibPath;
                // 添加插件
                addPlugin(fullLibPath);
            }
        }
        closedir(dir);
    } else {
        std::stringstream msg;
        msg << "opendir() err";
        throw std::runtime_error(msg.str());
    }
}

std::string PluginManager::mainBundlePath()
{
    static std::once_flag flag;
    static std::string path;
    std::call_once(flag, [&] {
        path = initMainBundlePath();
    });
    return path;
}

std::string PluginManager::initMainBundlePath()
{
    CFBundleRef bundleRef; // 该引用无需释放
    CFURLRef urlRef = NULL;
    CFStringRef stringRef = NULL;
    std::string errmsg;
    char path[PATH_MAX];
    
    memset(path, 0x0, PATH_MAX);
    
    if (!(bundleRef = CFBundleGetMainBundle())) {
        errmsg = "CFBundleGetMainBundle";
        goto bail;
    }
    if (!(urlRef = CFBundleCopyBundleURL(bundleRef))) {
        errmsg = "CFBundleCopyBundleURL";
        goto bail;
    }
    if (!(stringRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle))) {
        errmsg = "CFURLCopyFileSystemPath";
        goto bail;
    }
    if (!CFStringGetCString(stringRef, path, PATH_MAX, kCFStringEncodingUTF8)) {
        errmsg = "CFStringGetCString";
        goto bail;
    }

bail:
    if (stringRef) {
        CFRelease(stringRef);
    }
    if (urlRef) {
        CFRelease(urlRef);
    }
    if (!errmsg.empty()) {
        throw std::runtime_error(errmsg);
    } else {
        return std::string(path);
    }
}

std::string PluginManager::getAppLibDirectory()
{
    return mainBundlePath() + "/Frameworks";
}

// 动态库插件分两种类型: YADetectorXYZ.framework 或者 libYADetectorXYZ.dylib
std::string PluginManager::getRelativePluginPath(std::string &fileName)
{
    if (std::regex_match(fileName, std::regex("YADetector(.+)\\.framework"))) {
        std::size_t pos = fileName.find(".framework");
        std::string libName = fileName.substr(0, pos);
        // 返回形式: YADetectorXYZ.framework/YADetectorXYZ
        return fileName + "/" + libName;
    } else if (std::regex_match(fileName, std::regex("libYADetector(.+)\\.dylib"))) {
        // 返回形式: libYADetectorXYZ.dylib
        return fileName;
    }
    
    return "";
}

void PluginManager::addPlugin(const std::string &libName)
{
    YLOGD("add plugin, lib: %s", libName.c_str());
    
    void *handle = dlopen(libName.c_str(), RTLD_NOW);
    if (!handle) {
        YLOGE("dlopen() failed, libName: %s", libName.c_str());
        return;
    }

    // XXX iOS CFBundleGetFunctionPointerForName
    typedef Plugin *(*CreateYADetectorPluginFunc)();
    CreateYADetectorPluginFunc createYADPlugin = (CreateYADetectorPluginFunc)dlsym(handle, "createYADetectorPlugin");
    if (createYADPlugin) {
        if (!addPlugin((*createYADPlugin)())) {
            YLOGE("add %s plugin failed, libName: %s", libName.c_str());
            dlclose(handle);
        }
    } else {
        YLOGE("dlsym() failed, create symbols not found, libName: %s", libName.c_str());
        dlclose(handle);
    }
}

bool PluginManager::addPlugin(Plugin *plugin)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!plugin) {
        return false;
    }
    
    // 检查合法
    if (!plugin->getName) {
        YLOGE("plugin getName() is missing");
        return false;
    }
    std::string name = plugin->getName();
        
    if (!(plugin->setLog && plugin->load && plugin->sniff && plugin->createDetector)) {
        YLOGE("%s plugin implementation is missing", name.c_str());
        return false;
    }
    
    // 检查重复
    for (auto it = plugins_.begin(); it != plugins_.end(); ++it) {
        if (*it == plugin) {
            YLOGW("%s plugin has been added", name.c_str());
            return false;
        }
    }

    // 加载资源
    // TODO 从json配置文件中读取配置，比如路径等
    YADConfig config;
    int err = plugin->load(config);
    if (err != YAD_NO_ERROR) {
        YLOGW("load %s plugin failure", name.c_str());
        return false;
    }
    
    // 注册日志
    plugin->setLog(logCallback);
    
    plugins_.push_back(plugin);
    
    YLOGI("add %s plugin success", name.c_str());
    
    return true;
}

}; // namespace yad
