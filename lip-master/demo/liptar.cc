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
#define U(s) L##s
#define UF "%ls"
#define UNF "%.*ls"
#else
#define U(s) s
#define UF "%s"
#define UNF "%.*s"
#endif

using param_type = lip::gbpath::param_type;
using view_type = stdex::basic_string_view<lip::gbpath::char_type>;

class command_error : public std::runtime_error
{
public:
	command_error(view_type x, char const* msg)
	    : std::runtime_error(msg), cmd(x)
	{
	}

	view_type cmd;
};

struct args
{
	args(int argc, param_type* argv)
	{
		auto p = argv;
		if (++p == argv + argc)
		{
		err:
			fprintf(stderr,
			        "usage: " UF
			        " [ctx]f [-C <dir>] [--lz4] [--one-level] "
			        "<archive-file> [<directory>]\n",
			        argv[0]);
			exit(2);
		}

		cmd = *p;

		for (;;)
		{
			if (++p == argv + argc)
				goto err;
			if ((*p)[0] == U('-'))
			{
				view_type vp = *p;
				if (vp == U("-C"))
				{
					if (++p == argv + argc)
						goto err;
					cd = *p;
				}
				else if (vp == U("--lz4"))
					opts.feat =
					    lip::feature::lz4_compressed;
				else if (vp == U("--one-level"))
					opts.one_level = true;
				else
					goto err;
			}
			else
				break;
		}

		archive_file = *p;
		++p;
		directory = *p;
	}

	view_type cmd;
	lip::archive_options opts;
	param_type cd = nullptr, archive_file, directory;
};

static void create(param_type filename, param_type dirname,
                   lip::archive_options);

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[])
#else
int main(int argc, char* argv[])
#endif
{
	args a(argc, const_cast<param_type*>(argv));

	try
	{
                if (a.cd != nullptr)
                    chdir(a.cd);
		if (a.cmd == U("cf"))
		{
			if (a.directory == nullptr)
				throw command_error{ a.cmd,
					             "missing directory" };
			create(a.archive_file, a.directory, a.opts);
		}
		else
			throw command_error{ a.cmd, "unrecognized command" };
	}
	catch (command_error& e)
	{
		fprintf(stderr, UNF ": %s\n", int(e.cmd.size()), e.cmd.data(),
		        e.what());
		exit(2);
	}
	catch (std::exception& e)
	{
		fprintf(stderr, "ERROR: %s\n", e.what());
		exit(1);
	}
}

void create(param_type filename, param_type dirname, lip::archive_options opts)
{
	FILE* fp;
	std::unique_ptr<FILE, vvpkg::c_file_deleter> to_open;

	if (filename == view_type(U("-")))
	{
#ifdef _WIN32
		_setmode(_fileno(stdout), _O_BINARY);
#endif
		fp = stdout;
	}
	else
	{
		to_open.reset(vvpkg::xfopen(filename, U("wb")));
		fp = to_open.get();
	}

	lip::archive(vvpkg::to_c_file(fp), dirname, opts);
}
