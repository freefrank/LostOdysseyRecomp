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

	struct Engine2D {
		using __self = Engine2D;
		static constexpr unsigned __classid = 0x902d;
		struct SetDstFormat : __reg<__self, 0x080, 1> {
			using __reg::__reg;
		};
		struct SetDstMemoryLayout : __reg<__self, 0x081, 1> {
			using __reg::__reg;
			static constexpr auto BlockLinear = 0;
			static constexpr auto Pitch = 1;
		};
		struct SetDstBlockSize : __reg<__self, 0x082, 1> {
			using __reg::__reg;
			struct Height : __bit<4,3> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned TwoGobs = 0x00000010;
				static constexpr unsigned FourGobs = 0x00000020;
				static constexpr unsigned EightGobs = 0x00000030;
				static constexpr unsigned SixteenGobs = 0x00000040;
				static constexpr unsigned ThirtyTwoGobs = 0x00000050;
			};
			struct Depth : __bit<8,3> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned TwoGobs = 0x00000100;
				static constexpr unsigned FourGobs = 0x00000200;
				static constexpr unsigned EightGobs = 0x00000300;
				static constexpr unsigned SixteenGobs = 0x00000400;
				static constexpr unsigned ThirtyTwoGobs = 0x00000500;
			};
		};
		struct SetDstDepth : __reg<__self, 0x083, 1> {
			using __reg::__reg;
		};
		struct SetDstLayer : __reg<__self, 0x084, 1> {
			using __reg::__reg;
		};
		struct SetDstPitch : __reg<__self, 0x085, 1> {
			using __reg::__reg;
		};
		struct SetDstWidth : __reg<__self, 0x086, 1> {
			using __reg::__reg;
		};
		struct SetDstHeight : __reg<__self, 0x087, 1> {
			using __reg::__reg;
		};
		struct SetDstOffset : __reg<__self, 0x088, 2> {
			using __reg::__reg;
		};
		struct SetSrcFormat : __reg<__self, 0x08c, 1> {
			using __reg::__reg;
		};
		struct SetSrcMemoryLayout : __reg<__self, 0x08d, 1> {
			using __reg::__reg;
			static constexpr auto BlockLinear = 0;
			static constexpr auto Pitch = 1;
		};
		struct SetSrcBlockSize : __reg<__self, 0x08e, 1> {
			using __reg::__reg;
			struct Height : __bit<4,3> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned TwoGobs = 0x00000010;
				static constexpr unsigned FourGobs = 0x00000020;
				static constexpr unsigned EightGobs = 0x00000030;
				static constexpr unsigned SixteenGobs = 0x00000040;
				static constexpr unsigned ThirtyTwoGobs = 0x00000050;
			};
			struct Depth : __bit<8,3> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned TwoGobs = 0x00000100;
				static constexpr unsigned FourGobs = 0x00000200;
				static constexpr unsigned EightGobs = 0x00000300;
				static constexpr unsigned SixteenGobs = 0x00000400;
				static constexpr unsigned ThirtyTwoGobs = 0x00000500;
			};
		};
		struct SetSrcDepth : __reg<__self, 0x08f, 1> {
			using __reg::__reg;
		};
		struct SetSrcPitch : __reg<__self, 0x091, 1> {
			using __reg::__reg;
		};
		struct SetSrcWidth : __reg<__self, 0x092, 1> {
			using __reg::__reg;
		};
		struct SetSrcHeight : __reg<__self, 0x093, 1> {
			using __reg::__reg;
		};
		struct SetSrcOffset : __reg<__self, 0x094, 2> {
			using __reg::__reg;
		};
		struct SetClipEnable : __reg<__self, 0x0a4, 1> {
			using __reg::__reg;
		};
		struct SetBeta1 : __reg<__self, 0x0a9, 1> {
			using __reg::__reg;
			struct Value : __bit<23,8> {
				using __bit::__bit;
			};
		};
		struct SetBeta4 : __reg<__self, 0x0aa, 1> {
			using __reg::__reg;
			struct Red : __bit<0,8> {
				using __bit::__bit;
			};
			struct Green : __bit<8,8> {
				using __bit::__bit;
			};
			struct Blue : __bit<16,8> {
				using __bit::__bit;
			};
			struct Alpha : __bit<24,8> {
				using __bit::__bit;
			};
		};
		struct SetOperation : __reg<__self, 0x0ab, 1> {
			using __reg::__reg;
			static constexpr auto SrcCopyAnd = 0;
			static constexpr auto RopAnd = 1;
			static constexpr auto Blend = 2;
			static constexpr auto SrcCopy = 3;
			static constexpr auto Rop = 4;
			static constexpr auto SrcCopyPremult = 5;
			static constexpr auto BlendPremult = 6;
		};
		struct SetCompressionEnable : __reg<__self, 0x0b5, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryCorralSize : __reg<__self, 0x221, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemorySampleMode : __reg<__self, 0x223, 1> {
			using __reg::__reg;
			struct Origin : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned Center = 0x00000000;
				static constexpr unsigned Corner = 0x00000001;
			};
			struct Filter : __bit<4,1> {
				using __bit::__bit;
				static constexpr unsigned Point = 0x00000000;
				static constexpr unsigned Bilinear = 0x00000010;
			};
		};
		struct SetPixelsFromMemoryDstX0 : __reg<__self, 0x22c, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryDstY0 : __reg<__self, 0x22d, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryDstWidth : __reg<__self, 0x22e, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryDstHeight : __reg<__self, 0x22f, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryDuDxFrac : __reg<__self, 0x230, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryDuDxInt : __reg<__self, 0x231, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryDvDyFrac : __reg<__self, 0x232, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemoryDvDyInt : __reg<__self, 0x233, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemorySrcX0Frac : __reg<__self, 0x234, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemorySrcX0Int : __reg<__self, 0x235, 1> {
			using __reg::__reg;
		};
		struct SetPixelsFromMemorySrcY0Frac : __reg<__self, 0x236, 1> {
			using __reg::__reg;
		};
		struct PixelsFromMemorySrcY0Int : __reg<__self, 0x237, 1> {
			using __reg::__reg;
		};
	};

}
