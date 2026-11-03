// protobuf-decompiler
//
// Reconstructs .proto files from the serialized FileDescriptorProtos embedded in a
// binary (or a directory of .protoc files). Linked against modern protobuf (v30 /
// libprotobuf 6.30), which natively understands the Editions-era descriptor.proto
// and cpp_features.proto that current WoW clients (Blizzard fork protobuf_blz-6.30.1)
// embed. That makes the original's private-member pool hacking, manual ExtensionSet
// registration and leaked DynamicMessageFactory unnecessary: the scanned descriptors
// are layered over the library's own well-known types in a DescriptorDatabase and
// DescriptorPool resolves imports + custom options lazily.
//
// The binary scanner (BinaryMetadataExtractor) is carried over unchanged in behaviour.

#include "BinaryMetadataExtractor.h"
#include "FilesystemMetadataExtractor.h"
#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/descriptor_database.h>
#include <google/protobuf/io/coded_stream.h>

#include <absl/strings/string_view.h>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace gpb = google::protobuf;

namespace
{
// Collects per-file build errors so a failed FindFileByName() can be explained.
class CollectingErrorCollector : public gpb::DescriptorPool::ErrorCollector
{
public:
    void RecordError(absl::string_view filename, absl::string_view /*element_name*/,
                     gpb::Message const* /*descriptor*/, ErrorLocation /*location*/,
                     absl::string_view message) override
    {
        errors[std::string(filename)].push_back(std::string(message));
    }

    void RecordWarning(absl::string_view, absl::string_view, gpb::Message const*,
                       ErrorLocation, absl::string_view) override
    {
    }

    std::map<std::string, std::vector<std::string>> errors;
};

// google/protobuf/* files are provided by the library's own (Editions-era)
// built-ins; re-adding scanned copies risks version drift.
bool IsWellKnown(std::string const& name)
{
    return name.rfind("google/protobuf/", 0) == 0;
}

// A real embedded descriptor has a filename-shaped name (printable path chars,
// ends in ".proto"). The binary scanner can latch onto ".proto" substrings
// inside serialized option blobs, yielding bogus FileDescriptorProtos whose
// "name" is raw bytes -- reject those.
bool IsValidProtoName(std::string const& name)
{
    if (name.size() < 7 || name.compare(name.size() - 6, 6, ".proto") != 0)
        return false;
    for (unsigned char c : name)
        if (!(std::isalnum(c) || c == '.' || c == '_' || c == '/' || c == '-'))
            return false;
    // Security: this name becomes an output path (outDir / name) and comes from
    // an untrusted binary. Reject absolute paths and any ".." so a crafted or
    // garbled descriptor name can't traverse outside the output directory.
    // Real proto paths never need either.
    if (name.front() == '/' || name.find("..") != std::string::npos)
        return false;
    return true;
}

// Drop public_dependency / weak_dependency indices that point outside
// dependency[]. These are re-export hints; a scanner mis-parse can leave them
// dangling, which makes DescriptorPool reject the file ("Invalid public
// dependency index") even though the schema itself is fine. Repairing them
// loses no message/field/enum data.
void SanitizeDependencies(gpb::FileDescriptorProto& fdp)
{
    int const deps = fdp.dependency_size();
    auto prune = [&](auto* field) {
        gpb::RepeatedField<int32_t> kept;
        for (int32_t idx : *field)
            if (idx >= 0 && idx < deps)
                kept.Add(idx);
        field->Swap(&kept);
    };
    prune(fdp.mutable_public_dependency());
    prune(fdp.mutable_weak_dependency());
}

// Pure-noise descriptors carry no schema and no imports -- only scanner false
// positives look like this (e.g. a bare name field parsed off an option string).
bool HasSchema(gpb::FileDescriptorProto const& fdp)
{
    return fdp.message_type_size() > 0 || fdp.enum_type_size() > 0 ||
           fdp.service_size() > 0 || fdp.extension_size() > 0 ||
           fdp.dependency_size() > 0;
}

void PrintUsage(char const* argv0)
{
    std::cout << "protobuf-decompiler (protobuf 6.30.1): reconstruct .proto files from embedded descriptors\n\n"
              << "Usage:\n"
              << "  " << argv0 << " --binary <path> [--output <dir>]\n"
              << "  " << argv0 << " --directory <dir> [--output <dir>]\n"
              << "  " << argv0 << " <path> [-o <dir>]\n";
}
} // namespace

int main(int argc, char* argv[])
{
    std::string binaryPath;
    std::string directoryPath = ".";
    std::string outputDir = ".";

    // Minimal option parser (no Boost dependency).
    for (int i = 1; i < argc; ++i)
    {
        std::string a = argv[i];
        auto next = [&](std::string const& flag) -> std::string {
            if (i + 1 >= argc)
            {
                std::cerr << "Error: " << flag << " requires an argument\n";
                std::exit(1);
            }
            return argv[++i];
        };
        if (a == "-h" || a == "--help") { PrintUsage(argv[0]); return 0; }
        else if (a == "--binary") binaryPath = next(a);
        else if (a.rfind("--binary=", 0) == 0) binaryPath = a.substr(9);
        else if (a == "--directory") directoryPath = next(a);
        else if (a.rfind("--directory=", 0) == 0) directoryPath = a.substr(12);
        else if (a == "-o" || a == "--output") outputDir = next(a);
        else if (a.rfind("-o=", 0) == 0) outputDir = a.substr(3);
        else if (a.rfind("--output=", 0) == 0) outputDir = a.substr(9);
        else if (!a.empty() && a[0] != '-' && binaryPath.empty()) binaryPath = a;
        else { std::cerr << "Error: unexpected argument '" << a << "'\n"; PrintUsage(argv[0]); return 1; }
    }

    // 1. Scan for serialized FileDescriptorProtos.
    std::unique_ptr<MetadataExtractor> extractor;
    try
    {
        if (!binaryPath.empty())
        {
            extractor = std::make_unique<BinaryMetadataExtractor>();
            extractor->Parse(fs::path(binaryPath));
        }
        else
        {
            extractor = std::make_unique<FilesystemMetadataExtractor>();
            extractor->Parse(fs::path(directoryPath));
        }
    }
    catch (std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    if (extractor->GetMetadata().empty())
    {
        if (!binaryPath.empty())
            std::cout << "No .proto descriptors found in binary: " << binaryPath << "\n";
        else
        {
            std::cout << "No .protoc files found in directory: " << fs::absolute(directoryPath) << "\n";
            std::cout << "To extract from a binary, use: " << argv[0] << " --binary <path>\n";
        }
        return 1;
    }

    // 2. Parse each blob into a FileDescriptorProto; dedup by file name; collect
    //    non-well-known files into a database. Well-known google/protobuf/* files
    //    are left to the library's own (Editions-era) built-ins to avoid any
    //    version drift.
    gpb::SimpleDescriptorDatabase scannedDb;
    std::vector<std::string> blizzardNames;              // build/emit order (scan order)
    std::set<std::string> seen;
    std::map<std::string, gpb::FileDescriptorProto> rawByName; // for fallback dumps

    for (auto const& meta : extractor->GetMetadata())
    {
        auto in = meta->CreateCodedInputStream();
        if (!in)
            continue;

        gpb::FileDescriptorProto fdp;
        if (!fdp.ParseFromCodedStream(in.get()))
            continue;
        if (!IsValidProtoName(fdp.name()) || !seen.insert(fdp.name()).second)
            continue;

        SanitizeDependencies(fdp);
        if (!HasSchema(fdp))
            continue;           // scanner false positive, carries nothing

        rawByName[fdp.name()] = fdp;
        if (IsWellKnown(fdp.name()))
            continue;           // library provides these

        if (scannedDb.Add(fdp))
            blizzardNames.push_back(fdp.name());
    }

    // 3. Pool = scanned descriptors layered over the library's generated pool
    //    (descriptor.proto, cpp_features.proto, any/duration/timestamp/...
    //    well-known types).
    gpb::DescriptorPoolDatabase generatedDb(*gpb::DescriptorPool::generated_pool());
    gpb::MergedDescriptorDatabase merged(&scannedDb, &generatedDb);
    CollectingErrorCollector errorCollector;
    gpb::DescriptorPool pool(&merged, &errorCollector);
    pool.EnforceWeakDependencies(false);

    // 4. Resolve + emit. FindFileByName lazily pulls imports (incl. the options
    //    files that define custom field/message options) in dependency order, so
    //    custom options render as real extensions rather than uninterpreted bytes.
    fs::path outRoot(outputDir);
    std::error_code ec;
    fs::create_directories(outRoot, ec);
    fs::path const outRootNorm = (fs::absolute(outRoot, ec)).lexically_normal();

    auto writeFile = [&](std::string const& relPath, std::string const& content) {
        // Defense in depth (names are validated by IsValidProtoName, but never
        // trust once): normalize the final path and confirm it stays within
        // outRoot before writing.
        fs::path out = (outRootNorm / relPath).lexically_normal();
        std::string root = outRootNorm.string();
        if (!root.empty() && root.back() != '/')
            root += '/';
        if (out.string().rfind(root, 0) != 0)
        {
            std::cerr << "[skip] refusing out-of-tree path: " << relPath << "\n";
            return;
        }
        fs::create_directories(out.parent_path(), ec);
        std::ofstream f(out.string());
        f << content << "\n";
    };

    // Provenance header stamped on every emitted .proto. Source comments are not
    // stored in protobuf descriptors, so they cannot be recovered -- say so plainly.
    int const V = GOOGLE_PROTOBUF_VERSION;
    std::string const sourceLabel = binaryPath.empty() ? directoryPath : fs::path(binaryPath).filename().string();
    std::string const header =
        "// Reconstructed from " + sourceLabel +
        " by protobuf-decompiler (libprotobuf " + std::to_string(V / 1000000) + "." +
        std::to_string((V / 1000) % 1000) + "." + std::to_string(V % 1000) + ").\n"
        "// Field names/numbers/types/options are authoritative; original source\n"
        "// comments are NOT recoverable from protobuf descriptors.\n\n";

    size_t linked = 0, fallback = 0;
    for (std::string const& name : blizzardNames)
    {
        gpb::FileDescriptor const* fd = pool.FindFileByName(name);
        if (fd != nullptr)
        {
            writeFile(name, header + fd->DebugString());
            ++linked;
        }
        else
        {
            // Should be rare. Dump the raw (unlinked) descriptor so no data is lost.
            auto it = rawByName.find(name);
            if (it != rawByName.end())
            {
                writeFile(name + ".txt", it->second.DebugString());
                ++fallback;
            }
            auto eit = errorCollector.errors.find(name);
            std::cerr << "[unlinked] " << name;
            if (eit != errorCollector.errors.end() && !eit->second.empty())
                std::cerr << "  (" << eit->second.front() << ")";
            std::cerr << "\n";
        }
    }

    std::cout << "Wrote " << linked << " fully-linked .proto files.\n";
    std::cout << "Wrote " << fallback << " fallback raw-descriptor dumps (.proto.txt).\n";
    std::cout << "Total distinct FileDescriptorProto: " << blizzardNames.size()
              << " (plus " << (rawByName.size() - blizzardNames.size()) << " well-known skipped)\n";
    return 0;
}
