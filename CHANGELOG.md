# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- EditorConfig configuration (.editorconfig) for consistent coding style across
  editors and IDEs
- Git attributes file (.gitattributes) for proper line ending handling
- GitHub Actions workflow for automated builds (Ubuntu, macOS) with dependency
  management and artifact generation
- World of Warcraft-specific documentation with supported version information
  (6.0.2+ retail, 1.13.2+ Classic)
- README.md with project description, build instructions, usage examples, and
  system requirements
- MIT license badge
- Markdown linting configuration (.markdownlint.jsonc)
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

- Improved .gitignore organization with clearer structure and explicit ignores
- Updated README examples with version-specific output directory patterns
- LICENSE file converted to markdown format (LICENSE.md)
- Upgraded CMake minimum version requirement from 3.0 to 3.28
  - CMake configuration with target-based dependencies
  - Compiler warnings (-Wall -Wextra -Wpedantic for GCC/Clang, /W4 for MSVC)
  - C++17 standard requirement
  - Build artifact organization (outputs to build/bin/)
  - Automatic compile_commands.json generation for IDE support
  - Code formatting target using clang-format
- Build system detects and builds protobuf 2.6.1 from source if not found
  - Uses ExternalProject_Add with CMake-based build for Unix, MSBuild for
    Windows
  - Only builds from source if system protobuf is not version 2.6.1
  - Status messages during configuration
- Replaced autotools-based protobuf 2.6.1 build with CMake-based build
  - Added `cmake-build/protobuf-2.6.1-cmake/` with CMake files and va_copy
    patches
  - Eliminates autotools dependency (autoconf, automake, libtool) on Unix

### Fixed

- Fixed missing `#include <algorithm>` in MetadataExtractor.cpp
- Fixed GitHub Actions multi-platform build failures
  - Added missing `<list>` header for macOS clang compilation
  - Added missing `<boost/filesystem/directory.hpp>` for directory_iterator
    support
  - Implemented platform-specific build handling in CMakeLists.txt
  - Added macOS-specific compiler flags to suppress deprecated API warnings
    in protobuf 2.6.1
- Stabilized GitHub Actions CI/CD pipeline for Ubuntu and macOS platforms
- Resolved compiler detection issues for CMake presets

### Added

- PE section parsing for targeted metadata scanning
  - `ParsePESections()` reads DOS/PE headers to locate section table
  - Scans `.rdata`, `.rsrc`, and `.data` sections for descriptor candidates
- Two-pass descriptor discovery
  - `FindMetadata()`: bounded backward search for individual `.proto` descriptors
    with varint-safe decoding and deduplication
  - `FindFileDescriptorSets()`: scans for consecutive `FileDescriptorProto`
    entries (≥2 per sequence) in section ranges or full binary
- Varint decoder safety improvements
  - `DecodeVarint64` now takes a `bufferSize` parameter and validates bounds
    on every iteration
  - Returns `(0, 0)` on overflow or underflow instead of reading past buffer
- Deduplication of found descriptors via `_foundDescriptorNames` set
- `.markdownlint-cli2.jsonc` removed (merged into `.markdownlint.jsonc`)
- `--forward` flag added to patch command in CMakeLists.txt
- Fixed trailing newline in `add-va-copy-fix.patch`
- Updated README example path to `battle.net` directory convention

### Changed

- `DecodeVarint64` signature changed from `(char*, size_t*)` to
  `(char const*, size_t bufferSize, size_t*)`
- `BinaryMetadata` now stores `uint8_t const*` instead of `uint8_t*`
- `FindMetadata` uses bounded backward search window (300 bytes) instead of
  `rfind` from current position
- `FindMetadata` validates parsed descriptor name against backward-search
  extracted name to avoid garbled merges from back-to-back descriptors
- `FindMetadata` uses `ByteSize()` instead of `CurrentPosition()` to determine
  descriptor length, avoiding over-count in `FileDescriptorSet` containers

### Fixed

- Varint decoder buffer over-read: bounds check on every iteration prevents
  reading past end of binary (BinaryMetadataExtractor.cpp:82)
- Integer truncation: descriptor length derived from `ByteSize()` instead of
  `CurrentPosition()`, avoiding negative lengths for large descriptors
- Path traversal: descriptor names validated against backward-search extracted
  names before accepting candidates
- Missing newline at end of `add-va-copy-fix.patch` file

### Removed

- `.markdownlint-cli2.jsonc` (merged into `.markdownlint.jsonc`)
- Windows builds not supported in CI due to protobuf 2.6.1 compatibility
  requirements
  - vcpkg protobuf versions have incompatible API changes
  - protobuf 2.6.1 source build on Windows requires Visual Studio
    configuration

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
