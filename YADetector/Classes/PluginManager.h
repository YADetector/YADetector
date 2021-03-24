//
//  PluginManager.h
//  YAD
//

#include "YADetector.h"
#include "Singleton.h"
#include <mutex>
#include <list>
#include <string>

#ifndef PluginManager_h
#define PluginManager_h

namespace YAD {

class PluginManager : public Singleton<PluginManager> {
public:
    PluginManager();
    ~PluginManager();
    
    size_t getPluginCount() { return mPlugins.size(); }
    Detector *createDetector(int maxFaceCount, YADPixelFormat pixFormat, YADDataType dataType);
    
private:
    void registerBuildInPlugins();
    void registerExtendedPlugins();
    void getAllLibPaths(std::list<std::string> &libPaths); // 获取所有动态库的文件全名
    std::string getLibDir(); // 获取动态库目录名
    bool isYADPlugin(std::string &libName, std::string &libNameWithoutSuffix);
    void addPlugin(const std::string &libName);
    void addPlugin(Plugin *plugin);
    
    std::mutex mMutex;
    std::list<Plugin *> mPlugins;
    
    PluginManager(const Detector &);
    PluginManager &operator=(const Detector &);
};

}  // namespace YAD

#endif /* PluginManager_h */
