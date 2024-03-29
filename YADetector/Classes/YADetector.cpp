//
//  YADetector.cpp
//  YAD
//

#include "YADetector.h"
#include "PluginManager.h"

namespace yad {

// static
bool Detector::Exists()
{
    return PluginManager::getInstance().getPluginCount() > 0;
}

// static
Detector *Detector::Create(YADConfig &config)
{
    return PluginManager::getInstance().createDetector(config);
}

}; // namespace yad
