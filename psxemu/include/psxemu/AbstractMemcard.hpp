#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <string>
#include <vector>
#include <optional>

namespace psx {
	enum class MemcardType {
		NONE,
		OFFICIAL
	};

	class AbstractMemcard {
	public:
		AbstractMemcard() {}

		virtual u8 Send(u8 value) = 0;
		virtual bool Ack() = 0;
		virtual void Reset() = 0;
		virtual bool LoadFile(std::string const& path) = 0;

		virtual u32 GetUpdateSequenceNumber() const = 0;

		virtual std::optional<std::vector<u8>> ReadFrame(u32 frame_num) const = 0;
		virtual bool WriteFrame(u32 frame_num, std::vector<u8> const& data) = 0;

		virtual bool IsConnected() const = 0;
		virtual std::optional<std::string> GetMcPath() const = 0;

		virtual std::string GetType() const {
			return "NULL";
		}

		virtual ~AbstractMemcard() {}
	};
}