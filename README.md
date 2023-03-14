# 项目简介

> 使用海思MPP 实现功能包括以下 

* 视频推流(RTSP)

* 图像抓拍(JEPG)

* 视频录制(H.264)

* 相机变焦(Mavlink串口 通道9)


# 注意事项

* 支持在使用 arm-himix100-linux 编译器的海思平台上运行

* 如果不是 Hi3516EV300 的话可能需要修改 CMakeLists.txt 里的编译选项

* 使用时需要注意挂载SD卡,指定图片和视频路径

* RTSP 默认地址 rtsp://192.168.144.64:554/live0.264

# 使用方法

1. 编译项目

   `cd build`

   `cmake ..`

   `make`
   
2. 运行文件生成在bin文件夹下