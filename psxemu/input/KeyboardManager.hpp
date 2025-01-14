#pragma once

#include <common/Macros.hpp>

#include "IInputManager.hpp"

#include <unordered_map>
#include <string>

namespace psx::input {
	struct KeyboardButtonStatus {
		std::string_view key;
		bool pressed;
	};

	class KeyboardManager : public IInputManager {
	public :
		KeyboardManager();

		void Deliver(std::any status) override;
		void AttachController(AbstractController* controller) override;

		void SetKeyMap(std::unordered_map<std::string, std::string> const& new_keys) override;

		~KeyboardManager() override;

	private :
		void KeyMapDefault();

	private :
		AbstractController* m_controller;
		std::unordered_map<std::string, std::string> m_key_map;
	};
}