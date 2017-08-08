cmake_minimum_required(VERSION 3.1)
project(lbus)
aux_source_directory(src lbus_src)
set(CMAKE_CXX_STANDARD 14)

include(CheckCXXCompilerFlag)
check_cxx_compiler_flag("-std=c++11" SUPPORT_CXX11)
if (NOT SUPPORT_CXX11)
    message(FATAL_ERROR "compiler not support c++11")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src)

set(CMAKE_MACOSX_RPATH 0)

find_package(luna REQUIRED)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS} -undefined dynamic_lookup")
endif()

set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

add_library(lbus SHARED ${lbus_src})

install(TARGETS lbus RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)


