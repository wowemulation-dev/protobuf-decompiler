cmake_minimum_required(VERSION 3.31)

project(
    protobuf_decompiler
    VERSION 1.2.0
    DESCRIPTION "Reconstructs .proto files from Google protobuf reflection strings"
    LANGUAGES CXX)

# Set C++ standard. C++20 is required: protobuf v30.1 depends on abseil
# 20250127.0, which uses unguarded C++20 features (e.g. std::countr_zero in
# raw_hash_set.h) and therefore cannot be compiled in C++17 mode.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE
        "Release"
        CACHE STRING "Choose the type of build." FORCE)
    set_property(
        CACHE CMAKE_BUILD_TYPE
        PROPERTY STRINGS
                 "Debug"
                 "Release"
                 "MinSizeRel"
                 "RelWithDebInfo")
endif()

# Compiler warnings for this project's own sources only. Kept in a variable and
# applied per-target (below) so FetchContent dependencies (protobuf, abseil) are
# not compiled with our warning flags.
set(PROTOBUF_DECOMPILER_WARNINGS
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wpedantic>
    $<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4>)

include(GNUInstallDirs)
include(FetchContent)

# Fetch protobuf v30.1 -- the exact upstream version that Blizzard's WoW client
# fork (protobuf_blz-6.30.1, GOOGLE_PROTOBUF_VERSION 6030001) is based on. Its
# built-in descriptor.proto and cpp_features.proto are the Editions-era ones, so
# modern client descriptors link without any pool hacking. Abseil is resolved
# automatically by protobuf's CMake (find_package with FetchContent fallback).
# SYSTEM keeps dependency headers out of our warning diagnostics.
set(BUILD_SHARED_LIBS OFF)
set(protobuf_BUILD_SHARED_LIBS OFF)
set(protobuf_INSTALL OFF)
set(protobuf_BUILD_TESTS OFF)
set(protobuf_BUILD_PROTOC_BINARIES OFF)
set(protobuf_BUILD_LIBUPB OFF)
FetchContent_Declare(
    protobuf SYSTEM
    GIT_REPOSITORY https://github.com/protocolbuffers/protobuf.git
    GIT_TAG v30.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(protobuf)

# Create executable target
add_executable(
    protobuf_decompiler
    main.cpp
    MetadataExtractor.cpp
    MetadataExtractor.h
    FilesystemMetadataExtractor.cpp
    FilesystemMetadataExtractor.h
    BinaryMetadataExtractor.cpp
    BinaryMetadataExtractor.h)

# Link the fetched protobuf (static)
target_link_libraries(protobuf_decompiler PRIVATE protobuf::libprotobuf)

# Enforce C++20 on this target explicitly, so the requirement survives any
# caller that sets an older CMAKE_CXX_STANDARD in the cache.
target_compile_features(protobuf_decompiler PRIVATE cxx_std_20)
target_compile_options(protobuf_decompiler PRIVATE ${PROTOBUF_DECOMPILER_WARNINGS})

# Set output directories
set_target_properties(
    protobuf_decompiler
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
               RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
               RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
               RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
               RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin")

# Installation
install(TARGETS protobuf_decompiler RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Export compile commands for better IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add a custom target to format code (optional, requires clang-format)
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    set(FORMAT_SOURCE_FILES
        main.cpp
        MetadataExtractor.cpp
        MetadataExtractor.h
        FilesystemMetadataExtractor.cpp
        FilesystemMetadataExtractor.h
        BinaryMetadataExtractor.cpp
        BinaryMetadataExtractor.h)
    add_custom_target(
        format
        COMMAND ${CLANG_FORMAT} -i ${FORMAT_SOURCE_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-format on all source files")
endif()
