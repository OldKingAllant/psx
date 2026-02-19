#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx {
	enum class MemorySegment {
		KUSEG = 0,
		KSEG0 = 4,
		KSEG1 = 5,
		KSEG2 = 6
	};

	enum class MemoryRegion {
		RAM = 0x0,
		OTHERS = 0x1F
	};

	struct VirtualAddress {
		u32 addr;

		constexpr VirtualAddress(VirtualAddress const& other) = default;
		constexpr VirtualAddress(VirtualAddress&& other) = default;

		constexpr VirtualAddress(u32 addr) {
			this->addr = addr;
		}

		constexpr operator u32() const {
			return addr;
		}

		constexpr VirtualAddress& operator=(VirtualAddress const& other) {
			this->addr = other.addr;
			return *this;
		}

		constexpr VirtualAddress& operator=(u32 other) {
			this->addr = addr;
			return *this;
		}

		FORCE_INLINE constexpr MemorySegment memory_segment() const {
			return MemorySegment(addr >> 29);
		}

		FORCE_INLINE constexpr MemoryRegion memory_region() const {
			return MemoryRegion((addr >> 24) & 0x1F);
		}

		FORCE_INLINE constexpr u32 phisycal_address() const {
			return addr & ((1 << 29) - 1);
		}

		FORCE_INLINE constexpr bool operator>(VirtualAddress const& other) const {
			return addr > other.addr;
		}

		FORCE_INLINE constexpr bool operator<(VirtualAddress const& other) const {
			return addr < other.addr;
		}

		FORCE_INLINE constexpr bool operator>=(VirtualAddress const& other) const {
			return addr >= other.addr;
		}

		FORCE_INLINE constexpr bool operator<=(VirtualAddress const& other) const {
			return addr <= other.addr;
		}

		/// <summary>
		/// Check if virtual address is within bounds
		/// </summary>
		/// <param name="lower"></param>
		/// <param name="upper"></param>
		/// <returns></returns>
		FORCE_INLINE constexpr bool between(VirtualAddress lower, VirtualAddress upper) const {
			return (addr >= lower.addr) &&
				(addr <= upper.addr);
		}

		/// <summary>
		/// Check if the physical address is within bounds
		/// </summary>
		/// <param name="lower"></param>
		/// <param name="upper"></param>
		/// <returns></returns>
		FORCE_INLINE constexpr bool between(u32 lower, u32 upper) const {
			return (phisycal_address() >= lower) &&
				(phisycal_address() <= upper);
		}

		static constexpr VirtualAddress from_segment(MemorySegment segment, u32 offset) {
			u32 addr = (u32(segment) << 29) + offset;
			return VirtualAddress(addr);
		}

		FORCE_INLINE bool has_icache() const {
			switch (memory_segment())
			{
			case MemorySegment::KUSEG:
			case MemorySegment::KSEG0:
				return true;
			default:
				return false;
				break;
			}
		}

		FORCE_INLINE bool has_write_buffer() const {
			switch (memory_segment())
			{
			case MemorySegment::KUSEG:
			case MemorySegment::KSEG0:
				return true;
			default:
				return false;
				break;
			}
		}

		FORCE_INLINE VirtualAddress operator+(VirtualAddress const& other) {
			u32 new_addr = addr + other.addr;
			return VirtualAddress(new_addr);
		}

		FORCE_INLINE VirtualAddress operator-(VirtualAddress const& other) {
			u32 new_addr = addr - other.addr;
			return VirtualAddress(new_addr);
		}

		FORCE_INLINE VirtualAddress& operator+=(VirtualAddress const& other) {
			addr += other.addr;
			return *this;
		}

		FORCE_INLINE VirtualAddress& operator-=(VirtualAddress const& other) {
			addr -= other.addr;
			return *this;
		}
	};
}