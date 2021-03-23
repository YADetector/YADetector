# YADetector

Yet another Face Detector。</br>

## 背景
一些项目会需要人脸识别的结果，比如人脸关键点和3D姿态。然后才能进一步对图像处理。</br>
为了对图像处理做快速验证，本项目提供了人脸检测的接入层。该接入层会加载第三方插件，实现人脸检测逻辑。</br>
</br>
该项目有如下特点：</br>
该人脸检测结果是基于通用的106点的人脸关键点，效果类似"商汤"的检测结果。</br>
当前系统有 YADetectorTT 和 YADetectorFF 两个实现，你也可以根据 YADetector.h 实现自己的插件，接入本项目。至于不同的关键点个数和坐标，
可以通过简单的数学运算得出本项目一致的效果。</br>

## 安装

使用 CocoaPods 集成，参考项目目录中 Example/Podfile 文件。

## 使用

使用方法详见 Demo, 主要关注项目目录中 YADetector.h 即可。
