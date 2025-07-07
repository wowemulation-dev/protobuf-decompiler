cmake_minimum_required(VERSION 3.28)

# Set policy for FindBoost module
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

project(protobuf_decompiler
    VERSION 1.0.0
    DESCRIPTION "Reconstructs .proto files from Google protobuf reflection strings"
    LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# Enable useful warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

# Include GNUInstallDirs for standard install paths
include(GNUInstallDirs)
include(ExternalProject)

# Configure Boost to use static libraries (must be set before find_package)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# First, try to find system protobuf and check its version
find_package(Protobuf QUIET)

set(BUILD_PROTOBUF_FROM_SOURCE FALSE)
if(Protobuf_FOUND)
    # Check if the found version is exactly 2.6.1
    if(Protobuf_VERSION)
        if(Protobuf_VERSION VERSION_EQUAL "2.6.1")
            message(STATUS "Found system protobuf version 2.6.1 - using it")
        elseif(WIN32)
            # On Windows, prefer vcpkg protobuf over building from source due to MSVC compatibility
            message(STATUS "Found system protobuf version ${Protobuf_VERSION} on Windows - using it for MSVC compatibility")
        else()
            message(STATUS "Found system protobuf version ${Protobuf_VERSION}, but need 2.6.1 - will build from source")
            set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
        endif()
    else()
        if(WIN32)
            # On Windows, use any found protobuf to avoid MinGW/MSVC compatibility issues
            message(STATUS "Found system protobuf on Windows - using it for MSVC compatibility")
        else()
            # Older FindProtobuf modules might not set Protobuf_VERSION
            message(STATUS "Found system protobuf but couldn't determine version - will build from source")
            set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
        endif()
    endif()
else()
    message(STATUS "System protobuf not found - will build version 2.6.1 from source")
    set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
endif()

if(BUILD_PROTOBUF_FROM_SOURCE)
    message(STATUS "Building protobuf v2.6.1 from source (this may take a while on first build)")
    message(STATUS "Note: This requires autotools (autoconf, automake, libtool)")
    
    # Build protobuf v2.6.1 using ExternalProject
    # Note: protobuf 2.6.1 uses autotools, not CMake
    set(PROTOBUF_PREFIX ${CMAKE_BINARY_DIR}/protobuf-2.6.1)
    
    # Configure autotools commands based on platform
    if(WIN32)
        # On Windows, use MSBuild to build Visual Studio solution for MSVC compatibility
        # First run extract_includes.bat to prepare headers
        set(AUTOTOOLS_CONFIGURE_COMMAND
            ${CMAKE_COMMAND} -E chdir ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects 
            cmd /c extract_includes.bat
        )
        # Build using MSBuild - it will auto-upgrade old project files
        if(CMAKE_BUILD_PARALLEL_LEVEL)
            set(MSBUILD_PARALLEL_FLAG "/m:${CMAKE_BUILD_PARALLEL_LEVEL}")
        else()
            set(MSBUILD_PARALLEL_FLAG "/m")
        endif()
        set(AUTOTOOLS_BUILD_COMMAND
            ${CMAKE_COMMAND} -E chdir ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects
            msbuild protobuf.sln /p:Configuration=Release /p:Platform=x64 ${MSBUILD_PARALLEL_FLAG}
        )
        # Copy built libraries and headers to install location
        set(AUTOTOOLS_INSTALL_COMMAND
            ${CMAKE_COMMAND} -E make_directory ${PROTOBUF_PREFIX}/install/lib ${PROTOBUF_PREFIX}/install/bin ${PROTOBUF_PREFIX}/install/include &&
            ${CMAKE_COMMAND} -E copy_directory ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/include ${PROTOBUF_PREFIX}/install/include &&
            ${CMAKE_COMMAND} -E copy ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/x64/Release/libprotobuf.lib ${PROTOBUF_PREFIX}/install/lib/ &&
            ${CMAKE_COMMAND} -E copy ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/x64/Release/libprotoc.lib ${PROTOBUF_PREFIX}/install/lib/ &&
            ${CMAKE_COMMAND} -E copy ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/x64/Release/protoc.exe ${PROTOBUF_PREFIX}/install/bin/
        )
    elseif(APPLE)
        # On macOS, add flags to suppress deprecated API warnings from protobuf 2.6.1
        set(AUTOTOOLS_CONFIGURE_COMMAND
            cd ${PROTOBUF_PREFIX}/src/protobuf_external &&
            mkdir -p gtest &&
            touch gtest/CMakeLists.txt &&
            autoreconf -f -i -Wall,no-obsolete &&
            ./configure 
                --prefix=${PROTOBUF_PREFIX}/install
                --enable-static
                --disable-shared
                --disable-dependency-tracking
        )
        # Use environment variables to pass compiler flags for the build step
        set(AUTOTOOLS_BUILD_COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL} CXXFLAGS="-Wno-deprecated-declarations" CFLAGS="-Wno-deprecated-declarations")
        set(AUTOTOOLS_INSTALL_COMMAND make install)
    else()
        # On other Unix systems, use standard shell commands
        set(AUTOTOOLS_CONFIGURE_COMMAND
            cd ${PROTOBUF_PREFIX}/src/protobuf_external &&
            mkdir -p gtest &&
            touch gtest/CMakeLists.txt &&
            autoreconf -f -i -Wall,no-obsolete &&
            ./configure 
                --prefix=${PROTOBUF_PREFIX}/install
                --enable-static
                --disable-shared
                --disable-dependency-tracking
                CXX=${CMAKE_CXX_COMPILER}
                CC=${CMAKE_C_COMPILER}
        )
        set(AUTOTOOLS_BUILD_COMMAND make -j${CMAKE_BUILD_PARALLEL_LEVEL})
        set(AUTOTOOLS_INSTALL_COMMAND make install)
    endif()

    ExternalProject_Add(
        protobuf_external
        GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
        GIT_TAG        v2.6.1
        PREFIX         ${PROTOBUF_PREFIX}
        CONFIGURE_COMMAND ${AUTOTOOLS_CONFIGURE_COMMAND}
        BUILD_COMMAND ${AUTOTOOLS_BUILD_COMMAND}
        INSTALL_COMMAND ${AUTOTOOLS_INSTALL_COMMAND}
        BUILD_IN_SOURCE 1
    )
    
    # Set variables for the built protobuf
    set(PROTOBUF_FOUND TRUE)
    set(PROTOBUF_INCLUDE_DIRS ${PROTOBUF_PREFIX}/install/include)
    if(WIN32)
        set(PROTOBUF_LIBRARIES ${PROTOBUF_PREFIX}/install/lib/libprotobuf.lib)
        set(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PREFIX}/install/bin/protoc.exe)
    else()
        set(PROTOBUF_LIBRARIES ${PROTOBUF_PREFIX}/install/lib/libprotobuf.a)
        set(PROTOBUF_PROTOC_EXECUTABLE ${PROTOBUF_PREFIX}/install/bin/protoc)
    endif()
    
    # Create imported target for cleaner linking
    add_library(protobuf::libprotobuf STATIC IMPORTED GLOBAL)
    set_target_properties(protobuf::libprotobuf PROPERTIES
        IMPORTED_LOCATION ${PROTOBUF_LIBRARIES}
    )
    add_dependencies(protobuf::libprotobuf protobuf_external)
endif()

# Find Boost
find_package(Boost 1.51 REQUIRED COMPONENTS filesystem system program_options)

# Create executable target
add_executable(protobuf_decompiler
    main.cpp
    MetadataExtractor.cpp
    MetadataExtractor.h
    FilesystemMetadataExtractor.cpp
    FilesystemMetadataExtractor.h
    BinaryMetadataExtractor.cpp
    BinaryMetadataExtractor.h)

# Set target properties
target_compile_features(protobuf_decompiler PRIVATE cxx_std_17)

# Link libraries using modern CMake
if(BUILD_PROTOBUF_FROM_SOURCE)
    target_link_libraries(protobuf_decompiler
        PRIVATE
            protobuf::libprotobuf
            Boost::filesystem
            Boost::system
            Boost::program_options)
    # Add include directories for built protobuf
    target_include_directories(protobuf_decompiler
        PRIVATE
            ${PROTOBUF_INCLUDE_DIRS})
    # Add dependency to ensure protobuf is built first
    add_dependencies(protobuf_decompiler protobuf_external)
else()
    # Use system protobuf with modern CMake targets
    target_link_libraries(protobuf_decompiler
        PRIVATE
            protobuf::libprotobuf
            Boost::filesystem
            Boost::system
            Boost::program_options)
endif()

# Add project source directory to includes
target_include_directories(protobuf_decompiler
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})

# Set output directories
set_target_properties(protobuf_decompiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")

# Add RPATH handling for installed binaries
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Installation
install(TARGETS protobuf_decompiler
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Export compile commands for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add a custom target to format code (optional, requires clang-format)
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    file(GLOB_RECURSE ALL_SOURCE_FILES *.cpp *.h)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i ${ALL_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on all source files")
endif()
