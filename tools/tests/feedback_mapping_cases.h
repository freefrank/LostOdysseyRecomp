#pragma once
#include <cstdint>

// Reviewed source identities and all observed PS pairs:
// tools/shader_analysis/reviews/feedback_mapping_20260925.json.
// This fixture uses synthetic inputs, not captured vertices/constants.
enum class FeedbackProjection { Static, Depth0, Material0, Material1, Alternate7 };
struct FeedbackMappingCase { uint64_t vs, ps; unsigned slot; FeedbackProjection family; };
inline constexpr FeedbackMappingCase feedbackMappings[]{
    {0x2d458def192151acull,0x4f9ef9b24449894bull,0,FeedbackProjection::Material0},
    {0x6b757ded853a7fc5ull,0x4d5c5950fbf2614bull,0,FeedbackProjection::Material0},
    {0x6d3d954bb6d86bc1ull,0x0000000000000000ull,0,FeedbackProjection::Depth0},
    {0x0fa0396a659f8da5ull,0x69840967ab0852aeull,1,FeedbackProjection::Material1},
    {0x2b36b5ca7a88912eull,0x2c5592c279ce0048ull,4,FeedbackProjection::Static},
    {0xac81dd5f6ed83c3eull,0xfd5cf45672b8f9b6ull,4,FeedbackProjection::Static},
    {0x03a4238064e6c836ull,0xa85fa3f22e2524e4ull,7,FeedbackProjection::Static},
    {0x09f67586057d7083ull,0x2f606db52def4352ull,7,FeedbackProjection::Static},
    {0x1a2f72d1dce268bdull,0x5fff3ecff0e2f9a1ull,7,FeedbackProjection::Static},
    {0x22993b734c035ae6ull,0x042b472a22384e2aull,7,FeedbackProjection::Static},
    {0x276b01d4190fdc00ull,0x23ab0f72cb66d185ull,7,FeedbackProjection::Static},
    {0x3638b6b020b068fcull,0x4f69db2650939d07ull,7,FeedbackProjection::Static},
    {0x3bb0b196f11e64e5ull,0x069095bcbf086364ull,7,FeedbackProjection::Static},
    {0x4ce42af298a9baefull,0x2f606db52def4352ull,7,FeedbackProjection::Static},
    {0x59006824a7515704ull,0x23ab0f72cb66d185ull,7,FeedbackProjection::Static},
    {0x6508c631689c4ffaull,0x23ab0f72cb66d185ull,7,FeedbackProjection::Static},
    {0x6976f82de60cb915ull,0x26bc87530b577ba2ull,7,FeedbackProjection::Static},
    {0x753287173badc7d1ull,0x2f606db52def4352ull,7,FeedbackProjection::Static},
    {0x7f2f709e14788599ull,0x730efd8459839fb7ull,7,FeedbackProjection::Alternate7},
    {0x8060e3f548febc94ull,0xa85fa3f22e2524e4ull,7,FeedbackProjection::Static},
    {0x8261a0b7daeac888ull,0xe3a453ca3a9f399bull,7,FeedbackProjection::Static},
    {0x8b986c8d09eab4e4ull,0x23ab0f72cb66d185ull,7,FeedbackProjection::Static},
    {0x8f6ce5a4f714294aull,0xa85fa3f22e2524e4ull,7,FeedbackProjection::Static},
    {0xa0a7fc243e60b248ull,0x4836f2562fb65cfaull,7,FeedbackProjection::Static},
    {0xa6314f321efa4d14ull,0x4a32c94f383f60b7ull,7,FeedbackProjection::Static},
    {0xae45651b20b50163ull,0x2f606db52def4352ull,7,FeedbackProjection::Static},
    {0xb0b143a646a921a7ull,0x09358fffc4385f14ull,7,FeedbackProjection::Static},
    {0xb14ecb62fe79be01ull,0x63a139fb4b924758ull,7,FeedbackProjection::Alternate7},
    {0xc01a72e0eb5e026aull,0xa85fa3f22e2524e4ull,7,FeedbackProjection::Static},
    {0xc560d140940528bcull,0x15e058b115d8368bull,7,FeedbackProjection::Static},
    {0xd6ead6f46d70b19aull,0x6f0c25f9a0481ea8ull,7,FeedbackProjection::Static},
    {0xe8747802c970e598ull,0xfd351d6e88767cb4ull,7,FeedbackProjection::Static},
    {0xf5ca0812e57cd43cull,0x5b0c2c071555614dull,7,FeedbackProjection::Static},
    {0x0e5a12f70e7cb5beull,0x6d8a07a07c42e89full,8,FeedbackProjection::Static},
    {0xf3838aa008bc39d8ull,0x155d57e3b85a6f2cull,8,FeedbackProjection::Static},
};
inline constexpr uint64_t feedbackHeldMappings[]{
    0xeb5f611c4321708eull,
    0xbda41a11626a545cull,
    0xe810cfacc107fd3cull,
};
