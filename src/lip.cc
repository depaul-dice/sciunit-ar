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
#include <stdex/oneof.h>

#include "raw_pass.h"
#include "lz4_pass.h"

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
	static constexpr size_t bufsize =
	    sizeof(int) + LZ4_COMPRESSBOUND(reqsize);

	alignas(int) char buf[bufsize];

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
{
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
{
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
	using raw = io::raw_output_pass<hashfn, impl::reqsize>;
	using lz4 = io::lz4_output_pass<impl::reqsize>;

	auto start = cur_;
	auto flag = ftype::is_regular_file | feat;
	auto rep = flag & finfo::rep_mask;
	auto pass = [=]() -> stdex::oneof<raw, lz4> {
		if (rep == int(feature::lz4_compressed))
			return lz4{};
		else
			return raw{};
	}();

	for (error_code ec;;)
	{
		auto n = pass.match([&](auto&& x) {
			return x.make_available(f, impl_->buf, impl::bufsize,
			                        ec);
		});
		if (ec)
			throw std::system_error{ ec };
		else if (n == 0)
			break;

		cur_.offset += write_buffer(impl_->buf, n);
	}

	auto info = pass.match([](auto&& x) { return x.stat(); });
	info.flag = flag;
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

}
