cmake_minimum_required(VERSION 2.8)

#项目名称
project(demo)

MESSAGE(STATUS "PROJECT")

#设置交叉编译器
option(ARM_CROSS_COMPILE "Arm cross compiling option" ON)
if(ARM_CROSS_COMPILE)
    MESSAGE(STATUS "Arm cross compiling...")
    # 设置目标系统
    set(CMAKE_SYSTEM_NAME Linux)
    set(CMAKE_SYSTEM_PROCESSOR arm)

    # 设置工具链目录
    set(TOOL_CHAIN_DIR /opt/hisi-linux/x86-arm/arm-himix100-linux)
    set(TOOL_CHAIN_INCLUDE 
        ${TOOL_CHAIN_DIR}/include
        )
    set(TOOL_CHAIN_LIB 
        ${TOOL_CHAIN_DIR}/lib
        )
    # 设置编译器
    set(CMAKE_C_COMPILER "arm-himix100-linux-gcc")
    set(CMAKE_CXX_COMPILER "arm-himix100-linux-g++")

    MESSAGE(STATUS "CMAKE_C_COMPILER:" ${CMAKE_C_COMPILER})
    MESSAGE(STATUS "CMAKE_CXX_COMPILER:" ${CMAKE_CXX_COMPILER})
    # 设置Cmake查找主路径
    set(CMAKE_FIND_ROOT_PATH ${TOOL_CHAIN_DIR})

    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    # 只在指定目录下查找库文件
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    # 只在指定目录下查找头文件
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    # 只在指定目录下查找依赖包
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

    include_directories(
        ${TOOL_CHAIN_DIR}/include
        )

    set(CMAKE_INCLUDE_PATH 
        ${TOOL_CHAIN_INCLUDE}
        )

    set(CMAKE_LIBRARY_PATH 
        ${TOOL_CHAIN_LIB}
        )
endif(ARM_CROSS_COMPILE)
#添加头文件的路径 
include_directories(include) 
include_directories(src/include)
include_directories(src/common)
include_directories(src/util)
include_directories(src/app)
include_directories(src/mppclass)
include_directories(src/mavlinkclass)

include_directories(src/thirdparty/rtsp)
include_directories(src/thirdparty/CSerialPort)
include_directories(stc/thirdparty/mavlink)
include_directories(stc/thirdparty/mavlink/common)

#输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

#添加动态连接库的路径
link_directories(${CMAKE_SOURCE_DIR}/lib) 

#添加编译选项
add_definitions(-Wall -O2 -g -Wpsabi -fno-aggressive-loop-optimizations)
add_definitions(-Dhi3516ev300 -DHI_XXXX -DISP_V2)
add_definitions(-mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4)
add_definitions(-ffunction-sections -fdata-sections -fstack-protector -fPIC)
add_definitions(-DHI_RELEASE -Wno-error=implicit-function-declaration)
add_definitions(-DVER_X=1 -DVER_Y=0 -DVER_Z=0 -DVER_P=0 -DVER_B=10)
add_definitions(-DUSER_BIT_32 -DKERNEL_BIT_32 -Wno-date-time)
add_definitions(-DSENSOR0_TYPE=BT1120_2M_30FPS_8BIT)
add_definitions(-DSENSOR1_TYPE=SONY_IMX290_SLAVE_MIPI_2M_60FPS_10BIT)
add_definitions(-DSENSOR2_TYPE=SONY_IMX290_SLAVE_MIPI_2M_60FPS_10BIT)
add_definitions(-DSENSOR3_TYPE=SONY_IMX290_SLAVE_MIPI_2M_60FPS_10BIT)
add_definitions(-DSENSOR4_TYPE=SONY_IMX334_MIPI_8M_30FPS_12BIT)
add_definitions(-DHI_ACODEC_TYPE_INNER -DHI_ACODEC_TYPE_HDMI -DSMP)
add_definitions(-shared)
add_definitions(-nostartfiles)
#添加源文件路径
aux_source_directory(${CMAKE_SOURCE_DIR}/src/app SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/common/ SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/util SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/mppclass SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/mavlinkclass SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/thirdparty/rtsp SRC_LIST)
aux_source_directory(${CMAKE_SOURCE_DIR}/src/. SRC_LIST)

#add_executable(想要生成的程序名 源文件)
add_executable(demo  ${SRC_LIST})
#添加编译时的依赖库
target_link_libraries(${PROJECT_NAME}  -lpthread -lrt -ldl -lm )
target_link_libraries(${PROJECT_NAME} lib_hiae.a libisp.a lib_hidehaze.a lib_hidrc.a
libsns_sc4236.a   lib_hildci.a lib_hiawb.a libsns_imx327.a libsns_imx307.a libsns_imx307_2l.a
libive.a libmpi.a libmd.a  libsecurec.a  libsns_imx327_2l.a libsns_imx335.a libsns_sc4236.a
libVoiceEngine.a  libsns_sc2235.a libsns_sc2231.a libsns_f37.a libsns_sc3235.a libsns_gc2053.a
libupvqe.a  libsns_gc2053_forcar.a libsns_os05a.a libmpi.a libdnvqe.a libcserialport.a)
