#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <fmt/format.h>

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

		FORCE_INLINE constexpr u32 physical_address() const {
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

		FORCE_INLINE constexpr bool operator==(VirtualAddress const& other) const {
			return addr == other.addr;
		}

		/// <summary>
		/// Check if virtual address is within bounds (exclusive)
		/// </summary>
		/// <param name="lower"></param>
		/// <param name="upper"></param>
		/// <returns></returns>
		FORCE_INLINE constexpr bool between(VirtualAddress lower, VirtualAddress upper) const {
			return (addr >= lower.addr) &&
				(addr < upper.addr);
		}

		/// <summary>
		/// Check if the physical address is within bounds (exclusive)
		/// </summary>
		/// <param name="lower"></param>
		/// <param name="upper"></param>
		/// <returns></returns>
		FORCE_INLINE constexpr bool between_physical(u32 lower, u32 upper) const {
			return (physical_address() >= lower) &&
				(physical_address() < upper);
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

		FORCE_INLINE u32 operator&(VirtualAddress const& other) {
			return this->addr & other.addr;
		}

		FORCE_INLINE u32 operator&(u32 other) {
			return this->addr & other;
		}

		FORCE_INLINE u32 operator|(VirtualAddress const& other) {
			return this->addr | other.addr;
		}

		FORCE_INLINE u32 operator|(u32 other) {
			return this->addr | other;
		}

		FORCE_INLINE VirtualAddress& operator&=(VirtualAddress const& other) {
			addr &= other.addr;
			return *this;
		}

		FORCE_INLINE VirtualAddress& operator&=(u32 other) {
			addr &= other;
			return *this;
		}

		FORCE_INLINE VirtualAddress& operator|=(VirtualAddress const& other) {
			addr |= other.addr;
			return *this;
		}

		FORCE_INLINE VirtualAddress& operator|=(u32 other) {
			addr |= other;
			return *this;
		}
	};
}

template <> class fmt::formatter<psx::VirtualAddress> {
public:
	constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
	template <typename Context>
	constexpr auto format(psx::VirtualAddress const& addr, Context& ctx) const {
		return fmt::format_to(ctx.out(), "{}", addr.addr);
	}
};