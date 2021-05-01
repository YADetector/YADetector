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
#include <sys/stat.h>
#include <sstream>
#include <stdexcept>

// 该实现参考github上的例子：[gistfile1.txt](https://gist.github.com/ctliu3/35c9a9cf88ac8ddd7b3fab05973403e4)

#define YAD_TT_FRAMEWORK_DIR            "TTMLKit.framework"
#define YAD_TT_MODEL_DIR                "model"
#define YAD_TT_LIB_NAME                 "TTMLKit"
#define YAD_TT_FACE_MODEL_NAME          "ttface.model"
#define YAD_TT_FACE_EXTRA_MODLE_NAME    "ttfaceext.model"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kOrientation_UP = 0,
    kOrientation_RIGHT,
    kOrientation_BOTTOM,
    kOrientation_LEFT,
};

// tt facedetect支持范围: 0~3
enum {
    kPixelFormat_RGBA8888 = 0,
    kPixelFormat_BGRA8888,
    kPixelFormat_BGR888,
    kPixelFormat_RGB888,
    kPixelFormat_NV12,
    kPixelFormat_GRAY,
};

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

static bool g_first = true;
static void *g_lib_handle = NULL;
static std::mutex g_lib_mutex;

static CreateHandlerFunc g_CreateHandler;
static AddExtraModelFunc g_AddExtraModel;
static DoPredictFunc g_DoPredict;
static ReleaseHandleFunc g_ReleaseHandle;

TTDetector::TTDetector()
{
    YLOGV("YADetectorTT ctor");
    
    initNulls();
}

TTDetector::TTDetector(YADConfig &config)
{
    YLOGV("YADetectorTT ctor");
    
    initNulls();
    
    max_face_num_ = std::stoi(config[kYADMaxFaceCount]);
    max_face_num_ = std::min(max_face_num_, YAD_MAX_FACE_NUM);
    // 有些情况下，调用者希望定制资源，调用者可以在config增加字段实现该功能，key: 默认资源名，value: 资源指定的路径
    // 但是需要注意的是，仍旧会优先使用App本身的资源
    lib_path_ = config[YAD_TT_LIB_NAME];
    face_model_path_ = config[YAD_TT_FACE_MODEL_NAME];
    face_extra_model_path_ = config[YAD_TT_FACE_EXTRA_MODLE_NAME];

    if (!loadSymbols()) {
        YLOGE("symbols not loaded");
        init_check_ = YAD_SYMBOLS_NOT_LOADED;
        return;
    }
    
    std::string modelPath = getModelPath();
    if (!fileExists(modelPath)) {
        YLOGE("model file not found, path: %s", modelPath.c_str());
        init_check_ = YAD_MODEL_NOT_FOUND;
        return;
    }
    std::string extraModelPath = getExtraModelPath();
    if (!fileExists(extraModelPath)) {
        YLOGE("extra model file not found, path: %s", extraModelPath.c_str());
        init_check_ = YAD_MODEL_NOT_FOUND;
        return;
    }
    
    unsigned long long flags = 0x20007f;
    int ret = g_CreateHandler(flags, modelPath.c_str(), &handle_);
    if (ret) {
        YLOGE("create handler failed, err: %d", ret);
        return;
    }
    
    flags = 0x900;
    g_AddExtraModel(handle_, flags, extraModelPath.c_str());
    
    init_check_ = YAD_OK;
}

TTDetector::~TTDetector()
{
    YLOGV("YADetectorTT dtor");
    
    if (handle_) {
        g_ReleaseHandle(handle_);
        handle_ = NULL;
    }
}

void TTDetector::initNulls()
{
    init_check_ = YAD_NO_INIT;
    max_face_num_ = YAD_MAX_FACE_NUM;
    handle_ = NULL;
    lib_path_ = "";
    face_model_path_ = "";
    face_extra_model_path_ = "";
}

int TTDetector::initCheck() const
{
    return init_check_;
}

bool TTDetector::fileExists(std::string path)
{
    struct stat pathStat;
    memset(&pathStat, 0, sizeof(struct stat));
    return stat(path.c_str(), &pathStat) == 0;
}

bool TTDetector::loadSymbols()
{
    std::unique_lock<std::mutex> lock(g_lib_mutex);

    // 缓存句柄，增加加载速度
    if (!g_first) {
        return (g_lib_handle != NULL);
    }

    g_first = false;

    std::string libPath = getLibPath();
    if (!fileExists(libPath)) {
        YLOGE("lib not found, path: %s", libPath.c_str());
        return false;
    }
    g_lib_handle = dlopen(libPath.c_str(), RTLD_NOW);
    if (g_lib_handle == NULL) {
        YLOGE("dlopen failed\n");
        return false;
    }

    g_CreateHandler = (CreateHandlerFunc)dlsym(g_lib_handle, "FS_CreateHandler");
    if (g_CreateHandler == NULL) {
        YLOGE("dlsym 1 failed\n");
        goto fail;
    }
    
    g_AddExtraModel = (AddExtraModelFunc)dlsym(g_lib_handle, "FS_AddExtraModel");
    if (g_AddExtraModel == NULL) {
        YLOGE("dlsym 2 failed\n");
        goto fail;
    }

    g_DoPredict = (DoPredictFunc)dlsym(g_lib_handle, "FS_DoPredict");
    if (g_DoPredict == NULL) {
        YLOGE("dlsym 3 failed\n");
        goto fail;
    }

    g_ReleaseHandle = (ReleaseHandleFunc)dlsym(g_lib_handle, "FS_ReleaseHandle");
    if (g_ReleaseHandle == NULL) {
        YLOGE("dlsym 4 failed\n");
        goto fail;
    }
    
    return true;
    
fail:
    dlclose(g_lib_handle);
    g_lib_handle = NULL;
    return false;
}

std::string TTDetector::getAppLibDirectory()
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

std::string TTDetector::getDefalutLibDirectory()
{
    return (getAppLibDirectory() + "/" + YAD_TT_FRAMEWORK_DIR);
}

std::string TTDetector::getDefalutModelDirectory()
{
    return (getAppLibDirectory() + "/" + YAD_TT_FRAMEWORK_DIR + "/" + YAD_TT_MODEL_DIR);
}

std::string TTDetector::getDefalutLibPath()
{
    return getDefalutLibDirectory() + "/" + YAD_TT_LIB_NAME;
}

std::string TTDetector::getDefalutModelPath()
{
    return getDefalutModelDirectory() + "/" + YAD_TT_FACE_MODEL_NAME;
}

std::string TTDetector::getDefalutExtraModelPath()
{
    return getDefalutModelDirectory() + "/" + YAD_TT_FACE_EXTRA_MODLE_NAME;
}

// 优先使用App本身的库和资源
std::string TTDetector::getLibPath()
{
    std::string path = getDefalutLibPath();
    if (fileExists(path)) {
        return path;
    }
    return lib_path_;
}

std::string TTDetector::getModelPath()
{
    std::string path = getDefalutModelPath();
    if (fileExists(path)) {
        return path;
    }
    return face_model_path_;
}

std::string TTDetector::getExtraModelPath()
{
    std::string path = getDefalutExtraModelPath();
    if (fileExists(path)) {
        return path;
    }
    return face_extra_model_path_;
}

int TTDetector::detect(YADDetectImage *detectImage, YADDetectInfo *detectInfo, YADFeatureInfo *featureInfo)
{
    if (!detectImage || !detectInfo || !featureInfo) {
        YLOGE("params is null");
        return YAD_BAD_VALUE;
    }
    
    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)detectImage->data;
    if (!pixelBuffer) {
        YLOGE("data is null");
        return YAD_BAD_VALUE;
    }
    
    int pixelFormat = translatePixelFormat(detectImage->format);
    if (pixelFormat == INT_MAX) {
        YLOGE("format unsupported");
        return YAD_FORMAT_UNSUPPORTED;
    }
    
    int orientation = translateOrientation(detectInfo->rotate_mode);
    if (orientation == INT_MAX) {
        YLOGE("rotate unsupported");
        return YAD_ROTATE_UNSUPPORTED;
    }
    
    if (!handle_) {
        YLOGE("handle is null");
        return YAD_INVALID_OPERATION;
    }

    unsigned long long flags = 0x13f;
    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    unsigned char *baseAddress = (unsigned char *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
    
    // FIXME support flags
    tt_faces_info_t facesInfo;
    memset(&facesInfo, 0, sizeof(tt_faces_info_t));
    int ret = g_DoPredict(handle_, baseAddress, pixelFormat, detectImage->width, detectImage->height, detectImage->stride, orientation, flags, &facesInfo);
    if (ret) {
        YLOGE("DoPredict failed, ret: %d", ret);
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
        return YAD_DETECT_FAILED;
    }
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    
    //YLOGD("DoPredict succ, num_faces: %d", facesInfo.num_faces);
    
    // 转换结构
    featureInfo->num_faces = std::min(facesInfo.num_faces, max_face_num_);
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

int TTDetector::translatePixelFormat(YADPixelFormat pixelFormat)
{
    switch (pixelFormat) {
        case YAD_PIX_FMT_BGR888:
            return kPixelFormat_BGR888;
        case YAD_PIX_FMT_RGB888:
            return kPixelFormat_RGB888;
        case YAD_PIX_FMT_BGRA8888:
            return kPixelFormat_BGRA8888;
        case YAD_PIX_FMT_RGBA8888:
            return kPixelFormat_RGBA8888;
        default:
            break;
    }
    return INT_MAX;
}

int TTDetector::translateOrientation(YADRotateMode rotateMode)
{
    switch (rotateMode) {
        case YAD_ROTATE_0:
            return kOrientation_UP;
        case YAD_ROTATE_90:
            return kOrientation_RIGHT;
        case YAD_ROTATE_180:
            return kOrientation_BOTTOM;
        case YAD_ROTATE_270:
            return kOrientation_LEFT;
        default:
            break;
    }
    return INT_MAX;
}

static Detector *createDetector(YADConfig &config)
{
    return new TTDetector(config);
}

static const char *getName()
{
    return "YADetectorTT";
}

static void setLog(Log log)
{
    
}

static bool sniffDetector(YADConfig &config, float *confidence)
{
    if (!confidence) {
        YLOGE("confidence is null");
        return false;
    }
    
    YADPixelFormat pixFormat = (YADPixelFormat)std::stoi(config[kYADPixFormat]);
    YADDataType dataType = (YADDataType)std::stoi(config[kYADDataType]);
    
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
    plugin->getName = YAD::getName;
    plugin->setLog = YAD::setLog;
    plugin->sniff = YAD::sniffDetector;
    plugin->createDetector = YAD::createDetector;
    return plugin;
}

#endif // WITH_YAD_TT
