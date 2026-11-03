#include "FilesystemMetadataExtractor.h"
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <filesystem>
#include <fstream>
#include <optional>

namespace
{
class FilesystemMetadata : public MetadataExtractor::Metadata
{
public:
    explicit FilesystemMetadata(std::string id) : Metadata(std::move(id))
    {
    }

    std::shared_ptr<google::protobuf::io::CodedInputStream> CreateCodedInputStream() const override
    {
        struct Data
        {
            std::ifstream in;
            std::optional<google::protobuf::io::IstreamInputStream> zeroCopyInput;
            std::optional<google::protobuf::io::CodedInputStream> decoder;
        };

        std::shared_ptr<Data> data = std::make_shared<Data>();

        data->in.open(_id.c_str(), std::ios::binary | std::ios::in);
        if (!data->in.good())
            return nullptr;

        data->zeroCopyInput.emplace(&data->in);
        data->decoder.emplace(&*data->zeroCopyInput);
        return { data, &*data->decoder };
    }
};
}

void FilesystemMetadataExtractor::Parse(std::filesystem::path const& path)
{
    for (auto const& entry : std::filesystem::directory_iterator(path))
        if (entry.is_regular_file())
            if (entry.path().extension().string().find(".protoc") != std::string::npos)
                _metadatas.emplace_back(new FilesystemMetadata(entry.path().string()));
}
