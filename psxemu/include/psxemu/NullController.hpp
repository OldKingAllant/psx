#pragma once

#include "AbstractController.hpp"

namespace psx {
	class NullController : public AbstractController {
	public :
		NullController();

		u8 Send(u8 value) override;
		bool Ack() override;
		void Reset() override;

		ControllerType GetType() const override {
			return ControllerType::NONE;
		}

		~NullController() override;
	};
}