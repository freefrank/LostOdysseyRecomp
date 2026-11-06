#pragma once
#include <string>

namespace os::debug_server
{
// A PPM ("P6 w h 255", as video::SaveScreenshot writes it) as a PNG, compressed row by row: the image is never in
// memory whole (2.7 MB at 720p); the compressor (~300 KiB) and the compressed bytes are. False with why on a file
// that is not an 8-bit PPM or when memory runs out. Uses the miniz the updater links (updater/stage.cpp).
bool PpmToPng(const std::string& path, std::string& png, std::string& why);
}
