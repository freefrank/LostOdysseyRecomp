#pragma once

template<typename T>
inline T RoundUp(const T& value, uint32_t round)
{
    return (value + round - 1) & ~(T(round) - 1);
}

template<typename T>
inline T RoundDown(const T& value, uint32_t round)
{
    return value & ~(T(round) - 1);
}

inline size_t StringHash(const std::string_view& str)
{
    return XXH3_64bits(str.data(), str.size());
}

inline std::vector<uint8_t> ReadAllBytes(const std::filesystem::path& path)
{
    std::vector<uint8_t> data;
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open())
        return data;

    stream.seekg(0, std::ios::end);
    data.resize(size_t(stream.tellg()));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(data.data()), data.size());
    return data;
}
