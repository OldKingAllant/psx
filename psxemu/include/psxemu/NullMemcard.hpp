#pragma once

#include "AbstractMemcard.hpp"

namespace psx {
	class NullMemcard : public AbstractMemcard {
	public :
		NullMemcard();

		u8 Send(u8 value) override;
		bool Ack() override;
		void Reset() override;
		bool LoadFile(std::string const& path) override;

		~NullMemcard() override;
	};
}