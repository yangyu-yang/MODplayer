cmake_minimum_required(VERSION 3.10)
project(MediaServer VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找 FFmpeg 库
find_package(PkgConfig REQUIRED)

pkg_check_modules(AVCODEC REQUIRED libavcodec)
pkg_check_modules(AVFORMAT REQUIRED libavformat)
pkg_check_modules(AVUTIL REQUIRED libavutil)
pkg_check_modules(AVFILTER libavfilter)
pkg_check_modules(SWSCALE libswscale)

message(STATUS "Found FFmpeg libraries:")
message(STATUS "  libavcodec: ${AVCODEC_VERSION}")
message(STATUS "  libavformat: ${AVFORMAT_VERSION}")
message(STATUS "  libavutil: ${AVUTIL_VERSION}")

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${AVCODEC_INCLUDE_DIRS}
    ${AVFORMAT_INCLUDE_DIRS}
    ${AVUTIL_INCLUDE_DIRS}
)

# 查找所有源文件
file(GLOB_RECURSE SRC_FILES 
    "src/*.cpp"
)

# 计算源文件数量
list(LENGTH SRC_FILES SRC_COUNT)
if(SRC_COUNT EQUAL 0)
    message(FATAL_ERROR "No source files found!")
endif()

message(STATUS "Found ${SRC_COUNT} source files")
foreach(SRC_FILE ${SRC_FILES})
    message(STATUS "  ${SRC_FILE}")
endforeach()

# 创建可执行文件
add_executable(media_server ${SRC_FILES})

# 链接库
target_link_libraries(media_server
    pthread
    ${AVCODEC_LIBRARIES}
    ${AVFORMAT_LIBRARIES}
    ${AVUTIL_LIBRARIES}
)

# 添加额外的FFmpeg库（如果存在）
if(AVFILTER_FOUND)
    target_link_libraries(media_server ${AVFILTER_LIBRARIES})
    message(STATUS "Linked libavfilter")
endif()

if(SWSCALE_FOUND)
    target_link_libraries(media_server ${SWSCALE_LIBRARIES})
    message(STATUS "Linked libswscale")
endif()

# 编译器选项
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(media_server PRIVATE 
        -Wall 
        -Wextra 
        -Wpedantic
        -Wno-deprecated-declarations
    )
endif()

# 设置可执行文件属性
set_target_properties(media_server PROPERTIES
    OUTPUT_NAME media_server
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

message(STATUS "Build configuration completed successfully!")