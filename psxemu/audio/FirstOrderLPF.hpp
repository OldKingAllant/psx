#pragma once

#include <cstdint>

namespace psx {
	//Simple RC low pass filter
	//Cuts higher freqs. leaving
	//lower frequences almost
	//intact
	template <typename SampleType>
	class LowPassFilter {
	public:
		constexpr LowPassFilter(uint32_t sample_freq, uint32_t cutoff) :
			m_sample_freq{ sample_freq }, m_cutoff{ cutoff },
			m_dt{ 0.0 }, m_rc{ 0.0 }, m_alpha{ 0.0 }, m_last_sample{} {
			m_dt = (double)1 / sample_freq;
			m_rc = (double)1 / (TWO_PI * cutoff);
			m_alpha = m_dt / (m_rc + m_dt);
		}

		constexpr SampleType Filter(SampleType const& x) noexcept {
			SampleType y = (SampleType)(m_alpha * x + (1 - m_alpha) * m_last_sample);
			m_last_sample = y;
			return y;
		}

		static constexpr double TWO_PI = 2 * 3.14159;

	private:
		uint32_t m_sample_freq;
		uint32_t m_cutoff;
		double m_dt;
		double m_rc;
		double m_alpha;
		SampleType m_last_sample;
	};
}