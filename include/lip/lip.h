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
#include <algorithm>

#include <stdex/functional.h>
#include <stdex/string_view.h>

namespace lip
{

namespace chrono = std::chrono;

using stdex::string_view;
using std::error_code;
using read_sig = size_t(char*, size_t);
using write_sig = size_t(char const*, size_t);
using refill_sig = size_t(char*, size_t, error_code&);
using pread_sig = size_t(char*, size_t, int64_t);

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

	void adjust(void const* base, ptr where = {}) &
	{
		offset =
		    reinterpret_cast<intptr_t>(base) + (offset - where.offset);
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
	static constexpr int type_mask = 0xf;
	static constexpr int rep_mask = 0xff;

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

	ftype type() const
	{
		return static_cast<ftype>(info.flag & finfo::type_mask);
	}

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

	void start(std::function<write_sig> f);
	void add_directory(string_view arcname, ftime);
	void add_symlink(string_view arcname, ftime, string_view target);
	void add_regular_file(string_view arcname, ftime,
	                      stdex::signature<refill_sig>, feature = {});

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

	std::function<write_sig> write_;
	ptr cur_ = {};
	std::unique_ptr<impl> impl_;
};

class index
{
public:
	using iterator = fcard const*;

	explicit index(stdex::signature<pread_sig> f, int64_t filesize);

	iterator begin() const { return first_; }
	iterator end() const { return last_; }
	int size() const { return int(end() - begin()); }
	bool empty() const { return size() == 0; }
	fcard const& operator[](int i) const { return first_[i]; }

	iterator find(string_view arcname) const
	{
		auto it =
		    std::lower_bound(begin(), end(), arcname,
		                     [](fcard const& fc, string_view target) {
			                     return fc.arcname < target;
		                     });
		if (it != end() && it->arcname == arcname)
			return it;
		else
			return end();
	}

private:
	fcard *first_, *last_;
	std::unique_ptr<char[]> bp_;
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

class native_gbpath
{
public:
	native_gbpath();
	native_gbpath(native_gbpath&&) noexcept;
	native_gbpath& operator=(native_gbpath&&) noexcept;
	~native_gbpath();

	void assign(string_view filename);
	auto data() const noexcept -> gbpath::param_type;

private:
	struct impl;
	std::unique_ptr<impl> impl_;
};

struct archive_options
{
	bool one_level = false;
	feature feat = {};
};

void archive(std::function<write_sig>, gbpath::param_type src,
             archive_options = {});
}

#endif
