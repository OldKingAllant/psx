#include <psxemu/include/psxemu/BiosHash.hpp>

#include <thirdparty/hash/sha256.h>
#include <bit>

namespace psx::kernel {
	std::string CalcBiosSHA256(std::span<char> buf) {
		SHA256 sha{};
		return sha(std::bit_cast<void*>(buf.data()), buf.size_bytes());
	}
}

