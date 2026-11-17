# protobuf-decompiler

Reconstructs `.proto` files from Google Protocol Buffers reflection metadata
compiled into binaries. Designed for World of Warcraft client analysis.

<div align="center">

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE.md)

</div>

**Supported WoW Versions:**

- **WoW Retail**: 6.0.2 (Warlords of Draenor) through current retail builds
- **WoW Classic / Forever**: 1.13.2 through current builds (e.g. 1.60.1.70707)

See [Protobuf Version Support Boundary](#protobuf-version-support-boundary).

## Protobuf Version Support Boundary

Blizzard upgraded the protobuf runtime compiled into WoW clients for the first
time in over a decade. Binaries embed a `blizzard_protobuf.version` string:

| Client builds | Protobuf runtime | Supported by this tool |
|---|---|---|
| Up to build 69933 (e.g. retail 12.1.0.69933, MoP Classic 5.5.3.66290) | Blizzard fork of protobuf 2.6.1 (`protobuf_blz-2.6.1.13`) | Yes (descriptors predate editions; output omits edition-era fields) |
| Build 70032 and later (e.g. MoP Classic 5.5.4.70032, Classic Era 1.15.9.70003, beta 1.60.1.70707) | Blizzard fork of protobuf 6.30.1 (`protobuf_blz-6.30.1`, editions support) | Yes |

This release links against **protobuf v30.1** — the exact upstream version the
client's `protobuf_blz-6.30.1` fork is based on (`GOOGLE_PROTOBUF_VERSION
6030001`). Its built-in `descriptor.proto` and `cpp_features.proto` are the
Editions-era ones, so descriptors from build 70032 onward reconstruct with
custom options (e.g. `id:` on methods, routing options) fully resolved.

## Features

- Extracts protobuf descriptors from binary files
- Reconstructs `.proto` files with original structure and dependencies
- Supports editions-era descriptors (protobuf 6.30.1 / v30.1)
- Resolves custom options as real extensions instead of uninterpreted bytes
- Filters scanner false positives (garbled names, empty schemas)
- Sanitizes output paths (rejects absolute paths and `..` from descriptor names)
- Command-line interface with multiple input methods

## Quick Start

### Prerequisites

**Required:**

- **CMake** (≥ 3.31)
- **C++20** compatible compiler (GCC or Clang)

No other dependencies: protobuf v30.1 (and its abseil dependency) are fetched
and built automatically by CMake.

### Building

#### Using CMake Presets (Recommended)

```bash
# Clone the repository
git clone https://github.com/wowemulation-dev/protobuf-decompiler.git
cd protobuf-decompiler

# List available presets
cmake --list-presets

# Configure and build with a preset (Clang Debug by default)
cmake --preset default
cmake --build --preset default

# Release build with Clang
cmake --preset release
cmake --build --preset release

# Or GCC
cmake --preset gcc-debug
cmake --build --preset gcc-debug

cmake --preset gcc-release
cmake --build --preset gcc-release

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

# Build (first run fetches and builds protobuf v30.1 + abseil, which takes a while)
make -j$(nproc)

# Optional: Install system-wide
sudo make install
```

### Usage

```bash
# Extract protobuf schemas from binary file
./protobuf_decompiler --binary /path/to/binary

# Scan directory for .protoc files
./protobuf_decompiler --directory /path/to/directory

# Specify output directory
./protobuf_decompiler --binary /path/to/binary --output ./output/<version-build-os-arch>

# Show help and options
./protobuf_decompiler --help
```

**Examples:**

```bash
# Extract from a WoW client binary (output organized by version)
./protobuf_decompiler --binary /path/to/client/Wow.exe --output ./output/1.60.1.70707.windows-win64

# Legacy positional argument (backward compatibility)
./protobuf_decompiler /path/to/binary

# Scan directory for .protoc files
./protobuf_decompiler --directory ./extracted_files/
```

## How It Works

1. Scans the binary for embedded serialized `FileDescriptorProto` blobs
   (two-pass discovery: individual descriptors, then `FileDescriptorSet`
   containers, bounded by PE section scanning)
2. Parses each blob and validates it (filename-shaped name, real schema
   content, dependency indices pruned to valid range)
3. Layers the scanned descriptors over the library's own generated pool
   (well-known types, `descriptor.proto`, `cpp_features.proto`) in a
   `MergedDescriptorDatabase`
4. Resolves each file with `DescriptorPool::FindFileByName`, which pulls
   imports and custom-option definitions lazily in dependency order
5. Emits `FileDescriptor::DebugString()` as `.proto` files, preserving the
   original directory structure

## Build Options

```bash
# Debug build with symbols
cmake -DCMAKE_BUILD_TYPE=Debug .. && make -j$(nproc)

# Use specific compiler
CXX=clang++ cmake .. && make -j$(nproc)

# Use Ninja for faster builds
cmake -G Ninja .. && ninja

# Format C++ sources (uses the committed .clang-format)
make format

# Format the build files (requires cmake-format)
cmake-format -i CMakeLists.txt
```

## System Requirements

- **Operating System**: Linux, macOS (Windows configured but untested)
- **Architecture**: x86_64, ARM64
- **Memory**: 512MB RAM minimum
- **Disk Space**: ~500MB for the protobuf v30.1 + abseil build artifacts

## Contributing

Submit issues, feature requests, or pull requests.

## License

This project is licensed under the [MIT License](LICENSE.md).

## Important Notes

- Links against protobuf v30.1 (fetched at build time); the client's
  `protobuf_blz-6.30.1` fork is a fork of exactly this upstream version
- protobuf v30 reserves `namespace pb` internally — do not alias `pb` in
  code against this library
- Descriptor names become output paths; names are validated (printable
  filename characters, no absolute paths, no `..`) and final paths are
  normalized and checked to stay within the output directory
- Scanned well-known `google/protobuf/*` files are intentionally skipped in
  favor of the library's own built-ins
