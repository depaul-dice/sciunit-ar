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
#include <stdex/string_view.h>

namespace lip
{

namespace chrono = std::chrono;

using stdex::string_view;
using std::error_code;
using read_callback = stdex::signature<size_t(char*, size_t)>;
using write_callback = stdex::signature<size_t(char const*, size_t)>;
using refill_callback = stdex::signature<size_t(char*, size_t, error_code&)>;

enum class ftype
{
	is_regular_file = 0,
	is_directory = 1,
	is_symlink = 2,
};

enum class feature
{
	// if compressed, digest[0] and [1] contain the original size
	// if not, the digest field contains a blake2b-224 hash
	lz4_compressed = 0x10,
	executable = 0x100,
	readonly = 0x200,  // unimplemented
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

	static constexpr bool is_steady = false;

	static time_point now() noexcept;

	template <class T>
	static time_point from(T const&) noexcept;
};

using ftime = archive_clock::time_point;

struct ptr
{
	int64_t offset;

	void adjust(void const* base) &
	{
		auto p = reinterpret_cast<char const*>(base) + offset;
		offset = reinterpret_cast<intptr_t>(p);
	}

	template <class T>
	T* pointer_to() const
	{
		return reinterpret_cast<T*>(offset);
	}
};

struct fcard
{
	union {
		ptr name;
		char* arcname;
	};
	uint32_t digest[7];
	uint32_t flag;
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

	void start(write_callback f);
	void add_directory(string_view arcname, ftime);
	void add_symlink(string_view arcname, ftime, string_view target);
	void add_regular_file(string_view arcname, ftime, refill_callback,
	                      feature = {});

	void finish()
	{
		write_bss();
		write_index();
		write_section_pointers();
	}

private:
	struct impl;

	template <class T>
	size_t write_struct(T&& v = {})
	{
		static_assert(std::is_trivially_copyable<
		                  std::remove_reference_t<T>>::value,
		              "not plain");
		return write_buffer(
		    reinterpret_cast<char const*>(std::addressof(v)),
		    sizeof(v));
	}

	size_t write_buffer(char const* p, size_t sz)
	{
		if (write_(p, sz) != sz)
			throw std::system_error{ errno,
				                 std::system_category() };
		return sz;
	}

	void write_bss();
	void write_index();
	void write_section_pointers();

	ptr new_literal(string_view arcname);

	write_callback write_;
	ptr cur_ = {};
	std::unique_ptr<impl> impl_;
};

}

#endif
