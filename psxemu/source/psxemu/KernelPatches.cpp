#include <psxemu/include/psxemu/Kernel.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>
#include <psxemu/include/psxemu/KernelPatchInstructions.hpp>
#include <psxemu/include/psxemu/Interpreter.hpp>
#include <psxemu/include/psxemu/SystemConf.hpp>
#include <psxemu/include/psxemu/BiosHash.hpp>
#include <psxemu/include/psxemu/psxexe.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

#include <bit>
#include <algorithm>

namespace psx::kernel {
	const auto LOAD_EXE_INSTRUCTION_ADDRESS = std::unordered_map<u64, u32>{
		{SCPH1001, 0xbfc06bc8}
	};

	const auto AFTER_EXE_LOAD_INSTRUCTION_ADDRESS = std::unordered_map<u64, u32>{
		{SCPH1001, 0xbfc06bd0}
	};

	std::optional<Kernel::PatchError> Kernel::PatchInstruction(std::unordered_map<u64, u32> const& bios_version_map, 
		u32 instruction) {
		using PatchError = Kernel::PatchError;

		if (!m_bios_version.has_value()) {
			return PatchError::UNKOWN_BIOS;
		}

		if (!bios_version_map.contains(m_bios_version.value())) {
			return PatchError::UNKOWN_ADDRESS;
		}

		if (!m_sys_status->sysbus->MapBiosAsWriteable()) {
			return PatchError::MMAP_FAILED;
		}

		auto guest_base = m_sys_status->sysbus->GetGuestBase();
		auto address = bios_version_map.at(m_bios_version.value());
		auto addrspace_as_u32 = std::bit_cast<u32*>(guest_base +
			address);
		m_patched_values[address] = *addrspace_as_u32;
		*addrspace_as_u32 = instruction;

		if (!m_sys_status->sysbus->MapBiosAsReadonly()) {
			return PatchError::MMAP_FAILED;
		}

		return std::nullopt;
	}

	std::optional<Kernel::PatchError> Kernel::PatchInstruction(u32 address, u32 instruction) {
		if (!m_sys_status->sysbus->MapBiosAsWriteable()) {
			return PatchError::MMAP_FAILED;
		}

		auto guest_base = m_sys_status->sysbus->GetGuestBase();
		auto addrspace_as_u32 = std::bit_cast<u32*>(guest_base +
			address);
		m_patched_values[address] = *addrspace_as_u32;
		*addrspace_as_u32 = instruction;

		if (!m_sys_status->sysbus->MapBiosAsReadonly()) {
			return PatchError::MMAP_FAILED;
		}

		return std::nullopt;
	}

	void Kernel::PatchLoad() {
		auto patch_result = PatchInstruction(LOAD_EXE_INSTRUCTION_ADDRESS, FAKE_EXE_LOAD_INSTRUCTION);
		if (patch_result.has_value()) {
			LOG_ERROR("KERNEL", "[KERNEL] Patch call to Load() failed: {}", 
				magic_enum::enum_name(patch_result.value()));
		}
	}

#pragma optimize("", off)
	std::optional<Kernel::PatchError> Kernel::ApplyPatch(std::vector<std::string> pattern, std::vector<u8> const& values) {
		auto ram = m_sys_status->sysbus->GetGuestBase();
		auto ram_end = ram + memory::region_sizes::PSX_MAIN_RAM_SIZE;

		//std::transform(pattern.cbegin(), pattern.cend(), pattern.begin(), [](std::string bytes) {
		//	std::reverse(bytes.begin(), bytes.end());
		//	return bytes;
		//});

		auto new_pattern = std::reduce(pattern.begin(), pattern.end());
		new_pattern.erase(std::remove_if(new_pattern.begin(), new_pattern.end(), [](char c) {return std::isspace(c); }),
			new_pattern.end());
		auto pattern_len = new_pattern.size();

		if ((pattern_len & 0x1) != 0) {
			return PatchError::INVALID_FORMAT;
		}

		auto mem_values = new_pattern;
		auto masks = new_pattern;



		//while (ram < ram_end) {
		//	for (size_t curr_pos = 0; curr_pos < pattern_len; curr_pos += 2) {
		//		char curr_value[2] = { new_pattern[curr_pos], new_pattern[curr_pos + 1] };
		//		char curr_mask[2] = { curr_value[0], curr_value[1] };
		//		if (curr_value[0] == '?') {
		//			curr_value[0] = '0';
		//			curr_mask[0] = 'F';
		//		}
		//
		//		if (curr_value[1] == '?') {
		//			curr_value[1] = '0';
		//			curr_mask[1] = 'F';
		//		}
		//	}
		//	ram += pattern_len;
		//}

		return std::nullopt;
	}

	bool Kernel::FakeExeLoad() {
		cpu::MIPS1& the_cpu = *m_sys_status->cpu;

		u32 curr_pc = the_cpu.GetPc();
		u32 next_instruction = m_sys_status->sysbus->Read<u32, false, false>(curr_pc + 0x4);

		cpu::InterpretMips(m_sys_status, next_instruction);
		the_cpu.FlushLoadDelay();

		u32 exe_path = the_cpu.GetRegs().a0;
		u32 headerbuf = the_cpu.GetRegs().a1;

		if (m_sys_status->sys_conf->patch_load && !m_sys_status->sys_conf->exe_file.empty()) {
			std::string& args = m_sys_status->sys_conf->exe_args;
			auto args_span = std::optional{ std::span{ args } };
			if (args.empty()) {
				args_span = std::nullopt;
			}
			the_cpu.GetRegs().v0 = (u32)LoadExe(m_sys_status->sys_conf->exe_file, 
				args_span, exe_path, headerbuf, m_sys_status->sys_conf->force_run);
		}
		else {
			Load(exe_path, headerbuf);
		}

		if (m_sys_status->sys_conf->enable_exe_patching) {
			u8* headerbuf_ptr = m_sys_status->sysbus->GetGuestBase() + (headerbuf - 0x10);
			psxexe_header* header = std::bit_cast<psxexe_header*>(headerbuf_ptr);
			LOG_INFO("KERNEL", "[KERNEL] Executable header location: {:#010x}", headerbuf);
			LOG_INFO("KERNEL", "         Executable location: {:#010x}", header->dest_address);
			LOG_INFO("KERNEL", "         Executable size    : {:#010x}", header->filesize);

			auto exe_sha = ComputeMemoryHash(header->dest_address, header->filesize).value();

			LOG_INFO("KERNEL", "         Executable hash: {}", exe_sha);

			PatchInstruction(0x8003e668, NEXT_EVENT_INSTRUCTION);
			PatchInstruction(0x8003e6b0, GET_VBLANK_COUNT_INSTRUCTION);
			m_sys_status->sysbus->GetGPU().ResetVblankCount();
		}

		the_cpu.GetPc() += 0x4;

		return true;
	}

	bool Kernel::AfterExeLoad() {
		return false;
	}

	bool Kernel::NextEventInstruction() {
		(void)m_sys_status->scheduler.NextEvent();
		return true;
	}

	bool Kernel::GetVblankCountInstruction(u32 orig_instruction) {
		u32 rt = (orig_instruction >> 16) & 0x1F;
		auto curr_vblank = m_sys_status->sysbus->GetGPU().GetVblankCount();
		m_sys_status->AddLoadDelay(u32(curr_vblank), rt);
		return true;
	}

	bool Kernel::UndefinedInstruction(u32 instruction) {
		u32 curr_pc = m_sys_status->cpu->GetPc();
		if (!m_patched_values.contains(curr_pc)) {
			return false;
		}

		u32 old_instruction = m_patched_values[curr_pc];

		auto run_fake_instruction = [this, old_instruction](u32 instruction) {
			switch (instruction)
			{
			case FAKE_EXE_LOAD_INSTRUCTION:
				return FakeExeLoad();

			case AFTER_EXE_LOAD_INSTRUCTION:
				return AfterExeLoad();

			case NEXT_EVENT_INSTRUCTION:
				return NextEventInstruction();

			case GET_VBLANK_COUNT_INSTRUCTION:
				return GetVblankCountInstruction(old_instruction);
			}

			return false;
		};

		if (!run_fake_instruction(instruction)) {
			cpu::InterpretMips(m_sys_status, old_instruction);
		}

		return true;
	}
#pragma optimize("", on)
}