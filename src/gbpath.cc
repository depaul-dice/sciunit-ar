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
#include "gbconverter.h"

#include <vector>
#include <algorithm>
#include <assert.h>

namespace lip
{

struct gbpath::impl
{
	std::vector<char> name;
	std::shared_ptr<gbfromsys> gb = std::make_shared<gbfromsys>();
};

gbpath::gbpath(char_type const* s) : impl_(new impl)
{
	if (auto len = std::char_traits<char_type>::length(s))
	{
		impl_->name.resize(4 * len);
		auto nbytes = impl_->gb->convert(s, len, impl_->name.data(),
		                                 impl_->name.size());
		impl_->name.resize(nbytes);
	}
}

gbpath::gbpath(gbpath const& other) : impl_(new impl(*other.impl_))
{
}

gbpath& gbpath::operator=(gbpath const& other)
{
	auto temp = other;
	std::swap(temp, *this);
	return *this;
}

gbpath::gbpath(gbpath&&) noexcept = default;
gbpath& gbpath::operator=(gbpath&&) noexcept = default;
gbpath::~gbpath() = default;

auto gbpath::friendly_name() const noexcept -> string_view
{
	return { impl_->name.data(), impl_->name.size() };
}

void gbpath::push_back(char_type const* s)
{
	auto len = std::char_traits<char_type>::length(s);
	assert(len != 0);
	auto buflen = 4 * len;

	impl_->name.push_back('/');
	auto pfxlen = impl_->name.size();
	impl_->name.resize(pfxlen + buflen);
	auto p = impl_->name.data() + pfxlen;
	auto nbytes = impl_->gb->convert(s, len, p, buflen);
	impl_->name.resize(pfxlen + nbytes);
}

template <class Rg, class T>
void preserve_until_last(Rg& rg, T v)
{
	auto it = std::find(rg.rbegin(), rg.rend(), v);
	rg.erase(it.base() - (it != rg.rend()), rg.end());
}

void gbpath::pop_back()
{
	preserve_until_last(impl_->name, '/');
}
}
