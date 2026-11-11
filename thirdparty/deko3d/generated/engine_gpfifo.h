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

	struct EngineGpfifo {
		using __self = EngineGpfifo;
		static constexpr unsigned __classid = 0xb06f;
		struct SemaphoreOffset : __reg<__self, 0x004, 2> {
			using __reg::__reg;
		};
		struct SemaphorePayload : __reg<__self, 0x006, 1> {
			using __reg::__reg;
		};
		struct Semaphore : __reg<__self, 0x007, 1> {
			using __reg::__reg;
			struct Operation : __bit<0,5> {
				using __bit::__bit;
				static constexpr unsigned Acquire = 0x00000001;
				static constexpr unsigned Release = 0x00000002;
				static constexpr unsigned AcqGeq = 0x00000004;
				static constexpr unsigned AcqAnd = 0x00000008;
				static constexpr unsigned Reduction = 0x00000010;
			};
			struct AcquireSwitch : __bit<12,1> {
				using __bit::__bit;
			};
			struct ReleaseWfiDisable : __bit<20,1> {
				using __bit::__bit;
			};
			struct ReleaseSize : __bit<24,1> {
				using __bit::__bit;
				static constexpr unsigned _16 = 0x00000000;
				static constexpr unsigned _4 = 0x01000000;
			};
			struct Reduction : __bit<27,4> {
				using __bit::__bit;
				static constexpr unsigned Min = 0x00000000;
				static constexpr unsigned Max = 0x08000000;
				static constexpr unsigned Xor = 0x10000000;
				static constexpr unsigned And = 0x18000000;
				static constexpr unsigned Or = 0x20000000;
				static constexpr unsigned Add = 0x28000000;
				static constexpr unsigned Inc = 0x30000000;
				static constexpr unsigned Dec = 0x38000000;
			};
			struct Format : __bit<31,1> {
				using __bit::__bit;
				static constexpr unsigned Signed = 0x00000000;
				static constexpr unsigned Unsigned = 0x80000000;
			};
		};
		struct MemOpB : __reg<__self, 0x00b, 1> {
			using __reg::__reg;
			struct OperandHigh : __bit<0,8> {
				using __bit::__bit;
			};
			struct Operation : __bit<27,5> {
				using __bit::__bit;
				static constexpr unsigned SysMemBarFlush = 0x28000000;
				static constexpr unsigned SoftFlush = 0x30000000;
				static constexpr unsigned MmuTlbInvalidate = 0x48000000;
				static constexpr unsigned L2PeerMemInvalidate = 0x68000000;
				static constexpr unsigned L2SysmemInvalidate = 0x70000000;
				static constexpr unsigned L2CleanComptags = 0x78000000;
				static constexpr unsigned L2FlushDirty = 0x80000000;
			};
			struct MmuTlbInvalidatePdb : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned One = 0x00000000;
				static constexpr unsigned All = 0x00000001;
			};
			struct MmuTlbInvalidateGpc : __bit<1,1> {
				using __bit::__bit;
			};
		};
		struct SetReference : __reg<__self, 0x014, 1> {
			using __reg::__reg;
		};
		struct SyncpointPayload : __reg<__self, 0x01c, 1> {
			using __reg::__reg;
		};
		struct Syncpoint : __reg<__self, 0x01d, 1> {
			using __reg::__reg;
			struct Operation : __bit<0,1> {
				using __bit::__bit;
				static constexpr unsigned Wait = 0x00000000;
				static constexpr unsigned Incr = 0x00000001;
			};
			struct WaitSwitch : __bit<4,1> {
				using __bit::__bit;
			};
			struct SyncptIndex : __bit<8,8> {
				using __bit::__bit;
			};
		};
	};

}
