#include "BinaryMetadataExtractor.h"
#include <boost/filesystem.hpp>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/io/coded_stream.h>
#include <fstream>
#include <cstring>

namespace
{
constexpr int kMaxVarintBytes = 10;
constexpr size_t kMaxBackwardSearchWindow = 300;

uint64_t DecodeVarint64(char const* buffer, size_t bufferSize, size_t* count)
{
    uint8_t const* buf = reinterpret_cast<uint8_t const*>(buffer);
    uint64_t result = 0;
    uint32_t b;

    *count = 0;

    do
    {
        if (*count >= static_cast<size_t>(kMaxVarintBytes) || *count >= bufferSize)
        {
            *count = 0;
            return 0;
        }

        b = buf[*count];
        result |= static_cast<uint64_t>(b & 0x7F) << (7 * *count);
        ++*count;
    }
    while (b & 0x80);

    return result;
}

class BinaryMetadata : public MetadataExtractor::Metadata
{
public:
    explicit BinaryMetadata(std::string id, uint8_t const* data, int32_t length) : Metadata(std::move(id)), Data(data), Length(length)
    {
    }

    std::shared_ptr<google::protobuf::io::CodedInputStream> CreateCodedInputStream() const override
    {
        return std::make_shared<google::protobuf::io::CodedInputStream>(Data, Length);
    }

private:
    uint8_t const* Data;
    int32_t Length;
};
}

void BinaryMetadataExtractor::Parse(boost::filesystem::path const& binaryPath)
{
    ReadFile(binaryPath);
    ParsePESections();
    FindMetadata();
    FindFileDescriptorSets();
}

void BinaryMetadataExtractor::ReadFile(boost::filesystem::path const& binaryPath)
{
    std::ifstream ifs(binaryPath.string(), std::ifstream::binary);
    if (!ifs)
        throw std::runtime_error("could not open " + binaryPath.string());

    ifs >> std::noskipws;
    ifs.seekg(0, std::ios_base::end);
    _binary.reserve(ifs.tellg());
    ifs.seekg(0, std::ios_base::beg);

    std::copy(std::istream_iterator<char>(ifs), std::istream_iterator<char>(), std::back_inserter(_binary));
}

void BinaryMetadataExtractor::ParsePESections()
{
    // Minimum size: DOS header (64 bytes) + PE signature (4) + COFF header (20)
    if (_binary.size() < 64)
        return;

    // Check DOS signature "MZ"
    if (_binary[0] != 'M' || _binary[1] != 'Z')
        return;

    // Read e_lfanew at offset 0x3C (4-byte little-endian)
    uint32_t peOffset = 0;
    std::memcpy(&peOffset, &_binary[0x3C], sizeof(uint32_t));

    if (peOffset + 24 > _binary.size())
        return;

    // Check PE signature "PE\0\0"
    if (_binary[peOffset] != 'P' || _binary[peOffset + 1] != 'E' ||
        _binary[peOffset + 2] != '\0' || _binary[peOffset + 3] != '\0')
        return;

    // COFF header starts at peOffset + 4
    size_t coffOffset = peOffset + 4;

    uint16_t numberOfSections = 0;
    std::memcpy(&numberOfSections, &_binary[coffOffset + 2], sizeof(uint16_t));

    uint16_t optionalHeaderSize = 0;
    std::memcpy(&optionalHeaderSize, &_binary[coffOffset + 16], sizeof(uint16_t));

    // Section table starts after COFF header (20 bytes) + optional header
    size_t sectionTableOffset = coffOffset + 20 + optionalHeaderSize;
    constexpr size_t kSectionHeaderSize = 40;
    constexpr uint32_t kImageScnCntInitializedData = 0x00000040;

    for (uint16_t i = 0; i < numberOfSections; ++i)
    {
        size_t entryOffset = sectionTableOffset + i * kSectionHeaderSize;
        if (entryOffset + kSectionHeaderSize > _binary.size())
            break;

        // Section name: 8 bytes at offset 0
        char nameBytes[9] = {};
        std::memcpy(nameBytes, &_binary[entryOffset], 8);
        std::string name(nameBytes);

        // Raw data offset at byte 20, raw data size at byte 16
        uint32_t rawSize = 0;
        uint32_t rawOffset = 0;
        uint32_t characteristics = 0;
        std::memcpy(&rawSize, &_binary[entryOffset + 16], sizeof(uint32_t));
        std::memcpy(&rawOffset, &_binary[entryOffset + 20], sizeof(uint32_t));
        std::memcpy(&characteristics, &_binary[entryOffset + 36], sizeof(uint32_t));

        if (!(characteristics & kImageScnCntInitializedData))
            continue;

        if (rawOffset + rawSize > _binary.size())
            rawSize = static_cast<uint32_t>(_binary.size() - rawOffset);

        _sections.push_back({name, static_cast<size_t>(rawOffset), static_cast<size_t>(rawSize)});
    }
}

void BinaryMetadataExtractor::FindMetadata()
{
    size_t itr = 0;
    while (itr < _binary.size())
    {
        itr = _binary.find(".proto", itr);
        if (itr == std::string::npos)
            break;

        // Move past ".proto" for next iteration
        size_t protoEnd = itr + 6;

        // Bounded backward search: look for field tag 0x0A within a window
        size_t windowStart = (itr > kMaxBackwardSearchWindow) ? itr - kMaxBackwardSearchWindow : 0;

        bool found = false;
        for (size_t pos = itr; pos > windowStart && !found; --pos)
        {
            if (static_cast<uint8_t>(_binary[pos - 1]) != 0x0A)
                continue;

            size_t candidateStart = pos - 1;

            // Decode varint length following the 0x0A tag
            size_t decodedBytes = 0;
            size_t remaining = _binary.size() - pos;
            uint64_t nameLength = DecodeVarint64(&_binary[pos], remaining, &decodedBytes);
            if (decodedBytes == 0 || nameLength == 0)
                continue;

            // The name field value starts after tag + varint
            size_t nameStart = pos + decodedBytes;

            // Check that the name ends exactly at ".proto"
            if (nameStart + nameLength != protoEnd)
                continue;

            // Extract the expected name for validation after parse
            std::string expectedName(&_binary[nameStart], nameLength);

            // Try to parse as FileDescriptorProto from the candidate start
            size_t bytesAvailable = _binary.size() - candidateStart;
            google::protobuf::io::CodedInputStream stream(
                reinterpret_cast<uint8_t const*>(&_binary[candidateStart]),
                static_cast<int>(bytesAvailable));

            google::protobuf::FileDescriptorProto descriptor;
            if (!descriptor.MergeFromCodedStream(&stream))
                continue;

            // Validate parsed name matches the name we extracted from backward search.
            // MergeFromCodedStream consumes the entire buffer, so back-to-back descriptors
            // (e.g. inside a FileDescriptorSet) get merged and garble the name field.
            if (descriptor.name() != expectedName)
                continue;

            // Deduplicate
            if (!_foundDescriptorNames.insert(descriptor.name()).second)
            {
                found = true;
                continue;
            }

            // Use ByteSize() to get the actual serialized length of the descriptor,
            // not CurrentPosition() which reflects total bytes consumed from the
            // stream (including adjacent descriptors in FileDescriptorSet containers).
            int32_t descriptorSize = descriptor.ByteSize();
            if (descriptorSize <= 0)
                continue;

            _metadatas.emplace_back(new BinaryMetadata(
                std::to_string(_metadatas.size()),
                reinterpret_cast<uint8_t const*>(&_binary[candidateStart]),
                descriptorSize));
            found = true;
        }

        itr = protoEnd;
    }
}

void BinaryMetadataExtractor::FindFileDescriptorSets()
{
    struct ScanRange
    {
        size_t Start;
        size_t End;
    };

    std::vector<ScanRange> ranges;

    if (!_sections.empty())
    {
        for (auto const& section : _sections)
        {
            // Scan .rdata and .rsrc sections (most likely locations)
            if (section.Name == ".rdata" || section.Name == ".rsrc" || section.Name == ".data")
                ranges.push_back({section.Offset, section.Offset + section.Size});
        }
    }

    // Fall back to full binary if no PE sections found
    if (ranges.empty())
        ranges.push_back({0, _binary.size()});

    for (auto const& range : ranges)
    {
        size_t pos = range.Start;
        while (pos < range.End && pos < _binary.size())
        {
            if (static_cast<uint8_t>(_binary[pos]) != 0x0A)
            {
                ++pos;
                continue;
            }

            // Try to parse a sequence of consecutive FileDescriptorProto entries
            size_t seqPos = pos;
            std::vector<std::pair<size_t, int32_t>> candidates; // offset, length pairs

            while (seqPos < range.End && seqPos < _binary.size())
            {
                if (static_cast<uint8_t>(_binary[seqPos]) != 0x0A)
                    break;

                // Decode the inner length varint
                size_t decodedBytes = 0;
                size_t remaining = _binary.size() - (seqPos + 1);
                if (remaining == 0)
                    break;

                uint64_t innerLength = DecodeVarint64(&_binary[seqPos + 1], remaining, &decodedBytes);
                if (decodedBytes == 0 || innerLength == 0)
                    break;

                // The outer message starts at seqPos: tag(1) + varint(decodedBytes) + innerLength
                size_t entryStart = seqPos;
                size_t entryTotalSize = 1 + decodedBytes + innerLength;

                if (entryStart + entryTotalSize > _binary.size())
                    break;

                // Try parsing the inner bytes as FileDescriptorProto
                size_t innerStart = seqPos + 1 + decodedBytes;
                google::protobuf::io::CodedInputStream stream(
                    reinterpret_cast<uint8_t const*>(&_binary[innerStart]),
                    static_cast<int>(innerLength));

                google::protobuf::FileDescriptorProto descriptor;
                if (!descriptor.MergeFromCodedStream(&stream))
                    break;

                // Must have a name ending in .proto
                if (descriptor.name().size() < 6 ||
                    descriptor.name().compare(descriptor.name().size() - 6, 6, ".proto") != 0)
                    break;

                candidates.push_back({entryStart, static_cast<int32_t>(entryTotalSize)});
                seqPos = entryStart + entryTotalSize;
            }

            // Require at least 2 consecutive entries to distinguish from individual descriptors
            if (candidates.size() >= 2)
            {
                for (auto const& candidate : candidates)
                {
                    // Re-parse to get the name for deduplication
                    size_t tagAndVarintSize = 0;
                    size_t decodedBytes = 0;
                    size_t remaining = _binary.size() - (candidate.first + 1);
                    uint64_t innerLength = DecodeVarint64(&_binary[candidate.first + 1], remaining, &decodedBytes);
                    tagAndVarintSize = 1 + decodedBytes;

                    size_t innerStart = candidate.first + tagAndVarintSize;
                    google::protobuf::io::CodedInputStream stream(
                        reinterpret_cast<uint8_t const*>(&_binary[innerStart]),
                        static_cast<int>(innerLength));

                    google::protobuf::FileDescriptorProto descriptor;
                    if (!descriptor.MergeFromCodedStream(&stream))
                        continue;

                    if (!_foundDescriptorNames.insert(descriptor.name()).second)
                        continue;

                    _metadatas.emplace_back(new BinaryMetadata(
                        std::to_string(_metadatas.size()),
                        reinterpret_cast<uint8_t const*>(&_binary[innerStart]),
                        static_cast<int32_t>(innerLength)));
                }
                pos = seqPos;
            }
            else
            {
                ++pos;
            }
        }
    }
}
