#include <psxemu/include/psxemu/Kernel.hpp>
#include <psxemu/include/psxemu/SystemStatus.hpp>
#include <psxemu/include/psxemu/SystemBus.hpp>
#include <psxemu/include/psxemu/BiosHash.hpp>
#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>
#include <psxemu/include/psxemu/SyscallTables.hpp>
#include <psxemu/include/psxemu/psxexe.hpp>

#include <fstream>
#include <bit>
#include <filesystem>
#include <unordered_map>

#include <thirdparty/hash/sha256.h>

namespace psx::kernel {
	Kernel::Kernel(system_status* status) :
		m_rom_pointer{ nullptr },
		m_ram_pointer{ nullptr },
		m_syscall_entry_hooks{},
		m_syscall_exit_hooks{},
		m_hle{}, m_enable_hooks{}, 
		m_sys_status{status},
		m_hook_id{ 0 }, 
		m_entry_hooks_scheduled_for_removal{}, 
		m_exit_hooks_scheduled_for_removal{},
		m_bios_hash{},
		m_bios_version{std::nullopt},
		m_patched_values{} {
		status->kernel_instance = this;
	}

	void Kernel::ComputeHash() {
		auto base = std::bit_cast<char*>(m_rom_pointer);
		m_bios_hash = CalcBiosSHA256(std::span{ base, memory::region_sizes::PSX_BIOS_SIZE });

		if (KNOWN_BIOSES.contains(m_bios_hash)) {
			m_bios_version = KNOWN_BIOSES.at(m_bios_hash);
		}
	}

	std::optional<std::string> Kernel::ComputeMemoryHash(u32 start, u32 len) {
		u64 sum_u64 = (u64)start + (u64)len;
		u64 truncated_sum = (u64)(start + len);
		if (sum_u64 != truncated_sum) {
			return std::nullopt;
		}

		u8* base_address = m_sys_status->sysbus->GetGuestBase() + start;

		SHA256 sha{};
		return sha(std::bit_cast<void*>(base_address), len);
	}

	void Kernel::FlushCache() {}

	bool Kernel::LoadExe(std::string const& path, std::optional<std::span<char>> args, u32 path_ptr, u32 headerbuf, bool force_run) {
		namespace fs = std::filesystem;

		if (!fs::exists(path) || !fs::is_regular_file(path)) {
			LOG_ERROR("KERNEL", "[KERNEL] Load({}, {}) failed, file does not exist", path, headerbuf);
			return false;
		}
		
		if (path_ptr != 0) {
			if (headerbuf == 0) {
				LOG_ERROR("KERNEL", "[KERNEL] Load({}, {}) failed, called from guest code, but headerbuf is nullptr", path, headerbuf);
				return false;
			}

			u32 low_addr = path_ptr & ~0xF0000000;
			if (low_addr >= memory::region_offsets::PSX_MAIN_RAM_OFFSET +
				memory::region_sizes::PSX_MAIN_RAM_SIZE) {
				LOG_ERROR("KERNEL", "[KERNEL] Load({}, {}) failed, original path ptr is invalid", path, headerbuf);
				return false;
			}

			low_addr = headerbuf & ~0xF0000000;
			if (low_addr >= memory::region_offsets::PSX_MAIN_RAM_OFFSET +
				memory::region_sizes::PSX_MAIN_RAM_SIZE) {
				LOG_ERROR("KERNEL", "[KERNEL] Load({}, {}) failed, original headerbuf ptr is invalid", path, headerbuf);
				return false;
			}

			LogSyscall(0xA42, SyscallLogMode::PARAMETERS, m_sys_status);
		}

		std::ifstream file(path, std::ios::binary | std::ios::in);

		if (!file.is_open()) {
			LOG_ERROR("KERNEL", "[KERNEL] Load({}, {}) failed, open failed", path, headerbuf);
			return false;
		}

		file.seekg(0, std::ios::end);
		auto size = file.tellg();
		file.seekg(0, std::ios::beg);

		if (size <= 0x800) {
			LOG_ERROR("KERNEL", "[KERNEL] Load({}, {}) failed, invalid executable", path, headerbuf);
			return false;
		}

		u8* buf = new u8[size];
		file.read(std::bit_cast<char*>(buf), size);

		psxexe exe{ buf };
		auto header = exe.header();
		auto dest = header->dest_address;
		auto exe_size = header->filesize;

		if ((std::streampos)((u64)exe_size + 0x800) > size) {
			LOG_ERROR("KERNEL", "[KERNEL] Load({}, {}) failed, invalid executable", path, headerbuf);
			return false;
		}

		auto pc = header->start_pc;
		auto gp = header->start_gp;
		auto sp = exe.initial_sp();

		if (sp == 0x0) {
			sp = psxexe::DEFAULT_SP;
		}

		auto memfill_start = header->memfill_start;
		auto memfill_sz = header->memfill_size;

		constexpr u32 HEADER_END = 0x800;

		auto guest_space = m_sys_status->sysbus->GetGuestBase();

		if (memfill_sz != 0) {
			std::memset(guest_space + memfill_start, 0x0, memfill_sz);
		}

		m_sys_status->sysbus->CopyRaw(buf + 0x800, dest, exe_size);

		constexpr u32 ARGS_DEST = 0x180;

		if (args.has_value()) {
			auto const& data = args.value();
			m_sys_status->sysbus->CopyRaw(std::bit_cast<u8*>(data.data()), ARGS_DEST, (u32)data.size());
		}

		if (headerbuf != 0) {
			auto headerbuf_ptr = guest_space + headerbuf;
			constexpr auto COPY_SIZE = 0x4B - 0x10 + 1;
			std::copy_n(std::bit_cast<u8*>(header) + 0x10, COPY_SIZE, headerbuf_ptr);
		}

		FlushCache();

		LOG_INFO("KERNEL", "[KERNEL] Successfully loaded executable\n");
		LOG_INFO("KERNEL", "         Destination in memory : 0x{:x}\n", dest);
		LOG_INFO("KERNEL", "         Program counter : 0x{:x}\n", pc);
		LOG_INFO("KERNEL", "         Global pointer : 0x{:x}\n", gp);
		LOG_INFO("KERNEL", "         Stack pointer : 0x{:x}\n", sp);
		LOG_INFO("KERNEL", "         Memfill start : 0x{:x}, Size : 0x{:x}\n", memfill_start, memfill_sz);
		LOG_INFO("KERNEL", "         Size : 0x{:x}\n", exe_size);

		if (force_run) {
			LOG_INFO("KERNEL", "EXECUTE!");

			auto& cpu = *m_sys_status->cpu;

			if (path_ptr != 0) {
				pc -= 0x4;
			}

			cpu.GetPc() = pc;

			auto& regs = cpu.GetRegs();

			regs.gp = gp;
			regs.sp = sp;
			regs.fp = sp;

			cpu.FlushLoadDelay();
		}

		return true;
	}

	std::optional<std::string> Kernel::DecodeShiftJIS(std::span<char> data) {
		if (data.size() & 1) {
			LOG_ERROR("KERNEL", "[KERNEL] Cannot decode shift-jis string with odd length");
			return std::nullopt;
		}
		
		u16* char_ptr = std::bit_cast<u16*>(data.data());
		std::string decoded{};
		size_t curr_index = {};

		static bool char_map_inited = false;
		static auto jis_char_map = std::unordered_map{
			std::pair{ 
				0x81, 
				std::unordered_map{
					std::pair{ 0x40, ' ' },
					std::pair{ 0x43, ',' },
					std::pair{ 0x44, '.' },
					std::pair{ 0x46, ':' },
					std::pair{ 0x47, ';' },
					std::pair{ 0x48, '?' },
					std::pair{ 0x49, '!' },
					std::pair{ 0x4D, '`' },
					std::pair{ 0x4F, '^' },
					std::pair{ 0x51, '_' },
					std::pair{ 0x5E, '/' },
					std::pair{ 0x5F, '\\' },
					std::pair{ 0x60, '~' },
					std::pair{ 0x62, '|' },
					std::pair{ 0x65, '\'' },
					std::pair{ 0x66, '\'' },
					std::pair{ 0x67, '\"' },
					std::pair{ 0x68, '\"' },
					std::pair{ 0x69, '(' },
					std::pair{ 0x6A, ')' },
					std::pair{ 0x6D, '[' },
					std::pair{ 0x6E, ']' },
					std::pair{ 0x6F, '{' },
					std::pair{ 0x70, '}' },
					std::pair{ 0x7B, '+' },
					std::pair{ 0x7C, '-' },
					std::pair{ 0x81, '=' },
					std::pair{ 0x83, '<' },
					std::pair{ 0x84, '>' },
					std::pair{ 0x8C, '\'' },
					std::pair{ 0x8D, '\"' },
					std::pair{ 0x90, '$' },
					std::pair{ 0x93, '%' },
					std::pair{ 0x94, '#' },
					std::pair{ 0x95, '&' },
					std::pair{ 0x96, '*' },
					std::pair{ 0x97, '@' },
				}
			},
			std::pair {
				0x82,
				std::unordered_map{
					std::pair{ 0x4F, '0' },
					std::pair{ 0x50, '1' },
					std::pair{ 0x51, '2' },
					std::pair{ 0x52, '3' },
					std::pair{ 0x53, '4' },
					std::pair{ 0x54, '5' },
					std::pair{ 0x55, '6' },
					std::pair{ 0x56, '7' },
					std::pair{ 0x57, '8' },
					std::pair{ 0x58, '9' },
				}
			}
		};

		decoded.reserve(data.size() / 2);

		if (!char_map_inited) {
			u32 uppercase_start = 0x60;
			const u32 uppercase_end = 0x79;
			char curr_char = 'A';

			while (uppercase_start <= uppercase_end) {
				jis_char_map[0x82][uppercase_start] = curr_char;
				uppercase_start++;
				curr_char++;
			}

			u32 lowercase_start = 0x81;
			const u32 lowercase_end = 0x9A;
			curr_char = 'a';

			while (lowercase_start <= lowercase_end) {
				jis_char_map[0x82][lowercase_start] = curr_char;
				lowercase_start++;
				curr_char++;
			}

			char_map_inited = true;
		}

		while (*char_ptr != 0 && curr_index < data.size()) {
			u16 curr_char = *char_ptr;

			u32 jis_point = curr_char & 0xFF;
			u32 char_value = (curr_char >> 8) & 0xFF;

			if (!jis_char_map.contains(jis_point) ||
				!jis_char_map[jis_point].contains(char_value)) {
				return std::nullopt;
			}

			decoded.push_back(jis_char_map[jis_point][char_value]);

			char_ptr++;
			curr_index += 2;
		}

		return decoded;
	}
}