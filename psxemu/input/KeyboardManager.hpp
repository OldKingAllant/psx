#pragma once

#include <common/Macros.hpp>

#include "IInputManager.hpp"

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

		~KeyboardManager() override;

	private :
		AbstractController* m_controller;
	};
}