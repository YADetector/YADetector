//
//  YADetector.cpp
//  YAD
//

#include "YADetector.h"
#include "PluginManager.h"

namespace YAD {

// static
bool Detector::Exists()
{
    return PluginManager::getInstance().getPluginCount() > 0;
}

// static
Detector *Detector::Create(int maxFaceCount, YADPixelFormat pixFormat, YADDataType dataType)
{
    return PluginManager::getInstance().createDetector(maxFaceCount, pixFormat, dataType);
}

} // namespace YAD
