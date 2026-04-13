#pragma once

#include "Server.hpp"

#include <array>
#include <vector>

namespace psx::gdbstub {
	struct StopEmuCommand : public ServerCommand {
		void Execute() override;
		~StopEmuCommand() override {}
	};

	struct StopEmuResponse : public EmuCommand {
		void Execute() override;
		~StopEmuResponse() override {};
	};

	struct GetRegistersCommand : public ServerCommand {
		void Execute() override;
		~GetRegistersCommand() override {}
	};

	struct GetRegistersResponse : public EmuCommand {
		GetRegistersResponse() : regs{} {}
		GetRegistersResponse(std::array<uint32_t, Server::NUM_STUB_REGISTERS> const& arg) : regs{arg} {}
		std::array<uint32_t, Server::NUM_STUB_REGISTERS> regs;
		void Execute() override;
		~GetRegistersResponse() override {}
	};

	struct SetRegistersCommand : public ServerCommand {
		std::array<uint32_t, Server::NUM_STUB_REGISTERS> regs;
		SetRegistersCommand(std::array<uint32_t, Server::NUM_STUB_REGISTERS> const& arg) : regs{ arg } {}
		void Execute() override;
		~SetRegistersCommand() override {}
	};

	struct SetSingleRegisterCommand : public ServerCommand {
		uint32_t reg_index;
		uint32_t value;
		SetSingleRegisterCommand(uint32_t index, uint32_t val) :
			reg_index{index}, value{val} {}
		void Execute() override;
		~SetSingleRegisterCommand() override {}
	};

	struct ReadMemoryCommand : public ServerCommand {
		uint32_t address;
		uint32_t count;
		ReadMemoryCommand(uint32_t a, uint32_t c) : address{a}, count{c} {}
		void Execute() override;
		~ReadMemoryCommand() override {}
	};

	struct ReadMemoryResponse : public EmuCommand {
		std::vector<uint8_t> values;
		ReadMemoryResponse(std::vector<uint8_t>&& vals) : values{vals} {}
		void Execute() override;
		~ReadMemoryResponse() override {}
	};

	struct WriteMemoryCommand : public ServerCommand {
		uint32_t address;
		std::vector<uint8_t> bytes{};
		WriteMemoryCommand(uint32_t a, std::vector<uint8_t>&& b) : address{ a }, bytes{ b } {}
		void Execute() override;
		~WriteMemoryCommand() override {}
	};

	struct RunForNInstructionsCommand : public ServerCommand {
		size_t count;
		RunForNInstructionsCommand(size_t c) : count{ c } {}
		void Execute() override;
		~RunForNInstructionsCommand() override {}
	};

	struct RunInstructionsResponse : public EmuCommand {
		RunInstructionsResponse() {}
		void Execute() override;
		~RunInstructionsResponse() override {}
	};

	struct ContinueCommand : public ServerCommand {
		ContinueCommand() {}
		void Execute() override;
		~ContinueCommand() override {}
	};

	struct BreakTriggeredResponse : public EmuCommand {
		BreakTriggeredResponse() {}
		void Execute() override;
		~BreakTriggeredResponse() override {}
	};

	struct AddHardwareBreakpointCommand : public ServerCommand {
		uint32_t address;
		AddHardwareBreakpointCommand(uint32_t a) : address{a} {}
		void Execute() override;
		~AddHardwareBreakpointCommand() override {}
	};

	struct RemoveHardwareBreakpointCommand : public ServerCommand {
		uint32_t address;
		RemoveHardwareBreakpointCommand(uint32_t a) : address{ a } {}
		void Execute() override;
		~RemoveHardwareBreakpointCommand() override {}
	};
}