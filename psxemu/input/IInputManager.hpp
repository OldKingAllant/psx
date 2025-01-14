#pragma once

#include <common/Defs.hpp>
#include <psxemu/include/psxemu/AbstractController.hpp>

#include <any>
#include <unordered_map>
#include <string>

namespace psx::input {
	class IInputManager {
	public :
		IInputManager() {}

		virtual void Deliver(std::any status) = 0;
		virtual void AttachController(AbstractController* controller) = 0;
		virtual void SetKeyMap(std::unordered_map<std::string, std::string> const& new_keys) = 0;

		virtual ~IInputManager() {}
	};
}