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

# Always use CMake-based build for protobuf 2.6.1
# This eliminates the need for autotools on all platforms

# First, try to find system protobuf and check its version
set(BUILD_PROTOBUF_FROM_SOURCE FALSE)

if(WIN32)
    # On Windows, always build protobuf 2.6.1 from source to ensure API compatibility
    message(STATUS "Windows platform detected - will build protobuf 2.6.1 from source")
    set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
else()
    find_package(Protobuf QUIET)
    
    if(Protobuf_FOUND)
        # Check if the found version is exactly 2.6.1
        if(Protobuf_VERSION)
            if(Protobuf_VERSION VERSION_EQUAL "2.6.1")
                message(STATUS "Found system protobuf version 2.6.1 - using it")
            else()
                message(STATUS "Found system protobuf version ${Protobuf_VERSION}, but need 2.6.1 - will build from source")
                set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
            endif()
        else()
            # Older FindProtobuf modules might not set Protobuf_VERSION
            message(STATUS "Found system protobuf but couldn't determine version - will build from source")
            set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
        endif()
    else()
        message(STATUS "System protobuf not found - will build version 2.6.1 from source")
        set(BUILD_PROTOBUF_FROM_SOURCE TRUE)
    endif()
endif()

if(BUILD_PROTOBUF_FROM_SOURCE)
    message(STATUS "Building protobuf v2.6.1 from source using CMake (this may take a while on first build)")
    
    # Build protobuf v2.6.1 using ExternalProject
    # Note: protobuf 2.6.1 uses autotools, not CMake
    set(PROTOBUF_PREFIX ${CMAKE_BINARY_DIR}/protobuf-2.6.1)
    
    # Configure autotools commands based on platform
    if(WIN32)
        # On Windows, download pre-built protobuf 2.6.1 binaries
        # This avoids the complexity of building with Visual Studio
        ExternalProject_Add(
            protobuf_external
            URL https://github.com/protocolbuffers/protobuf/releases/download/v2.6.1/protobuf-2.6.1.zip
            PREFIX ${PROTOBUF_PREFIX}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND 
                ${CMAKE_COMMAND} -E chdir ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects
                cmd /c extract_includes.bat
            INSTALL_COMMAND
                ${CMAKE_COMMAND} -E make_directory ${PROTOBUF_PREFIX}/install/lib &&
                ${CMAKE_COMMAND} -E make_directory ${PROTOBUF_PREFIX}/install/bin &&
                ${CMAKE_COMMAND} -E make_directory ${PROTOBUF_PREFIX}/install/include &&
                ${CMAKE_COMMAND} -E copy_directory 
                    ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/include 
                    ${PROTOBUF_PREFIX}/install/include &&
                ${CMAKE_COMMAND} -E copy_directory
                    ${PROTOBUF_PREFIX}/src/protobuf_external/src
                    ${PROTOBUF_PREFIX}/install/src
            BUILD_IN_SOURCE 1
        )
        
        # Build protobuf libraries after extraction
        add_custom_command(TARGET protobuf_external POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E chdir ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects
                    msbuild protobuf.sln /p:Configuration=Release /p:Platform=x64
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/x64/Release/libprotobuf.lib
                    ${PROTOBUF_PREFIX}/install/lib/libprotobuf.lib
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/x64/Release/libprotoc.lib
                    ${PROTOBUF_PREFIX}/install/lib/libprotoc.lib
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${PROTOBUF_PREFIX}/src/protobuf_external/vsprojects/x64/Release/protoc.exe
                    ${PROTOBUF_PREFIX}/install/bin/protoc.exe
            COMMENT "Building protobuf libraries with MSBuild"
        )
    else()
        # Build list of CMAKE_ARGS conditionally
        set(PROTOBUF_CMAKE_ARGS
            -DCMAKE_INSTALL_PREFIX=${PROTOBUF_PREFIX}/install
            -DCMAKE_BUILD_TYPE=Release
            -DBUILD_SHARED_LIBS=OFF
            -DBUILD_TESTING=OFF
        )
        
        # Only pass compiler variables if they are defined
        if(DEFINED CMAKE_CXX_COMPILER AND NOT CMAKE_CXX_COMPILER STREQUAL "")
            list(APPEND PROTOBUF_CMAKE_ARGS "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
        endif()
        if(DEFINED CMAKE_C_COMPILER AND NOT CMAKE_C_COMPILER STREQUAL "")
            list(APPEND PROTOBUF_CMAKE_ARGS "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
        endif()
        if(DEFINED CMAKE_CXX_FLAGS AND NOT CMAKE_CXX_FLAGS STREQUAL "")
            list(APPEND PROTOBUF_CMAKE_ARGS "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}")
        endif()
        if(DEFINED CMAKE_C_FLAGS AND NOT CMAKE_C_FLAGS STREQUAL "")
            list(APPEND PROTOBUF_CMAKE_ARGS "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}")
        endif()

        # On Unix systems (Linux, macOS), use CMake
        ExternalProject_Add(
            protobuf_external
            GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
            GIT_TAG        v2.6.1
            PREFIX         ${PROTOBUF_PREFIX}
            # Apply our CMake patch after download
            PATCH_COMMAND
                ${CMAKE_COMMAND} -E copy_directory 
                    ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build/protobuf-2.6.1-cmake/
                    ${PROTOBUF_PREFIX}/src/protobuf_external/ &&
                cd ${PROTOBUF_PREFIX}/src/protobuf_external &&
                patch -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/cmake-build/protobuf-2.6.1-cmake/patches/add-va-copy-fix.patch
            # Now we can use CMake to build
            CMAKE_ARGS ${PROTOBUF_CMAKE_ARGS}
        )
    endif()
    
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
