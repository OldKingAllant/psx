#pragma once

#include "DmaBase.hpp"

namespace psx {
	class CDROMDma : public DmaBase {
	public:
		CDROMDma(system_status* sys_status, DmaController* dma_controller);

		static constexpr u32 CDROM_DATA = 0x1F801802;

		u32 GetPort() const override {
			return CDROM_DATA;
		}

		void DreqRisingEdge();

		void SetDreq(bool dreq) {
			m_dreq = dreq;
		}

		bool Dreq() const {
			return m_dreq;
		}

		void DoBurst() override;

	private:
		bool m_dreq;
	};
}