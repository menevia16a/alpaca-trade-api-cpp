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

# Public include for project headers
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

# JSON
find_package(jsoncpp REQUIRED)
target_link_libraries(alpaca PRIVATE jsoncpp)

# libwebsockets
if(WIN32)
    # vcpkg provides libwebsockets config
    find_package(libwebsockets CONFIG REQUIRED)

    # Prefer shared, fallback to static if that’s what’s installed
    if(TARGET websockets_shared)
        target_link_libraries(alpaca PRIVATE websockets_shared)
    elseif(TARGET websockets_static)
        target_link_libraries(alpaca PRIVATE websockets_static)
    else()
        message(FATAL_ERROR "No libwebsockets target found (expected websockets_shared or websockets_static)")
    endif()
else()
    # Linux (system-installed libwebsockets)
    find_path(LWS_INCLUDE_DIR NAMES libwebsockets.h PATHS /usr/include /usr/local/include)
    find_library(LWS_LIB NAMES websockets libwebsockets PATHS /usr/lib /usr/local/lib)

    if(NOT LWS_INCLUDE_DIR OR NOT LWS_LIB)
        message(FATAL_ERROR "libwebsockets not found!")
    endif()

    target_include_directories(alpaca PRIVATE ${LWS_INCLUDE_DIR})
    target_link_libraries(alpaca PRIVATE ${LWS_LIB})
endif()
