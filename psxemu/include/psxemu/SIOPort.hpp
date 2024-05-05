#pragma once

#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include "SIOStructs.hpp"
#include "SIOAbstractDevice.hpp"

#include <memory>

namespace psx {
	static constexpr u32 SIO0_START = 0x40;
	static constexpr u32 SIO0_END = 0x50;

	static constexpr u32 SIO1_START = 0x50;
	static constexpr u32 SIO1_END = 0x60;

	struct system_status;

	class SIOPort {
	public :
		SIOPort(system_status* sys_status, u8 id);

		u32 Read32(u32 address);
		u16 Read16(u32 address);
		u8 Read8(u32 address);

		//IDK what happens with 32 bit writes
		void Write32(u32 address, u32 value);
		void Write16(u32 address, u16 value);
		void Write8(u32 address, u8 value);

		void ComputeClocksPerBit();
		void HandlePendingTransfer();

		FORCE_INLINE void Port1Connect(std::unique_ptr<SIOAbstractDevice> dev) {
			m_port1.swap(dev);
		}

		FORCE_INLINE void Port2Connect(std::unique_ptr<SIOAbstractDevice> dev) {
			m_port2.swap(dev);
		}

		friend void transfer_end_callback(void* port, u64 cycles_late);

	private :
		void TransferComplete();

	private :
		system_status* m_sys_status;
		u8 m_id;

		bool m_pending_transfer;
		bool m_new_transfer;
		bool m_tx_enable_latch;
		u8   m_tx_data;
		u8   m_old_tx;
		u8   m_new_rx_data;
		bool m_has_ack;
		bool m_has_data;

		SIOFifo m_rx_fifo;
		SIOStat m_stat;
		SIOMode m_mode;
		SIOControl m_control;

		u16 m_baud_rate;
		u16 m_baud_counter;
		u64 m_clocks_per_bit;

		u64 m_event_id;
		
		std::unique_ptr<SIOAbstractDevice> m_port1;
		std::unique_ptr<SIOAbstractDevice> m_port2;
	};
}