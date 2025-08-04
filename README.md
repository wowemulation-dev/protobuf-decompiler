# World of Warcraft Protocol Buffer Decompiler

Reconstructs `.proto` files from Google Protocol Buffers reflection metadata
compiled into World of Warcraft game client binaries.

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.md)

</div>

**Supported WoW Versions:**

- **WoW Retail**: 6.0.2 (Warlords of Draenor) through current retail releases
- **WoW Classic**: 1.13.2 through current Classic releases

## Features

- **WoW Client Analysis**: Extracts protobuf descriptors from WoW game client binaries
- **Schema Reconstruction**: Rebuilds `.proto` files with structure and dependencies
- **Cross-Platform**: Works on Windows, Linux, and macOS
- **C++17**: Built with C++17 and CMake
- **Command-Line Interface**: CLI with options for different input methods

## Quick Start

### Prerequisites

**Required:**

- **CMake** (≥ 3.28)
- **C++17** compatible compiler (GCC, Clang, or MSVC)
- **Boost** (≥ 1.51) - components: filesystem, system, program_options


### Building

#### Using CMake Presets (Recommended)

```bash
# Clone the repository
git clone https://github.com/danielsreichenbach/protobuf-decompiler.git
cd protobuf-decompiler

# List available presets
cmake --list-presets

# Configure and build with a preset (e.g., release build with GCC)
cmake --preset gcc-release
cmake --build --preset gcc-release

# Or for debug builds
cmake --preset gcc-debug
cmake --build --preset gcc-debug

# The binary will be in build/<preset-name>/bin/
```

#### Traditional CMake Build

```bash
# Clone the repository
git clone https://github.com/danielsreichenbach/protobuf-decompiler.git
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
# Extract protobuf schemas from WoW client binary
./protobuf_decompiler /path/to/Wow.exe

# Scan current directory for .protoc files (legacy mode)
./protobuf_decompiler

# Show help and options
./protobuf_decompiler --help
```

**Examples:**

```bash
# Analyze WoW client on Windows
./protobuf_decompiler "C:/Program Files (x86)/World of Warcraft/Wow.exe"

# Analyze WoW client on macOS
./protobuf_decompiler "/Applications/World of Warcraft/World of Warcraft.app/Contents/MacOS/World of Warcraft"

# Legacy mode with .protoc files
./protobuf_decompiler --directory ./extracted_files/
```

## How It Works

1. **Binary Scanning**: Searches WoW client executable for embedded protobuf descriptors
2. **Metadata Extraction**: Parses protobuf reflection metadata using internal APIs
3. **Dependency Resolution**: Resolves cross-references between message types and imports
4. **Schema Reconstruction**: Generates `.proto` files

The tool uses:

- Direct binary analysis for descriptor extraction
- Protobuf internal API manipulation for parsing
- Two-pass dependency resolution for schemas
- Directory structure preservation for output

## Build Options

```bash
# Debug build with symbols
cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(nproc)

# Use specific compiler
CXX=clang++ cmake .. && make -j$(nproc)

# Use Ninja for faster builds
cmake -G Ninja .. && ninja

# Format code (if clang-format available)
make format
```

## System Requirements

- **Operating System**: Windows, Linux, macOS
- **Architecture**: x86_64, ARM64
- **Memory**: 512MB RAM
- **Disk Space**: 100MB for build artifacts

## Contributing

Submit issues, feature requests, or pull requests.

## License

This project is licensed under the [MIT License](LICENSE.md).

## Important Notes

- This tool requires **exactly** protobuf version 2.6.1 for compatibility
- Designed specifically for modern World of Warcraft client analysis
  (supported versions: 6.0.2+ retail, 1.13.2+ Classic)
- Contains intentional memory management choices to avoid protobuf double-free issues
- Uses pointer arithmetic for accessing protobuf internal structures
- Output maintains original directory structure when possible
