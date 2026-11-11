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

	struct EngineCompute {
		using __self = EngineCompute;
		static constexpr unsigned __classid = 0xb1c0;
		struct WaitForIdle : __reg<__self, 0x044, 1> {
			using __reg::__reg;
		};
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
		struct SetShaderSharedMemoryWindow : __reg<__self, 0x085, 1> {
			using __reg::__reg;
		};
		struct InvalidateShaderCaches : __reg<__self, 0x087, 1> {
			using __reg::__reg;
			struct Instruction : __bit<0,1> {
				using __bit::__bit;
			};
			struct Locks : __bit<1,1> {
				using __bit::__bit;
			};
			struct FlushData : __bit<2,1> {
				using __bit::__bit;
			};
			struct Data : __bit<4,1> {
				using __bit::__bit;
			};
			struct Constant : __bit<12,1> {
				using __bit::__bit;
			};
		};
		struct SendPcasA : __reg<__self, 0x0ad, 1> {
			using __reg::__reg;
			struct QmdAddressShifted8 : __bit<0,32> {
				using __bit::__bit;
			};
		};
		struct SendPcasB : __reg<__self, 0x0ae, 1> {
			using __reg::__reg;
			struct From : __bit<0,24> {
				using __bit::__bit;
			};
			struct Delta : __bit<24,8> {
				using __bit::__bit;
			};
		};
		struct SendSignalingPcasB : __reg<__self, 0x0af, 1> {
			using __reg::__reg;
			struct Invalidate : __bit<0,1> {
				using __bit::__bit;
			};
			struct Schedule : __bit<1,1> {
				using __bit::__bit;
			};
		};
		struct SetShaderLocalMemoryNonThrottledA : __reg<__self, 0x0b9, 1> {
			using __reg::__reg;
			struct SizeUpper : __bit<0,8> {
				using __bit::__bit;
			};
		};
		struct SetShaderLocalMemoryNonThrottledB : __reg<__self, 0x0ba, 1> {
			using __reg::__reg;
			struct SizeLower : __bit<0,32> {
				using __bit::__bit;
			};
		};
		struct SetShaderLocalMemoryNonThrottledC : __reg<__self, 0x0bb, 1> {
			using __reg::__reg;
			struct MaxSmCount : __bit<0,9> {
				using __bit::__bit;
			};
		};
		struct SetShaderLocalMemoryThrottledA : __reg<__self, 0x0bc, 1> {
			using __reg::__reg;
			struct SizeUpper : __bit<0,8> {
				using __bit::__bit;
			};
		};
		struct SetShaderLocalMemoryThrottledB : __reg<__self, 0x0bd, 1> {
			using __reg::__reg;
			struct SizeLower : __bit<0,32> {
				using __bit::__bit;
			};
		};
		struct SetShaderLocalMemoryThrottledC : __reg<__self, 0x0be, 1> {
			using __reg::__reg;
			struct MaxSmCount : __bit<0,9> {
				using __bit::__bit;
			};
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
		struct SetShaderLocalMemoryWindow : __reg<__self, 0x1df, 1> {
			using __reg::__reg;
		};
		struct SetShaderLocalMemory : __reg<__self, 0x1e4, 2> {
			using __reg::__reg;
		};
		struct SetShaderExceptions : __reg<__self, 0x54a, 1> {
			using __reg::__reg;
		};
		struct SetTexSamplerPool : __reg<__self, 0x557, 2> {
			using __reg::__reg;
		};
		struct SetTexSamplerPoolMaximumIndex : __reg<__self, 0x559, 1> {
			using __reg::__reg;
		};
		struct SetTexHeaderPool : __reg<__self, 0x55d, 2> {
			using __reg::__reg;
		};
		struct SetTexHeaderPoolMaximumIndex : __reg<__self, 0x55f, 1> {
			using __reg::__reg;
		};
		struct SetProgramRegion : __reg<__self, 0x582, 2> {
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
				static constexpr unsigned Trap = 0x00000003;
			};
			struct FlushDisable : __bit<2,1> {
				using __bit::__bit;
			};
			struct ReductionEnable : __bit<3,1> {
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
			struct Format : __bit<17,2> {
				using __bit::__bit;
				static constexpr unsigned Unsigned32 = 0x00000000;
				static constexpr unsigned Signed32 = 0x00020000;
			};
			struct AwakenEnable : __bit<20,1> {
				using __bit::__bit;
			};
			struct StructureSize : __bit<28,1> {
				using __bit::__bit;
				static constexpr unsigned FourWords = 0x00000000;
				static constexpr unsigned OneWord = 0x10000000;
			};
		};
		struct SetBindlessTexture : __reg<__self, 0x982, 1> {
			using __reg::__reg;
			struct ConstantBufferSlotSelect : __bit<0,3> {
				using __bit::__bit;
			};
		};
	};

}
