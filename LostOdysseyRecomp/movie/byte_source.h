#pragma once
#include <modding/mod_api.h>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

namespace movie {
// Random-access read-only bytes. One thread uses a source at a time.
class ByteSource {
public:
    virtual ~ByteSource();
    // False when the range is not entirely inside the source or the read fails.
    virtual bool ReadAt(uint64_t offset, void* dst, size_t size) = 0;
    virtual uint64_t Size() const = 0;
};

// A loose file or the byte range of a mod archive entry (see modding::OpenAsset).
// Keeps one open stream and seeks per read. Null and *error on failure.
std::unique_ptr<ByteSource> OpenByteSource(const modding::ResolvedAsset& asset, std::string* error = nullptr);
}
