#include <Windows.h>
#include <list>
#include <cstdint>
#include <bit>
#include <iostream>
#include <algorithm>

#pragma comment(lib, "mincore")


/*
In order to implement fastmem, we first need
to create an image of the guest address space
inside the host's address space. 

This is required, since fastmem requires
a pointer to the guest address space to
perform direct read/writes without the need
of emulating the bus. 

Unfortunately, the psx has many regions that
are mirrored in different locations in the address
space, like the main RAM, which is mapped to
3 different base addresses.
To achieve the same behaviour on modern
OS we can use memory mirroring.

This technique is easier to perform on POSIX/LINUX
systems due to syscalls as memfd_create/shm_open
and mmap, while on windows we need to perform
some shenanigans.

Steps:
1. Create a paging file backed file mapping object which is as big
   as the guest address space (4 GB) (or different files for each region)
2. Allocate the same amount of virtual memory using MEM_RESERVE_PLACEHOLDER
3. For each new guest region mapping, split a previously allocated host memory region
   by using VirtualFree and MEM_PRESERVE_PLACEHOLDER, at the wanted base address
   and wanted size (this effectively splits the allocated region in two or three regions)
4. From the file mapping object, map a view starting at the wanted base and with wanted
   size, with BaseAddress set to GUEST_BASE + MIRROR_OFFSET. Repeat this process for
   each mirror of a given region
5. Now you can read/write GUEST_BASE + OFFSET, and the memory should behave
   as intended (non mirrored regions have values that can be observed only
   in one single address range, mirrored regions like main RAM repeat themselves
   at the correct offsets)
*/


//Create memory backed file mapping object
auto CreateMemoryFile(uint64_t buf_size) -> HANDLE {
	return CreateFileMappingW(INVALID_HANDLE_VALUE,
		NULL, PAGE_READWRITE, 0x0, buf_size, NULL);
}

auto CreateAllocation(uint64_t buf_size) -> PVOID {
	return VirtualAlloc2(GetCurrentProcess(), NULL, buf_size, MEM_RESERVE |
		MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
}

struct Placeholder {
	uint64_t base;
	uint64_t extent;
};

/*
  Keeping track of the unused regions is not necessary in principle,
  but useful for avoiding errors and also useful if we want to
  add regions that do not exist on real hardware.

  This function takes the list of unused regions with base address and
  extent of the region that we want to use.

  If base and base + extent are not contained in one of the
  unused regions, the function fails.

  Otherwise, it splits the region in the list and in virtual
  memory
*/
auto SplitRegion(uint64_t base, uint64_t size, std::list<Placeholder>& placeholders, uint8_t* base_address) -> bool {
	auto position = std::find_if(placeholders.cbegin(), placeholders.cend(),
		[base, size](Placeholder const& placeholder) { return base >= placeholder.base &&
		base + size <= placeholder.base + placeholder.extent; });

	if (position == placeholders.cend()) {
		return false;
	}

	Placeholder original_placeholder = *position;

	//Remove old placeholder, since region changes configuration
	placeholders.erase(position);

	LPVOID effective_base = std::bit_cast<uint8_t*>(base_address) + base;

	if (original_placeholder.base == base) {
		//Split right
		if (size != original_placeholder.extent) {
			uint64_t new_extent = original_placeholder.extent - size;
			uint64_t new_base = base + size;
			placeholders.push_back(Placeholder{ new_base, new_extent });
		}
		//Otherwise, the region that we want to use has exactly the 
		//same size as the free one, so we simply remove it from the list
	}
	else if(original_placeholder.base + original_placeholder.extent == 
		base + size) {
		//Split left
		uint64_t new_base = original_placeholder.base;
		uint64_t new_extent = base - new_base;
		placeholders.push_back(Placeholder{ new_base, new_extent });
	}
	else {
		//Split both left and right
		uint64_t base_left = original_placeholder.base;
		uint64_t extent_left = base - base_left;

		uint64_t base_right = base + size;
		uint64_t extent_right =  (original_placeholder.base + original_placeholder.extent) - base_right;

		placeholders.push_back(Placeholder{ base_left, extent_left });
		placeholders.push_back(Placeholder{ base_right, extent_right });
	}

	return VirtualFree(effective_base, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
}



int main(int argc, char* argv[]) {
	SYSTEM_INFO sys_info{};

	GetSystemInfo(&sys_info);

	auto pageGranularity = sys_info.dwPageSize;

	//"Allocate" 4 GBs of virtual memory
	uint8_t* base = std::bit_cast<uint8_t*>( CreateAllocation((uint64_t)4 * 1024 * 1024 * 1024) );

	if (base == nullptr) {
		std::cout << "Address space allocation failed" << std::endl;
		std::exit(1);
	}

	std::list<Placeholder> free_placeholders{};

	//Add a single massive free region
	free_placeholders.push_back(Placeholder{ 0, (uint64_t)4 * 1024 * 1024 * 1024 });

	//Split regions 3 times

	bool result = SplitRegion(0x0, 1024 * 2048, free_placeholders, base);

	if (!result) {
		std::cout << "Ram allocation failed" << std::endl;
		std::exit(1);
	}

	result = SplitRegion(0x80000000, 1024 * 2048, free_placeholders, base);

	if (!result) {
		std::cout << "Ram allocation failed" << std::endl;
		std::exit(1);
	}

	result = SplitRegion(0xA0000000, 1024 * 2048, free_placeholders, base);

	if (!result) {
		std::cout << "Ram allocation failed" << std::endl;
		std::exit(1);
	}

	//Create a single 2MB memory file used only for main RAM
	auto ram_file_mapping = CreateMemoryFile(1024 * 2048);

	if (ram_file_mapping == NULL) {
		std::cout << "Ram file creation failed" << std::endl;
		std::exit(1);
	}

	//Mirror the main ram to the 3 different regions:
	//- KUSEG (starts at 0x0)
	//- KSEG0 (starts at 0x80000000)
	//- KSEG1 (starts at 0xA0000000)
	uint8_t* first_view = std::bit_cast<uint8_t*>(MapViewOfFile3(ram_file_mapping, GetCurrentProcess(), base, 0,
		1024 * 2048, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0));

	if (first_view == nullptr) {
		std::cout << "Ram file mapping failed" << std::endl;
		std::exit(1);
	}

	uint8_t* second_view = std::bit_cast<uint8_t*>(MapViewOfFile3(ram_file_mapping, GetCurrentProcess(), 
		base + 0x80000000, 0, 1024 * 2048, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0));

	if (second_view == nullptr) {
		std::cout << "Ram file mapping failed" << std::endl;
		std::exit(1);
	}

	uint8_t* third_view = std::bit_cast<uint8_t*>(MapViewOfFile3(ram_file_mapping, GetCurrentProcess(),
		base + 0xA0000000, 0, 1024 * 2048, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0));

	if (third_view == nullptr) {
		std::cout << "Ram file mapping failed" << std::endl;
		std::exit(1);
	}

	//Test read/writes

	first_view[0x0] = 0xAA;

	std::cout << std::hex << (uint32_t)first_view[0x80000000] << std::endl;
	std::cout << std::hex << (uint32_t)first_view[0xA0000000] << std::endl;

	for (uint32_t i = 0; i < 1024 * 2048; i++) {
		first_view[i] = 0xAA;
	}

	for (uint32_t i = 0; i < 1024 * 2048; i++) {
		if (first_view[0x80000000 + i] != 0xAA || first_view[0xA0000000 + i] != 0xAA) {
			std::cout << "Mirror not confirmed" << std::endl;
			std::exit(0);
		}
	}

	//Free used memory locations and close mapping object

	UnmapViewOfFile(first_view);
	UnmapViewOfFile(second_view);
	UnmapViewOfFile(third_view);

	CloseHandle(ram_file_mapping);

	std::cin.get();
}