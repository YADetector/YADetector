//
//  YADetector.h
//  YAD
//

#ifndef YAD_DETECTOR_H
#define YAD_DETECTOR_H

#include <stdarg.h>
#include <errno.h>
#include <string>
#include <unordered_map>

// 人脸关键点Landmark使用106点，同商汤科技，face++

#define YAD_FACE_LANDMARK_NUM   106 // 人脸关键点个数
#define YAD_MAX_FACE_NUM        10  // 最大人脸个数

#ifdef __cplusplus
extern "C" {
#endif

// 错误码
enum {
    // 公共错误
    YAD_OK                  = 0,      // Preferred constant for checking success.
    YAD_NO_ERROR            = YAD_OK, // Deprecated synonym for `OK`. Prefer `OK` because it doesn't conflict with Windows.

    YAD_UNKNOWN_ERROR       = (-2147483647-1), // INT32_MIN value

    YAD_NO_MEMORY           = -ENOMEM,
    YAD_INVALID_OPERATION   = -ENOSYS,
    YAD_BAD_VALUE           = -EINVAL,
    YAD_BAD_TYPE            = (YAD_UNKNOWN_ERROR + 1),
    YAD_NAME_NOT_FOUND      = -ENOENT,
    YAD_PERMISSION_DENIED   = -EPERM,
    YAD_NO_INIT             = -ENODEV,
    YAD_ALREADY_EXISTS      = -EEXIST,
    YAD_DEAD_OBJECT         = -EPIPE,
    YAD_FAILED_TRANSACTION  = (YAD_UNKNOWN_ERROR + 2),
#if !defined(_WIN32)
    YAD_BAD_INDEX           = -EOVERFLOW,
    YAD_NOT_ENOUGH_DATA     = -ENODATA,
    YAD_WOULD_BLOCK         = -EWOULDBLOCK,
    YAD_TIMED_OUT           = -ETIMEDOUT,
    YAD_UNKNOWN_TRANSACTION = -EBADMSG,
#else
    YAD_BAD_INDEX           = -E2BIG,
    YAD_NOT_ENOUGH_DATA     = (YAD_UNKNOWN_ERROR + 3),
    YAD_WOULD_BLOCK         = (YAD_UNKNOWN_ERROR + 4),
    YAD_TIMED_OUT           = (YAD_UNKNOWN_ERROR + 5),
    YAD_UNKNOWN_TRANSACTION = (YAD_UNKNOWN_ERROR + 6),
#endif
    YAD_FDS_NOT_ALLOWED     = (YAD_UNKNOWN_ERROR + 7),
    YAD_UNEXPECTED_NULL     = (YAD_UNKNOWN_ERROR + 8),
    
    // 私有错误
    YAD_DETECT_ERROR_BASE       = -1000,
    
    YAD_SYMBOLS_NOT_LOADED      = YAD_DETECT_ERROR_BASE,
    YAD_MODEL_NOT_FOUND         = YAD_DETECT_ERROR_BASE - 1,
    YAD_HANDLE_INVALID          = YAD_DETECT_ERROR_BASE - 2,
    YAD_FORMAT_UNSUPPORTED      = YAD_DETECT_ERROR_BASE - 3,
    YAD_ROTATE_UNSUPPORTED      = YAD_DETECT_ERROR_BASE - 4,
    YAD_DETECT_FAILED           = YAD_DETECT_ERROR_BASE - 5,
};

// 点
typedef struct YADPoint2i {
    int x, y;
} YADPoint2i;

// 点
typedef struct YADPoint2f {
    float x, y;
} YADPoint2f;

/// 矩形
typedef struct YADRecti {
    int x, y, w, h; // (x, y)左上角, w宽，h高
} YADRecti;

/// 矩形
typedef struct YADRectf {
    float x, y, w, h; // (x, y)左上角, w宽，h高
} YADRectf;

// 矩形
typedef struct YADBboxi {
    int x1, y1, x2, y2; // (x1, y1)左上角，(x2, y2)右下角
} YADBboxi;

// 矩形
typedef struct YADBboxf {
    float x1, y1, x2, y2; // (x1, y1)左上角，(x2, y2)右下角
} YADBboxf;

// 图像像素格式，模仿ffmpeg
typedef enum YADPixelFormat {
    YAD_PIX_FMT_NONE = -1,
    YAD_PIX_FMT_NV21,
    YAD_PIX_FMT_NV12,
    YAD_PIX_FMT_BGR888,
    YAD_PIX_FMT_RGB888,
    YAD_PIX_FMT_BGRA8888,
    YAD_PIX_FMT_RGBA8888,
    YAD_PIX_FMT_BGR565,
    YAD_PIX_FMT_RGB565,
    YAD_PIX_FMT_MAX,
} YADPixelFormat;

// 旋转模式
typedef enum YADRotateMode {
    YAD_ROTATE_0     = 0,    // 无旋转
    YAD_ROTATE_90    = 90,   // 顺时针旋转90度
    YAD_ROTATE_180   = 180,  // 顺时针旋转180度
    YAD_ROTATE_270   = 270,  // 顺时针旋转270度
} YADRotateMode;

typedef enum YADDataType {
    YAD_DATA_TYPE_NONE = -1,
    YAD_DATA_TYPE_RAW ,             // 裸数据，数据为 char *
    YAD_DATA_TYPE_IOS_UIIMAGE,      // iOS UIImage, 实际类型：UIImage
    YAD_DATA_TYPE_IOS_PIXEL_BUFFER, // iOS PixelBuffer, 实际类型：CVPixelBufferRef
    YAD_DATA_TYPE_MAX
} YADImageType;

// 检测图像
typedef struct YADDetectImage {
    YADPixelFormat format;  // 数据格式
    YADDataType type;       // 数据类型
    void *data;             // 数据地址，根据YADDataType需要强制转化
    int width;              // 宽
    int height;             // 高
    int stride;             // 步长，单位为字节
} YADDetectImage;

// 检测信息
typedef struct YADDetectInfo {
    YADRotateMode rotate_mode;
} YADDetectInfo;

typedef struct YADFaceInfo {
    int track_id;   // 跟踪id
    YADRectf rect;  // 人脸区域，注意不是归一化到0-1的值
    YADPoint2f landmarks[YAD_FACE_LANDMARK_NUM];    // 人脸关键点坐标，注意不是归一化到0-1的值
    float visibilites[YAD_FACE_LANDMARK_NUM];       // 遮挡信息，未遮挡1，遮挡0，该字段未使用
    float yaw;      // 绕z轴角度
    float pitch;    // 绕y轴角度
    float roll;     // 绕x轴角度
} YADFaceInfo;

// feature信息组合
typedef struct YADFeatureInfo {
    int num_faces;
    YADFaceInfo faces[YAD_MAX_FACE_NUM];
    
    // ...预留，可能还有手势识别等feature
} YADFeatureInfo;

typedef std::unordered_map<std::string, std::string> YADConfig;

#define kYADMaxFaceCount    "max_face_count"    // value: int
#define kYADPixFormat       "pix_format"        // value: YADPixelFormat
#define kYADDataType        "data_type"         // value: YADDataType

#if defined(__cplusplus)
}
#endif

namespace yad {

// 检测类
class Detector {
public:
    // 是否存在Detector插件
    static bool Exists();
    // 创建Detector
    static Detector *Create(YADConfig &config);
    
    Detector() {}
    Detector(YADConfig &config) {}
    virtual ~Detector() {}
    // 对象构造后是否正常，返回0正常，负数异常
    virtual int initCheck() const = 0;
    // 检测函数
    virtual int detect(YADDetectImage *detectImage, YADDetectInfo *detectInfo, YADFeatureInfo *featureInfo) = 0;

private:
    Detector(const Detector &);
    Detector &operator=(const Detector &);
};

typedef enum {
    YAD_LOG_LEVEL_VERBOSE,
    YAD_LOG_LEVEL_DEBUG,
    YAD_LOG_LEVEL_INFO,
    YAD_LOG_LEVEL_WARN,
    YAD_LOG_LEVEL_ERROR,
    YAD_LOG_LEVEL_FAULT,
} YADLogLevel;

typedef void (*Log)(int level, const char *tag, const char *file, int line, const char *function, const char *format, va_list args);

// 获取名字
typedef const char *(*GetNameFunc)();
typedef void (*SetLogFunc)(Log log);
// 嗅探。插件根据参数返回confidence。
// confidence范围：0~1.0，该值越高，插件优先级就越高。主要用于在多个都能实现功能的插件中，选取优化最好的插件。
typedef bool (*SniffFunc)(YADConfig &config, float *confidence);
// 检测
typedef Detector *(*CreateDetectorFunc)(YADConfig &config);

// 插件类，框架支持第三方插件，用户可以扩展自定义。
// 用户需要以Detector为基类，派生一个自己的XXXDetector。并实现和导出"createYADetectorPlugin"函数。
// 插件管理器会搜索通用的动态库目录，查找符合库命名规则的动态库，加载库内符号"createYADetectorPlugin"(该函数返回Plugin对象)。
// iOS的库命名规则：YADetector(XXX).framework，如YADetectorXYZ.framework
struct Plugin {
    GetNameFunc getName;
    SetLogFunc setLog;
    SniffFunc sniff;
    CreateDetectorFunc createDetector;
};

}; // namespace yad

#endif /* YAD_DETECTOR_H */

