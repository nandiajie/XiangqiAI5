# CMakeLists.txt - NDK 编译配置
cmake_minimum_required(VERSION 3.18.1)
project("xiangqiengine")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译优化: 移动端追求执行速度
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 通用优化标志
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -fno-exceptions -fno-rtti -fvisibility=hidden")

# ARM64 (arm64-v8a): 启用 NEON (默认已启用), 添加 architecture tuning
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+simd")
endif()

# ARMv7 (armeabi-v7a): 启用 NEON
if(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mfpu=neon -mfloat-abi=softfp")
endif()

# x86_64 (用于模拟器)
if(ANDROID_ABI STREQUAL "x86_64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
endif()

# Position Independent Code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# 引擎源文件
set(ENGINE_SOURCES
    types.h
    zobrist.h
    zobrist.cpp
    position.h
    position.cpp
    movegen.h
    movegen.cpp
    evaluate.h
    evaluate.cpp
    tt.h
    tt.cpp
    search.h
    search.cpp
    engine_jni.cpp
)

add_library(xiangqiengine SHARED ${ENGINE_SOURCES})

# 链接 Android 日志库
find_library(log-lib log)
target_link_libraries(xiangqiengine ${log-lib})

# 移除符号信息减小 .so 体积
set_target_properties(xiangqiengine PROPERTIES
    LINK_FLAGS_RELEASE "-Wl,-s"
)
