# 简化的CMakeLists.txt - 直接强制MinGW交叉编译
cmake_minimum_required(VERSION 3.16)

# 强制设置交叉编译器（在project()之前）
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

project(xyz_monitor VERSION 1.1.0 LANGUAGES CXX)

# C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源文件
file(GLOB SOURCES "src/*.cpp")
file(GLOB HEADERS "src/*.h")

# 资源文件
set(RESOURCE_FILE "resources/xyz_monitor.rc")

# 创建可执行文件
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${RESOURCE_FILE})
    add_executable(xyz_monitor WIN32 ${SOURCES} ${RESOURCE_FILE})
    message(STATUS "Building with resources")
else()
    add_executable(xyz_monitor WIN32 ${SOURCES})
    message(STATUS "Building without resources")
endif()

# 编译选项（复制你Makefile的设置）
target_compile_options(xyz_monitor PRIVATE 
    -Wall -Wextra -O2 -static-libgcc -static-libstdc++
)

# 链接选项
target_link_options(xyz_monitor PRIVATE 
    -static -mwindows
)

# 库链接
target_link_libraries(xyz_monitor PRIVATE
    user32 kernel32 shell32 gdi32
)

# 包含目录
target_include_directories(xyz_monitor PRIVATE src)

# 输出目录
set_target_properties(xyz_monitor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 创建目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/temp)

message(STATUS "Cross-compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Target system: ${CMAKE_SYSTEM_NAME}")