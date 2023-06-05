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
    
    int initCheck() const override;
    int detect(YADDetectImage *detectImage, YADDetectInfo *detectInfo, YADFeatureInfo *featureInfo) override;
    
private:
    bool fileExists(std::string path);
    bool loadSymbols();
    
    std::string mainBundlePath();
    std::string initMainBundlePath();
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
    
    int init_check_;
    void *handle_;
    std::string lib_path_;
    std::string face_model_path_;
    std::string face_extra_model_path_;
    int max_face_num_;

    TTDetector(const TTDetector &);
    TTDetector &operator=(const TTDetector &);
};

}; // namespace yad

extern "C" yad::Plugin *createYADetectorTTPlugin();

#endif /* YAD_DETECTOR_TT_H */

#endif // WITH_YAD_TT
