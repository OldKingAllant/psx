#pragma once

#include <psxemu/include/psxemu/HLEFilesystem.hpp>
#include <common/Defs.hpp>
#include <common/Macros.hpp>

#include <memory>
#include <string>
#include <optional>
#include <vector>

namespace psx {
	class CDROM;
}

namespace psx::kernel {
	class CdromFs {
	public :
		CdromFs();

		std::string ReadLicenseString();
		void DumpPsLogo(std::string const& path);

		FORCE_INLINE void SetCdrom(std::shared_ptr<CDROM> new_cdrom) {
			m_volume_descriptor.reset();
			m_root_dir.reset();
			m_cdrom = new_cdrom;
			TraverseFilesystem();
		}

		std::optional<std::shared_ptr<HLEFsEntry>> GetEntry(std::string path);
		std::optional<CompleteDirectoryRecord> GetRecord(std::string path);

		std::optional<std::vector<u8>> ReadFileFromPath(std::string path);
		std::optional<std::vector<u8>> ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry);
		std::optional<std::vector<u8>> ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry, u32 off, u32 len);

		void TraverseFilesystem();

	private :
		std::shared_ptr<CDROM> m_cdrom;
		std::shared_ptr<HLEDirectory> m_root_dir;
		std::unique_ptr<CdVolumeDescriptor> m_volume_descriptor;
	};
}