cmake_minimum_required(VERSION 3.10)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

file(GLOB SOURCES "*.cpp")
file(GLOB HEADERS "*.hpp")

set(BUILD_DIR ../build)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${BUILD_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BUILD_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BUILD_DIR}/bin)
set(LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BUILD_DIR}/bin)

# Check if there are all necessary external dependencies
# Boost includes
if(DEFINED ENV{BOOST_DIR_INCLUDE})
	list(APPEND EXTRA_INCLUDES $ENV{BOOST_DIR_INCLUDE})
else()
    message(FATAL_ERROR "No BOOST_DIR_INCLUDE environment variable!")
endif()

add_library(DnsResolver STATIC ${HEADERS} ${SOURCES})
target_include_directories(DnsResolver PUBLIC 
                           "${EXTRA_INCLUDES}")