#include "byte_source.h"
#include <fstream>

namespace movie {
ByteSource::~ByteSource() = default;

namespace {
class AssetByteSource final : public ByteSource {
public:
    bool Open(const modding::ResolvedAsset& asset) {
        if (!modding::OpenAsset(asset, in_, size_)) return false;
        base_ = in_.tellg();
        return base_ != std::ifstream::pos_type(-1);
    }
    bool ReadAt(uint64_t offset, void* dst, size_t size) override {
        if (offset > size_ || size > size_ - offset) return false;
        if (size == 0) return true;
        in_.clear();
        in_.seekg(base_ + std::streamoff(offset));
        in_.read(static_cast<char*>(dst), std::streamsize(size));
        return in_ && size_t(in_.gcount()) == size;
    }
    uint64_t Size() const override { return size_; }
private:
    std::ifstream in_;
    std::ifstream::pos_type base_{};
    uint64_t size_ = 0;
};
}

std::unique_ptr<ByteSource> OpenByteSource(const modding::ResolvedAsset& asset, std::string* error) {
    auto source = std::make_unique<AssetByteSource>();
    if (!source->Open(asset)) {
        if (error) *error = "cannot open " + modding::DescribeAsset(asset);
        return nullptr;
    }
    return source;
}
}
