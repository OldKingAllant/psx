#include "SetThreadName.hpp"

#ifdef _WIN32
#include <Windows.h>

const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)  
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.  
    LPCSTR szName; // Pointer to name (in user addr space).  
    DWORD dwThreadID; // Thread ID (-1=caller thread).  
    DWORD dwFlags; // Reserved for future use, must be zero.  
} THREADNAME_INFO;
#pragma pack(pop)  

#endif // _WIN32


namespace psx {
	bool SetThreadName(std::string const& name) {
#ifdef _WIN32
        THREADNAME_INFO info{};
        info.dwType = 0x1000;
        info.szName = name.c_str();
        info.dwThreadID = -1;
        info.dwFlags = 0;

        __try {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
        }

        return true;
#else
		return false;
#endif // _WIN32
	}
}