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

	struct EngineInline {
		using __self = EngineInline;
		static constexpr unsigned __classid = 0xa140;
		struct LineLengthIn : __reg<__self, 0x060, 1> {
			using __reg::__reg;
		};
		struct LineCount : __reg<__self, 0x061, 1> {
			using __reg::__reg;
		};
		struct OffsetOut : __reg<__self, 0x062, 2> {
			using __reg::__reg;
		};
		struct PitchOut : __reg<__self, 0x064, 1> {
			using __reg::__reg;
		};
		struct SetDstBlockSize : __reg<__self, 0x065, 1> {
			using __reg::__reg;
			struct Width : __bit<0,4> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
			};
			struct Height : __bit<4,4> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned TwoGobs = 0x00000010;
				static constexpr unsigned FourGobs = 0x00000020;
				static constexpr unsigned EightGobs = 0x00000030;
				static constexpr unsigned SixteenGobs = 0x00000040;
				static constexpr unsigned ThirtyTwoGobs = 0x00000050;
			};
			struct Depth : __bit<8,4> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned TwoGobs = 0x00000100;
				static constexpr unsigned FourGobs = 0x00000200;
				static constexpr unsigned EightGobs = 0x00000300;
				static constexpr unsigned SixteenGobs = 0x00000400;
				static constexpr unsigned ThirtyTwoGobs = 0x00000500;
			};
		};
		struct SetDstWidth : __reg<__self, 0x066, 1> {
			using __reg::__reg;
		};
		struct SetDstHeight : __reg<__self, 0x067, 1> {
			using __reg::__reg;
		};
		struct SetDstDepth : __reg<__self, 0x068, 1> {
			using __reg::__reg;
		};
		struct SetDstLayer : __reg<__self, 0x069, 1> {
			using __reg::__reg;
		};
		struct SetOriginBytesX : __reg<__self, 0x06a, 1> {
			using __reg::__reg;
		};
		struct SetOriginSamplesY : __reg<__self, 0x06b, 1> {
			using __reg::__reg;
		};
		struct LaunchDma : __reg<__self, 0x06c, 1> {
			using __reg::__reg;
			struct DstMemoryLayout : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned BlockLinear = 0x00000000;
				static constexpr unsigned Pitch = 0x00000001;
			};
			struct ReductionEnable : __bit<1,1> {
				using __bit::__bit;
			};
			struct ReductionFormat : __bit<2,2> {
				using __bit::__bit;
				static constexpr unsigned Unsigned32 = 0x00000000;
				static constexpr unsigned Signed32 = 0x00000004;
			};
			struct CompletionType : __bit<4,2> {
				using __bit::__bit;
				static constexpr unsigned FlushDisable = 0x00000000;
				static constexpr unsigned FlushOnly = 0x00000010;
				static constexpr unsigned ReleaseSemaphore = 0x00000020;
			};
			struct SysmembarDisable : __bit<6,1> {
				using __bit::__bit;
			};
			struct InterruptType : __bit<8,2> {
				using __bit::__bit;
				static constexpr unsigned None = 0x00000000;
				static constexpr unsigned Interrupt = 0x00000100;
			};
			struct SemaphoreStructSize : __bit<12,1> {
				using __bit::__bit;
				static constexpr unsigned FourWords = 0x00000000;
				static constexpr unsigned OneWord = 0x00001000;
			};
			struct ReductionOp : __bit<13,3> {
				using __bit::__bit;
				static constexpr unsigned Add = 0x00000000;
				static constexpr unsigned Min = 0x00002000;
				static constexpr unsigned Max = 0x00004000;
				static constexpr unsigned Inc = 0x00006000;
				static constexpr unsigned Dec = 0x00008000;
				static constexpr unsigned And = 0x0000A000;
				static constexpr unsigned Or = 0x0000C000;
				static constexpr unsigned Xor = 0x0000E000;
			};
		};
		struct LoadInlineData : __reg<__self, 0x06d, 1> {
			using __reg::__reg;
		};
	};

}
