#pragma once

#ifdef _MSC_VER
	#define FORCE_INLINE __declspec(noinline)
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