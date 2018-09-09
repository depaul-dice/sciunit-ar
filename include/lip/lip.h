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

#ifndef _LIP_LIP_H
#define _LIP_LIP_H

#include <stdint.h>
#include <chrono>
#include <memory>
#include <cerrno>
#include <system_error>

#include <stdex/functional.h>

namespace lip
{

namespace chrono = std::chrono;

enum class ftype
{
	is_regular_file = 0,
	is_directory = 1,
	is_symlink = 2,
};

enum class feature
{
	// if compressed, the digest field contains the original size
	// if not, the digest field contains a blake2b-224 hash
	lz4_compressed = 0x10,
	executable = 0x100,
	readonly = 0x200,
};

constexpr auto operator|(feature a, feature b)
{
	return feature(int(a) | int(b));
}

struct archive_clock
{
	using duration = chrono::duration<int64_t, std::ratio<1, 10000000>>;
	using rep = duration::rep;
	using period = duration::period;
	using time_point = chrono::time_point<archive_clock, duration>;
};

using ftime = archive_clock::time_point;

struct ptr
{
	int64_t offset;
};

struct fcard
{
	union {
		ptr name;
		char* arcname;
	};
	uint32_t flag;
	uint32_t digest[7];
	ftime mtime;
	ptr begin;
	ptr end;
};

static_assert(sizeof(fcard) == 64, "unsupported");

struct header
{
	char magic[4] = "LIP";
	int32_t epoch = 584755;
};

class packer
{
public:
	packer();
	~packer();

	template <class F>
	void start(F&& f)
	{
		write_ = std::forward<F>(f);
		write_struct<header>();
	}

	void add_directory(char const* arcname, ftime);
	void add_symlink(char const* arcname, ftime);
	void finish();

private:
	struct impl;

	template <class T>
	void write_struct(T&& v = {})
	{
		static_assert(std::is_trivially_copyable<
		                  std::remove_reference_t<T>>::value,
		              "not plain");
		auto nbytes =
		    write_(reinterpret_cast<char const*>(std::addressof(v)),
		           sizeof(v));
		if (nbytes != sizeof(v))
			throw std::system_error{ errno,
				                 std::system_category() };
		cur_.offset += nbytes;
	}

	stdex::signature<size_t(char const*, size_t)> write_;
	ptr cur_ = {};
	std::unique_ptr<impl> impl_;
};

}

#endif
