#ifdef WITH_YAD_TT

//
//  YADetectorTT.h
//  YAD
//

#ifndef YAD_DETECTOR_TT_H
#define YAD_DETECTOR_TT_H

#include "YADetector.h"
#include <string>

namespace yad {

class TTDetector : public Detector
{
public:
    TTDetector() = delete;
    TTDetector(YADConfig &config);
    virtual ~TTDetector();
    
    static int load(YADConfig &config);
    
    int initCheck() const override;
    int detect(YADDetectImage *detectImage, YADDetectInfo *detectInfo, YADFeatureInfo *featureInfo) override;
    
private:
    static bool loadSymbols(std::string libPath);
    
    static std::string mainBundlePath();
    static std::string initMainBundlePath();
    static std::string getAppLibDirectory();
    static std::string getDefalutLibDirectory();
    static std::string getDefalutModelDirectory();
    static std::string getDefalutLibPath();
    static std::string getDefalutModelPath();
    static std::string getDefalutExtraModelPath();
    static std::string getLibPath();
    static std::string getModelPath();
    static std::string getExtraModelPath();
    
    static bool fileExists(std::string path);
    static int translatePixelFormat(YADPixelFormat pixelFormat);
    static int translateOrientation(YADRotateMode rotateMode);
    
    int init_check_;
    void *handle_;
    int max_face_num_;

    TTDetector(const TTDetector &);
    TTDetector &operator=(const TTDetector &);
};

}; // namespace yad

extern "C" yad::Plugin *createYADetectorTTPlugin();

#endif /* YAD_DETECTOR_TT_H */

#endif // WITH_YAD_TT
