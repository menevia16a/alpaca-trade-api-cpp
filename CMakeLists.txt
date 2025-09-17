cmake_minimum_required(VERSION 3.5)

project(alpaca LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB ALPACA_SOURCES
    "alpaca/*.cpp"
    "alpaca/*.h"
)

set(SYSTEM_INCLUDE_DIR /usr/include)
set(SYSTEM_LIB_DIR /usr/lib)

# Include the correct directory for headers
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${SYSTEM_INCLUDE_DIR})

# Create a static library
add_library(alpaca STATIC ${ALPACA_SOURCES})

# Link necessary libraries
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(glog REQUIRED)
find_package(jsoncpp REQUIRED)

target_include_directories(alpaca PRIVATE ${SYSTEM_LIB_DIR}/cmake/httplib)
target_link_libraries(alpaca PRIVATE ${SYSTEM_LIB_DIR}/libwebsockets.so)
target_link_libraries(alpaca PRIVATE OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(alpaca PRIVATE CURL::libcurl)
target_link_libraries(alpaca PRIVATE glog::glog)
target_link_libraries(alpaca PRIVATE jsoncpp)
