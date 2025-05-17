#include <psxemu/include/psxemu/SIOPort.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/Interrupts.hpp>

#include <common/Errors.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <fmt/format.h>
#include <algorithm>

namespace psx {
	SIOPort::SIOPort(system_status* sys_status, u8 id) 
		: m_sys_status{ sys_status }, m_id{id}, m_pending_transfer{false}, m_new_transfer{false},
		m_tx_enable_latch {false}, m_tx_data{}, m_rx_fifo{}, m_stat{}, m_mode{}, m_control{},
		m_baud_rate{}, m_baud_counter{}, m_clocks_per_bit{},
		m_old_tx{}, m_new_rx_data{}, m_has_ack{ false }, m_has_data{false},
		m_event_id {INVALID_EVENT}, m_port1{}, m_port2{}
	{
		m_stat.tx_idle = true;
		m_stat.tx_not_full = true;
	}

	static constexpr u32 DATA_ADDRESS = 0x0;
	static constexpr u32 STAT_ADDRESS = 0x4;
	static constexpr u32 MODE_ADDRESS = 0x8;
	static constexpr u32 CTRL_ADDRESS = 0xA;
	static constexpr u32 MISC_ADDRESS = 0xC;
	static constexpr u32 BAUD_ADDRESS = 0xE;

	u32 SIOPort::Read32(u32 address) {
		if (address >= DATA_ADDRESS && address < DATA_ADDRESS + 4) {
			return Read8(DATA_ADDRESS) |
				(Read8(DATA_ADDRESS) << 8) |
				(Read8(DATA_ADDRESS) << 16) |
				(Read8(DATA_ADDRESS) << 24);
		}

		if (address >= STAT_ADDRESS && address < STAT_ADDRESS + 4) {
			m_stat.dsr_input_level = m_has_ack;
			u64 timestamp = m_sys_status->scheduler.GetTimestamp();
			u32 baudrate_timer = u32(timestamp % m_baud_rate);
			return m_stat.reg | (baudrate_timer << 11);
		}

		if (address >= MODE_ADDRESS && address < CTRL_ADDRESS + 2) {
			return Read16(MODE_ADDRESS) | (Read16(CTRL_ADDRESS) << 16);
		}

		if (address >= MISC_ADDRESS && address < BAUD_ADDRESS + 2) {
			return (u32(m_baud_rate) << 16);
		}

		LOG_ERROR("SIO", "[SIO{}] Reading invalid register {:#x}", m_id, address);
		return 0;
	}

	u16 SIOPort::Read16(u32 address) {
		if (address >= DATA_ADDRESS && address < DATA_ADDRESS + 4) {
			u32 offset = address - DATA_ADDRESS;
			if (offset == 2) {
				LOG_ERROR("SIO", "[SIO{}] UNHANDLED: Accessing RX FIFO in the middle", m_id);
				error::DebugBreak();
			}

			return Read8(DATA_ADDRESS + 1) | (Read8(DATA_ADDRESS) << 8);
		}

		if (address >= STAT_ADDRESS && address < STAT_ADDRESS + 4) {
			u32 whole_reg = Read32(STAT_ADDRESS);
			u32 offset = (address - STAT_ADDRESS) / 2;
			return u16(whole_reg >> (8 * offset));
		}

		if (address >= MODE_ADDRESS && address < MODE_ADDRESS + 2) {
			return m_mode.reg;
		}

		if (address >= CTRL_ADDRESS && address < CTRL_ADDRESS + 2) {
			return m_control.reg;
		}

		if (address >= BAUD_ADDRESS && address < BAUD_ADDRESS + 2) {
			return m_baud_rate;
		}

		LOG_ERROR("SIO", "[SIO{}] Reading invalid register {:#x}", m_id, address);
		return 0;
	}

	u8 SIOPort::Read8(u32 address) {
		if (address >= DATA_ADDRESS && address < DATA_ADDRESS + 4) {
			u8 offset = (address - DATA_ADDRESS);
			if (m_rx_fifo.num_bytes <= offset)
				return 0;
			u8 return_val = m_rx_fifo.entries[offset];
			if (offset == 0) {
				std::shift_left(m_rx_fifo.entries, std::end(m_rx_fifo.entries), 1);
				m_rx_fifo.num_bytes -= 1;
				if (m_rx_fifo.num_bytes == 0)
					m_stat.rx_not_empty = false;
			}
			return return_val;
		}

		LOG_WARN("SIO", "[SIO{}] 8 bit read!", m_id);
		return 0;
	}

	void SIOPort::Write32(u32 address, u32 value) {
		//NO$PSX says that SIO 32 bit writes
		//result in cropped writes:
		//write only lower 16bit (and leave upper 16bit unchanged)
		Write16(address, u16(value));
	}

	void SIOPort::Write16(u32 address, u16 value) {
		if (address >= BAUD_ADDRESS && address < BAUD_ADDRESS + 2) {
			u8 baud_factor = m_mode.baudrate_reload_factor;
			constexpr u32 MULS[] = { 1, 1, 16, 64 };
			u32 mul = MULS[baud_factor];
			
			m_baud_rate = value;
			m_baud_counter = (m_baud_rate * mul) / 2;

			ComputeClocksPerBit();
			return;
		}

		if (address >= STAT_ADDRESS && address < STAT_ADDRESS + 4)
			return;

		if (address >= DATA_ADDRESS && address < DATA_ADDRESS + 4) {
			if (!m_stat.tx_not_full)
				return;

			m_tx_enable_latch = m_control.tx_enable;
			m_stat.tx_not_full = false;
			
			if (m_pending_transfer) {
				//There is already one transfer
				m_new_transfer = true;
				m_tx_data = u8(value);
			}
			else {
				m_pending_transfer = true;
				m_old_tx = u8(value);
			}

			HandlePendingTransfer();

			return;
		}

		if (address >= MODE_ADDRESS && address < MODE_ADDRESS + 2) {
			m_mode.reg = value;
			m_mode.reg &= 0x1FF;

			if (m_id == 0) {
				m_mode.stop_bit_len = StopBit::RESERVED_ONE;
			}
			else {
				m_mode.clock_polarity = ClockPolarity::IDLE_HIGH;
			}

			LOG_DEBUG("SIO", "[SIO{}] MODE CHANGED", m_id);
			LOG_DEBUG("SIO", "        Reload factor   : {}", (u32)m_mode.baudrate_reload_factor);
			LOG_DEBUG("SIO", "        Char len        : {}", (u32)m_mode.char_len);
			LOG_DEBUG("SIO", "        Parity enable   : {}", (bool)m_mode.parity_enable);
			LOG_DEBUG("SIO", "        Parity type     : {}", (u32)m_mode.parity_type);
			LOG_DEBUG("SIO", "        Stop bit        : {}", (u32)m_mode.stop_bit_len);
			LOG_DEBUG("SIO", "        Clock polarity  : {}", (u32)m_mode.clock_polarity);

			ComputeClocksPerBit();

			return;
		}

		if (address >= CTRL_ADDRESS && address < CTRL_ADDRESS + 2) {
			bool old_tx_enable = m_control.tx_enable;
			auto old_port = m_control.port_select;
			m_control.reg = value;

			if (old_port != m_control.port_select) {
				if (old_port == PortSelect::PORT1 && m_port1)
					m_port1->Unselect();
				else if(m_port2)
					m_port2->Unselect();
				m_rx_fifo.num_bytes = 0;
				m_stat.rx_not_empty = false;
			}

			if (!m_control.dtr_output_level) {
				if(m_port1) m_port1->Unselect();
				if(m_port2) m_port2->Unselect();
			}

			if (!old_tx_enable && m_control.tx_enable) {
				HandlePendingTransfer();
			}

			if ((m_control.reg >> 4) & 1) {
				//ACK
				//Reset SIO_STAT.Bits 3,4,5,9
				m_control.reg &= ~(1 << 4);
				m_stat.rx_parity_err = false;
				m_stat.rx_buffer_overrun = false;
				m_stat.rx_bad_stop = false;
				m_stat.int_req = false;
			}

			if ((m_control.reg >> 6) & 1) {
				//RESET
				LOG_INFO("SIO", "[SIO{}] RESET", m_id);
				m_control.reg &= ~(1 << 6);
			}
			return;
		}

		LOG_ERROR("SIO", "[SIO{}] Writing invalid register {:#x}", m_id, address);
	}

	void SIOPort::Write8(u32 address, u8 value) {
		//Again, NO$PSX states that 8/32 bit
		//writes result always in 16 bit writes
		Write16(address & ~1, u16(value << (8 * (address & ~1))));
	}

	void SIOPort::ComputeClocksPerBit() {
		u8 baud_factor = m_mode.baudrate_reload_factor;

		constexpr u32 MULS[] = { 1, 1, 16, 64 };
		u32 mul = MULS[baud_factor];

		if (m_id == 0) {
			m_clocks_per_bit = std::max((m_baud_rate * mul) & ~1, 1u);
		}
		else {
			m_clocks_per_bit = std::max((m_baud_rate * mul) & ~1, mul);
		}

		u64 bits_second = SYSTEM_CLOCK / m_clocks_per_bit;

		LOG_INFO("SIO", "[SIO{}] BAUD RELOAD = {}, BITS/SECOND = {}, CLOCKS/BIT = {}",
			m_id, m_baud_rate, bits_second, m_clocks_per_bit);
	}

	void transfer_end_callback(void* port, u64 cycles_late) {
		std::bit_cast<SIOPort*>(port)->TransferComplete();
	}

	void SIOPort::HandlePendingTransfer() {
		static constexpr u64 ACK_INT_DELAY = 1000;

		if (m_pending_transfer && (m_control.tx_enable || m_tx_enable_latch) && 
			m_stat.tx_idle && m_control.dtr_output_level) {
			m_stat.tx_idle = false;
			m_stat.tx_not_full = true;
			
			if (m_control.port_select == PortSelect::PORT1) {
				m_new_rx_data = m_port1->Send(m_old_tx, m_has_data);
				m_has_ack = m_port1->Ack();
			}
			else {
				m_new_rx_data = m_port2->Send(m_old_tx, m_has_data);
				m_has_ack = m_port2->Ack();
			}

			m_event_id = m_sys_status->scheduler.Schedule(ACK_INT_DELAY, transfer_end_callback, this);
		}
	}

	void SIOPort::TransferComplete() {
		m_stat.tx_idle = true;
		m_pending_transfer = m_new_transfer;
		m_old_tx = m_tx_data;

		bool request = m_has_ack && m_control.dsr_int_enable;

		constexpr u32 N_BYTES[] = { 1, 2, 4, 8 };

		if ((m_id == 1 && m_control.rx_enable) || 
			(m_id == 0 && m_control.dtr_output_level && !m_control.rx_enable) ||
			(m_id == 0 && m_control.rx_enable)) {
			if (m_id == 0 && m_control.rx_enable)
				m_control.rx_enable = false;

			if (m_rx_fifo.num_bytes == 8) {
				m_rx_fifo.entries[7] = m_new_rx_data;
				if (m_id == 1)
					m_stat.rx_buffer_overrun = true;
			}
			else {
				m_rx_fifo.entries[m_rx_fifo.num_bytes++] = m_new_rx_data;
				if (m_control.rx_int_enable) {
					u32 requested_bytes = N_BYTES[m_control.rx_irq_after_n_bytes];
					if (m_rx_fifo.num_bytes == requested_bytes)
						request = true;
				}
				m_stat.rx_not_empty = true;
			}
		}

		if (request) {
			m_stat.int_req = true;
			m_sys_status->Interrupt(u32(Interrupts::PAD_CARD));
		}

		HandlePendingTransfer();
	}
}