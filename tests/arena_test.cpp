#include "pch.h"

#include <common/FixedSizeArenaAllocator.hpp>

#include <stack>
#include <algorithm>
#include <vector>
#include <random>

TEST(ArenaTests, TestCreation) {
	psx::FixedArenaAllocator<int> pool{ 20 };
}

TEST(ArenaTests, TestSingleAlloc) {
	psx::FixedArenaAllocator<int> pool{ 20 };

	ASSERT_NE(pool.Alloc(), nullptr);
}

TEST(ArenaTests, TestTwoAllocs) {
	psx::FixedArenaAllocator<int> pool{ 20 };

	ASSERT_NE(pool.Alloc(), nullptr);
	ASSERT_NE(pool.Alloc(), nullptr);
}

TEST(ArenaTests, TestAllAlloc) {
	psx::FixedArenaAllocator<int> pool{ 20 };

	for (int i = 0; i < 20; i++) {
		ASSERT_NE(pool.Alloc(), nullptr);
	}

	ASSERT_EQ(pool.Alloc(), nullptr);
}

TEST(ArenaTests, TestDeallocAlloc) {
	psx::FixedArenaAllocator<int> pool{ 20 };

	int* ptr = pool.Alloc();

	ASSERT_TRUE(pool.Free(ptr));
}

TEST(ArenaTests, TestMultipleDeallocs) {
	psx::FixedArenaAllocator<int> pool{ 20 };

	int* ptr1 = pool.Alloc();
	int* ptr2 = pool.Alloc();
	int* ptr3 = pool.Alloc();

	ASSERT_TRUE(pool.Free(ptr2));
	ASSERT_TRUE(pool.Free(ptr3));
}

TEST(ArenaTests, TestAllocDeallocAll) {
	psx::FixedArenaAllocator<int> pool{ 20 };

	std::stack<int*> pointers{};

	for (int i = 0; i < 20; i++)
		pointers.push(pool.Alloc());

	for (int i = (int)pointers.size(); i > 0; i--) {
		ASSERT_TRUE(pool.Free(pointers.top()));
		pointers.pop();
	}
}

TEST(ArenaTests, TestShuffledDealloc) {
	psx::FixedArenaAllocator<int> pool{ 20 };

	std::vector<int*> pointers{};

	for (int i = 0; i < 20; i++)
		pointers.push_back(pool.Alloc());

	std::random_device rd;
	std::mt19937 g(rd());

	std::shuffle(pointers.begin(), pointers.end(), g);

	for (int* ptr : pointers) {
		ASSERT_TRUE(pool.Free(ptr));
	}
}