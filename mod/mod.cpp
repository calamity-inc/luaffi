#include <cstdint>

__declspec(dllexport) extern "C" uintptr_t add(uintptr_t a, uintptr_t b)
{
	return a + b;
}
