#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

namespace psx {
	enum class ControllerType {
		NONE,
		STANDARD
	};

	class AbstractController {
	public :
		AbstractController() {}

		virtual u8 Send(u8 value) = 0;
		virtual bool Ack() = 0;
		virtual ControllerType GetType() const = 0;
		virtual void Reset() = 0;

		virtual ~AbstractController() {}
	};
}