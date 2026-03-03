#include <psxemu/include/psxemu/FirResample.hpp>

#include <ranges>
#include <numeric>

namespace psx {
#pragma optimize("", off)
	FirResampler::FirResampler() : m_buffer{} {}

	void FirResampler::Push(i32 value) {
		if (m_buffer.size() >= FIR_SIZE) {
			m_buffer.pop_back();
		}
		m_buffer.push_front(value);
	}

	i32 FirResampler::Apply() {
		auto filtered_range = std::views::zip_transform([](i32 val, i32 coeff) {return (val * coeff) >> 15; },
			m_buffer, FIR_FILTER);

		auto value = std::reduce(std::ranges::begin(filtered_range), std::ranges::end(filtered_range));

		return value;
	}
#pragma optimize("", on)
}