#pragma once

#include "AbstractController.hpp"

#include <common/Queue.hpp>

namespace psx {
	class StandardController : public AbstractController {
	public :
		StandardController();

		u8 Send(u8 value) override;
		bool Ack() override;
		void Reset() override;

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

		static constexpr u16 DIGITAL_ID = 0x5A41;
		static constexpr u16 ANALOG_ID = 0x5A73;

	private :
		void EnqueueStatus();

	private :
		ControllerStatus m_status;
		ControllerMode m_mode;
		Queue<u8, 16> m_response;
	};
}