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

namespace lip
{

struct packer::impl
{
	struct c_delete
	{
		void operator()(void* p) const { free(p); }
	};

	cedar::da<int> m;
	std::vector<fcard> v;
	int64_t bss_size = 0;
	std::unique_ptr<char, c_delete> buf;

	static constexpr auto npos = decltype(m)::CEDAR_NO_PATH;
	static constexpr size_t bufsize = 64 * 1024;

	ptr get_bes(ptr base) const { return { base.offset + bss_size }; }

	ptr get_index(ptr base) const
	{
		constexpr int x = alignof(int64_t);
		return { (get_bes(base).offset + (x - 1)) / x * x };
	}
};

packer::packer()
    : write_([](char const*, size_t x) { return x; }), impl_(new impl())
{
	impl_->v.reserve(1024);
	impl_->buf.reset((char*)malloc(impl::bufsize));
}

packer::~packer() = default;

constexpr auto operator|(ftype a, feature b)
{
	return uint32_t(a) | uint32_t(b);
}

void packer::start(write_callback f)
{
	write_ = f;
	cur_.offset += write_struct<header>();
}

inline ptr packer::new_literal(string_view arcname)
{
	impl_->m.update(arcname.data(), arcname.size()) = int(impl_->v.size());
	return {};
}

void packer::add_directory(string_view arcname, ftime mtime)
{
	impl_->v.push_back(
	    { { new_literal(arcname) }, {}, int(ftype::is_directory), mtime });
}

void packer::add_symlink(string_view arcname, ftime mtime, string_view target)
{
	auto start = cur_;
	cur_.offset += write_buffer(target.data(), target.size());
	impl_->v.push_back({ { new_literal(arcname) },
	                     {},  // hash it
	                     int(ftype::is_symlink),
	                     mtime,
	                     start,
	                     cur_ });
}

void packer::add_regular_file(string_view arcname, ftime mtime,
                              refill_callback f, feature feat)
{
	auto start = cur_;

	for (error_code ec;;)
	{
		auto n = f(impl_->buf.get(), impl::bufsize, ec);
		if (ec)
			throw std::system_error{ ec };
		else if (n == 0)
			break;

		cur_.offset += write_buffer(impl_->buf.get(), n);
	}

	impl_->v.push_back({ { new_literal(arcname) },
	                     {},  // hash it
	                     ftype::is_regular_file | feat,
	                     mtime,
	                     start,
	                     cur_ });
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
	write_struct(cur_);
}

}
