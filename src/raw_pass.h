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

#ifndef _LIP_SRC_RAW__PASS_H
#define _LIP_SRC_RAW__PASS_H

#include "io_pass.h"

#include <lip/lip.h>

namespace lip
{
namespace io
{

template <class Hasher>
class raw_output_pass
{
public:
	template <class F>
	avail make_available(F&& f, error_code& ec)
	{
		auto n = std::forward<F>(f)(buf_, sizeof(buf_), ec);
		h_.update(buf_, n);
		return { buf_, n };
	}

	finfo stat() const { return { { {}, h_.digest() } }; }

private:
	char buf_[65536];
	Hasher h_;
};

class raw_regional_input_pass
{
public:
	raw_regional_input_pass(int64_t where, int64_t end) noexcept
	    : where_(where), end_(end)
	{
	}

	template <class F>
	avail make_available(F&& f, error_code& ec)
	{
		auto x = (std::min)(int64_t(sizeof(buf_)), end_ - where_);
		auto n = std::forward<F>(f)(buf_, size_t(x), where_);
		if (n != size_t(x))
			ec.assign(errno, std::system_category());
		where_ += int64_t(n);
		return { buf_, n };
	}

private:
	char buf_[65536];
	int64_t where_, end_;
};

}
}

#endif
