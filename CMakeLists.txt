cmake_minimum_required(VERSION 3.10)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Specify option to include optional libraries
option(USE_ALGORITHMS "Use library with Algorithms" ON)
option(USE_DNS_RESOLVER "Use library with DnsResolver" ON)

# Specify project dirs
set(BUILD_DIR "build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${BUILD_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${BUILD_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BUILD_DIR}/bin")
set(LIBRARY_OUTPUT_DIRECTORY_DEBUG "${BUILD_DIR}/bin")

# Create project
project(TestTask VERSION 1.0)

# Add configuration file.
configure_file(config.h.in config.h)

# Check if there are all necessary external dependencies
# Boost includes
if(DEFINED ENV{BOOST_DIR_INCLUDE})
	list(APPEND EXTRA_INCLUDES $ENV{BOOST_DIR_INCLUDE})
else()
    message(FATAL_ERROR "No BOOST_DIR_INCLUDE environment variable!")
endif()

# Boost libs
if(DEFINED ENV{BOOST_DIR_LIB})
	file(GLOB BOOST_DEBUG_LIBS "$ENV{BOOST_DIR_LIB}/*gd-x64*")
	file(GLOB BOOST_RELEASE_LIBS "$ENV{BOOST_DIR_LIB}/*mt-x64*")
else()
    message(FATAL_ERROR "No BOOST_DIR_LIB environment variable!")
endif()

# Optionally include projects
if(USE_ALGORITHMS)
    message(STATUS "Add Algorithms subproject")
	add_subdirectory(Algorithms)
	list(APPEND EXTRA_LIBS Algorithms)
	list(APPEND EXTRA_INCLUDES "${CMAKE_SOURCE_DIR}/Algorithms")
endif()

if(USE_DNS_RESOLVER)
	message(STATUS "Add DnsResolver subproject")
	add_subdirectory(DnsResolver)
	list(APPEND EXTRA_LIBS DnsResolver)
	list(APPEND EXTRA_INCLUDES "${CMAKE_SOURCE_DIR}/DnsResolver")
endif()

# Add windows libs
list(APPEND EXTRA_LIBS Ws2_32.lib Dnsapi.lib Ntdll.dll)

# Add executable
add_executable(TestTask main.cpp)

# Add linked libraries
target_link_libraries(TestTask PUBLIC 
		          "${EXTRA_LIBS}"
		debug     "${BOOST_DEBUG_LIBS}"
		optimized "${BOOST_RELEASE_LIBS}"
)

# Add target include directories
target_include_directories(TestTask PUBLIC 
		"${CMAKE_BINARY_DIR}"
		"${EXTRA_INCLUDES}"
)