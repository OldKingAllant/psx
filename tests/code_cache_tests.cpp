#include "pch.h"

#include <psxemu/include/psxemu/CodeCache.hpp>

#include <asmjit/asmjit.h>

TEST(CodeCacheTests, TestCreation) {
	ASSERT_NO_THROW({
			psx::cpu::CodeCache cache( 4096, 2048 );
	});
}

TEST(CodeCacheTests, TestBlockAdd) {
	psx::cpu::CodeCache cache(4096, 2048);

	psx::cpu::JitBlock block{};

	block.guest_base = 0x0;
	block.guest_end = (psx::u64)4 * 20;

	asmjit::CodeHolder holder{};
	holder.init(cache.GetRuntime().environment());

	asmjit::x86::Assembler as(&holder);

	as.mov(asmjit::x86::eax, 0x1);
	as.ret();

	ASSERT_TRUE(cache.AddBlock(block, holder));
}

TEST(CodeCacheTests, TestAddMultipleBlocks) {
	psx::cpu::CodeCache cache(4096, 2048);

	psx::cpu::JitBlock block{};

	{
		block.guest_base = 0x0;
		block.guest_end = 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	{
		block.guest_base = 0x1;
		block.guest_end = 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	{
		auto block_sz = cache.GetBlockSize();

		block.guest_base = block_sz;
		block.guest_end =  block_sz + 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	{
		auto block_sz = cache.GetBlockSize();

		block.guest_base = block_sz + 0x1;
		block.guest_end = block_sz + 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	{
		auto block_sz = cache.GetBlockSize();

		block.guest_base = block_sz + 0x2;
		block.guest_end = block_sz + 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	auto block_sz = cache.GetBlockSize();

	ASSERT_NE(cache.GetBlock(0x0), nullptr);
	ASSERT_NE(cache.GetBlock(0x1), nullptr);
	ASSERT_EQ(cache.GetBlock(0x2), nullptr);

	ASSERT_NE(cache.GetBlock(block_sz), nullptr);
	ASSERT_NE(cache.GetBlock(block_sz + 0x1), nullptr);
	ASSERT_NE(cache.GetBlock(block_sz + 0x2), nullptr);
}

TEST(CodeCacheTests, TestInvalidateBlocks) {
	psx::cpu::CodeCache cache(4096, 2048);

	psx::cpu::JitBlock block{};

	{
		block.guest_base = 0x0;
		block.guest_end = 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	{
		block.guest_base = 0x1;
		block.guest_end = 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	{
		auto block_sz = cache.GetBlockSize();

		block.guest_base = block_sz;
		block.guest_end = block_sz + 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	{
		auto block_sz = cache.GetBlockSize();

		block.guest_base = block_sz + 0x1;
		block.guest_end = block_sz + 0x10;

		asmjit::CodeHolder holder{};
		holder.init(cache.GetRuntime().environment());

		asmjit::x86::Assembler as(&holder);

		as.mov(asmjit::x86::eax, 0x1);
		as.ret();

		ASSERT_TRUE(cache.AddBlock(block, holder));
	}

	auto block_sz = cache.GetBlockSize();

	ASSERT_EQ(cache.InvalidateBlocks(block_sz * 2, 0x10), 0);
	ASSERT_EQ(cache.InvalidateBlocks(0x0, 0x1), 1);
	ASSERT_EQ(cache.InvalidateBlocks(block_sz, 0x2), 2);
	ASSERT_EQ(cache.InvalidateBlocks(0x4, 0xF), 1);
}