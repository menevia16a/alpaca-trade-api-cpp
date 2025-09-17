cmake_minimum_required(VERSION 3.5)

project(alpaca LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Gather source files
file(GLOB ALPACA_SOURCES
    "alpaca/*.cpp"
    "alpaca/*.h"
)

# Create a static library
add_library(alpaca STATIC ${ALPACA_SOURCES})

# Cross-platform include directories
target_include_directories(alpaca PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# --- Dependencies ---

# OpenSSL
find_package(OpenSSL REQUIRED)
target_link_libraries(alpaca PRIVATE OpenSSL::SSL OpenSSL::Crypto)

# CURL
find_package(CURL REQUIRED)
target_link_libraries(alpaca PRIVATE CURL::libcurl)

# glog
find_package(glog REQUIRED)
target_link_libraries(alpaca PRIVATE glog::glog)

# JSON (using nlohmann/json or jsoncpp)
find_package(jsoncpp REQUIRED)
target_link_libraries(alpaca PRIVATE jsoncpp)

# libwebsockets (platform-specific handling)
if(WIN32)
    # Assume installed via vcpkg or manually
    find_path(LWS_INCLUDE_DIR "libwebsockets.h")
    find_library(LWS_LIB "libwebsockets")
else()
    # Linux/macOS
    find_path(LWS_INCLUDE_DIR NAMES libwebsockets.h PATHS /usr/include /usr/local/include)
    find_library(LWS_LIB NAMES websockets libwebsockets PATHS /usr/lib /usr/local/lib)
endif()

if(NOT LWS_INCLUDE_DIR OR NOT LWS_LIB)
    message(FATAL_ERROR "libwebsockets not found!")
endif()

target_include_directories(alpaca PRIVATE ${LWS_INCLUDE_DIR})
target_link_libraries(alpaca PRIVATE ${LWS_LIB})
