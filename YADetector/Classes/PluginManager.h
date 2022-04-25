//
//  PluginManager.h
//  YAD
//

#ifndef YAD_PLUGIN_MANAGER_H
#define YAD_PLUGIN_MANAGER_H

#include "YADetector.h"

#include <mutex>
#include <list>
#include <string>

namespace YAD {

class PluginManager {
public:
    static PluginManager &getInstance();
    
    size_t getPluginCount();
    Detector *createDetector(YADConfig &config);
    
private:
    PluginManager();
    ~PluginManager();
    PluginManager(const PluginManager &) = delete;
    PluginManager(PluginManager&&) = delete;
    PluginManager &operator=(const PluginManager &) = delete;
    PluginManager &operator=(PluginManager&&) = delete;
    
    void registerBuildInPlugins();
    void registerExtendedPlugins();
    void registerPlugins(std::string libDirectory);
    void getPluginDirectories(std::list<std::string> &libDirectories);
    std::string mainBundlePath();
    std::string initMainBundlePath();
    std::string getAppLibDirectory(); // 获取应用程序的库目录
    std::string getRelativePluginPath(std::string &fileName); // 获取插件相对路径
    void addPlugin(const std::string &libName);
    bool addPlugin(Plugin *plugin);
    
    std::mutex mutex_;
    std::list<Plugin *> plugins_;
};

}; // namespace YAD

#endif /* YAD_PLUGIN_MANAGER_H */
