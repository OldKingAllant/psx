#pragma once

#ifdef _MSC_VER
	#define FORCE_INLINE __forceinline
	#define OFFSETOFF(STRUCT, MEMBER) (offsetof(STRUCT, MEMBER))
#else
	#define FORCE_INLINE __attribute__((always_inline))
#endif // _MSC_VER

#define THROW_IF_FALSE(OPERATION, EXCEPT) if(!(OPERATION)) { throw EXCEPT; }
#define THROW_IF_EQ(OPERATION, RES, EXCEPT) if((OPERATION) == (RES)) { throw EXCEPT; }

#define RETURN_IF_FALSE(OPERATION, RET) if(!(OPERATION)) { return (RET); }
#define RETURN_IF_EQ(OPERATION, RES, RET) if((OPERATION) == (RES)) { return (RET); }

#define DEBUG
#define DEBUG_CPU_ERRORS
#define DEBUG_IO

template <typename Result, unsigned Shift, typename Integral>
auto sign_extend(Integral value) -> Result {
	Result res = static_cast<Result>(value);
	constexpr auto num_bits = sizeof(Result) * 8 - 1;
	res <<= (num_bits - Shift);
	res >>= (num_bits - Shift);
	return res;
}