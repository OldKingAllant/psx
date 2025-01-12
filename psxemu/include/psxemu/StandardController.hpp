#pragma once

#include "AbstractController.hpp"

#include <common/Queue.hpp>

#include <unordered_map>
#include <string>

namespace psx {
	class StandardController : public AbstractController {
	public :
		StandardController();

		u8 Send(u8 value) override;
		bool Ack() override;
		void Reset() override;
		void UpdateStatus(ButtonStatus status) override;

		ControllerType GetType() const override {
			return ControllerType::STANDARD;
		}

		FORCE_INLINE void SetMode(bool analog) {
			m_mode = analog ?
				ControllerMode::ANALOG :
				ControllerMode::DIGITAL;
		}

		~StandardController() override;

		enum class ControllerStatus {
			IDLE,
			READ_STAT,
			COMMAND_END
		};

		enum class ControllerMode {
			DIGITAL,
			ANALOG
		};

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

	private :
		void EnqueueStatus();

	private :
		ControllerStatus m_status;
		ControllerMode m_mode;
		Queue<u8, 16> m_response;
		StandardButtonStatus m_btn_status;

		std::unordered_map<std::string, u64> m_btn_map;
	};
}