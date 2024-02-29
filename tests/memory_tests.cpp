#include "pch.h"

#include <psxemu/include/psxemu/MemoryMapper.hpp>

TEST(MemoryMapTest, TestCreation) {
	ASSERT_NO_THROW({
		psx::memory::MemoryMapper mapper{ std::nullopt };
	});
}

TEST(MemoryMapTest, TestSplitRight) {
	psx::memory::MemoryMapper mapper{ std::nullopt };
	
	ASSERT_TRUE(mapper.ReserveRegion(0x0, psx::memory::region_sizes::PSX_MAIN_RAM_SIZE));

	auto const& regions = mapper.GetFreeRegions();

	ASSERT_EQ(regions.size(), 1);

	auto const& reg1 = *regions.cbegin();

	ASSERT_EQ(reg1.guest_base, psx::memory::region_sizes::PSX_MAIN_RAM_SIZE);
	ASSERT_EQ((uint32_t)(reg1.guest_base + reg1.extent), 0x0);

	auto const& reserved_regions = mapper.GetReservedRegions();

	ASSERT_EQ(reserved_regions.size(), 1);

	auto const& reserved1 = *reserved_regions.cbegin();

	ASSERT_EQ(reserved1.guest_base, 0x0);
	ASSERT_EQ(reserved1.extent, psx::memory::region_sizes::PSX_MAIN_RAM_SIZE);
}

TEST(MemoryMapTest, TestSplitLeft) {
	psx::memory::MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion((uint64_t)UINT32_MAX + 1 - 4096, 4096));

	auto const& regions = mapper.GetFreeRegions();

	ASSERT_EQ(regions.size(), 1);

	auto const& reg1 = *regions.cbegin();

	ASSERT_EQ(reg1.guest_base, 0x0);
	ASSERT_EQ(reg1.guest_base + reg1.extent, (uint64_t)UINT32_MAX + 1 - 4096);
}

TEST(MemoryMapTest, TestSplitMiddle) {
	psx::memory::MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(0x80000000, psx::memory::region_sizes::PSX_MAIN_RAM_SIZE));

	auto const& regions = mapper.GetFreeRegions();

	ASSERT_EQ(regions.size(), 2);

	auto const& reg1 = *regions.cbegin();
	auto const& reg2 = *(++regions.cbegin());

	ASSERT_EQ(reg1.guest_base, 0x0);
	ASSERT_EQ(reg1.extent, 0x80000000);

	ASSERT_EQ(reg2.guest_base, 0x80000000 + psx::memory::region_sizes::PSX_MAIN_RAM_SIZE);
	ASSERT_EQ((uint32_t)(reg2.guest_base + reg2.extent), 0x0);
}

TEST(MemoryMapTest, TestReserveOutOfBoundsFail) {
	psx::memory::MemoryMapper mapper{ std::nullopt };

	ASSERT_FALSE(mapper.ReserveRegion(psx::memory::PSX_ADDRESS_SPACE_SIZE, 
		mapper.GetPageGranularity()));
}

TEST(MemoryMapTest, TestReserveInvalidAlign) {
	psx::memory::MemoryMapper mapper{ std::nullopt };

	ASSERT_FALSE(mapper.ReserveRegion(0x0, 1024));
}

TEST(MemoryMapTest, TestRamMap) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));
	ASSERT_TRUE(mapper.ReserveRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));
	ASSERT_TRUE(mapper.ReserveRegion(KSEG1_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));

	psx::u8* kuseg_ram = mapper.MapRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kuseg_ram, nullptr);

	psx::u8* kseg0_ram = mapper.MapRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kseg0_ram, nullptr);

	psx::u8* kseg1_ram = mapper.MapRegion(KSEG1_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kseg1_ram, nullptr);
}

TEST(MemoryMapTest, TestRamMirror) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE)) << "KUSEG Reserve failed";
	ASSERT_TRUE(mapper.ReserveRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE)) << "KSEG0 Reserve failed";

	psx::u8* kuseg_ram = mapper.MapRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kuseg_ram, nullptr) << "KUSEG Map failed";

	psx::u8* kseg0_ram = mapper.MapRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kseg0_ram, nullptr) << "KSEG0 Map failed";

	//Test read/writes

	//Test write through KUSEG and read through KSEG0
	kuseg_ram[0x0] = 0xAA;

	ASSERT_EQ(kseg0_ram[0x0], 0xAA);

	//Test write through KSEG0 and read through KUSEG
	kseg0_ram[0x1] = 0xAA;

	ASSERT_EQ(kuseg_ram[0x1], 0xAA);

	//Test unique continuos address space
	psx::u8* guest_base = mapper.GetGuestBase();

	guest_base[0x2] = 0x55;

	ASSERT_EQ(guest_base[KSEG0_START + 0x2], 0x55);
}

TEST(MemoryMapTest, TestRamUnmap) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE)) << "KUSEG Reserve failed";
	ASSERT_TRUE(mapper.ReserveRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE)) << "KSEG0 Reserve failed";

	psx::u8* kuseg_ram = mapper.MapRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kuseg_ram, nullptr) << "KUSEG Map failed";

	psx::u8* kseg0_ram = mapper.MapRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kseg0_ram, nullptr) << "KSEG0 Map failed";

	ASSERT_TRUE(mapper.UnmapRegion(kuseg_ram));
	ASSERT_TRUE(mapper.UnmapRegion(kseg0_ram));
}

TEST(MemoryMapTest, TestRamRemap) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE)) << "KUSEG Reserve failed";
	ASSERT_TRUE(mapper.ReserveRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE)) << "KSEG0 Reserve failed";

	psx::u8* kuseg_ram = mapper.MapRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kuseg_ram, nullptr) << "KUSEG Map failed";

	psx::u8* kseg0_ram = mapper.MapRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kseg0_ram, nullptr) << "KSEG0 Map failed";

	ASSERT_TRUE(mapper.UnmapRegion(kuseg_ram));
	ASSERT_TRUE(mapper.UnmapRegion(kseg0_ram));

	kuseg_ram = mapper.MapRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kuseg_ram, nullptr) << "KUSEG Remap failed";

	kseg0_ram = mapper.MapRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, 0x0, PageProtection::READ_WRITE);

	ASSERT_NE(kseg0_ram, nullptr) << "KSEG0 Remap failed";
}

TEST(MemoryMapTest, TestMapUnder64K) {
	//The Microsoft documentation states that MapViewOfFile3 requires
	//offset and size of the mapping to be aligned to 64KB, unless
	//MEM_REPLACE_PLACEHOLDER is specified.
	//Since SCRATCHPAD has 1KB size and the remaining space
	//until the next region is only 3KB, such alignment restriction
	//would be catastrophic, so we test if the flag specified by
	//the documentation produces the wanted behaviour
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_SCRATCHPAD_OFFSET, region_sizes::PSX_SCRATCHPAD_PADDED_SIZE)) << "SCRATCHPAD Reserve failed";

	psx::u8* scratchpad = mapper.MapRegion(KUSEG_START + region_offsets::PSX_SCRATCHPAD_OFFSET,
		region_sizes::PSX_SCRATCHPAD_PADDED_SIZE, region_mappings::PSX_SCRATCHPAD_MAPPING,
		PageProtection::READ_WRITE);

	ASSERT_NE(scratchpad, nullptr) << "SCRATCHPAD Map failed";

	std::ptrdiff_t offset = scratchpad - mapper.GetGuestBase();

	ASSERT_EQ(offset, region_offsets::PSX_SCRATCHPAD_OFFSET) << "SCRATCHPAD Offset has been rounded down";
}

TEST(MemoryMapTest, TestMultipleMapOffsets) {
	//Test mapping from multiple offsets inside the memory
	//file
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));
	ASSERT_TRUE(mapper.ReserveRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));
	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_BIOS_OFFSET, region_sizes::PSX_BIOS_SIZE));

	psx::u8* kuseg_main_ram = mapper.MapRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, region_mappings::PSX_MAIN_RAM_MAPPING, PageProtection::READ_WRITE);

	ASSERT_NE(kuseg_main_ram, nullptr);

	psx::u8* kseg0_main_ram = mapper.MapRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET,
		region_sizes::PSX_MAIN_RAM_SIZE, region_mappings::PSX_MAIN_RAM_MAPPING, PageProtection::READ_WRITE);

	ASSERT_NE(kseg0_main_ram, nullptr);

	psx::u8* bios = mapper.MapRegion(KUSEG_START + region_offsets::PSX_BIOS_OFFSET,
		region_sizes::PSX_BIOS_SIZE, region_mappings::PSX_BIOS_MAPPING, PageProtection::READ_WRITE);

	ASSERT_NE(bios, nullptr);

	//Test working mirror
	psx::u8* guest_base = mapper.GetGuestBase();

	guest_base[0x0] = 0xAA;

	ASSERT_EQ(guest_base[KSEG0_START], 0xAA);

	//Test BIOS is independent
	ASSERT_NE(guest_base[KUSEG_START + region_offsets::PSX_BIOS_OFFSET], 0xAA);
}

TEST(MemoryMapTest, TestCoalesceRight) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));

	ASSERT_TRUE(mapper.FreeRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET));
}

TEST(MemoryMapTest, TestCoalesceBoth) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));

	ASSERT_TRUE(mapper.FreeRegion(KSEG0_START + region_offsets::PSX_MAIN_RAM_OFFSET));
}

TEST(MemoryMapTest, TestCoalesceAndReserve) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));

	ASSERT_TRUE(mapper.FreeRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET));

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));
}

TEST(MemoryMapTest, TestMapAndCoalesce) {
	using namespace psx::memory;

	MemoryMapper mapper{ std::nullopt };

	ASSERT_TRUE(mapper.ReserveRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE));

	psx::u8* main_ram = mapper.MapRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET, region_sizes::PSX_MAIN_RAM_SIZE,
		region_mappings::PSX_MAIN_RAM_MAPPING, PageProtection::READ_WRITE);

	ASSERT_NE(main_ram, nullptr);

	ASSERT_TRUE(mapper.UnmapRegion(main_ram));

	ASSERT_TRUE(mapper.FreeRegion(KUSEG_START + region_offsets::PSX_MAIN_RAM_OFFSET));
}