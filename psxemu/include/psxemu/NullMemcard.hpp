#pragma once

#include "AbstractMemcard.hpp"

#include <vector>
#include <optional>

namespace psx {
	class NullMemcard : public AbstractMemcard {
	public :
		NullMemcard();

		u8 Send(u8 value) override;
		bool Ack() override;
		void Reset() override;
		bool LoadFile(std::string const& path) override;

		u32 GetUpdateSequenceNumber() const override { return 0; };
		std::optional<std::vector<u8>> ReadFrame(u32 frame_num) const override {
			return std::nullopt;
		}

		bool WriteFrame(u32 frame_num, std::vector<u8> const& data) override {
			return true;
		}

		virtual bool IsConnected() const override {
			return false;
		}

		virtual std::optional<std::string> GetMcPath() const override {
			return std::nullopt;
		}

		~NullMemcard() override;
	};
}