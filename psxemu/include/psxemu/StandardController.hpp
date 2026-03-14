#pragma once

#include "AbstractController.hpp"

#include <common/Queue.hpp>

#include <unordered_map>
#include <string>
#include <array>

namespace psx {
	class StandardController : public AbstractController {
	public :
		StandardController();

		u8 Send(u8 value) override;
		bool Ack() override;
		void Reset() override;
		void UpdateStatus(ButtonStatus status) override;

		u32 GetTransferLength() const;

		static constexpr u32 CONFIG_MODE_TRANSFER_LEN = 9;

		ControllerType GetType() const override {
			return ControllerType::STANDARD;
		}

		FORCE_INLINE void SetMode(bool analog) {
			m_mode = analog ?
				ControllerMode::ANALOG :
				ControllerMode::DIGITAL;
		}

		~StandardController() override;

		enum class Command {
			READ = 0x42,
			CONFIG_MODE = 0x43,
			SET_LET_STATE = 0x44,
			GET_LED_STATE = 0x45,
			GET_VARIABLE_RESPONSE_A = 0x46,
			GET_WHATEVER = 0x47,
			GET_VARIABLE_RESPONSE_B = 0x4C,
			GET_SET_RUMBLE_PROTOCOL = 0x4D
		};

		enum class ControllerStatus {
			IDLE,
			READ_STAT,
			CONFIG,
			COMMAND_END
		};

		enum class ConfigStatus {
			JUST_ENTERED,
			CONFIG_IDLE,
			CONFIG_COMMAND_END
		};

		enum class ControllerMode {
			DIGITAL,
			ANALOG
		};

		enum class ConfigModeEnterExit {
			STAY = 0,
			ENTER_EXIT = 1
		};

		enum class LedState : u8 {
			OFF,
			ON
		};

		static constexpr u8 CONTROLLER_TYPE_ANALOG = 0x1;

		union StandardButtonStatus {
			struct {
#pragma pack(push, 1)
				bool select : 1;
				bool l3 : 1;
				bool r3 : 1;
				bool start : 1;
				bool up : 1;
				bool right : 1;
				bool down : 1;
				bool left : 1;
				bool l2 : 1;
				bool r2 : 1;
				bool l1 : 1;
				bool r1 : 1;
				bool triangle : 1;
				bool circle : 1;
				bool cross : 1;
				bool square : 1;
#pragma pack(pop)
			};

			u16 reg;
		};

		static constexpr u16 DIGITAL_ID = 0x5A41;
		static constexpr u16 ANALOG_ID = 0x5A73;
		static constexpr u16 CONFIG_ID = 0x5AF3;

	private :
		void EnqueueStatus(bool force_analog);

		void IdleProcessByte(u8 value);
		void ConfigProcessByte(u8 value);
		void ConfigIdleProcessByte(u8 value);

		void ConfigCommandEnd();

	private :
		ControllerStatus m_status;
		ConfigStatus m_conf_status;
		ControllerMode m_mode;
		Queue<u8, 16> m_response;
		Queue<u8, 32> m_in_fifo;
		StandardButtonStatus m_btn_status;
		LedState m_led_state;
		std::array<u8, 6> m_curr_rumble_protocol;

		std::unordered_map<std::string, u64> m_btn_map;
	};
}