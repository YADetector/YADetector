#ifdef WITH_YAD_TT

//
//  YADetectorTT.h
//  YAD
//

#ifndef YAD_DETECTOR_TT_h
#define YAD_DETECTOR_TT_h

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
    bool fileExists(const char *path);
    bool loadSymbols();
    std::string getLibPath();
    std::string getModelPath();
    std::string getExtraModelPath();
    std::string getLibDir();
    int translatePixelFormat(YADPixelFormat pixelFormat);
    int translateOrientation(YADRotateMode rotateMode);
    
    int mInitCheck;
    int mMaxFaceNum;
    void *mHandle;
    
    TTDetector(const TTDetector &);
    TTDetector &operator=(const TTDetector &);
};

}  // namespace YAD

extern "C" YAD::Plugin *createYADetectorTTPlugin();

#endif /* YAD_DETECTOR_TT_h */

#endif // WITH_YAD_TT
