#include <vvpkg/fd_funcs.h>

#if defined(_WIN32)
#include <Windows.h>
#include <fileapi.h>
#endif

namespace vvpkg
{

#if defined(_WIN32)

struct _pread_fn
{
	size_t operator()(char* p, size_t sz, int64_t offset)
	{
	}

	int fd;
};

_pread_fn from_seekable_descriptor(int fd)
{
	return { fd };
}

#endif

}
