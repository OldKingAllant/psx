#include <psxemu/include/psxemu/CDFilesystem.hpp>
#include <psxemu/include/psxemu/CDROM.hpp>
#include <psxemu/include/psxemu/Logger.hpp>
#include <psxemu/include/psxemu/LoggerMacros.hpp>

#include <fstream>
#include <bit>
#include <algorithm>
#include <utility>
#include <set>
#include <vector>
#include <sstream>

namespace psx::kernel {
	static constexpr auto LICENSE_STRING_LOC = CdLocation(0, 2, 4);
	static constexpr auto PS_LOGO_LOC = CdLocation(0, 2, 5);
	static constexpr auto PS_LOGO_NUM_SECTORS = 7ULL;
	static constexpr auto PRIMARY_VOLUME_DESCRIPTOR_LOC = CdLocation(0, 2, 16);

	static constexpr auto PSX_LEN_SU = 14ULL;

	CdromFs::CdromFs() :
		m_cdrom{}, m_root_dir{}, m_volume_descriptor{}
	{
	}

	std::string CdromFs::ReadLicenseString() {
		constexpr u64 LICENSE_BASE_LEN = 0x40;
		auto license_sector = m_cdrom->ReadSector(LICENSE_STRING_LOC.mm, LICENSE_STRING_LOC.ss,
			LICENSE_STRING_LOC.sect);

		auto license_end = LICENSE_BASE_LEN;
		while (license_sector[license_end] != '\0' && license_sector[license_end] != '\n' &&
			license_end < license_sector.size())
			license_end++;

		return std::string{ license_sector.data(), license_sector.data() + license_end };
	}

	void CdromFs::DumpPsLogo(std::string const& path) {
		std::ofstream out{ path, std::ios::binary };
		if (!out.is_open()) {
			LOG_ERROR("KERNEL", "[KERNELFS] Playstation logo dump error: cannot create dump file");
			return;
		}

		auto loc = PS_LOGO_LOC;

		for (u64 curr_sector = 0; curr_sector < PS_LOGO_NUM_SECTORS; curr_sector++) {
			auto logo_sector = m_cdrom->ReadSector(loc.mm, loc.ss, loc.sect);
			out.write(std::bit_cast<char*>(logo_sector.data()), logo_sector.size());
			loc++;
		}

		LOG_INFO("KERNEL", "[KERNELFS] Dumped Playstation logo at {}", path);
	}

	std::optional<std::shared_ptr<HLEFsEntry>> CdromFs::GetEntry(std::string path) {
		std::vector<std::string> dirs{};
		std::istringstream stream{};
		std::string curr_dir{};
		stream.str(path);

		while (std::getline(stream, curr_dir, '/')) {
			dirs.push_back(curr_dir);
		}

		dirs.erase(std::remove_if(dirs.begin(), dirs.end(), [](std::string const& curr_dir) {return curr_dir.empty(); }),
			dirs.end());

		if (dirs.size() == 0) {
			return m_root_dir;
		}

		std::shared_ptr<HLEDirectory> curr_entry = m_root_dir;
		std::size_t curr_index = 0;

		while (curr_index != dirs.size()) {
			for (auto& entry : curr_entry->entries) {
				if (entry->cd_record.entry_name == dirs[curr_index]) {
					curr_index++;

					if (auto dir_entry = std::dynamic_pointer_cast<HLEDirectory>(entry)) {
						curr_entry = dir_entry;
					}
					else if (curr_index != dirs.size()) {
						return std::nullopt;
					}

					if (curr_index == dirs.size()) {
						return entry;
					}

					break;
				}
			}
		}

		return std::nullopt;
	}

	std::optional<CompleteDirectoryRecord> CdromFs::GetRecord(std::string path) {
		auto entry = GetEntry(path);
		if (entry.has_value()) {
			return entry.value()->cd_record;
		}
		return std::nullopt;
	}

	void CdromFs::TraverseFilesystem() {
		auto primary_volume_loc = PRIMARY_VOLUME_DESCRIPTOR_LOC;

		CdVolumeDescriptor volume_descriptor{};
		auto descriptor_data = m_cdrom->ReadSector(
			primary_volume_loc.mm, primary_volume_loc.ss,
			primary_volume_loc.sect
		);

		std::copy_n(std::bit_cast<char*>(descriptor_data.data()),
			sizeof(CdVolumeDescriptor), std::bit_cast<char*>(&volume_descriptor));

		LOG_DEBUG("KERNEL", "[KERNELFS] Primary volume descriptor");
		LOG_DEBUG("KERNEL", "           Standard Identifier: {}", std::string{ std::begin(volume_descriptor.standard_identifier), std::end(volume_descriptor.standard_identifier) });
		LOG_DEBUG("KERNEL", "           System identifier  : {}", std::string{ std::begin(volume_descriptor.system_identifier), std::end(volume_descriptor.system_identifier) });
		LOG_DEBUG("KERNEL", "           Volume identifier  : {}", std::string{ std::begin(volume_descriptor.volume_identifier), std::end(volume_descriptor.volume_identifier) });
		LOG_DEBUG("KERNEL", "           CD-XA signature    : {}", std::string{ std::begin(volume_descriptor.cd_xa_signature), std::end(volume_descriptor.cd_xa_signature) });
		LOG_DEBUG("KERNEL", "           Logical block size : {}", volume_descriptor.le_logical_block_size());
		LOG_DEBUG("KERNEL", "           Volume space size  : {}", volume_descriptor.le_volume_space_size().num_logical_blocks);

		m_volume_descriptor = std::make_unique<CdVolumeDescriptor>();
		std::copy_n(&volume_descriptor, 1, m_volume_descriptor.get());

		auto read_directory_record = [](u8* base, u64& rem_bytes) {
			CompleteDirectoryRecord record{};
			if (*base == 0x0) {
				//Padding
				rem_bytes = 0;
				return std::pair{ record, base };
			}

			if (rem_bytes < sizeof(record.main_record)) {
				throw std::runtime_error("Parsing filesystem error: a directory entry crosses the sector boundary");
			}

			std::copy_n(std::bit_cast<char*>(base), sizeof(record.main_record),
				std::bit_cast<char*>(&record.main_record));
			if (rem_bytes < record.main_record.len_dr) {
				throw std::runtime_error("Parsing filesystem error: a directory entry crosses the sector boundary");
			}

			base += sizeof(record.main_record);
			record.entry_name = std::string{ std::bit_cast<char*>(base),
				std::bit_cast<char*>(base + record.main_record.len_fi) };
			base += record.main_record.len_fi;
			base += record.main_record.padding_len();

			if (record.main_record.calc_len_su() != PSX_LEN_SU) {
				LOG_WARN("KERNEL", "[KERNELFS] Directory record with name {} has LEN_SU = {}",
					record.entry_name, record.main_record.calc_len_su());
			}
			
			std::copy_n(std::bit_cast<char*>(base), sizeof(record.sys_area),
				std::bit_cast<char*>(&record.sys_area));

			rem_bytes -= record.main_record.len_dr;
			return std::pair{ record, base + record.main_record.calc_len_su() };
		};

		u64 rem_bytes = std::distance(std::begin(volume_descriptor.root_dir_record),
			std::end(volume_descriptor.root_dir_record));
		HLEDirectory root_dir{};
		root_dir.entry_location = EntryLocation::CDROM;
		root_dir.cd_record = read_directory_record(std::bit_cast<u8*>(&volume_descriptor.root_dir_record[0]),
			rem_bytes).first;

		LOG_DEBUG("KERNEL", "[KERNEL] Root directory logical block  : {:#x}", root_dir.cd_record.main_record.le_logical_block_num(0).block.id);
		LOG_DEBUG("KERNEL", "         Root directory data size bytes: {:#x}", root_dir.cd_record.main_record.le_data_size_bytes());

		std::set<u64> visited_offsets{};

		auto walk_dir = [&visited_offsets, &volume_descriptor, fs = this, &read_directory_record](HLEDirectory& dir) {
			auto walk_dir_impl = [&visited_offsets, &volume_descriptor, fs, &read_directory_record](this auto& walk_dir_ref, HLEDirectory& dir) -> void {
				auto data_size = dir.cd_record.main_record.le_data_size_bytes();
				auto logical_block_size = volume_descriptor.le_logical_block_size();
				auto logical_block_num = dir.cd_record.main_record.le_logical_block_num(logical_block_size);
				auto absolute_offset = logical_block_num.get_absolute_location();
				visited_offsets.insert(absolute_offset);
				auto sector_loc_orig = fs->m_cdrom->LogicalToPhysical(0, 0, absolute_offset, logical_block_size);
				auto sector_loc_copy = sector_loc_orig;
				sector_loc_copy.ss -= 2;
				absolute_offset = sector_loc_copy.to_mode2_absolute();

				if ((data_size & 0x7FF) != 0) {
					throw std::runtime_error("Parsing filesystem error: a directory has a size not multiple of the sector size");
				}

				LOG_DEBUG("KERNEL", "[KERNELFS] Directory {}", dir.cd_record.entry_name);
				LOG_DEBUG("KERNEL", "           Size  : {:#x}", data_size);
				LOG_DEBUG("KERNEL", "           Offset: {:#x}, or (m={},s={},sect={})", absolute_offset, 
					sector_loc_orig.mm, sector_loc_orig.ss, sector_loc_orig.sect);

				u32 total_sectors = data_size / logical_block_size;
				while (total_sectors--) {
					auto sector = fs->m_cdrom->ReadSector(
						sector_loc_orig.mm,
						sector_loc_orig.ss,
						sector_loc_orig.sect
					);
					u64 rem_bytes = logical_block_size;
					u8* entry_base = sector.data();
					while (rem_bytes) {
						auto [record, new_base] = read_directory_record(
							entry_base, rem_bytes
						);
						entry_base = new_base;
						if (record.main_record.len_dr == 0) {
							break;
						}

						auto logical_block_num = record.main_record.le_logical_block_num(logical_block_size);
						auto absolute_offset = logical_block_num.get_absolute_location();

						if (visited_offsets.find(absolute_offset) != visited_offsets.end()) {
							continue;
						}
						
						if (u32(record.main_record.file_flags) &
							u32(FileFlags::DIRECTORY)) {
							HLEDirectory new_dir{};
							new_dir.cd_record = record;
							new_dir.entry_location = EntryLocation::CDROM;
							walk_dir_ref(new_dir);
							dir.entries.push_back(std::make_shared<HLEDirectory>(new_dir));
						}
						else {
							HLEFile new_file{};
							new_file.cd_record = record;
							new_file.entry_location = EntryLocation::CDROM;
							dir.entries.push_back(std::make_shared<HLEFile>(new_file));
						}
					}
					sector_loc_orig++;
				}
			};

			walk_dir_impl(dir);
		};

		walk_dir(root_dir);
		m_root_dir = std::make_shared<HLEDirectory>(root_dir);

		LOG_DEBUG("KERNEL", "[KERNELFS] Dump filesystem");
		auto visit_function = [](std::shared_ptr<HLEFsEntry> entry) {
			if (auto dir_entry = std::dynamic_pointer_cast<HLEDirectory>(entry)) {
				LOG_DEBUG("KERNEL", "           Directory {}", dir_entry->cd_record.entry_name);
			}
			else {
				auto file_entry = std::dynamic_pointer_cast<HLEFile>(entry);
				LOG_DEBUG("KERNEL", "           File {}", file_entry->cd_record.entry_name);
			}
		};
		m_root_dir->DepthVisit(visit_function);
	}

	std::optional<std::vector<u8>> CdromFs::ReadFileFromPath(std::string path) {
		auto entry = GetEntry(path);
		if (entry.has_value()) {
			return ReadFileFromEntry(entry.value());
		}
		return std::nullopt;
	}

	std::optional<std::vector<u8>> CdromFs::ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry) {
		if (entry->entry_location != EntryLocation::CDROM) {
			return std::nullopt;
		}

		auto data_size = entry->cd_record.main_record.le_data_size_bytes();
		auto logical_block_size = m_volume_descriptor->le_logical_block_size();
		auto logical_block_num = entry->cd_record.main_record.le_logical_block_num(logical_block_size);
		auto absolute_offset = logical_block_num.get_absolute_location();
		auto sector_loc = m_cdrom->LogicalToPhysical(0, 0, absolute_offset, logical_block_size);

		std::vector<u8> file_data{};
		file_data.resize(data_size);

		u32 total_sectors = (u32)std::ceil(float(data_size) / logical_block_size);
		u32 rem_data_size = data_size;
		u32 curr_pos = 0;
		while (total_sectors--) {
			auto sector = m_cdrom->ReadSector(
				sector_loc.mm,
				sector_loc.ss,
				sector_loc.sect
			);

			auto to_copy = (size_t)rem_data_size >= logical_block_size ? logical_block_size : (size_t)rem_data_size;
			std::copy_n(sector.cbegin(), to_copy, file_data.begin() + curr_pos);
			sector_loc++;
			rem_data_size -= u32(to_copy);
			curr_pos += u32(to_copy);
		}

		return file_data;
	}

	std::optional<std::vector<u8>> CdromFs::ReadFileFromEntry(std::shared_ptr<HLEFsEntry> entry, u32 off, u32 len) {
		if (entry->entry_location != EntryLocation::CDROM) {
			return std::nullopt;
		}

		auto data_size = entry->cd_record.main_record.le_data_size_bytes();

		if (off + len > data_size) {
			return std::nullopt;
		}

		auto logical_block_size = m_volume_descriptor->le_logical_block_size();
		auto offset_block = off / logical_block_size;
		auto offset_inside_first_block = off % logical_block_size;
		auto logical_block_num = entry->cd_record.main_record.le_logical_block_num(logical_block_size);
		logical_block_num.block.id += offset_block;
		auto absolute_offset = logical_block_num.get_absolute_location();
		auto sector_loc = m_cdrom->LogicalToPhysical(0, 0, absolute_offset, logical_block_size);

		std::vector<u8> file_data{};
		file_data.resize(len);

		auto first_sector = m_cdrom->ReadSector(
			sector_loc.mm,
			sector_loc.ss,
			sector_loc.sect
		);
		auto to_copy = (size_t)len >= logical_block_size ? logical_block_size : (size_t)len;
		
		if (to_copy + offset_inside_first_block >= logical_block_size) {
			to_copy = u64(logical_block_size) - offset_inside_first_block;
		}

		len -= u32(to_copy);
		u32 curr_pos = 0;

		std::copy_n(first_sector.cbegin() + offset_inside_first_block, to_copy, file_data.begin() + curr_pos);
		sector_loc++;
		curr_pos += u32(to_copy);

		while (len) {
			auto sector = m_cdrom->ReadSector(
				sector_loc.mm,
				sector_loc.ss,
				sector_loc.sect
			);

			auto to_copy = (size_t)len >= logical_block_size ? logical_block_size : (size_t)len;
			std::copy_n(sector.cbegin(), to_copy, file_data.begin() + curr_pos);
			sector_loc++;
			len -= u32(to_copy);
			curr_pos += u32(to_copy);
		}

		return file_data;
	}
}