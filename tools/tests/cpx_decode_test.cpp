#include "gpu/shader/cpx_decode.h"

#include <fstream>
#include <iostream>
#include <string>

using namespace xenos::resources;
using Bytes = std::vector<uint8_t>;

static void Require(bool value, const char* why) {
    if (!value) throw std::runtime_error(why);
}
static void PutLE(Bytes& data, size_t offset, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data[offset + i] = uint8_t(value >> (i * 8));
}
static Bytes Package(const std::vector<Bytes>& blocks, unsigned width, uint32_t decoded) {
    Bytes data(16 + 4 * blocks.size());
    data[0] = 'c'; data[1] = 'p'; data[2] = 'x';
    data[4] = width == 8 ? 0x10 : 0x20;
    data[6] = uint8_t(blocks.size()); data[7] = uint8_t(blocks.size() >> 8);
    PutLE(data, 12, decoded);
    for (size_t i = 0; i < blocks.size(); ++i) {
        PutLE(data, 16 + 4 * i, uint32_t(data.size()));
        data.insert(data.end(), blocks[i].begin(), blocks[i].end());
    }
    PutLE(data, 8, uint32_t(data.size()));
    return data;
}
static Bytes Raw(const Bytes& bytes) {
    Bytes block{255, 0, uint8_t(bytes.size() - 1), uint8_t((bytes.size() - 1) >> 8)};
    block.insert(block.end(), bytes.begin(), bytes.end());
    return block;
}
struct BitWriter {
    Bytes bytes;
    size_t count = 0;
    void Put(unsigned value, unsigned bits) {
        while (bits) {
            if (!(count % 8)) bytes.push_back(0);
            bytes.back() |= ((value >> --bits) & 1) << (7 - count++ % 8);
        }
    }
    void Literal(uint8_t value) { Put(0, 1); Put(value, 8); }
};
static Bytes Compressed(const Bytes& stream, uint8_t modes, uint8_t widths, size_t decoded) {
    Bytes block{modes, widths, uint8_t(decoded - 1), uint8_t((decoded - 1) >> 8)};
    block.insert(block.end(), stream.begin(), stream.end());
    return block;
}
static void Accept(const Bytes& input, const Bytes& expected, const char* why) {
    Bytes output{0xde, 0xad};
    Require(cpx::Decode(input, output) && output == expected, why);
}
static void Reject(const Bytes& input, const char* why) {
    Bytes output{0xde, 0xad};
    Require(!cpx::Decode(input, output) && output.empty(), why);
}
static Bytes ReadFile(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    Require(bool(file), "open comparison file");
    const auto size = file.tellg();
    Require(size >= 0 && uint64_t(size) <= cpx::kMaxDecodedSize, "comparison file size");
    Bytes data(static_cast<size_t>(size));
    file.seekg(0);
    file.read(reinterpret_cast<char*>(data.data()), size);
    Require(bool(file), "read comparison file");
    return data;
}

int main(int argc, char** argv) {
    try {
        // Optional local-only fixtures stay outside the repository. This mode
        // compares a resource extent with an independently decoded PPC result.
        if (argc == 6 && std::string(argv[1]) == "--compare") {
            const auto offset = std::stoull(argv[3]), size = std::stoull(argv[4]);
            Require(size <= cpx::kMaxStoredSize, "comparison input size");
            std::ifstream file(argv[2], std::ios::binary);
            file.seekg(offset);
            Bytes input(static_cast<size_t>(size));
            file.read(reinterpret_cast<char*>(input.data()), static_cast<std::streamsize>(size));
            Require(bool(file), "read comparison extent");
            const auto expected = ReadFile(argv[5]);
            Accept(input, expected, "original PPC comparison mismatch");
            std::cout << "PASS: exact PPC comparison, " << expected.size() << " bytes\n";
            return 0;
        }
        Require(argc == 1, "usage: cpx_decode_test [--compare input offset size expected]");
        const Bytes hello{'h', 'e', 'l', 'l', 'o'};
        auto raw = Package({Raw(hello)}, 16, uint32_t(hello.size()));
        Accept(raw, hello, "raw block");
        auto reserved = raw; reserved[3] = 10;
        Accept(reserved, hello, "loader reserve field is valid");
        cpx::Header header;
        Require(cpx::ReadHeader(std::span(raw).first(16), header) && header.headerSize == 20 &&
                header.storedSize == raw.size() && header.decodedSize == hello.size(), "prefix header");
        auto alias = raw;
        Require(cpx::Decode(alias, alias) && alias == hello, "input/output alias");

        Bytes first(65536, 0x37), tail{1, 2, 3}, joined = first;
        joined.insert(joined.end(), tail.begin(), tail.end());
        const auto twoBlocks = Package({Raw(first), Raw(tail)}, 8, uint32_t(joined.size()));
        Accept(twoBlocks, joined, "full block followed by partial block");

        for (unsigned width : {8u, 16u}) {
            BitWriter literals;
            literals.Literal('X'); literals.Literal('Y');
            Accept(Package({Compressed(literals.bytes, 0, 0, 2)}, width, 2), {'X', 'Y'},
                   "literal stream with partial final word");
            for (unsigned lengthMode : {0u, 1u, 2u, 3u}) {
                BitWriter bits;
                bits.Literal('A'); bits.Put(1, 1);
                if (lengthMode == 1 || lengthMode == 2) bits.Put(0, 1);
                bits.Put(3, lengthMode == 1 ? 2 : lengthMode == 2 ? 3 : 9);
                Accept(Package({Compressed(bits.bytes, uint8_t(lengthMode << 6 | 0x30), 0, 7)}, width, 7),
                       Bytes(7, 'A'), "overlapping back-reference and length mode");
            }
            for (unsigned offsetMode : {1u, 2u}) {
                BitWriter bits;
                bits.Literal('B'); bits.Put(1, 1); bits.Put(0, 1); bits.Put(0, 2);
                bits.Put(0, 1); bits.Put(3, 2);
                Accept(Package({Compressed(bits.bytes, uint8_t(0x40 | offsetMode << 4), 0x22, 7)}, width, 7),
                       Bytes(7, 'B'), "selected offset width");
            }
        }
        // In a 16-bit stream offset mode 0 reads a raw BE word beyond the
        // current bitword. The remaining length bits still come from 0x28d8.
        Accept(Package({Compressed({0x28, 0xd8, 0, 0}, 0x40, 0, 7)}, 16, 7), Bytes(7, 'Q'),
               "direct word offset with overlapping back-reference");
        BitWriter boundary;
        for (uint8_t c = 'A'; c <= 'G'; ++c) boundary.Literal(c);
        boundary.Put(1, 1);
        Require(boundary.count == 64, "synthetic prefetch boundary");
        boundary.bytes.insert(boundary.bytes.end(), {0x60, 0, 0, 6});
        Accept(Package({Compressed(boundary.bytes, 0x40, 0, 13)}, 16, 13),
               {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F'},
               "direct offset skips immediately prefetched word");
        BitWriter byteOffset;
        byteOffset.Literal('Q'); byteOffset.Put(1, 1); byteOffset.Put(0, 16);
        byteOffset.Put(0, 1); byteOffset.Put(3, 2);
        Accept(Package({Compressed(byteOffset.bytes, 0x40, 0, 7)}, 8, 7), Bytes(7, 'Q'),
               "byte stream direct offset uses the bitstream");

        for (size_t n = 0; n < raw.size(); ++n)
            Reject(Bytes(raw.begin(), raw.begin() + n), "truncated package");
        auto bad = raw; bad[0] = 'C'; Reject(bad, "wrong magic");
        bad = raw; PutLE(bad, 8, uint32_t(raw.size() + 1)); Reject(bad, "stored size mismatch");
        bad = raw; PutLE(bad, 16, 19); Reject(bad, "first offset before table");
        bad = raw; PutLE(bad, 16, 0xffffffff); Reject(bad, "overflowing first offset");
        bad = raw; bad[6] = 0; Reject(bad, "zero block count");
        bad = raw; bad[6] = 2; Reject(bad, "block count and decoded size disagree");
        bad = raw; PutLE(bad, 12, 0); Reject(bad, "empty output");
        bad = raw; PutLE(bad, 12, 0xffffffff); Reject(bad, "huge allocation header");
        bad = raw; PutLE(bad, 12, uint32_t(cpx::kMaxDecodedSize + 1)); Reject(bad, "decoded cap");
        bad = raw; PutLE(bad, 8, uint32_t(cpx::kMaxStoredSize + 1)); Reject(bad, "stored cap");
        bad = raw; bad[21] = 1; Reject(bad, "invalid raw block marker");
        bad = raw; bad[22]++; Reject(bad, "block decoded size mismatch");
        bad = raw; bad.pop_back(); PutLE(bad, 8, uint32_t(bad.size())); Reject(bad, "truncated raw payload");
        bad = twoBlocks; PutLE(bad, 20, 24); Reject(bad, "nonincreasing offsets");
        bad = twoBlocks; PutLE(bad, 20, uint32_t(bad.size() + 1)); Reject(bad, "offset past stored extent");
        bad = twoBlocks; PutLE(bad, 20, 26); Reject(bad, "undersized block header");
        Reject(Package({Compressed({0x20}, 0, 0, 1)}, 16, 1), "truncated literal bits");
        Reject(Package({Compressed({0x28, 0xd8, 0}, 0x40, 0, 7)}, 16, 7), "truncated direct word");
        Reject(Package({Compressed({0x98}, 0x70, 0, 6)}, 8, 6), "back-reference before output start");
        Reject(Package({Compressed({0x20, 0xd8}, 0x70, 0, 6)}, 8, 6), "back-reference past output end");
        // Invalid second block must not expose an otherwise valid first block.
        bad = Package({Raw(first), Compressed({0xff}, 0, 0, 3)}, 16, 65539);
        Reject(bad, "failure discards partial output");
        header.decodedSize = 123;
        Require(!cpx::ReadHeader({}, header) && header.decodedSize == 0, "failure resets header");
        std::cout << "PASS: CPX raw/multiple blocks, 8/16-bit streams, overlap, modes, prefetch, framing, caps, transactional failure\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
