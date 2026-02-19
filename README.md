# protobuf-decompiler

Reconstructs `.proto` files from Google Protocol Buffers reflection metadata
compiled into binaries. Designed for World of Warcraft client analysis.

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.md)

</div>

**Supported WoW Versions:**

- **WoW Retail**: 6.0.2 (Warlords of Draenor) through current retail releases
- **WoW Classic**: 1.13.2 through current Classic releases

## Features

- Extracts protobuf descriptors from binary files
- Reconstructs `.proto` files with original structure and dependencies
- Supports Linux and macOS platforms
- Command-line interface with multiple input methods

## Quick Start

### Prerequisites

**Required:**

- **CMake** (≥ 3.28)
- **C++17** compatible compiler (GCC or Clang)
- **Boost** (≥ 1.51) - components: filesystem, system, program_options

### Building

#### Using CMake Presets (Recommended)

```bash
# Clone the repository
git clone https://github.com/wowemulation-dev/protobuf-decompiler.git
cd protobuf-decompiler

# List available presets
cmake --list-presets

# Configure and build with a preset (e.g., release build with GCC)
cmake --preset gcc-release
cmake --build --preset gcc-release

# Or for debug builds
cmake --preset gcc-debug
cmake --build --preset gcc-debug

# The binary will be in build/<preset-name>/bin/protobuf_decompiler
```

#### Traditional CMake Build

```bash
# Clone the repository
git clone https://github.com/wowemulation-dev/protobuf-decompiler.git
cd protobuf-decompiler

# Create build directory and configure
mkdir build && cd build
cmake ..

# Build (automatically handles protobuf 2.6.1 dependency)
make -j$(nproc)

# Optional: Install system-wide
sudo make install
```

The build system automatically:

- Detects if your system has protobuf 2.6.1 and uses it
- Downloads and builds protobuf 2.6.1 from source if needed
- Handles all dependencies and linking

### Usage

```bash
# Extract protobuf schemas from binary file
./protobuf_decompiler --binary /path/to/binary

# Scan directory for .protoc files
./protobuf_decompiler --directory /path/to/directory

# Specify output directory
./protobuf_decompiler --binary /path/to/binary --output /path/to/output

# Show help and options
./protobuf_decompiler --help
```

**Examples:**

```bash
# Analyze binary file
./protobuf_decompiler --binary "/path/to/binary"

# Legacy positional argument (backward compatibility)
./protobuf_decompiler /path/to/binary

# Scan directory for .protoc files
./protobuf_decompiler --directory ./extracted_files/
```

## How It Works

1. Searches executable files for embedded protobuf descriptors
2. Parses protobuf reflection metadata using internal APIs
3. Resolves dependencies between message types and imports
4. Generates `.proto` files maintaining original structure

Implementation details:

- Direct binary analysis for descriptor extraction
- Two-pass dependency resolution
- Uses protobuf internal API manipulation
- Preserves directory structure in output

## Build Options

```bash
# Debug build with symbols
cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(nproc)

# Use specific compiler
CXX=clang++ cmake .. && make -j$(nproc)

# Use Ninja for faster builds
cmake -G Ninja .. && ninja

# Format code (requires .clang-format in project root)
make format
```

## System Requirements

- **Operating System**: Linux, macOS
- **Architecture**: x86_64, ARM64
- **Memory**: 512MB RAM minimum
- **Disk Space**: 100MB for build artifacts

## Contributing

Submit issues, feature requests, or pull requests.

## License

This project is licensed under the [MIT License](LICENSE.md).

## Important Notes

- Requires protobuf version 2.6.1 for API compatibility
- Tested with World of Warcraft clients (6.0.2+ retail, 1.13.2+ Classic)
- Contains intentional memory leaks to avoid protobuf double-free issues
- Uses pointer arithmetic to access protobuf internal structures
- Windows builds currently not supported in CI due to protobuf 2.6.1 compatibility requirements
