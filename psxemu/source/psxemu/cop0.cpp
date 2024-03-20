#include <psxemu/include/psxemu/cop0.hpp>

namespace psx::cpu {
	void cop0::Exception(Excode exception_code) {
		registers.cause.exception_code = exception_code;

		//Perform stack copy operation 
		//and set current mode and int enable
		//to false
		registers.sr.old_mode = registers.sr.prev_mode;
		registers.sr.old_int_enable = registers.sr.prev_int_enable;
		registers.sr.prev_mode = registers.sr.current_mode;
		registers.sr.prev_int_enable = registers.sr.curr_int_enable;

		//Set current mode to kernel and disable interrupts
		registers.sr.current_mode = false;
		registers.sr.curr_int_enable = false;
	}

	void cop0::Rfe() {
		//Perform the "opposite" operations
		//wrt the other procedure
		registers.sr.current_mode = registers.sr.prev_mode;
		registers.sr.curr_int_enable = registers.sr.prev_int_enable;
		registers.sr.prev_mode = registers.sr.old_mode;
		registers.sr.prev_int_enable = registers.sr.old_int_enable;
	}
}