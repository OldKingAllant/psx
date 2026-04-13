#include <psxemu/include/psxemu/Server.hpp>

#include <psxemu/include/psxemu/System.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>

#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>
#include <psxemu/include/psxemu/ServerAsyncCommands.hpp>

namespace psx::gdbstub {
	void Server::HandleAsyncCommands() {
		if (!m_open.load() || !m_thread_state.is_running.load()) {
			return;
		}

		std::scoped_lock lk{ m_thread_state.server_queue_mutex };
		while (!m_thread_state.server_to_emu.empty()) {
			std::unique_ptr<ServerCommand> cmd{};
			cmd.swap(m_thread_state.server_to_emu.front());
			m_thread_state.server_to_emu.pop_front();
			cmd->Execute();
		}
	}

	void Server::HandleEmuCommands() {
		if (!m_open.load() || !m_thread_state.is_running.load()) {
			return;
		}

		std::scoped_lock lk{ m_thread_state.emu_queue_mutex };
		while (!m_thread_state.emu_to_server.empty()) {
			std::unique_ptr<EmuCommand> cmd{};
			cmd.swap(m_thread_state.emu_to_server.front());
			m_thread_state.emu_to_server.pop_front();
			cmd->Execute();
		}
	}

	/////////////////////////////////////////////

	void StopEmuCommand::Execute() {
		auto sys = server->GetSystem();
		sys->SetStopped(true);
		server->PushEmuToServerCommand<StopEmuResponse>();
	}

	void StopEmuResponse::Execute() {}

	void GetRegistersCommand::Execute() {
		auto sys = server->GetSystem();
		std::array<uint32_t, Server::NUM_STUB_REGISTERS> regs{};
		for (uint32_t reg_index = 0; reg_index < Server::NUM_STUB_REGISTERS; reg_index++) {
			regs[reg_index] = server->GetRegValueFromIndex(reg_index);
		}
		server->PushEmuToServerCommand<GetRegistersResponse>(regs);
	}
	
	void GetRegistersResponse::Execute() {}

	void SetRegistersCommand::Execute() {
		auto sys = server->GetSystem();
		for (uint32_t reg_index = 0; reg_index < Server::NUM_STUB_REGISTERS; reg_index++) {
			server->SetRegValueFromIndex(reg_index, this->regs[reg_index]);
		}
	}

	void SetSingleRegisterCommand::Execute() {
		auto sys = server->GetSystem();
		server->SetRegValueFromIndex((uint8_t)reg_index, value);
	}

	void ReadMemoryCommand::Execute() {
		auto sys = server->GetSystem();

		std::vector<uint8_t> values{};
		values.reserve(count);

		while (count) {
			auto value = sys->GetStatus().sysbus->Read<u8, false>(address);
			values.push_back(value);
			count--;
			address++;
		}

		server->PushEmuToServerCommand<ReadMemoryResponse>(std::move(values));
	}
	
	void ReadMemoryResponse::Execute() {}

	void WriteMemoryCommand::Execute() {
		auto sys = server->GetSystem();
		auto bus = sys->GetStatus().sysbus;

		auto curr_address = address;
		for (auto value : bytes) {
			bus->Write<psx::u8, false, false>(curr_address, value);
			curr_address++;
		}
	}

	void RunForNInstructionsCommand::Execute() {
		auto sys = server->GetSystem();
		sys->RunInterpreter((uint32_t)count);
		server->PushEmuToServerCommand<RunInstructionsResponse>();
	}

	void RunInstructionsResponse::Execute() {}

	void ContinueCommand::Execute() {
		auto sys = server->GetSystem();
		sys->SetStopped(false);
	}

	void BreakTriggeredResponse::Execute() {
		server->BreakTriggered();
	}

	void AddHardwareBreakpointCommand::Execute() {
		auto sys = server->GetSystem();
		sys->AddHardwareBreak(address);
	}

	void RemoveHardwareBreakpointCommand::Execute() {
		auto sys = server->GetSystem();
		sys->RemoveHardwareBreak(address);
	}
}