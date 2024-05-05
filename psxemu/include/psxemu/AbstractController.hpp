#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <string_view>

namespace psx {
	enum class ControllerType {
		NONE,
		STANDARD
	};

	enum class ButtonType {
		NORMAL,
		ANALOG
	};

	struct ButtonStatus {
		ButtonType ty;
		std::string_view name;
		union {
			struct {
				bool pressed;
			} normal;
		};
	};

	class AbstractController {
	public :
		AbstractController() {}

		virtual u8 Send(u8 value) = 0;
		virtual bool Ack() = 0;
		virtual ControllerType GetType() const = 0;
		virtual void Reset() = 0;
		virtual void UpdateStatus(ButtonStatus status) = 0;

		virtual ~AbstractController() {}
	};
}