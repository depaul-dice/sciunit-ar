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
#include <array>
#include <chrono>
#include <memory>
#include <cerrno>
#include <system_error>

#include <stdex/functional.h>
#include <stdex/string_view.h>

#include <FileSystem/File.h>
#include <assert.h>

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
	// if compressed, the sizeopt field contains the original size
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

	friend int64_t operator-(ptr x, ptr y) { return x.offset - y.offset; }
};

using fhash = std::array<unsigned char, 28>;

union finfo
{
	struct
	{
		uint32_t flag;
		fhash digest;
	};
	struct
	{
		uint32_t flag_;
		uint32_t reserved;
		int64_t sizeopt;
	};
};

struct fcard
{
	union
	{
		ptr name;
		char* arcname;
	};
	finfo info;
	ftime mtime;
	ptr begin;
	ptr end;

	int64_t stored_size() const { return end - begin; }

	int64_t size() const
	{
		if (is_lz4_compressed())
			return info.sizeopt;
		else
			return stored_size();
	}

	ftype type() const { return static_cast<ftype>(info.flag & 0xf); }

	bool is_lz4_compressed() const
	{
		return (info.flag & int(feature::lz4_compressed)) != 0;
	}

	bool is_executable() const
	{
		return (info.flag & int(feature::executable)) != 0;
	}
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
	packer(packer&&) noexcept;
	packer& operator=(packer&&) noexcept;
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

class gbpath
{
public:
#ifdef _WIN32
	using char_type = wchar_t;
#else
	using char_type = char;
#endif
	using param_type = char_type const*;

	gbpath(param_type);
	gbpath(gbpath const&);
	gbpath& operator=(gbpath const&);
	gbpath(gbpath&&) noexcept;
	gbpath& operator=(gbpath&&) noexcept;
	~gbpath();

	auto friendly_name() const noexcept -> string_view;
	void push_back(param_type);
	void pop_back();

private:
	struct impl;
	std::unique_ptr<impl> impl_;
};

struct archive_options
{
	bool one_level = false;
	feature feat = {};
};

void archive(write_callback, gbpath::param_type src, archive_options = {});

// TODO:: make this easy serializable and deserializable so that it can be
// pulled to and from disk to a nice little structure the packer struct is cool
// and all but it could be much more strightforward to add things to the LIP if
// i had an existing lip on disk and I wanted to append I could load it into a
// LIP class and then add things I'm not focusing on this functionality now
// because it isn't explicitly needed. The goal is though to deal with the LIP
// class as an abstraction of the file itself and expose any necessary methods
// and hide all implementation details behind a nice clean interface
class LIP
{
public:
	class Index
	{
		fcard* indexPtr;
		uint numCards;

	public:

		Index() : indexPtr(0) {}

		void FillIndex(char* rawIndexBuffer, int64_t size) 
		{
			assert(size % 64 == 0);

			numCards = size / sizeof(fcard);

			indexPtr = (fcard*)rawIndexBuffer;
		}

		~Index()
		{
			if (indexPtr != nullptr)
				delete indexPtr;
		}

		// returns the number of fcards in the index
		uint getIndexSize() { return numCards; }
	};

private:
	File::Handle fh;
	Index LIPIndex;

public:
	LIP(const char* const filePath);
	~LIP() { File::Close(fh); }

	LIP(const LIP&) = delete;
	LIP operator=(const LIP&) = delete;

	Index* getIndex() { return &LIPIndex; }
};
}

#endif

// bonus documentation as I go
// so the very first thing that's written by the packer is the header structure
// it writes LIP\0 in the first 4 bytes of the file so it can be used as a
// check to make sure the path fed is a LIP