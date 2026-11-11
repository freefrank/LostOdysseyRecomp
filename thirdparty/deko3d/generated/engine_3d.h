#pragma once

namespace maxwell {

#ifndef __dekodef_helpers
#define __dekodef_helpers
	template <typename T> constexpr unsigned ClassId = T::__classid;
	template <typename T, unsigned index, unsigned size>
	struct __reg {
		using Parent = T;
		static constexpr unsigned Index = index;
		static constexpr unsigned Size = size;
		const unsigned __idx;
		constexpr operator unsigned() const noexcept { return __idx; }
		constexpr __reg() noexcept : __idx{Index} {}
		constexpr __reg(unsigned i) noexcept : __idx{ Parent::Index + i*Parent::Size + Index } {}
	};
	template <typename T, unsigned index, unsigned count, unsigned shift>
	struct __array : __reg<T,index,(1U<<shift)> {
		using Parent = T;
		static constexpr bool __isarray = true;
		static constexpr unsigned Count = count;
		static constexpr unsigned Shift = shift;
	};
	template <unsigned shift, unsigned size>
	struct __bit {
		static constexpr unsigned Shift = shift;
		static constexpr unsigned Size = size;
		static constexpr unsigned Mask = ((1U<<Size)-1)<<Shift;
		const unsigned __value;
		constexpr operator unsigned() const noexcept { return __value; }
		constexpr __bit(unsigned x = ~0U) noexcept : __value{ (x<<Shift) & Mask } {}
	};
#endif

	struct Engine3D {
		using __self = Engine3D;
		static constexpr unsigned __classid = 0xb197;
		struct NoOperation : __reg<__self, 0x040, 1> {
			using __reg::__reg;
		};
		struct WaitForIdle : __reg<__self, 0x044, 1> {
			using __reg::__reg;
		};
		struct MmeInstructionRamPointer : __reg<__self, 0x045, 1> {
			using __reg::__reg;
		};
		struct MmeInstructionRamLoad : __reg<__self, 0x046, 1> {
			using __reg::__reg;
		};
		struct MmeStartAddressRamPointer : __reg<__self, 0x047, 1> {
			using __reg::__reg;
		};
		struct MmeStartAddressRamLoad : __reg<__self, 0x048, 1> {
			using __reg::__reg;
		};
		struct MmeShadowRamControl : __reg<__self, 0x049, 1> {
			using __reg::__reg;
			static constexpr auto MethodTrack = 0;
			static constexpr auto MethodTrackWithFilter = 1;
			static constexpr auto MethodPassthrough = 2;
			static constexpr auto MethodReplay = 3;
		};
		struct SetInstrumentationMethodHeader : __reg<__self, 0x054, 1> {
			using __reg::__reg;
		};
		struct SetInstrumentationMethodData : __reg<__self, 0x055, 1> {
			using __reg::__reg;
		};
		struct LineWidthSeparate : __reg<__self, 0x083, 1> {
			using __reg::__reg;
		};
		struct Unknown086 : __reg<__self, 0x086, 1> {
			using __reg::__reg;
		};
		struct SyncptAction : __reg<__self, 0x0b2, 1> {
			using __reg::__reg;
			struct Id : __bit<0,12> {
				using __bit::__bit;
			};
			struct FlushCache : __bit<16,1> {
				using __bit::__bit;
			};
			struct Increment : __bit<20,1> {
				using __bit::__bit;
			};
		};
		struct SurfaceDecompress : __reg<__self, 0x0b8, 1> {
			using __reg::__reg;
		};
		struct ZcullUnkFeatureEnable : __reg<__self, 0x0ba, 1> {
			using __reg::__reg;
		};
		struct Unknown0bb : __reg<__self, 0x0bb, 1> {
			using __reg::__reg;
		};
		struct Unknown0c0 : __reg<__self, 0x0c0, 1> {
			using __reg::__reg;
		};
		struct Unknown0c3 : __reg<__self, 0x0c3, 1> {
			using __reg::__reg;
		};
		struct SetSpaVersion : __reg<__self, 0x0c4, 1> {
			using __reg::__reg;
			struct Minor : __bit<0,8> {
				using __bit::__bit;
			};
			struct Major : __bit<8,8> {
				using __bit::__bit;
			};
		};
		struct TessellationMode : __reg<__self, 0x0c8, 1> {
			using __reg::__reg;
			struct PrimitiveType : __bit<0,2> {
				using __bit::__bit;
				static constexpr unsigned Isolines = 0x00000000;
				static constexpr unsigned Triangles = 0x00000001;
				static constexpr unsigned Quads = 0x00000002;
			};
			struct Spacing : __bit<4,2> {
				using __bit::__bit;
				static constexpr unsigned Equal = 0x00000000;
				static constexpr unsigned FractionalOdd = 0x00000010;
				static constexpr unsigned FractionalEven = 0x00000020;
			};
			struct WindingAndConnectednessFlags : __bit<8,2> {
				using __bit::__bit;
				static constexpr unsigned CcwAndNotConnected = 0x00000000;
				static constexpr unsigned IsolineConnected = 0x00000100;
				static constexpr unsigned TriangleCw = 0x00000100;
				static constexpr unsigned TriangleConnected = 0x00000200;
				static constexpr unsigned TriangleConnectedCw = 0x00000300;
			};
		};
		struct TessellationOuterLevels : __reg<__self, 0x0c9, 4> {
			using __reg::__reg;
		};
		struct TessellationInnerLevels : __reg<__self, 0x0cd, 2> {
			using __reg::__reg;
		};
		struct Unknown0dd : __reg<__self, 0x0dd, 1> {
			using __reg::__reg;
		};
		struct RasterizerEnable : __reg<__self, 0x0df, 1> {
			using __reg::__reg;
		};
		struct Unknown1d3 : __reg<__self, 0x1d3, 1> {
			using __reg::__reg;
		};
		struct SetShaderLocalMemoryWindow : __reg<__self, 0x1df, 1> {
			using __reg::__reg;
		};
		struct SetShaderLocalMemory : __reg<__self, 0x1e4, 2> {
			using __reg::__reg;
		};
		struct SetShaderLocalMemorySize : __reg<__self, 0x1e6, 2> {
			using __reg::__reg;
		};
		struct ZcullWidth : __reg<__self, 0x1f0, 1> {
			using __reg::__reg;
		};
		struct ZcullHeight : __reg<__self, 0x1f1, 1> {
			using __reg::__reg;
		};
		struct ZcullDepth : __reg<__self, 0x1f2, 1> {
			using __reg::__reg;
		};
		struct ZcullUnknown0 : __reg<__self, 0x1f3, 1> {
			using __reg::__reg;
		};
		struct ZcullImageSizeAliquots : __reg<__self, 0x1f8, 1> {
			using __reg::__reg;
		};
		struct ZcullLayerSizeAliquots : __reg<__self, 0x1f9, 1> {
			using __reg::__reg;
		};
		struct RenderTarget : __array<__self, 0x200, 8, 4> {
			using __self = RenderTarget;
			struct Addr : __reg<__self, 0x000, 2> {
				using __reg::__reg;
			};
			struct Horizontal : __reg<__self, 0x002, 1> {
				using __reg::__reg;
			};
			struct Vertical : __reg<__self, 0x003, 1> {
				using __reg::__reg;
			};
			struct Format : __reg<__self, 0x004, 1> {
				using __reg::__reg;
			};
			struct TileMode : __reg<__self, 0x005, 1> {
				using __reg::__reg;
				struct Width : __bit<0,4> {
					using __bit::__bit;
				};
				struct Height : __bit<4,4> {
					using __bit::__bit;
				};
				struct Depth : __bit<8,4> {
					using __bit::__bit;
				};
				struct IsLinear : __bit<12,1> {
					using __bit::__bit;
				};
				struct Is3D : __bit<16,1> {
					using __bit::__bit;
				};
			};
			struct ArrayMode : __reg<__self, 0x006, 1> {
				using __reg::__reg;
				struct Layers : __bit<0,16> {
					using __bit::__bit;
				};
				struct Volume : __bit<16,1> {
					using __bit::__bit;
				};
			};
			struct LayerStride : __reg<__self, 0x007, 1> {
				using __reg::__reg;
			};
			struct BaseLayer : __reg<__self, 0x008, 1> {
				using __reg::__reg;
			};
		};
		struct ViewportTransform : __array<__self, 0x280, 16, 3> {
			using __self = ViewportTransform;
			struct ScaleX : __reg<__self, 0x000, 1> {
				using __reg::__reg;
			};
			struct ScaleY : __reg<__self, 0x001, 1> {
				using __reg::__reg;
			};
			struct ScaleZ : __reg<__self, 0x002, 1> {
				using __reg::__reg;
			};
			struct TranslateX : __reg<__self, 0x003, 1> {
				using __reg::__reg;
			};
			struct TranslateY : __reg<__self, 0x004, 1> {
				using __reg::__reg;
			};
			struct TranslateZ : __reg<__self, 0x005, 1> {
				using __reg::__reg;
			};
			struct Swizzles : __reg<__self, 0x006, 1> {
				using __reg::__reg;
				struct X : __bit<0,3> {
					using __bit::__bit;
				};
				struct Y : __bit<4,3> {
					using __bit::__bit;
				};
				struct Z : __bit<8,3> {
					using __bit::__bit;
				};
				struct W : __bit<12,3> {
					using __bit::__bit;
				};
			};
			struct SubpixelPrecisionBias : __reg<__self, 0x007, 1> {
				using __reg::__reg;
				struct X : __bit<0,5> {
					using __bit::__bit;
				};
				struct Y : __bit<8,5> {
					using __bit::__bit;
				};
			};
		};
		struct Viewport : __array<__self, 0x300, 16, 2> {
			using __self = Viewport;
			struct Horizontal : __reg<__self, 0x000, 1> {
				using __reg::__reg;
				struct X : __bit<0,16> {
					using __bit::__bit;
				};
				struct Width : __bit<16,16> {
					using __bit::__bit;
				};
			};
			struct Vertical : __reg<__self, 0x001, 1> {
				using __reg::__reg;
				struct Y : __bit<0,16> {
					using __bit::__bit;
				};
				struct Height : __bit<16,16> {
					using __bit::__bit;
				};
			};
			struct DepthRangeNear : __reg<__self, 0x002, 1> {
				using __reg::__reg;
			};
			struct DepthRangeFar : __reg<__self, 0x003, 1> {
				using __reg::__reg;
			};
		};
		struct WindowRectangle : __array<__self, 0x340, 8, 1> {
			using __self = WindowRectangle;
			struct Horizontal : __reg<__self, 0x000, 1> {
				using __reg::__reg;
				struct Min : __bit<0,16> {
					using __bit::__bit;
				};
				struct Max : __bit<16,16> {
					using __bit::__bit;
				};
			};
			struct Vertical : __reg<__self, 0x001, 1> {
				using __reg::__reg;
				struct Min : __bit<0,16> {
					using __bit::__bit;
				};
				struct Max : __bit<16,16> {
					using __bit::__bit;
				};
			};
		};
		struct SetApiVisibleCallLimit : __reg<__self, 0x359, 1> {
			using __reg::__reg;
			struct Cta : __bit<0,4> {
				using __bit::__bit;
				static constexpr unsigned _0 = 0x00000000;
				static constexpr unsigned _1 = 0x00000001;
				static constexpr unsigned _2 = 0x00000002;
				static constexpr unsigned _4 = 0x00000003;
				static constexpr unsigned _8 = 0x00000004;
				static constexpr unsigned _16 = 0x00000005;
				static constexpr unsigned _32 = 0x00000006;
				static constexpr unsigned _64 = 0x00000007;
				static constexpr unsigned _128 = 0x00000008;
				static constexpr unsigned NoCheck = 0x0000000F;
			};
		};
		struct DrawArraysFirst : __reg<__self, 0x35d, 1> {
			using __reg::__reg;
		};
		struct DrawArraysCount : __reg<__self, 0x35e, 1> {
			using __reg::__reg;
		};
		struct SetDepthMode : __reg<__self, 0x35f, 1> {
			using __reg::__reg;
			static constexpr auto MinusOneToOne = 0;
			static constexpr auto ZeroToOne = 1;
		};
		struct ClearColor : __reg<__self, 0x360, 4> {
			using __reg::__reg;
		};
		struct ClearDepth : __reg<__self, 0x364, 1> {
			using __reg::__reg;
		};
		struct ColorReductionEnable : __reg<__self, 0x367, 1> {
			using __reg::__reg;
		};
		struct ClearStencil : __reg<__self, 0x368, 1> {
			using __reg::__reg;
		};
		struct InvalidateShaderCaches : __reg<__self, 0x369, 1> {
			using __reg::__reg;
			struct Instruction : __bit<0,1> {
				using __bit::__bit;
			};
			struct GlobalData : __bit<4,1> {
				using __bit::__bit;
			};
			struct Constant : __bit<12,1> {
				using __bit::__bit;
			};
		};
		struct SetPolygonModeFront : __reg<__self, 0x36b, 1> {
			using __reg::__reg;
			static constexpr auto Point = 6912;
			static constexpr auto Line = 6913;
			static constexpr auto Fill = 6914;
		};
		struct SetPolygonModeBack : __reg<__self, 0x36c, 1> {
			using __reg::__reg;
			static constexpr auto Point = 6912;
			static constexpr auto Line = 6913;
			static constexpr auto Fill = 6914;
		};
		struct PolygonSmoothEnable : __reg<__self, 0x36d, 1> {
			using __reg::__reg;
		};
		struct ZcullClearDepth : __reg<__self, 0x36f, 1> {
			using __reg::__reg;
			struct IsLessThanHalf : __bit<0,1> {
				using __bit::__bit;
			};
			struct IsOneOrZero : __bit<16,1> {
				using __bit::__bit;
			};
		};
		struct PolygonOffsetPointEnable : __reg<__self, 0x370, 1> {
			using __reg::__reg;
		};
		struct PolygonOffsetLineEnable : __reg<__self, 0x371, 1> {
			using __reg::__reg;
		};
		struct PolygonOffsetFillEnable : __reg<__self, 0x372, 1> {
			using __reg::__reg;
		};
		struct TessellationPatchSize : __reg<__self, 0x373, 1> {
			using __reg::__reg;
		};
		struct AdvancedBlendEnable : __reg<__self, 0x374, 1> {
			using __reg::__reg;
		};
		struct ZcullStencilCriteria : __reg<__self, 0x376, 1> {
			using __reg::__reg;
			struct Func : __bit<0,8> {
				using __bit::__bit;
			};
			struct FuncRef : __bit<16,8> {
				using __bit::__bit;
			};
			struct FuncMask : __bit<24,8> {
				using __bit::__bit;
			};
		};
		struct FragmentBarrier : __reg<__self, 0x378, 1> {
			using __reg::__reg;
		};
		struct PrimitiveRestartWithDrawArrays : __reg<__self, 0x37a, 1> {
			using __reg::__reg;
		};
		struct Scissor : __array<__self, 0x380, 16, 2> {
			using __self = Scissor;
			struct Enable : __reg<__self, 0x000, 1> {
				using __reg::__reg;
			};
			struct Horizontal : __reg<__self, 0x001, 1> {
				using __reg::__reg;
				struct Min : __bit<0,16> {
					using __bit::__bit;
				};
				struct Max : __bit<16,16> {
					using __bit::__bit;
				};
			};
			struct Vertical : __reg<__self, 0x002, 1> {
				using __reg::__reg;
				struct Min : __bit<0,16> {
					using __bit::__bit;
				};
				struct Max : __bit<16,16> {
					using __bit::__bit;
				};
			};
		};
		struct StencilBackFuncRef : __reg<__self, 0x3d5, 1> {
			using __reg::__reg;
		};
		struct StencilBackMask : __reg<__self, 0x3d6, 1> {
			using __reg::__reg;
		};
		struct StencilBackFuncMask : __reg<__self, 0x3d7, 1> {
			using __reg::__reg;
		};
		struct TiledCacheEnable : __reg<__self, 0x3d8, 1> {
			using __reg::__reg;
		};
		struct TiledCacheTileSize : __reg<__self, 0x3d9, 1> {
			using __reg::__reg;
			struct Width : __bit<0,16> {
				using __bit::__bit;
			};
			struct Height : __bit<16,16> {
				using __bit::__bit;
			};
		};
		struct TiledCacheUnknownConfig0 : __reg<__self, 0x3da, 1> {
			using __reg::__reg;
		};
		struct TiledCacheUnknownConfig1 : __reg<__self, 0x3db, 1> {
			using __reg::__reg;
		};
		struct TiledCacheUnknownConfig3 : __reg<__self, 0x3dc, 1> {
			using __reg::__reg;
		};
		struct InvalidateTextureDataCache : __reg<__self, 0x3dd, 1> {
			using __reg::__reg;
			struct Lines : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned All = 0x00000000;
				static constexpr unsigned One = 0x00000001;
			};
			struct Tag : __bit<4,22> {
				using __bit::__bit;
			};
		};
		struct DiscardRenderTarget : __reg<__self, 0x3de, 1> {
			using __reg::__reg;
			struct DepthStencil : __bit<0,1> {
				using __bit::__bit;
			};
			struct Color : __bit<4,3> {
				using __bit::__bit;
			};
		};
		struct TiledCacheBarrier : __reg<__self, 0x3df, 1> {
			using __reg::__reg;
		};
		struct TiledCacheFlush : __reg<__self, 0x3e0, 1> {
			using __reg::__reg;
			static constexpr auto Flush = 0;
			static constexpr auto Unknown = 1;
		};
		struct VertexRunoutBufferIova : __reg<__self, 0x3e1, 2> {
			using __reg::__reg;
		};
		struct Unknown3e3 : __reg<__self, 0x3e3, 1> {
			using __reg::__reg;
		};
		struct DepthBoundsNear : __reg<__self, 0x3e7, 1> {
			using __reg::__reg;
		};
		struct DepthBoundsFar : __reg<__self, 0x3e8, 1> {
			using __reg::__reg;
		};
		struct SetMultisampleRasterEnable : __reg<__self, 0x3ed, 1> {
			using __reg::__reg;
		};
		struct MultisampleRasterSamples : __reg<__self, 0x3ee, 1> {
			using __reg::__reg;
		};
		struct MultisampleSampleMask : __reg<__self, 0x3ef, 4> {
			using __reg::__reg;
		};
		struct CoverageModulationMode : __reg<__self, 0x3f5, 1> {
			using __reg::__reg;
			struct Color : __bit<0,1> {
				using __bit::__bit;
			};
			struct Alpha : __bit<1,1> {
				using __bit::__bit;
			};
		};
		struct SetCoverageModulationTableEnable : __reg<__self, 0x3f6, 1> {
			using __reg::__reg;
		};
		struct Unknown3f7 : __reg<__self, 0x3f7, 1> {
			using __reg::__reg;
		};
		struct DepthTargetAddr : __reg<__self, 0x3f8, 2> {
			using __reg::__reg;
		};
		struct DepthTargetFormat : __reg<__self, 0x3fa, 1> {
			using __reg::__reg;
		};
		struct DepthTargetTileMode : __reg<__self, 0x3fb, 1> {
			using __reg::__reg;
			struct Width : __bit<0,4> {
				using __bit::__bit;
			};
			struct Height : __bit<4,4> {
				using __bit::__bit;
			};
			struct Depth : __bit<8,4> {
				using __bit::__bit;
			};
		};
		struct DepthTargetLayerStride : __reg<__self, 0x3fc, 1> {
			using __reg::__reg;
		};
		struct ScreenScissorHorizontal : __reg<__self, 0x3fd, 1> {
			using __reg::__reg;
			struct X : __bit<0,16> {
				using __bit::__bit;
			};
			struct Width : __bit<16,16> {
				using __bit::__bit;
			};
		};
		struct ScreenScissorVertical : __reg<__self, 0x3fe, 1> {
			using __reg::__reg;
			struct Y : __bit<0,16> {
				using __bit::__bit;
			};
			struct Height : __bit<16,16> {
				using __bit::__bit;
			};
		};
		struct Unknown400 : __reg<__self, 0x400, 1> {
			using __reg::__reg;
		};
		struct CoverageModulationTable : __reg<__self, 0x40c, 4> {
			using __reg::__reg;
		};
		struct ColorReductionConfig0 : __reg<__self, 0x433, 1> {
			using __reg::__reg;
		};
		struct ColorReductionConfig1 : __reg<__self, 0x438, 1> {
			using __reg::__reg;
		};
		struct ColorReductionConfig2 : __reg<__self, 0x439, 1> {
			using __reg::__reg;
		};
		struct ColorReductionConfig3 : __reg<__self, 0x43a, 1> {
			using __reg::__reg;
		};
		struct ColorReductionConfig4 : __reg<__self, 0x43b, 1> {
			using __reg::__reg;
		};
		struct ColorReductionConfig5 : __reg<__self, 0x43c, 1> {
			using __reg::__reg;
		};
		struct ClearBufferFlags : __reg<__self, 0x43e, 1> {
			using __reg::__reg;
			struct StencilMask : __bit<0,1> {
				using __bit::__bit;
			};
			struct Scissor : __bit<8,1> {
				using __bit::__bit;
			};
			struct Viewport : __bit<12,1> {
				using __bit::__bit;
			};
		};
		struct Unknown43f : __reg<__self, 0x43f, 1> {
			using __reg::__reg;
		};
		struct TiledCacheUnknownConfig2 : __reg<__self, 0x442, 1> {
			using __reg::__reg;
		};
		struct VertexIdBase : __reg<__self, 0x446, 1> {
			using __reg::__reg;
		};
		struct Unknown44c : __reg<__self, 0x44c, 1> {
			using __reg::__reg;
		};
		struct TiledCacheUnkFeatureEnable : __reg<__self, 0x44d, 1> {
			using __reg::__reg;
		};
		struct FillRectangleConfig : __reg<__self, 0x44f, 1> {
			using __reg::__reg;
			struct Enable : __bit<1,1> {
				using __bit::__bit;
			};
		};
		struct Unknown450 : __reg<__self, 0x450, 1> {
			using __reg::__reg;
		};
		struct UnknownFlush : __reg<__self, 0x451, 1> {
			using __reg::__reg;
		};
		struct SetConservativeRasterEnable : __reg<__self, 0x452, 1> {
			using __reg::__reg;
		};
		struct VertexAttribState : __reg<__self, 0x458, 32> {
			using __reg::__reg;
			struct BufferId : __bit<0,5> {
				using __bit::__bit;
			};
			struct IsFixed : __bit<6,1> {
				using __bit::__bit;
			};
			struct Offset : __bit<7,14> {
				using __bit::__bit;
			};
			struct Size : __bit<21,6> {
				using __bit::__bit;
			};
			struct Type : __bit<27,3> {
				using __bit::__bit;
			};
			struct IsBgra : __bit<31,1> {
				using __bit::__bit;
			};
		};
		struct MultisampleSampleLocations : __reg<__self, 0x478, 4> {
			using __reg::__reg;
		};
		struct MultisampleCoverageToColor : __reg<__self, 0x47e, 1> {
			using __reg::__reg;
			struct Enable : __bit<0,1> {
				using __bit::__bit;
			};
			struct Target : __bit<4,3> {
				using __bit::__bit;
			};
		};
		struct ResolveDepthValues : __reg<__self, 0x47f, 1> {
			using __reg::__reg;
		};
		struct TiledCacheUnkConfig : __reg<__self, 0x485, 1> {
			using __reg::__reg;
		};
		struct RenderTargetControl : __reg<__self, 0x487, 1> {
			using __reg::__reg;
			struct NumTargets : __bit<0,4> {
				using __bit::__bit;
			};
			struct Map0 : __bit<4,3> {
				using __bit::__bit;
			};
			struct Map1 : __bit<7,3> {
				using __bit::__bit;
			};
			struct Map2 : __bit<10,3> {
				using __bit::__bit;
			};
			struct Map3 : __bit<13,3> {
				using __bit::__bit;
			};
			struct Map4 : __bit<16,3> {
				using __bit::__bit;
			};
			struct Map5 : __bit<19,3> {
				using __bit::__bit;
			};
			struct Map6 : __bit<22,3> {
				using __bit::__bit;
			};
			struct Map7 : __bit<25,3> {
				using __bit::__bit;
			};
		};
		struct Unknown488 : __reg<__self, 0x488, 1> {
			using __reg::__reg;
		};
		struct DepthTargetHorizontal : __reg<__self, 0x48a, 1> {
			using __reg::__reg;
		};
		struct DepthTargetVertical : __reg<__self, 0x48b, 1> {
			using __reg::__reg;
		};
		struct DepthTargetArrayMode : __reg<__self, 0x48c, 1> {
			using __reg::__reg;
			struct Layers : __bit<0,16> {
				using __bit::__bit;
			};
			struct Volume : __bit<16,1> {
				using __bit::__bit;
			};
		};
		struct LinkedTsc : __reg<__self, 0x48d, 1> {
			using __reg::__reg;
		};
		struct InvalidateTextureDataCacheNoWfi : __reg<__self, 0x4a2, 1> {
			using __reg::__reg;
			struct Lines : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned All = 0x00000000;
				static constexpr unsigned One = 0x00000001;
			};
			struct Tag : __bit<4,22> {
				using __bit::__bit;
			};
		};
		struct Unknown4a4 : __reg<__self, 0x4a4, 1> {
			using __reg::__reg;
		};
		struct InvalidateZcullNoWfi : __reg<__self, 0x4b2, 1> {
			using __reg::__reg;
		};
		struct DepthTestEnable : __reg<__self, 0x4b3, 1> {
			using __reg::__reg;
		};
		struct Unknown4b6 : __reg<__self, 0x4b6, 2> {
			using __reg::__reg;
		};
		struct AlphaToCoverageDither : __reg<__self, 0x4b8, 1> {
			using __reg::__reg;
		};
		struct IndependentBlendEnable : __reg<__self, 0x4b9, 1> {
			using __reg::__reg;
		};
		struct DepthWriteEnable : __reg<__self, 0x4ba, 1> {
			using __reg::__reg;
		};
		struct AlphaTestEnable : __reg<__self, 0x4bb, 1> {
			using __reg::__reg;
		};
		struct DepthTestFunc : __reg<__self, 0x4c3, 1> {
			using __reg::__reg;
			static constexpr auto Never = 1;
			static constexpr auto Less = 2;
			static constexpr auto Equal = 3;
			static constexpr auto Lequal = 4;
			static constexpr auto Greater = 5;
			static constexpr auto NotEqual = 6;
			static constexpr auto Gequal = 7;
			static constexpr auto Always = 8;
		};
		struct AlphaTestRef : __reg<__self, 0x4c4, 1> {
			using __reg::__reg;
		};
		struct AlphaTestFunc : __reg<__self, 0x4c5, 1> {
			using __reg::__reg;
			static constexpr auto Never = 1;
			static constexpr auto Less = 2;
			static constexpr auto Equal = 3;
			static constexpr auto Lequal = 4;
			static constexpr auto Greater = 5;
			static constexpr auto NotEqual = 6;
			static constexpr auto Gequal = 7;
			static constexpr auto Always = 8;
		};
		struct BlendConstant : __reg<__self, 0x4c7, 4> {
			using __reg::__reg;
		};
		struct ColorBlendEnable : __reg<__self, 0x4d8, 8> {
			using __reg::__reg;
		};
		struct StencilEnable : __reg<__self, 0x4e0, 1> {
			using __reg::__reg;
		};
		struct StencilFrontOpFail : __reg<__self, 0x4e1, 1> {
			using __reg::__reg;
		};
		struct StencilFrontOpZFail : __reg<__self, 0x4e2, 1> {
			using __reg::__reg;
		};
		struct StencilFrontOpZPass : __reg<__self, 0x4e3, 1> {
			using __reg::__reg;
		};
		struct StencilFrontFunc : __reg<__self, 0x4e4, 1> {
			using __reg::__reg;
		};
		struct StencilFrontFuncRef : __reg<__self, 0x4e5, 1> {
			using __reg::__reg;
		};
		struct StencilFrontFuncMask : __reg<__self, 0x4e6, 1> {
			using __reg::__reg;
		};
		struct StencilFrontMask : __reg<__self, 0x4e7, 1> {
			using __reg::__reg;
		};
		struct SetWindowOriginMode : __reg<__self, 0x4eb, 1> {
			using __reg::__reg;
			struct NegateY : __bit<0,1> {
				using __bit::__bit;
			};
			struct Mode : __bit<4,1> {
				using __bit::__bit;
				static constexpr unsigned UpperLeft = 0x00000000;
				static constexpr unsigned LowerLeft = 0x00000010;
			};
		};
		struct LineWidthSmooth : __reg<__self, 0x4ec, 1> {
			using __reg::__reg;
		};
		struct LineWidthAliased : __reg<__self, 0x4ed, 1> {
			using __reg::__reg;
		};
		struct InvalidateSamplerCacheNoWfi : __reg<__self, 0x509, 1> {
			using __reg::__reg;
			struct Lines : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned All = 0x00000000;
				static constexpr unsigned One = 0x00000001;
			};
			struct Tag : __bit<4,22> {
				using __bit::__bit;
			};
		};
		struct InvalidateTextureHeaderCacheNoWfi : __reg<__self, 0x50a, 1> {
			using __reg::__reg;
			struct Lines : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned All = 0x00000000;
				static constexpr unsigned One = 0x00000001;
			};
			struct Tag : __bit<4,22> {
				using __bit::__bit;
			};
		};
		struct DrawBaseVertex : __reg<__self, 0x50d, 1> {
			using __reg::__reg;
		};
		struct DrawBaseInstance : __reg<__self, 0x50e, 1> {
			using __reg::__reg;
		};
		struct Unknown514 : __reg<__self, 0x514, 1> {
			using __reg::__reg;
		};
		struct ClipDistanceEnable : __reg<__self, 0x544, 1> {
			using __reg::__reg;
		};
		struct SampleCounterEnable : __reg<__self, 0x545, 1> {
			using __reg::__reg;
		};
		struct PointSpriteSize : __reg<__self, 0x546, 1> {
			using __reg::__reg;
		};
		struct ZcullStatCountersEnable : __reg<__self, 0x547, 1> {
			using __reg::__reg;
		};
		struct PointSpriteEnable : __reg<__self, 0x548, 1> {
			using __reg::__reg;
		};
		struct SetShaderExceptions : __reg<__self, 0x54a, 1> {
			using __reg::__reg;
		};
		struct ResetCounter : __reg<__self, 0x54c, 1> {
			using __reg::__reg;
			static constexpr auto SamplesPassed = 1;
			static constexpr auto ZcullStats = 2;
			static constexpr auto TransformFeedbackPrimitivesWritten = 16;
			static constexpr auto InputVertices = 18;
			static constexpr auto InputPrimitives = 19;
			static constexpr auto VertexShaderInvocations = 21;
			static constexpr auto TessControlShaderInvocations = 22;
			static constexpr auto TessEvaluationShaderInvocations = 23;
			static constexpr auto TessEvaluationShaderPrimitives = 24;
			static constexpr auto GeometryShaderInvocations = 26;
			static constexpr auto GeometryShaderPrimitives = 27;
			static constexpr auto ClipperInputPrimitives = 28;
			static constexpr auto ClipperOutputPrimitives = 29;
			static constexpr auto FragmentShaderInvocations = 30;
			static constexpr auto PrimitivesGenerated = 31;
		};
		struct MultisampleEnable : __reg<__self, 0x54d, 1> {
			using __reg::__reg;
		};
		struct DepthTargetEnable : __reg<__self, 0x54e, 1> {
			using __reg::__reg;
		};
		struct MultisampleControl : __reg<__self, 0x54f, 1> {
			using __reg::__reg;
			struct AlphaToCoverage : __bit<0,1> {
				using __bit::__bit;
			};
			struct AlphaToOne : __bit<4,1> {
				using __bit::__bit;
			};
		};
		struct SetTexSamplerPool : __reg<__self, 0x557, 2> {
			using __reg::__reg;
		};
		struct SetTexSamplerPoolMaximumIndex : __reg<__self, 0x559, 1> {
			using __reg::__reg;
		};
		struct PolygonOffsetFactor : __reg<__self, 0x55b, 1> {
			using __reg::__reg;
		};
		struct LineSmoothEnable : __reg<__self, 0x55c, 1> {
			using __reg::__reg;
		};
		struct SetTexHeaderPool : __reg<__self, 0x55d, 2> {
			using __reg::__reg;
		};
		struct SetTexHeaderPoolMaximumIndex : __reg<__self, 0x55f, 1> {
			using __reg::__reg;
		};
		struct ZcullRegion : __reg<__self, 0x564, 1> {
			using __reg::__reg;
		};
		struct StencilTwoSideEnable : __reg<__self, 0x565, 1> {
			using __reg::__reg;
		};
		struct StencilBackOpFail : __reg<__self, 0x566, 1> {
			using __reg::__reg;
		};
		struct StencilBackOpZFail : __reg<__self, 0x567, 1> {
			using __reg::__reg;
		};
		struct StencilBackOpZPass : __reg<__self, 0x568, 1> {
			using __reg::__reg;
		};
		struct StencilBackFunc : __reg<__self, 0x569, 1> {
			using __reg::__reg;
		};
		struct CsaaEnable : __reg<__self, 0x56d, 1> {
			using __reg::__reg;
		};
		struct PolygonOffsetUnits : __reg<__self, 0x56f, 1> {
			using __reg::__reg;
		};
		struct ZcullZetaType : __reg<__self, 0x572, 1> {
			using __reg::__reg;
		};
		struct SetRenderLayer : __reg<__self, 0x573, 1> {
			using __reg::__reg;
			struct Index : __bit<0,16> {
				using __bit::__bit;
			};
			struct UseIndexFromVTG : __bit<16,1> {
				using __bit::__bit;
			};
		};
		struct MultisampleMode : __reg<__self, 0x574, 1> {
			using __reg::__reg;
		};
		struct EdgeFlag : __reg<__self, 0x579, 1> {
			using __reg::__reg;
		};
		struct ZcullWindowOffsetX : __reg<__self, 0x57f, 1> {
			using __reg::__reg;
		};
		struct ZcullWindowOffsetY : __reg<__self, 0x580, 1> {
			using __reg::__reg;
		};
		struct PointCoordReplace : __reg<__self, 0x581, 1> {
			using __reg::__reg;
			struct Unknown0 : __bit<0,2> {
				using __bit::__bit;
			};
			struct CoordOrigin : __bit<2,1> {
				using __bit::__bit;
				static constexpr unsigned LowerLeft = 0x00000000;
				static constexpr unsigned UpperLeft = 0x00000004;
			};
			struct Enable : __bit<3,10> {
				using __bit::__bit;
			};
		};
		struct SetProgramRegion : __reg<__self, 0x582, 2> {
			using __reg::__reg;
		};
		struct Unknown584 : __reg<__self, 0x584, 1> {
			using __reg::__reg;
		};
		struct VertexEndGl : __reg<__self, 0x585, 1> {
			using __reg::__reg;
		};
		struct VertexBeginGl : __reg<__self, 0x586, 1> {
			using __reg::__reg;
			struct Primitive : __bit<0,16> {
				using __bit::__bit;
				static constexpr unsigned Points = 0x00000000;
				static constexpr unsigned Lines = 0x00000001;
				static constexpr unsigned LineLoop = 0x00000002;
				static constexpr unsigned LineStrip = 0x00000003;
				static constexpr unsigned Triangles = 0x00000004;
				static constexpr unsigned TriangleStrip = 0x00000005;
				static constexpr unsigned TriangleFan = 0x00000006;
				static constexpr unsigned Quads = 0x00000007;
				static constexpr unsigned QuadStrip = 0x00000008;
				static constexpr unsigned Polygon = 0x00000009;
				static constexpr unsigned LinesAdjacency = 0x0000000A;
				static constexpr unsigned LineStripAdjacency = 0x0000000B;
				static constexpr unsigned TrianglesAdjacency = 0x0000000C;
				static constexpr unsigned TriangleStripAdjacency = 0x0000000D;
				static constexpr unsigned Patches = 0x0000000E;
			};
			struct InstanceNext : __bit<26,1> {
				using __bit::__bit;
			};
			struct InstanceCont : __bit<27,1> {
				using __bit::__bit;
			};
		};
		struct PrimitiveRestartEnable : __reg<__self, 0x591, 1> {
			using __reg::__reg;
		};
		struct PrimitiveRestartIndex : __reg<__self, 0x592, 1> {
			using __reg::__reg;
		};
		struct VertexIdConfig : __reg<__self, 0x593, 1> {
			using __reg::__reg;
			struct DrawArraysAddStart : __bit<12,1> {
				using __bit::__bit;
			};
		};
		struct PointSmoothEnable : __reg<__self, 0x596, 1> {
			using __reg::__reg;
		};
		struct PointRasterRules : __reg<__self, 0x597, 1> {
			using __reg::__reg;
			static constexpr auto OpenGL = 0;
			static constexpr auto Direct3D = 1;
		};
		struct LineSmoothBlur : __reg<__self, 0x59a, 1> {
			using __reg::__reg;
			static constexpr auto Low = 0;
			static constexpr auto Medium = 1;
			static constexpr auto High = 2;
		};
		struct LineStippleEnable : __reg<__self, 0x59b, 1> {
			using __reg::__reg;
		};
		struct LineStipplePattern : __reg<__self, 0x5a0, 1> {
			using __reg::__reg;
		};
		struct ProvokingVertexLast : __reg<__self, 0x5a1, 1> {
			using __reg::__reg;
		};
		struct PolygonStippleEnable : __reg<__self, 0x5a3, 1> {
			using __reg::__reg;
		};
		struct Unknown5aa : __reg<__self, 0x5aa, 1> {
			using __reg::__reg;
		};
		struct Unknown5ad : __reg<__self, 0x5ad, 1> {
			using __reg::__reg;
		};
		struct PolygonStipplePattern : __reg<__self, 0x5c0, 32> {
			using __reg::__reg;
		};
		struct Unknown5e5 : __reg<__self, 0x5e5, 1> {
			using __reg::__reg;
		};
		struct IndexArrayStartIova : __reg<__self, 0x5f2, 2> {
			using __reg::__reg;
		};
		struct IndexArrayLimitIova : __reg<__self, 0x5f4, 2> {
			using __reg::__reg;
		};
		struct IndexArrayFormat : __reg<__self, 0x5f6, 1> {
			using __reg::__reg;
			static constexpr auto Uint8 = 0;
			static constexpr auto Uint16 = 1;
			static constexpr auto Uint32 = 2;
		};
		struct DrawElementsFirst : __reg<__self, 0x5f7, 1> {
			using __reg::__reg;
		};
		struct DrawElementsCount : __reg<__self, 0x5f8, 1> {
			using __reg::__reg;
		};
		struct PolygonOffsetClamp : __reg<__self, 0x61f, 1> {
			using __reg::__reg;
		};
		struct IsVertexArrayPerInstance : __reg<__self, 0x620, 16> {
			using __reg::__reg;
		};
		struct VertexProgramPointSize : __reg<__self, 0x644, 1> {
			using __reg::__reg;
			struct Enable : __bit<0,1> {
				using __bit::__bit;
			};
			struct Unknown4 : __bit<4,8> {
				using __bit::__bit;
			};
		};
		struct CullFaceEnable : __reg<__self, 0x646, 1> {
			using __reg::__reg;
		};
		struct SetFrontFace : __reg<__self, 0x647, 1> {
			using __reg::__reg;
			static constexpr auto CW = 2304;
			static constexpr auto CCW = 2305;
		};
		struct SetCullFace : __reg<__self, 0x648, 1> {
			using __reg::__reg;
			static constexpr auto Front = 1028;
			static constexpr auto Back = 1029;
			static constexpr auto FrontAndBack = 1032;
		};
		struct SetPixelCenterInteger : __reg<__self, 0x649, 1> {
			using __reg::__reg;
		};
		struct ViewportTransformEnable : __reg<__self, 0x64b, 1> {
			using __reg::__reg;
		};
		struct ViewVolumeClipControl : __reg<__self, 0x64f, 1> {
			using __reg::__reg;
			struct ForceDepthRangeZeroToOne : __bit<0,1> {
				using __bit::__bit;
			};
			struct Unknown1 : __bit<1,2> {
				using __bit::__bit;
			};
			struct DepthClampNear : __bit<3,1> {
				using __bit::__bit;
			};
			struct DepthClampFar : __bit<4,1> {
				using __bit::__bit;
			};
			struct Unknown7 : __bit<7,1> {
				using __bit::__bit;
			};
			struct Unknown10 : __bit<10,1> {
				using __bit::__bit;
			};
			struct Unknown11 : __bit<11,1> {
				using __bit::__bit;
			};
			struct Unknown12 : __bit<12,2> {
				using __bit::__bit;
			};
		};
		struct ZcullUnknown65a : __reg<__self, 0x65a, 1> {
			using __reg::__reg;
		};
		struct ZcullTestMask : __reg<__self, 0x65b, 1> {
			using __reg::__reg;
			struct AllowZIncrease : __bit<0,1> {
				using __bit::__bit;
			};
			struct AllowZDecrease : __bit<4,1> {
				using __bit::__bit;
			};
		};
		struct DepthBoundsEnable : __reg<__self, 0x66f, 1> {
			using __reg::__reg;
		};
		struct Unknown670 : __reg<__self, 0x670, 1> {
			using __reg::__reg;
		};
		struct ColorLogicOpEnable : __reg<__self, 0x671, 1> {
			using __reg::__reg;
		};
		struct ColorLogicOpType : __reg<__self, 0x672, 1> {
			using __reg::__reg;
		};
		struct ClearBuffers : __reg<__self, 0x674, 1> {
			using __reg::__reg;
			struct Depth : __bit<0,1> {
				using __bit::__bit;
			};
			struct Stencil : __bit<1,1> {
				using __bit::__bit;
			};
			struct Red : __bit<2,1> {
				using __bit::__bit;
			};
			struct Green : __bit<3,1> {
				using __bit::__bit;
			};
			struct Blue : __bit<4,1> {
				using __bit::__bit;
			};
			struct Alpha : __bit<5,1> {
				using __bit::__bit;
			};
			struct TargetId : __bit<6,4> {
				using __bit::__bit;
			};
			struct LayerId : __bit<10,11> {
				using __bit::__bit;
			};
		};
		struct ColorWriteMask : __reg<__self, 0x680, 8> {
			using __reg::__reg;
			struct Red : __bit<0,1> {
				using __bit::__bit;
			};
			struct Green : __bit<4,1> {
				using __bit::__bit;
			};
			struct Blue : __bit<8,1> {
				using __bit::__bit;
			};
			struct Alpha : __bit<12,1> {
				using __bit::__bit;
			};
		};
		struct PipeNop : __reg<__self, 0x68b, 1> {
			using __reg::__reg;
		};
		struct SetReportSemaphoreOffset : __reg<__self, 0x6c0, 2> {
			using __reg::__reg;
		};
		struct SetReportSemaphorePayload : __reg<__self, 0x6c2, 1> {
			using __reg::__reg;
		};
		struct SetReportSemaphore : __reg<__self, 0x6c3, 1> {
			using __reg::__reg;
			struct Operation : __bit<0,2> {
				using __bit::__bit;
				static constexpr unsigned Release = 0x00000000;
				static constexpr unsigned Acquire = 0x00000001;
				static constexpr unsigned Counter = 0x00000002;
				static constexpr unsigned Trap = 0x00000003;
			};
			struct FlushDisable : __bit<2,1> {
				using __bit::__bit;
			};
			struct ReductionEnable : __bit<3,1> {
				using __bit::__bit;
			};
			struct FenceEnable : __bit<4,1> {
				using __bit::__bit;
			};
			struct ReductionOp : __bit<9,3> {
				using __bit::__bit;
				static constexpr unsigned Add = 0x00000000;
				static constexpr unsigned Min = 0x00000200;
				static constexpr unsigned Max = 0x00000400;
				static constexpr unsigned Inc = 0x00000600;
				static constexpr unsigned Dec = 0x00000800;
				static constexpr unsigned And = 0x00000A00;
				static constexpr unsigned Or = 0x00000C00;
				static constexpr unsigned Xor = 0x00000E00;
			};
			struct Unit : __bit<12,4> {
				using __bit::__bit;
				static constexpr unsigned VFetch = 0x00001000;
				static constexpr unsigned VP = 0x00002000;
				static constexpr unsigned Rast = 0x00004000;
				static constexpr unsigned StrmOut = 0x00005000;
				static constexpr unsigned GP = 0x00006000;
				static constexpr unsigned ZCull = 0x00007000;
				static constexpr unsigned TessCtrl = 0x00008000;
				static constexpr unsigned TessEval = 0x00009000;
				static constexpr unsigned Prop = 0x0000A000;
				static constexpr unsigned Crop = 0x0000F000;
			};
			struct SyncCondition : __bit<16,1> {
				using __bit::__bit;
				static constexpr unsigned NotEqual = 0x00000000;
				static constexpr unsigned GreaterThan = 0x00010000;
			};
			struct Format : __bit<17,2> {
				using __bit::__bit;
				static constexpr unsigned Unsigned32 = 0x00000000;
				static constexpr unsigned Signed32 = 0x00020000;
			};
			struct AwakenEnable : __bit<20,1> {
				using __bit::__bit;
			};
			struct Counter : __bit<23,5> {
				using __bit::__bit;
				static constexpr unsigned Payload = 0x00000000;
				static constexpr unsigned InputVertices = 0x00800000;
				static constexpr unsigned InputPrimitives = 0x01800000;
				static constexpr unsigned VertexShaderInvocations = 0x02800000;
				static constexpr unsigned GeometryShaderInvocations = 0x03800000;
				static constexpr unsigned GeometryShaderPrimitives = 0x04800000;
				static constexpr unsigned ZcullStats0 = 0x05000000;
				static constexpr unsigned TransformFeedbackPrimitivesWritten = 0x05800000;
				static constexpr unsigned ZcullStats1 = 0x06000000;
				static constexpr unsigned ZcullStats2 = 0x07000000;
				static constexpr unsigned ClipperInputPrimitives = 0x07800000;
				static constexpr unsigned ZcullStats3 = 0x08000000;
				static constexpr unsigned ClipperOutputPrimitives = 0x08800000;
				static constexpr unsigned PrimitivesGenerated = 0x09000000;
				static constexpr unsigned FragmentShaderInvocations = 0x09800000;
				static constexpr unsigned SamplesPassed = 0x0A800000;
				static constexpr unsigned TransformFeedbackOffset = 0x0D000000;
				static constexpr unsigned TessControlShaderInvocations = 0x0D800000;
				static constexpr unsigned TessEvaluationShaderInvocations = 0x0E800000;
				static constexpr unsigned TessEvaluationShaderPrimitives = 0x0F800000;
			};
			struct StructureSize : __bit<28,1> {
				using __bit::__bit;
				static constexpr unsigned FourWords = 0x00000000;
				static constexpr unsigned OneWord = 0x10000000;
			};
		};
		struct VertexArray : __array<__self, 0x700, 16, 2> {
			using __self = VertexArray;
			struct Config : __reg<__self, 0x000, 1> {
				using __reg::__reg;
				struct Stride : __bit<0,12> {
					using __bit::__bit;
				};
				struct Enable : __bit<12,1> {
					using __bit::__bit;
				};
			};
			struct Start : __reg<__self, 0x001, 2> {
				using __reg::__reg;
			};
			struct Divisor : __reg<__self, 0x003, 1> {
				using __reg::__reg;
			};
		};
		struct IndependentBlend : __array<__self, 0x780, 8, 3> {
			using __self = IndependentBlend;
			struct EquationRgb : __reg<__self, 0x001, 1> {
				using __reg::__reg;
			};
			struct FuncRgbSrc : __reg<__self, 0x002, 1> {
				using __reg::__reg;
			};
			struct FuncRgbDst : __reg<__self, 0x003, 1> {
				using __reg::__reg;
			};
			struct EquationAlpha : __reg<__self, 0x004, 1> {
				using __reg::__reg;
			};
			struct FuncAlphaSrc : __reg<__self, 0x005, 1> {
				using __reg::__reg;
			};
			struct FuncAlphaDst : __reg<__self, 0x006, 1> {
				using __reg::__reg;
			};
		};
		struct VertexArrayLimit : __reg<__self, 0x7c0, 32> {
			using __reg::__reg;
		};
		struct SetProgram : __array<__self, 0x800, 6, 4> {
			using __self = SetProgram;
			struct Config : __reg<__self, 0x000, 1> {
				using __reg::__reg;
				struct Enable : __bit<0,1> {
					using __bit::__bit;
				};
				struct StageId : __bit<4,4> {
					using __bit::__bit;
					static constexpr unsigned VertexA = 0x00000000;
					static constexpr unsigned VertexB = 0x00000010;
					static constexpr unsigned TessCtrl = 0x00000020;
					static constexpr unsigned TessEval = 0x00000030;
					static constexpr unsigned Geometry = 0x00000040;
					static constexpr unsigned Fragment = 0x00000050;
				};
			};
			struct Offset : __reg<__self, 0x001, 1> {
				using __reg::__reg;
			};
			struct NumRegisters : __reg<__self, 0x003, 1> {
				using __reg::__reg;
			};
		};
		struct FirmwareCall : __reg<__self, 0x8c0, 32> {
			using __reg::__reg;
		};
		struct ConstbufSelectorSize : __reg<__self, 0x8e0, 1> {
			using __reg::__reg;
			struct Size : __bit<0,17> {
				using __bit::__bit;
			};
		};
		struct ConstbufSelectorAddr : __reg<__self, 0x8e1, 2> {
			using __reg::__reg;
		};
		struct LoadConstbufOffset : __reg<__self, 0x8e3, 1> {
			using __reg::__reg;
			struct Offset : __bit<0,16> {
				using __bit::__bit;
			};
		};
		struct LoadConstbufData : __reg<__self, 0x8e4, 16> {
			using __reg::__reg;
		};
		struct Bind : __array<__self, 0x900, 5, 3> {
			using __self = Bind;
			struct Constbuf : __reg<__self, 0x004, 1> {
				using __reg::__reg;
				struct Valid : __bit<0,1> {
					using __bit::__bit;
				};
				struct Index : __bit<4,5> {
					using __bit::__bit;
				};
			};
		};
		struct SetBindlessTexture : __reg<__self, 0x982, 1> {
			using __reg::__reg;
			struct ConstantBufferSlotSelect : __bit<0,3> {
				using __bit::__bit;
			};
		};
		struct MmeFirmwareArgs : __reg<__self, 0xd00, 8> {
			using __reg::__reg;
		};
		struct MmeScratch : __reg<__self, 0xd08, 16> {
			using __reg::__reg;
		};
		struct MmeDriverConstbufIova : __reg<__self, 0xd18, 1> {
			using __reg::__reg;
		};
		struct MmeDriverConstbufSize : __reg<__self, 0xd19, 1> {
			using __reg::__reg;
		};
		struct MmeProgramIds : __reg<__self, 0xd1a, 6> {
			using __reg::__reg;
		};
		struct MmeProgramOffsets : __reg<__self, 0xd20, 6> {
			using __reg::__reg;
		};
		struct MmeDepthRenderTargetIova : __reg<__self, 0xd26, 1> {
			using __reg::__reg;
		};
		struct MmeStencilCullCriteria : __reg<__self, 0xd27, 1> {
			using __reg::__reg;
		};
		struct MmeConservativeRasterDilateEnabled : __reg<__self, 0xd28, 1> {
			using __reg::__reg;
		};
	};

}
