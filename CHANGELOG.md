# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- GitHub Actions workflow for automated multi-platform builds (Ubuntu, Windows, macOS) with dependency management and artifact generation
- World of Warcraft-specific documentation with supported version information (6.0.2+ retail, 1.13.2+ Classic)
- README.md with project description, build instructions, usage examples, and system requirements
- MIT license badge
- Markdown linting configuration (.markdownlint-cli2.jsonc, .markdownlint.jsonc)
- Command-line help feature using Boost.Program_options
  - Added `--help` / `-h` option to display usage information
  - Added `--binary` option for specifying binary file path
  - Added `--directory` option for specifying directory to scan
  - Error handling for invalid command-line arguments
- CMake Presets support for modern build workflows
  - Added CMakePresets.json with debug/release configurations
  - Platform-specific presets for GCC, Clang, and MSVC
  - CI-specific presets for GitHub Actions
  - Updated documentation with preset usage instructions

### Changed
- LICENSE file converted to markdown format (LICENSE.md)
- Upgraded CMake minimum version requirement from 3.0 to 3.28
  - CMake configuration with target-based dependencies
  - Compiler warnings (-Wall -Wextra -Wpedantic for GCC/Clang, /W4 for MSVC)
  - C++17 standard requirement
  - Build artifact organization (outputs to build/bin/)
  - Automatic compile_commands.json generation for IDE support
  - Code formatting target using clang-format
- Build system detects and builds protobuf 2.6.1 from source if not found
  - Uses ExternalProject_Add to handle protobuf's autotools build system
  - Only builds from source if system protobuf is not version 2.6.1
  - Status messages during configuration

### Fixed
- Fixed missing `#include <algorithm>` in MetadataExtractor.cpp
- Fixed GitHub Actions multi-platform build failures
  - Added missing `<list>` header for macOS clang compilation
  - Added missing `<boost/filesystem/directory.hpp>` for directory_iterator support
  - Implemented platform-specific autotools command handling in CMakeLists.txt
  - Added macOS-specific compiler flags to suppress deprecated API warnings in protobuf 2.6.1
- Stabilized GitHub Actions CI/CD pipeline for Ubuntu and macOS platforms
- Resolved compiler detection issues for CMake presets

### Removed
- Windows builds not supported in CI due to protobuf 2.6.1 compatibility requirements
  - vcpkg protobuf versions have incompatible API changes
  - protobuf 2.6.1 source build on Windows requires complex Visual Studio configuration

## [1.0.0] - 2023-04-14

### Changed
- Updated README with additional usage information

## [1.0.0-rc.2] - 2022-10-15

### Added
- Early exit when no .proto descriptors are found

### Fixed
- Corrected searching for protobuf descriptors in binaries

## [1.0.0-rc.1] - 2022-03-22

### Added
- Binary metadata extraction capability
  - Implemented `BinaryMetadataExtractor` class for parsing executable files
  - Added `FilesystemMetadataExtractor` class for legacy .protoc file support
  - Created abstract `MetadataExtractor` base class
- Support for extracting protobuf metadata directly from compiled binaries
- Refactored codebase into modular extractor classes

### Changed
- Major refactoring of main.cpp to support multiple extraction methods
- Enhanced CMake configuration with Boost dependencies

## [0.3.0] - 2017-06-27

### Added
- MIT License

## [0.2.0] - 2017-04-23

### Fixed
- Fixed parsing of extension fields that are messages
- Improved extension registration for message and group types

## [0.1.1] - 2016-02-14

### Added
- Initial README.md documentation

## [0.1.0] - 2016-02-13

### Added
- Full protobuf descriptor reconstruction functionality
- Support for rebuilding .proto files from compiled protobuf descriptors
- Two-pass processing for proper dependency resolution
- Extension registration system
- Dynamic message factory integration

### Changed
- Enhanced CMake build system
- Expanded main.cpp with complete reconstruction logic

## [0.0.1] - 2015-05-28

### Added
- Initial project structure
- Basic CMake configuration
- Initial main.cpp implementation
- .gitignore for KDevelop project files