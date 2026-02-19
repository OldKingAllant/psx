#include <psxemu/include/psxemu/Kernel.hpp>
#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <thirdparty/magic_enum/include/magic_enum/magic_enum.hpp>

#include <algorithm>

namespace psx::kernel {
	std::optional<std::shared_ptr<HLEFsEntry>> Kernel::GetFilesystemEntry(std::string path) {
		auto colon_pos = path.find_first_of(':');
		if (colon_pos == std::string::npos) {
			return std::nullopt;
		}

		std::transform(path.begin(), path.end(), path.begin(), [](char c) {
			if (c == '\\')
				return '/';
			return char(std::toupper(c));
		});

		auto backslash_pos = path.find_first_of('/');
		if (backslash_pos != std::string::npos && backslash_pos < colon_pos) {
			return std::nullopt;
		}
		auto dev_name = path.substr(0, colon_pos);
		auto rem_path = std::string{};

		if (backslash_pos == std::string::npos) {
			rem_path = path.substr(colon_pos + 1);
		}
		else {
			rem_path = path.substr(backslash_pos + 1);
		}

		if (dev_name == "CDROM") {
			return m_cd_fs.GetEntry(rem_path);
		}
		else if (dev_name == "BU00") {
			return m_mc0_fs.GetEntry(rem_path);
		}
		else if (dev_name == "BU01") {
			return m_mc1_fs.GetEntry(rem_path);
		}

		LOG_ERROR("KERNEL", "[KERNELFS] Cannot get file entry for {}, device not supported", path);
		return std::nullopt;
	}

	std::optional<std::vector<u8>> Kernel::ReadFileFromPath(std::string path) {
		auto entry = GetFilesystemEntry(path);
		if (entry.has_value()) {
			return ReadFileFromEntry(entry.value());
		}
		return std::nullopt;
	}

	std::optional<std::vector<u8>> Kernel::ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry) {
		switch (entry->entry_location)
		{
		case EntryLocation::CDROM:
			return m_cd_fs.ReadFileFromEntry(entry);
		case EntryLocation::CARD: {
			if (entry->mc_data.card_slot == 0) {
				return m_mc0_fs.ReadFileFromEntry(entry);
			}
			else if (entry->mc_data.card_slot == 1) {
				return m_mc1_fs.ReadFileFromEntry(entry);
			}
			else {
				LOG_ERROR("KERNEL", "[KERNELFS] Reading card file {} with invalid slot", 
					entry->mc_data.fname);
			}
		}
		break;
		default:
			LOG_ERROR("KERNEL", "[KERNELFS] Cannot read unsupported device {}",
				magic_enum::enum_name(entry->entry_location));
			break;
		}

		return std::nullopt;
	}

	std::optional<std::vector<u8>> Kernel::ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry, u32 off, u32 len)
	{
		switch (entry->entry_location)
		{
		case EntryLocation::CDROM:
			return m_cd_fs.ReadFileFromEntry(entry, off, len);
		case EntryLocation::CARD: {
			if (entry->mc_data.card_slot == 0) {
				return m_mc0_fs.ReadFileFromEntry(entry, off, len);
			}
			else if (entry->mc_data.card_slot == 1) {
				return m_mc1_fs.ReadFileFromEntry(entry, off, len);
			}
			else {
				LOG_ERROR("KERNEL", "[KERNELFS] Reading card file {} with invalid slot",
					entry->mc_data.fname);
			}
		}
		break;
		default:
			LOG_ERROR("KERNEL", "[KERNELFS] Cannot read unsupported device {}",
				magic_enum::enum_name(entry->entry_location));
			break;
		}

		return std::nullopt;
	}
}