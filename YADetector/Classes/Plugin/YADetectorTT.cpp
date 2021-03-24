#ifdef WITH_YAD_TT

//
//  YADetectorTT.cpp
//  YAD
//

//#define LOG_NDEBUG 0
#define LOG_TAG "YADTT"
#import "LogMacros.h"

#include "YADetectorTT.h"
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMedia/CMSampleBuffer.h>
#include <dlfcn.h>
#include <mutex>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sstream>
#include <stdexcept>

// 该实现参考github上的例子：[gistfile1.txt](https://gist.github.com/ctliu3/35c9a9cf88ac8ddd7b3fab05973403e4)

#define YAD_TT_FRAMEWORK_FULL_NAME  "TTMLKit.framework"
#define YAD_TT_FRAMEWORK_BASE_NAME  "TTMLKit"
#define YAD_TT_FACE_MODEL           "model/face.model"
#define YAD_TT_FACE_EXTRA_MODLE     "model/faceext.model"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum tt_orientation {
    TT_ORIENTATION_UP       = 0,
    TT_ORIENTATION_RIGHT    = 1,
    TT_ORIENTATION_BOTTOM   = 2,
    TT_ORIENTATION_LEFT     = 3,
} tt_orientation;

typedef struct tt_rect_t
{
    int left;
    int top;
    int right;
    int bottom;
} tt_rect_t;

typedef struct tt_point_t
{
    float x;
    float y;
} tt_point_t;

typedef struct tt_face_base_t
{
    tt_rect_t rect;
    float dummy0;
    tt_point_t points[YAD_FACE_LANDMARK_NUM];
    float visibilites[YAD_FACE_LANDMARK_NUM];
    float yaw;
    float pitch;
    float roll;
    float dummy1;
    int face_id;
    unsigned int dummy2;
    unsigned int tracking_count; // 检测个数
} tt_face_base_t;

typedef struct tt_face_extra_t
{
    int dummy0[4];
    tt_point_t dummy1[174];
} tt_face_extra_t;

typedef struct tt_faces_info_t
{
  tt_face_base_t faces[YAD_MAX_FACE_NUM];
  tt_face_extra_t dummy1[YAD_MAX_FACE_NUM];
  int num_faces;
} tt_faces_info_t;

#if defined(__cplusplus)
}
#endif

namespace YAD {

typedef int (*CreateHandlerFunc)(unsigned long long flags, char const *param_path, void **p_handle);
typedef void (*AddExtraModelFunc)(void *handle, unsigned long long flags, char const *param_path);
typedef int (*DoPredictFunc)(void *handle, unsigned char const *baseAddress, unsigned int pixelFormatType, signed int width, signed int height, int stride, int screenOrient, unsigned long long flags, tt_faces_info_t *facesInfo);
typedef void (*ReleaseHandleFunc)(void *handle);

static bool gFirst = true;
static void *gLibHandle = NULL;
static std::mutex gLibMutex;

static CreateHandlerFunc gCreateHandler;
static AddExtraModelFunc gAddExtraModel;
static DoPredictFunc gDoPredict;
static ReleaseHandleFunc gReleaseHandle;

TTDetector::TTDetector()
{
    YLOGV("YADetectorTT ctor");
    
    initNulls();
}

TTDetector::TTDetector(int maxFaceCount, YADPixelFormat pixFormat, YADDataType dataType)
{
    YLOGV("YADetectorTT ctor");
    
     initNulls();
    
    mMaxFaceNum = std::min(maxFaceCount, YAD_MAX_FACE_NUM);
    
    if (!loadSymbols()) {
        return;
    }
    
    unsigned long long flags = 0x20007f;
    int ret = gCreateHandler(flags, getModelPath().c_str(), &mHandle);
    if (ret) {
        YLOGE("create handler failed, err: %d\n", ret);
        return;
    }
    
    flags = 0x900;
    gAddExtraModel(mHandle, 0x900, getExtraModelPath().c_str());
    
    mInitCheck = YAD_OK;
}

TTDetector::~TTDetector()
{
    YLOGV("YADetectorTT dtor");
    
    if (mHandle) {
        gReleaseHandle(mHandle);
        mHandle = NULL;
    }
}

void TTDetector::initNulls()
{
    mInitCheck = YAD_NO_INIT;
    mMaxFaceNum = YAD_MAX_FACE_NUM;
    mHandle = NULL;
}

int TTDetector::initCheck() const
{
    return mInitCheck;
}

bool TTDetector::loadSymbols()
{
    std::unique_lock<std::mutex> lock(gLibMutex);

    if (!gFirst) {
        return (gLibHandle != NULL);
    }

    gFirst = false;

    gLibHandle = dlopen(getLibPath().c_str(), RTLD_NOW);
    if (gLibHandle == NULL) {
        YLOGE("dlopen failed\n");
        return false;
    }

    gCreateHandler = (CreateHandlerFunc)dlsym(gLibHandle, "FS_CreateHandler");
    if (gCreateHandler == NULL) {
        YLOGE("dlsym 1 failed\n");
        goto fail;
    }
    
    gAddExtraModel = (AddExtraModelFunc)dlsym(gLibHandle, "FS_AddExtraModel");
    if (gAddExtraModel == NULL) {
        YLOGE("dlsym 2 failed\n");
        goto fail;
    }

    gDoPredict = (DoPredictFunc)dlsym(gLibHandle, "FS_DoPredict");
    if (gDoPredict == NULL) {
        YLOGE("dlsym 3 failed\n");
        goto fail;
    }

    gReleaseHandle = (ReleaseHandleFunc)dlsym(gLibHandle, "FS_ReleaseHandle");
    if (gReleaseHandle == NULL) {
        YLOGE("dlsym 4 failed\n");
        goto fail;
    }
    
    return true;
    
fail:
    gLibHandle = NULL;
    return false;
}

std::string TTDetector::getLibPath()
{
    return getLibDir() + "/" + YAD_TT_FRAMEWORK_FULL_NAME + "/" + YAD_TT_FRAMEWORK_BASE_NAME;
}

std::string TTDetector::getModelPath()
{
    return getLibDir() + "/" + YAD_TT_FRAMEWORK_FULL_NAME + "/" + YAD_TT_FACE_MODEL;
}

std::string TTDetector::getExtraModelPath()
{
    return getLibDir() + "/" + YAD_TT_FRAMEWORK_FULL_NAME + "/" + YAD_TT_FACE_EXTRA_MODLE;
}

std::string TTDetector::getLibDir()
{
    CFBundleRef mainBundle;
    CFURLRef bundleUrl;
    CFStringRef sr;
    char path[FILENAME_MAX];

    if (!(mainBundle = CFBundleGetMainBundle())) {
        std::stringstream msg;
        msg << "CFBundleGetMainBundle() err";
        throw std::runtime_error(msg.str());
    }
    
    if (!(bundleUrl = CFBundleCopyBundleURL(mainBundle))) {
        std::stringstream msg;
        msg << "CFBundleCopyBundleURL() err";
        throw std::runtime_error(msg.str());
    }
    if (!(sr = CFURLCopyFileSystemPath(bundleUrl, kCFURLPOSIXPathStyle))) {
        CFRelease(bundleUrl);
        std::stringstream msg;
        msg << "CFURLCopyFileSystemPath() err";
        throw std::runtime_error(msg.str());
    }
    if (!CFStringGetCString(sr, path, FILENAME_MAX, kCFStringEncodingASCII)) {
        CFRelease(bundleUrl);
        CFRelease(sr);
        std::stringstream msg;
        msg << "CFStringGetCString() err";
        throw std::runtime_error(msg.str());
    }
    CFRelease(bundleUrl);
    CFRelease(sr);

    return std::string(path) + "/Frameworks";
}

int TTDetector::translateRotate(YADRotateMode rotateMode)
{
    switch (rotateMode) {
        case YAD_ROTATE_0:
            return TT_ORIENTATION_UP;
        case YAD_ROTATE_90:
            return TT_ORIENTATION_RIGHT;
        case YAD_ROTATE_180:
            return TT_ORIENTATION_BOTTOM;
        case YAD_ROTATE_270:
            return TT_ORIENTATION_LEFT;
        default:
            break;
    }
}

int TTDetector::detect(YADDetectImage *detectImage, YADDetectInfo *detectInfo, YADFeatureInfo *featureInfo)
{
    assert(detectImage != NULL);
    assert(detectInfo != NULL);
    assert(featureInfo != NULL);

    memset(featureInfo, 0, sizeof(YADFeatureInfo));
    
    if (!loadSymbols()) {
        return YAD_INVALID_OPERATION;
    }
    
    unsigned int pixelFormatType = 0;
    int screenOrient = translateRotate(detectInfo->rotate_mode);
    unsigned long long flags = 0x13f;
    
    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)detectImage->data;
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    unsigned char *baseAddress = (unsigned char *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
    
    // FIXME support pixelFormatType, flags
    tt_faces_info_t facesInfo;
    int ret = gDoPredict(mHandle, baseAddress, pixelFormatType, detectImage->width, detectImage->height, detectImage->stride, screenOrient, flags, &facesInfo);
    if (ret) {
        YLOGE("DoPredict failed, ret: %d", ret);
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
        return YAD_UNKNOWN_ERROR;
    }
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    
    // 转换结构
    featureInfo->num_faces = std::min(facesInfo.num_faces, mMaxFaceNum);
    for (int i = 0; i < facesInfo.num_faces; i++) {
        YADFaceInfo *dst = &(featureInfo->faces[i]);
        tt_face_base_t *src = &(facesInfo.faces[i]);
        
        dst->track_id = src->face_id;
        dst->rect = {(float)src->rect.left, (float)src->rect.top, (float)(src->rect.right - src->rect.left), (float)(src->rect.bottom - src->rect.top)};
        memcpy(dst->landmarks, src->points, sizeof(YADPoint2f) * YAD_FACE_LANDMARK_NUM);
        memcpy(dst->visibilites, src->visibilites, sizeof(YADPoint2f) * YAD_FACE_LANDMARK_NUM);
        dst->yaw = src->yaw;
        dst->pitch = src->pitch;
        dst->roll = src->roll;
    }

    return YAD_OK;
}

static Detector *createDetector(int maxFaceCount, YADPixelFormat pixFormat, YADDataType dataType)
{
    return new TTDetector(maxFaceCount, pixFormat, dataType);
}

static const char *GetName()
{
    return "YADetectorTT";
}

static void SetLog(Log log)
{
    
}

static bool sniffDetector(int maxFaceCount, YADPixelFormat pixFormat, YADDataType dataType, float *confidence)
{
    if (pixFormat == YAD_PIX_FMT_BGRA8888 && dataType == YAD_DATA_TYPE_IOS_PIXEL_BUFFER) {
        *confidence = 0.8f;
    } else {
        *confidence = 0.0f;
    }
    
    return *confidence > 0.0f;
}

} // namespace YAD

YAD::Plugin *createYADetectorTTPlugin()
{
    YAD::Plugin *plugin = new YAD::Plugin;
    plugin->getName = YAD::GetName;
    plugin->setLog = YAD::SetLog;
    plugin->sniff = YAD::sniffDetector;
    plugin->createDetector = YAD::createDetector;
    return plugin;
}

#endif // WITH_YAD_TT
