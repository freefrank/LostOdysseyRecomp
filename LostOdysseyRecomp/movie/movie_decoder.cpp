#include "movie_decoder.h"

namespace movie {
MovieDecoder::~MovieDecoder() = default;

#ifdef _WIN32
std::unique_ptr<MovieDecoder> CreateMfMovieDecoder(bool preferHardware);
#endif

std::unique_ptr<MovieDecoder> CreateMovieDecoder(bool preferHardware) {
#ifdef _WIN32
    return CreateMfMovieDecoder(preferHardware);
#else
    (void)preferHardware;
    return nullptr;
#endif
}
}
