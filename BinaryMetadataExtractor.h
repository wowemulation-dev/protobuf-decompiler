#ifndef BINARY_METADATA_EXTRACTOR_H
#define BINARY_METADATA_EXTRACTOR_H

#include "MetadataExtractor.h"
#include <set>
#include <string>
#include <vector>

class BinaryMetadataExtractor : public MetadataExtractor
{
public:
    void Parse(boost::filesystem::path const& binaryPath) override;

private:
    struct SectionInfo
    {
        std::string Name;
        size_t Offset;
        size_t Size;
    };

    void ReadFile(boost::filesystem::path const& binaryPath);
    void ParsePESections();
    void FindMetadata();
    void FindFileDescriptorSets();

    std::string _binary;
    std::vector<SectionInfo> _sections;
    std::set<std::string> _foundDescriptorNames;
};

#endif // BINARY_METADATA_EXTRACTOR_H
