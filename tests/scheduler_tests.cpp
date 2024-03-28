#include "pch.h"

#include <psxemu/include/psxemu/Scheduler.hpp>

#include <bit>

TEST(SchedulerTests, TestSchedule) {
	psx::Scheduler sched{};

	ASSERT_EQ(sched.Schedule(10, nullptr, nullptr), 0);
	ASSERT_EQ(sched.Schedule(5, nullptr, nullptr), 1);
	ASSERT_EQ(sched.Schedule(8, nullptr, nullptr), 2);
	ASSERT_EQ(sched.Schedule(12, nullptr, nullptr), 3);

	psx::psx_event const* events = sched.GetEvents();

	ASSERT_EQ(sched.GetNextID(), 4);
	ASSERT_EQ(sched.GetNumEvents(), 4);

	ASSERT_EQ(events->trigger_timestamp, 5);
}

TEST(SchedulerTests, TestDeschedule) {
	psx::Scheduler sched{};

	ASSERT_EQ(sched.Schedule(10, nullptr, nullptr), 0);
	ASSERT_EQ(sched.Schedule(5, nullptr, nullptr), 1);
	ASSERT_EQ(sched.Schedule(8, nullptr, nullptr), 2);
	ASSERT_EQ(sched.Schedule(12, nullptr, nullptr), 3);

	ASSERT_TRUE(sched.Deschedule(1));

	ASSERT_EQ(sched.GetEvents()->trigger_timestamp, 8);

	ASSERT_TRUE(sched.Deschedule(0));

	ASSERT_EQ(sched.GetEvents()->trigger_timestamp, 8);

	ASSERT_EQ(sched.GetNumEvents(), 2);

	ASSERT_TRUE(sched.Deschedule(2));

	ASSERT_EQ(sched.GetEvents()->trigger_timestamp, 12);
}

void Callback1(void* data, psx::u64 overcycles) {
	std::vector<int16_t>* trace = std::bit_cast<std::vector<int16_t>*>(data);

	ASSERT_EQ(overcycles, 10);

	trace->push_back(0);
}

void Callback2(void* data, psx::u64 overcycles) {
	std::vector<int16_t>* trace = std::bit_cast<std::vector<int16_t>*>(data);

	ASSERT_EQ(overcycles, 15);

	trace->push_back(1);
}

void Callback3(void* data, psx::u64 overcycles) {
	std::vector<int16_t>* trace = std::bit_cast<std::vector<int16_t>*>(data);

	ASSERT_EQ(overcycles, 12);

	trace->push_back(2);
}

void Callback4(void* data, psx::u64 overcycles) {
	std::vector<int16_t>* trace = std::bit_cast<std::vector<int16_t>*>(data);

	ASSERT_EQ(overcycles, 8);

	trace->push_back(3);
}

TEST(SchedulerTests, TestAdvance) {
	psx::Scheduler sched{};

	std::vector<int16_t> exec_trace{};

	ASSERT_EQ(sched.Schedule(10, Callback1, &exec_trace), 0);
	ASSERT_EQ(sched.Schedule(5, Callback2, &exec_trace), 1);
	ASSERT_EQ(sched.Schedule(8, Callback3, &exec_trace), 2);
	ASSERT_EQ(sched.Schedule(12, Callback4, &exec_trace), 3);

	sched.Advance(20);

	ASSERT_EQ(exec_trace.size(), 4);

	ASSERT_EQ(exec_trace[0], 1);
	ASSERT_EQ(exec_trace[1], 2);
	ASSERT_EQ(exec_trace[2], 0);
	ASSERT_EQ(exec_trace[3], 3);
}