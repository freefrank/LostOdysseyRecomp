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

	struct EngineCopy {
		using __self = EngineCopy;
		static constexpr unsigned __classid = 0xb0b5;
		struct LaunchDma : __reg<__self, 0x0c0, 1> {
			using __reg::__reg;
			struct TransferType : __bit<0,2> {
				using __bit::__bit;
				static constexpr unsigned None = 0x00000000;
				static constexpr unsigned Pipelined = 0x00000001;
				static constexpr unsigned NonPipelined = 0x00000002;
			};
			struct FlushEnable : __bit<2,1> {
				using __bit::__bit;
			};
			struct SemaphoreType : __bit<3,2> {
				using __bit::__bit;
				static constexpr unsigned None = 0x00000000;
				static constexpr unsigned ReleaseOneWord = 0x00000008;
				static constexpr unsigned ReleaseFourWords = 0x00000010;
			};
			struct InterruptType : __bit<5,2> {
				using __bit::__bit;
				static constexpr unsigned None = 0x00000000;
				static constexpr unsigned Blocking = 0x00000020;
				static constexpr unsigned NonBlocking = 0x00000040;
			};
			struct SrcMemoryLayout : __bit<7,1> {
				using __bit::__bit;
				static constexpr unsigned BlockLinear = 0x00000000;
				static constexpr unsigned Pitch = 0x00000080;
			};
			struct DstMemoryLayout : __bit<8,1> {
				using __bit::__bit;
				static constexpr unsigned BlockLinear = 0x00000000;
				static constexpr unsigned Pitch = 0x00000100;
			};
			struct MultiLineEnable : __bit<9,1> {
				using __bit::__bit;
			};
			struct RemapEnable : __bit<10,1> {
				using __bit::__bit;
			};
			struct ForceRmwDisable : __bit<11,1> {
				using __bit::__bit;
			};
			struct SrcType : __bit<12,1> {
				using __bit::__bit;
				static constexpr unsigned Virtual = 0x00000000;
				static constexpr unsigned Physical = 0x00001000;
			};
			struct DstType : __bit<13,1> {
				using __bit::__bit;
				static constexpr unsigned Virtual = 0x00000000;
				static constexpr unsigned Physical = 0x00002000;
			};
			struct SemaphoreReduction : __bit<14,4> {
				using __bit::__bit;
				static constexpr unsigned Imin = 0x00000000;
				static constexpr unsigned Imax = 0x00004000;
				static constexpr unsigned Ixor = 0x00008000;
				static constexpr unsigned Iand = 0x0000C000;
				static constexpr unsigned Ior = 0x00010000;
				static constexpr unsigned Iadd = 0x00014000;
				static constexpr unsigned Inc = 0x00018000;
				static constexpr unsigned Dec = 0x0001C000;
				static constexpr unsigned Fadd = 0x00028000;
			};
			struct SemaphoreReductionSign : __bit<18,1> {
				using __bit::__bit;
				static constexpr unsigned Signed = 0x00000000;
				static constexpr unsigned Unsigned = 0x00040000;
			};
			struct SemaphoreReductionEnable : __bit<19,1> {
				using __bit::__bit;
			};
			struct BypassL2 : __bit<20,1> {
				using __bit::__bit;
				static constexpr unsigned UsePteSetting = 0x00000000;
				static constexpr unsigned ForceVolatile = 0x00100000;
			};
		};
		struct OffsetIn : __reg<__self, 0x100, 2> {
			using __reg::__reg;
		};
		struct OffsetOut : __reg<__self, 0x102, 2> {
			using __reg::__reg;
		};
		struct PitchIn : __reg<__self, 0x104, 1> {
			using __reg::__reg;
		};
		struct PitchOut : __reg<__self, 0x105, 1> {
			using __reg::__reg;
		};
		struct LineLengthIn : __reg<__self, 0x106, 1> {
			using __reg::__reg;
		};
		struct LineCount : __reg<__self, 0x107, 1> {
			using __reg::__reg;
		};
		struct SetRemapConst : __reg<__self, 0x1c0, 2> {
			using __reg::__reg;
		};
		struct SetRemapComponents : __reg<__self, 0x1c2, 1> {
			using __reg::__reg;
			struct DstX : __bit<0,3> {
				using __bit::__bit;
			};
			struct DstY : __bit<4,3> {
				using __bit::__bit;
			};
			struct DstZ : __bit<8,3> {
				using __bit::__bit;
			};
			struct DstW : __bit<12,3> {
				using __bit::__bit;
			};
			struct ComponentSize : __bit<16,2> {
				using __bit::__bit;
				static constexpr unsigned One = 0x00000000;
				static constexpr unsigned Two = 0x00010000;
				static constexpr unsigned Three = 0x00020000;
				static constexpr unsigned Four = 0x00030000;
			};
			struct NumSrcComponents : __bit<20,2> {
				using __bit::__bit;
				static constexpr unsigned One = 0x00000000;
				static constexpr unsigned Two = 0x00100000;
				static constexpr unsigned Three = 0x00200000;
				static constexpr unsigned Four = 0x00300000;
			};
			struct NumDstComponents : __bit<24,2> {
				using __bit::__bit;
				static constexpr unsigned One = 0x00000000;
				static constexpr unsigned Two = 0x01000000;
				static constexpr unsigned Three = 0x02000000;
				static constexpr unsigned Four = 0x03000000;
			};
		};
		struct SetDstBlockSize : __reg<__self, 0x1c3, 1> {
			using __reg::__reg;
			struct Width : __bit<0,4> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned QuarterGob = 0x0000000E;
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
			struct GobHeight : __bit<12,4> {
				using __bit::__bit;
				static constexpr unsigned Tesla4 = 0x00000000;
				static constexpr unsigned Fermi8 = 0x00001000;
			};
		};
		struct SetDstWidth : __reg<__self, 0x1c4, 1> {
			using __reg::__reg;
		};
		struct SetDstHeight : __reg<__self, 0x1c5, 1> {
			using __reg::__reg;
		};
		struct SetDstDepth : __reg<__self, 0x1c6, 1> {
			using __reg::__reg;
		};
		struct SetDstLayer : __reg<__self, 0x1c7, 1> {
			using __reg::__reg;
		};
		struct SetDstOrigin : __reg<__self, 0x1c8, 1> {
			using __reg::__reg;
			struct X : __bit<0,16> {
				using __bit::__bit;
			};
			struct Y : __bit<16,16> {
				using __bit::__bit;
			};
		};
		struct SetSrcBlockSize : __reg<__self, 0x1ca, 1> {
			using __reg::__reg;
			struct Width : __bit<0,4> {
				using __bit::__bit;
				static constexpr unsigned OneGob = 0x00000000;
				static constexpr unsigned QuarterGob = 0x0000000E;
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
			struct GobHeight : __bit<12,4> {
				using __bit::__bit;
				static constexpr unsigned Tesla4 = 0x00000000;
				static constexpr unsigned Fermi8 = 0x00001000;
			};
		};
		struct SetSrcWidth : __reg<__self, 0x1cb, 1> {
			using __reg::__reg;
		};
		struct SetSrcHeight : __reg<__self, 0x1cc, 1> {
			using __reg::__reg;
		};
		struct SetSrcDepth : __reg<__self, 0x1cd, 1> {
			using __reg::__reg;
		};
		struct SetSrcLayer : __reg<__self, 0x1ce, 1> {
			using __reg::__reg;
		};
		struct SetSrcOrigin : __reg<__self, 0x1cf, 1> {
			using __reg::__reg;
			struct X : __bit<0,16> {
				using __bit::__bit;
			};
			struct Y : __bit<16,16> {
				using __bit::__bit;
			};
		};
	};

}
