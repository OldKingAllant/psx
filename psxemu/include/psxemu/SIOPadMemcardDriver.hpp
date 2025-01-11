#pragma once

#include "SIOAbstractDevice.hpp"
#include "AbstractController.hpp"
#include "AbstractMemcard.hpp"

#include <memory>

namespace psx {
	enum class SelectedDevice {
		NONE = 0x00,
		PAD = 0x01,
		MEMCARD = 0x81
	};

	class SIOPadCardDriver : public SIOAbstractDevice {
	public :
		SIOPadCardDriver();

		SIODeviceType GetDeviceType() const override {
			return SIODeviceType::PAD_MEMCARD_DRIVER;
		}

		u8 Send(u8 value, bool& has_data) override;
		bool Ack() override;
		void Unselect() override;

		~SIOPadCardDriver() override;

		void SelectDevice(u8 address);

		FORCE_INLINE void ConnectController(std::unique_ptr<AbstractController> controller) {
			m_controller.swap(controller);
		}

		FORCE_INLINE AbstractController* GetController() const {
			return m_controller.get();
		}

		FORCE_INLINE void ConnectCard(std::unique_ptr<AbstractMemcard> card) {
			m_card.swap(card);
		}

		FORCE_INLINE AbstractMemcard* GetCard() const {
			return m_card.get();
		}

	private :
		SelectedDevice m_selected;
		std::unique_ptr<AbstractController> m_controller;
		std::unique_ptr<AbstractMemcard> m_card;
	};
}