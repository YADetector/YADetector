#ifdef WITH_YAD_TT

//
//  YADetectorTT.cpp
//  YAD
//

//#define LOG_NDEBUG 0
#define LOG_TAG "YADTT"
#include "LogMacros.h"

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

namespace yad {

typedef int (*CreateHandlerFnPtr)(unsigned long long flags, char const *param_path, void **p_handle);
typedef void (*AddExtraModelFnPtr)(void *handle, unsigned long long flags, char const *param_path);
typedef int (*DoPredictFnPtr)(void *handle, unsigned char const *baseAddress, unsigned int pixelFormatType,signed int width, signed int height, int stride, int screenOrient, unsigned long long flags, tt_faces_info_t *facesInfo);
typedef void (*ReleaseHandleFnPtr)(void *handle);

typedef struct {
    CreateHandlerFnPtr CreateHandler;
    AddExtraModelFnPtr AddExtraModel;
    DoPredictFnPtr DoPredict;
    ReleaseHandleFnPtr ReleaseHandle;
} TTSymbolTable;

static void *s_lib_handle = nullptr;
static TTSymbolTable s_symbol_table;

TTDetector::TTDetector(YADConfig &config) :
    init_check_(YAD_NO_INIT),
    handle_(nullptr),
    max_face_num_(YAD_MAX_FACE_NUM)
{
    YLOGV("YADetectorTT ctor");
    
    max_face_num_ = std::stoi(config[kYADMaxFaceCount]);
    max_face_num_ = std::min(max_face_num_, YAD_MAX_FACE_NUM);
    // 有些情况下，调用者希望定制资源，调用者可以在config增加字段实现该功能，key: 默认资源名，value: 资源指定的路径
    // 但是需要注意的是，仍旧会优先使用App本身的资源
    lib_path_ = config[YAD_TT_LIB_NAME];
    face_model_path_ = config[YAD_TT_FACE_MODEL_NAME];
    face_extra_model_path_ = config[YAD_TT_FACE_EXTRA_MODLE_NAME];

    static std::once_flag flag;
    std::call_once(flag, [&] {
        loadSymbols();
    });
    if (s_lib_handle == nullptr) {
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
    int ret = s_symbol_table.CreateHandler(flags, modelPath.c_str(), &handle_);
    if (ret) {
        YLOGE("create handler failed, err: %d", ret);
        init_check_ = YAD_HANDLE_INVALID;
        return;
    }
    
    flags = 0x900;
    s_symbol_table.AddExtraModel(handle_, flags, extraModelPath.c_str());
    
    init_check_ = YAD_OK;
}

TTDetector::~TTDetector()
{
    YLOGV("YADetectorTT dtor");
    
    if (handle_) {
        s_symbol_table.ReleaseHandle(handle_);
        handle_ = nullptr;
    }
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
    std::string libPath = getLibPath();
    if (!fileExists(libPath)) {
        YLOGE("lib not found, path: %s", libPath.c_str());
        return false;
    }
    s_lib_handle = dlopen(libPath.c_str(), RTLD_NOW);
    if (s_lib_handle == nullptr) {
        YLOGE("dlopen failed");
        return false;
    }

    s_symbol_table.CreateHandler = (CreateHandlerFnPtr)dlsym(s_lib_handle, "FS_CreateHandler");
    if (s_symbol_table.CreateHandler == nullptr) {
        YLOGE("dlsym 1 failed");
        goto fail;
    }
    
    s_symbol_table.AddExtraModel = (AddExtraModelFnPtr)dlsym(s_lib_handle, "FS_AddExtraModel");
    if (s_symbol_table.AddExtraModel == nullptr) {
        YLOGE("dlsym 2 failed");
        goto fail;
    }

    s_symbol_table.DoPredict = (DoPredictFnPtr)dlsym(s_lib_handle, "FS_DoPredict");
    if (s_symbol_table.DoPredict == nullptr) {
        YLOGE("dlsym 3 failed");
        goto fail;
    }

    s_symbol_table.ReleaseHandle = (ReleaseHandleFnPtr)dlsym(s_lib_handle, "FS_ReleaseHandle");
    if (s_symbol_table.ReleaseHandle == nullptr) {
        YLOGE("dlsym 4 failed");
        goto fail;
    }
    
    return true;
    
fail:
    dlclose(s_lib_handle);
    s_lib_handle = nullptr;
    return false;
}

std::string TTDetector::mainBundlePath()
{
    static std::once_flag flag;
    static std::string path;
    std::call_once(flag, [&] {
        path = initMainBundlePath();
    });
    return path;
}

std::string TTDetector::initMainBundlePath()
{
    CFBundleRef bundleRef; // 该引用无需释放
    CFURLRef urlRef = NULL;
    CFStringRef stringRef = NULL;
    std::string errmsg;
    char path[PATH_MAX];
    
    memset(path, 0x0, PATH_MAX);
    
    if (!(bundleRef = CFBundleGetMainBundle())) {
        errmsg = "CFBundleGetMainBundle";
        goto bail;
    }
    if (!(urlRef = CFBundleCopyBundleURL(bundleRef))) {
        errmsg = "CFBundleCopyBundleURL";
        goto bail;
    }
    if (!(stringRef = CFURLCopyFileSystemPath(urlRef, kCFURLPOSIXPathStyle))) {
        errmsg = "CFURLCopyFileSystemPath";
        goto bail;
    }
    if (!CFStringGetCString(stringRef, path, PATH_MAX, kCFStringEncodingUTF8)) {
        errmsg = "CFStringGetCString";
        goto bail;
    }

bail:
    if (stringRef) {
        CFRelease(stringRef);
    }
    if (urlRef) {
        CFRelease(urlRef);
    }
    if (!errmsg.empty()) {
        throw std::runtime_error(errmsg);
    } else {
        return std::string(path);
    }
}

std::string TTDetector::getAppLibDirectory()
{
    return mainBundlePath() + "/Frameworks";
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
    int ret = s_symbol_table.DoPredict(handle_, baseAddress, pixelFormat, detectImage->width, detectImage->height, detectImage->stride, orientation, flags, (tt_faces_info_t *)&facesInfo);
    if (ret) {
        YLOGE("DoPredict failed, ret: %d", ret);
        CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
        return YAD_DETECT_FAILED;
    }
    
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);
    
    //YLOGD("DoPredict succuss, num_faces: %d", facesInfo.num_faces);
    
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

}; // namespace yad

yad::Plugin *createYADetectorTTPlugin()
{
    yad::Plugin *plugin = new yad::Plugin;
    plugin->getName = yad::getName;
    plugin->setLog = yad::setLog;
    plugin->sniff = yad::sniffDetector;
    plugin->createDetector = yad::createDetector;
    return plugin;
}

#endif // WITH_YAD_TT
