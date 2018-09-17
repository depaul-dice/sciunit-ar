/*-
 * Copyright (c) 2018 Zhihao Yuan.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <lip/lip.h>
#include <vvpkg/c_file_funcs.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define U(s) stdex::wstring_view(L##s)
#define UF "%ls"
#else
#define U(s) stdex::string_view(s)
#define UF "%s"
#endif

using param_type = lip::gbpath::param_type;

static void create(param_type filename, param_type dirname);

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	if (argc < 4)
	{
	err:
		fprintf(stderr,
		        "usage: " UF " [ctx]f <archive-file> [<directory>]\n",
		        argv[0]);
		exit(2);
	}

	try
	{
		if (argv[1] == U("cf"))
			create(argv[2], argv[3]);
		else
			goto err;
	}
	catch (std::exception& e)
	{
		fprintf(stderr, "ERROR: %s\n", e.what());
		exit(1);
	}
}

void create(param_type filename, param_type dirname)
{
	FILE* fp;
	std::unique_ptr<FILE, vvpkg::c_file_deleter> to_open;

	if (filename == U("-"))
	{
#ifdef _WIN32
		_setmode(_fileno(stdout), _O_BINARY);
#endif
		fp = stdout;
	}
	else
	{
		to_open.reset(vvpkg::xfopen(filename, U("wb").data()));
		fp = to_open.get();
	}

	lip::archive(vvpkg::to_c_file(fp), dirname);
}
