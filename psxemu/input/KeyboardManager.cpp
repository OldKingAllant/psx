#include "KeyboardManager.hpp"
#include <psxemu/include/psxemu/StandardController.hpp>

#include <fmt/format.h>
#include <ranges>

namespace psx::input {
	KeyboardManager::KeyboardManager() :
		m_controller{nullptr}
	{}

	void KeyboardManager::Deliver(std::any any_status) {
		auto status = std::any_cast<KeyboardButtonStatus>(any_status);

		ButtonStatus btn_status{};

		auto uppercase_name = std::views::transform(
			status.key, [](char ch) { return std::toupper(ch); }
		) | std::ranges::to<std::string>();

		if (uppercase_name == "UP") {
			btn_status.name = "DPAD-UP";
			btn_status.ty = ButtonType::NORMAL;
			btn_status.normal.pressed = status.pressed;
		}
		else if(uppercase_name == "DOWN") {
			btn_status.name = "DPAD-DOWN";
			btn_status.ty = ButtonType::NORMAL;
			btn_status.normal.pressed = status.pressed;
		}

		m_controller->UpdateStatus(btn_status);
	}

	void KeyboardManager::AttachController(AbstractController* controller) {
		m_controller = controller;

		if (m_controller->GetType() == ControllerType::STANDARD) {
			fmt::println("[INPUT] Connected STANDARD controller");
		}
	}

	KeyboardManager::~KeyboardManager() {}
}