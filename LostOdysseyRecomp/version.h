#pragma once

// The application target gets the single source version from root CMake.
// Standalone fixtures may include video.cpp without that target definition.
#ifndef LO_SOURCE_VERSION
#define LO_SOURCE_VERSION "development"
#endif

namespace lo_version
{
inline constexpr char Source[] = LO_SOURCE_VERSION;
inline constexpr char WindowTitle[] = "Lost Odyssey Recompiled " LO_SOURCE_VERSION;
}
