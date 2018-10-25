#include <vvpkg/fd_funcs.h>

#if defined(_WIN32)
#include <Windows.h>
#include <fileapi.h>
#endif

namespace vvpkg
{

#if defined(_WIN32)

size_t _pread_fn::operator()(char* p, size_t sz, int64_t offset)
{
	return 0;
}

#endif

}
