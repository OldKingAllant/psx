#include "KeyboardManager.hpp"
#include <psxemu/include/psxemu/StandardController.hpp>

#include <fmt/format.h>
#include <ranges>
#include <unordered_set>

namespace psx::input {
	KeyboardManager::KeyboardManager() :
		m_controller{nullptr}, m_key_map{}
	{
		KeyMapDefault();
	}

	void KeyboardManager::KeyMapDefault() {
		m_key_map.insert({ "UP", "DPAD-UP" });
		m_key_map.insert({ "RIGHT", "DPAD-RIGHT" });
		m_key_map.insert({ "LEFT", "DPAD-LEFT" });
		m_key_map.insert({ "DOWN", "DPAD-DOWN" });
		m_key_map.insert({ "RETURN", "START" });
		m_key_map.insert({ "X", "CROSS" });
	}

	void KeyboardManager::Deliver(std::any any_status) {
		auto status = std::any_cast<KeyboardButtonStatus>(any_status);

		ButtonStatus btn_status{};

		auto uppercase_name = std::views::transform(
			status.key, [](char ch) { return std::toupper(ch); }
		) | std::ranges::to<std::string>();

		if (m_key_map.contains(uppercase_name)) {
			btn_status.name = m_key_map[uppercase_name];
			btn_status.ty = ButtonType::NORMAL;
			btn_status.normal.pressed = status.pressed;

			m_controller->UpdateStatus(btn_status);
		}
	}

	void KeyboardManager::AttachController(AbstractController* controller) {
		m_controller = controller;

		if (m_controller->GetType() == ControllerType::STANDARD) {
			fmt::println("[INPUT] Connected STANDARD controller");
		}
	}

	void KeyboardManager::SetKeyMap(std::unordered_map<std::string, std::string> const& new_keys) {
		if (new_keys.size() != m_key_map.size()) {
			fmt::println("[INPUT] Button map should contain all buttons");
			return;
		}

		m_key_map = new_keys;
	}

	KeyboardManager::~KeyboardManager() {}
}