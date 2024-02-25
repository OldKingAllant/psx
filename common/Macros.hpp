#pragma once

#ifdef _MSC_VER
	#define FORCE_INLINE __declspec(noinline)
#else
	#define FORCE_INLINE __attribute__((always_inline))
#endif // _MSC_VER