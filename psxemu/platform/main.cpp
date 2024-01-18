#include <iostream>

#include <fmt/format.h>
#include <asmjit/asmjit.h>

#include <Windows.h>

#include <atomic>


/*
For now, this is only a test of a fastmem + backpatching
implementation.

General idea:
When emulating a console, the guest code will rarely write
to portions of memory that require immediate/specific actions,
and most of the time, read and writes can be implemented directly
without emulating the guest bus. 
For this reason, fastmem provides a faster way of dealing with
read and writes.

In practice:
- We create a page table where we map the guest address space,
  setting page protections as needed (e.g. READONLY on I/O ports)
- When recompiling guest machine code to host, we assume that
  a memory instruction interacts with normal memory (unless
  we can statically assess the opposite), so we do something like
  mov reg, [guest_base + offset]
- If, instead, the instruction accesses I/O, it will
  trigger an access violation exception
- We can catch this exception and check if the cause 
  is in fact a recompiled instruction
- If it is not, a real unwanted error occured and we kill
  the process
- If it is, we find the base address of the recompiled code's
  page
- We change the page permissions to read/write
- We patch the instruction that caused the fault 
  replacing it with a trampoline
- We continue execution as if nothing happened

This renders the special case incredibly slow the first time around,
but all the rest is blazingly fast (in theory)

Note that the implementation of such things depends completely
on the host OS and arch.
*/


//Use cdecl calling convention
//Since parameters are passed on the stack
//and cleared by the caller
//Assume 16-byte alignment for the stack pointer
void __declspec(cdecl) writeDestination(uint32_t instruction) {
	std::cout << fmt::vformat("instruction 0x{:x}", fmt::make_format_args(instruction)) << std::endl;
}

//On an x64 system, this should be lock free 
//and signal safe
volatile static std::atomic<uint64_t> page_align;

LONG ExceptionFilter(EXCEPTION_POINTERS* info) {
	if (info->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION) {
		fprintf(stdout, "Unexpected exception type at %p", info->ExceptionRecord->ExceptionAddress);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if(info->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) {
		fprintf(stdout, "Exception will kill the current process");
		return EXCEPTION_CONTINUE_SEARCH;
	}

	//Contains a flag indicating whether the cause was a read or write
	auto cause = info->ExceptionRecord->ExceptionInformation[0];

	if (cause == 0) {
		fprintf(stdout, "Read exception not expected");
		return EXCEPTION_CONTINUE_SEARCH;
	}

	//Write destination
	auto address = info->ExceptionRecord->ExceptionInformation[1];

	//Rip of fault
	auto rip = info->ContextRecord->Rip;
	auto align = page_align.load();

	//Get base of the code page
	auto base = rip & ~(align - 1); //Base address of code page

	/*
	When a write happens, we expect the following register
	layout:

	rdx = Base of guest memory
	rcx = Guest instruction
	rbx = Write handler

	We want to replace the original instruction with 
	push rcx -> Push current instruction
	push rcx -> Again push, to align stack pointer to 16 byte boundary
	call rbx -> Call write handler
	pop rcx -> Remove current instruction
	pop rcx -> Remove align
	Insert nops until instruction is filled

	For write instructions, we should always pad 
	to 8 bytes

	"51 51 ff d3 59 59" NOP NOP
	*/

	DWORD old_protect{};

	if (!VirtualProtect(std::bit_cast<LPVOID>(base), align, PAGE_READWRITE, &old_protect)) {
		fprintf(stdout, "Could not change page permissions to r/w");
		return EXCEPTION_CONTINUE_SEARCH;
	}

	//Patch the instruction
	auto code = std::bit_cast<uint8_t*>(rip);

	code[0] = 0x51;
	code[1] = 0x51;
	code[2] = 0xff;
	code[3] = 0xd3;
	code[4] = 0x59;
	code[5] = 0x59;
	code[6] = 0x90;
	code[7] = 0x90;

	if (!VirtualProtect(std::bit_cast<LPVOID>(base), align, old_protect, &old_protect)) {
		fprintf(stdout, "Could not change page permissions to r/x");
		return EXCEPTION_CONTINUE_SEARCH;
	}

	//Continue execution
	return EXCEPTION_CONTINUE_EXECUTION;
}

/*
IMPORTANT:
This is only a toy example. A lot of things are taken
for granted here! (e.g. for example that the
instruction to patch does not cross a page boundary)

In the final implementation I still do not want to
do complicated shenanigans.
Assumptions for final usage:
- When executing guest code, we can ALWAYS obtain read and write
  handlers from specific registers -> This allows direct far jumps
  through CALL <reg>
- Read/Write instructions are all padded with NOPs until they reach
  a fixed length -> Easier backpatching, does not require instruction
  disassembly
- Current guest instruction is passed through a specific register -> 
  Does not require finding out the offset in the memory where the instruction
  was trying to read/write from/to. In practice, we can simply interpret the
  original instruction

Fortunately for us, instruction decoding for MIPS is thousands of times
easier WRT that of the ARM instruction set (look my GBA emulator for reference)
*/

int main(int argc, char* argv[]) {
	SYSTEM_INFO inf{};

	GetSystemInfo(&inf);

	page_align.store(inf.dwPageSize);

	uint8_t* memory = std::bit_cast<uint8_t*>(VirtualAlloc(NULL, inf.dwPageSize * 2, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

	if (memory == nullptr) {
		std::cout << "Could not map memory";
		std::cin.get();
		std::abort();
	}

	DWORD old_protect{};

	if (!VirtualProtect(memory + inf.dwPageSize, inf.dwPageSize, PAGE_READONLY, &old_protect)) {
		std::cout << "Could not protect memory";
		std::cin.get();
		std::abort();
	}

	std::cout << fmt::vformat("Protected region start : 0x{:x}\n", fmt::make_format_args(
		(uint64_t)(memory + inf.dwPageSize)
	));

	AddVectoredExceptionHandler(1, ExceptionFilter);

	asmjit::JitRuntime rt;
	asmjit::CodeHolder holder;

	holder.init(rt.environment());

	asmjit::x86::Assembler as(&holder);

	memory[0] = 0x20;

	as.push(asmjit::x86::rdx);
	as.push(asmjit::x86::rcx);
	as.push(asmjit::x86::rbx);
	as.mov(asmjit::x86::rbx, writeDestination);
	as.mov(asmjit::x86::rdx, std::bit_cast<uint64_t>(memory));
	as.mov(asmjit::x86::eax, 0x10);
	as.mov(asmjit::x86::word_ptr(asmjit::x86::rdx), asmjit::x86::eax);
	as.mov(asmjit::x86::rcx, 0x1);
	as.mov(asmjit::x86::word_ptr(asmjit::x86::rdx, inf.dwPageSize), asmjit::x86::eax); //This causes SIGSEGV
	as.nop(); //Insert NOP instructions for padding
	as.nop();
	as.pop(asmjit::x86::rbx);
	as.pop(asmjit::x86::rcx);
	as.pop(asmjit::x86::rdx);
	as.ret();

	using Func = int(*)(void);

	Func fn{nullptr};

	asmjit::Error err = rt.add(&fn, &holder);

	if (err) {
		std::abort();
	}

	fn(); //SIGSEGV!!!
	//Oh! Nothing happened

	std::cout << (int)memory[0] << std::endl;

	std::cin.get();

	VirtualFree(memory, 0, MEM_RELEASE);
}