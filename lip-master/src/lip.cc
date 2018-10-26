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
#include <cedar/cedarpp.h>
#include <vector>

#include <stdex/hashlib.h>

#include "raw_pass.h"
#include "lz4_pass.h"

#include <FileSystem/File.h>

// TODO:: fix the unsigned/signed implicit conversions here I wanna ensure that
// making the ptr unsigned truely will have no side effect i think I saw some
// offset and bit fieldy shennagins having to do with when compression is
// turned on so I wanna get a battery of test cases written before I change
// that.

namespace lip
{

using hashfn = stdex::hashlib::blake2b_224;

struct packer::impl
{
	cedar::da<int> m;
	std::vector<fcard> v;
	int64_t bss_size = 0;

	static constexpr auto npos = decltype(m)::CEDAR_NO_PATH;
	static constexpr size_t reqsize = 64 * 1024;
	static constexpr size_t bufsize = LZ4_COMPRESSBOUND(reqsize);

	char buf[bufsize];

	ptr get_bes(ptr base) const { return { base.offset + bss_size }; }

	ptr get_index(ptr base) const
	{
		constexpr int x = alignof(int64_t);
		return { (get_bes(base).offset + (x - 1)) / x * x };
	}

	ptr get_bss(ptr base) const
	{
		constexpr int x = alignof(int64_t);
		return { base.offset / x * x };
	}
};

// TODO:: this :write_([]... shows compiler error for visual studio but it
// builds I'm worried that this may indicate a warning that's not shown by
// default investigate further
packer::packer()
    : write_([](char const*, size_t x) { return x; }), impl_(new impl())
{
	impl_->v.reserve(1024);
}

packer::packer(packer&&) noexcept = default;
packer& packer::operator=(packer&&) noexcept = default;
packer::~packer() = default;

constexpr auto operator|(ftype a, feature b)
{
	return uint32_t(a) | uint32_t(b);
}

void packer::start(write_callback f)
{  // warning: implicit unsigned signed conversion
	write_ = f;
	cur_.offset += write_struct(header{});
}

inline ptr packer::new_literal(string_view arcname)
{
	impl_->m.update(arcname.data(), arcname.size()) = int(impl_->v.size());
	return {};
}

void packer::add_directory(string_view arcname, ftime mtime)
{
	impl_->v.push_back({ { new_literal(arcname) },
	                     { { int(ftype::is_directory) } },
	                     mtime });
}

void packer::add_symlink(string_view arcname, ftime mtime, string_view target)
{  // warning unsigned signed conversion
	auto start = cur_;
	cur_.offset += write_buffer(target.data(), target.size());
	impl_->v.push_back(
	    { { new_literal(arcname) },
	      { { int(ftype::is_symlink), hashfn(target).digest() } },
	      mtime,
	      start,
	      cur_ });
}

void packer::add_regular_file(string_view arcname, ftime mtime,
                              refill_callback f, feature feat)
{
	auto start = cur_;
	io::raw_output_pass<hashfn, impl::reqsize> pass;

	for (error_code ec;;)
	{
		auto n = pass.make_available(f, impl_->buf, impl::bufsize, ec);
		if (ec)
			throw std::system_error{ ec };
		else if (n == 0)
			break;

		cur_.offset += write_buffer(impl_->buf, n);
	}

	auto info = pass.stat();
	info.flag = ftype::is_regular_file | feat;
	impl_->v.push_back(
	    { { new_literal(arcname) }, info, mtime, start, cur_ });
}

void packer::write_bss()
{
	std::vector<char> s;
	cedar::npos_t from = 0;
	size_t sz = 0;
	auto& m = impl_->m;
	for (int i = m.begin(from, sz); i != impl::npos; i = m.next(from, sz))
	{
		s.resize(sz + 1);
		m.suffix(s.data(), sz, from);
		impl_->v[size_t(i)].name = impl_->get_bes(cur_);
		impl_->bss_size += write_buffer(s.data(), s.size());
	}

	auto diff = size_t(impl_->get_index(cur_).offset -
	                   impl_->get_bes(cur_).offset);
	write_buffer("\0\0\0\0\0\0\0", diff);
}

void packer::write_index()
{
	cedar::npos_t from = 0;
	size_t sz = 0;
	auto& m = impl_->m;
	for (int i = m.begin(from, sz); i != impl::npos; i = m.next(from, sz))
	{
		write_struct(impl_->v[size_t(i)]);
	}
}

void packer::write_section_pointers()
{
	write_struct(impl_->get_index(cur_));
	write_struct(impl_->get_bss(cur_));
}

// NOTE:: below this line I'm implementing the LIP class I want to restructure
// a lot of this to hide the implementation of the LIP file so I'm going to
// implement the new features like read, find file, read index etc... inside
// the LIP class

// The idea is to be able to query the file and let the implementation details
// be handled magically
LIP::LIP(const char* const filePath)
{  // This constructor is not ideal for making new LIP's as it includes a check
   // for if is valid, which would not work for making new LIP's
	// but for now this will get me started, it's going to be confusing to
	// a user who wants to make a new lip at a given filepath.

	// May wanna do read/write by default
	// this opens the underlying file
	File::Open(this->fh, filePath, File::Mode::READ);

	// this ensures the filepath provided was to a LIP
	char buffer[4] = { 0x00 };
	File::Read(this->fh, buffer, 4);
	assert(buffer[0] == 'L');
	assert(buffer[1] == 'I');
	assert(buffer[2] == 'P');
	assert(buffer[3] == 0);
	File::Seek(this->fh, File::Location::BEGIN, 0);

	//Populates the index from the handle
	LIPIndex.FillIndex(this->fh);
	
}

// Index& LIP::getIndex()
//{
//	return LIPIndex;
//};
//
//
// LIP::~LIP()
//{
//	File::Close(fh);
//};
}
