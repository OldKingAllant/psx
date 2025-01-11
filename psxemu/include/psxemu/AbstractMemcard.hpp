#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <string>

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

		virtual ~AbstractMemcard() {}
	};
}