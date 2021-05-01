//
//  PluginManager.h
//  YAD
//

#ifndef YAD_PLUGIN_MANAGER_H
#define YAD_PLUGIN_MANAGER_H

#include "YADetector.h"
#include "Singleton.h"
#include <mutex>
#include <list>
#include <string>

namespace YAD {

class PluginManager : public Singleton<PluginManager> {
public:
    PluginManager();
    ~PluginManager();
    
    size_t getPluginCount();
    Detector *createDetector(YADConfig &config);
    
private:
    void registerBuildInPlugins();
    void registerExtendedPlugins();
    void registerPlugins(std::string libDirectory);
    void getPluginDirectories(std::list<std::string> &libDirectories);
    std::string getAppLibDirectory(); // 获取应用程序的库目录
    std::string getRelativePluginPath(std::string &fileName); // 获取插件相对路径
    void addPlugin(const std::string &libName);
    bool addPlugin(Plugin *plugin);
    
    std::mutex mutex_;
    std::list<Plugin *> plugins_;
    
    PluginManager(const Detector &);
    PluginManager &operator=(const Detector &);
};

}  // namespace YAD

#endif /* YAD_PLUGIN_MANAGER_H */
