# FindLlamaCpp.cmake
# 查找预编译的 llama.cpp（头文件 + 静态库）。
# 默认情况下本项目通过 add_subdirectory(third_party/llama.cpp) 直接编译源码，
# 本 find 模块仅作为备选：当设置了 STUDENTAI_USE_SYSTEM_LLAMA=ON 时使用。
#
# 用法:
#   find_package(LlamaCpp)
#   target_link_libraries(foo PRIVATE LlamaCpp::llama)
#
# 返回变量:
#   LlamaCpp_FOUND       是否找到
#   LlamaCpp_INCLUDE_DIRS llama.h 所在目录
#   LlamaCpp_LIBRARY      静态库路径（llama.lib）

if(TARGET LlamaCpp::llama)
    set(LlamaCpp_FOUND TRUE)
    return()
endif()

find_path(LlamaCpp_INCLUDE_DIR
    NAMES llama.h
    HINTS ${LLAMA_CPP_ROOT} $ENV{LLAMA_CPP_ROOT}
    PATH_SUFFIXES include
)

find_library(LlamaCpp_LIBRARY
    NAMES llama llama_static
    HINTS ${LLAMA_CPP_ROOT} $ENV{LLAMA_CPP_ROOT}
    PATH_SUFFIXES build/bin/Release build/bin/Debug build/bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LlamaCpp
    REQUIRED_VARS LlamaCpp_INCLUDE_DIR LlamaCpp_LIBRARY
)

if(LlamaCpp_FOUND AND NOT TARGET LlamaCpp::llama)
    add_library(LlamaCpp::llama STATIC IMPORTED)
    set_target_properties(LlamaCpp::llama PROPERTIES
        IMPORTED_LOCATION ${LlamaCpp_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${LlamaCpp_INCLUDE_DIR}
    )
endif()

mark_as_advanced(LlamaCpp_INCLUDE_DIR LlamaCpp_LIBRARY)
