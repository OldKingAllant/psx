#pragma once

#include "GpuBuffer.hpp"

#include <expected>
#include <optional>
#include <bit>

namespace psx::video {
	class UniformRingBuffer {
	public :
		static std::expected<UniformRingBuffer, GLError> CreateRingBuffer(size_t _slot_size, size_t _num_slots);

		UniformRingBuffer();

		UniformRingBuffer(UniformRingBuffer const& _other) = delete;
		UniformRingBuffer& operator=(UniformRingBuffer const& _other) = delete;

		UniformRingBuffer(UniformRingBuffer&& _other) noexcept;
		UniformRingBuffer& operator=(UniformRingBuffer&& _other) noexcept;

		~UniformRingBuffer();

		template <typename SlotTy>
		std::optional<GLError> WriteSlot(size_t _slot_id, SlotTy const& _data) {
			if (sizeof(_data) != m_slot_size) {
				return GLError::INVALID_TYPE_SIZE;
			}
			return WriteSlotImpl(_slot_id, std::bit_cast<const void*>(&_data));
		}

		std::optional<GLError> WriteSlotImpl(size_t _slot_id, const void* _data);

		std::optional<GLError> FlushSlotWrites(size_t _slot_id);

		void BindBuffer() const;
		void UnbindBuffer() const;

		void BindTo(uint32_t _bind_point);

		void SetLabel(std::string_view _label);

	private :
		GpuBuffer                m_buf;
		size_t                   m_slot_size;
		size_t                   m_num_slots;
		std::optional<uint32_t>  m_bound_to;
	};
}