#ifdef WITH_YAD_TT

//
//  YADetectorTT.h
//  YAD
//

#ifndef YAD_DETECTOR_TT_H
#define YAD_DETECTOR_TT_H

#include "YADetector.h"
#include <string>

namespace YAD {

class TTDetector : public Detector
{
public:
    TTDetector();
    TTDetector(YADConfig &config);
    virtual ~TTDetector();
    virtual int initCheck() const;
    virtual int detect(YADDetectImage *detectImage, YADDetectInfo *detectInfo, YADFeatureInfo *featureInfo);
    
private:
    void initNulls();
    bool fileExists(std::string path);
    bool loadSymbols();
    
    std::string getAppLibDirectory();
    std::string getDefalutLibDirectory();
    std::string getDefalutModelDirectory();
    std::string getDefalutLibPath();
    std::string getDefalutModelPath();
    std::string getDefalutExtraModelPath();
    std::string getLibPath();
    std::string getModelPath();
    std::string getExtraModelPath();
    
    int translatePixelFormat(YADPixelFormat pixelFormat);
    int translateOrientation(YADRotateMode rotateMode);
    
    int mInitCheck;
    int mMaxFaceNum;
    void *mHandle;
    std::string mLibPath;
    std::string mFaceModelPath;
    std::string mFaceExtraModelPath;
    
    TTDetector(const TTDetector &);
    TTDetector &operator=(const TTDetector &);
};

}  // namespace YAD

extern "C" YAD::Plugin *createYADetectorTTPlugin();

#endif /* YAD_DETECTOR_TT_H */

#endif // WITH_YAD_TT
