#pragma once

#include <common/Defs.hpp>
#include <psxemu/include/psxemu/AbstractController.hpp>

#include <any>

namespace psx::input {
	class IInputManager {
	public :
		IInputManager() {}

		virtual void Deliver(std::any status) = 0;
		virtual void AttachController(AbstractController* controller) = 0;

		virtual ~IInputManager() {}
	};
}