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

#ifndef _LIP_SRC_LZ4__PASS_H
#define _LIP_SRC_LZ4__PASS_H

#include "io_pass.h"

#include <lip/lip.h>
#include <lz4.h>
#include <assert.h>

namespace lip
{
namespace io
{

class lz4_output_pass
{
public:
	lz4_output_pass() noexcept { LZ4_resetStream(handle_); }

	template <class F>
	avail make_available(F&& f, error_code& ec)
	{
		if (auto n = std::forward<F>(f)(buf_[i_], reqsize, ec))
		{
			total_ += n;
			auto block_size = LZ4_compress_fast_continue(
			    handle_, buf_[i_], obuf_ + sizeof(int), int(n),
			    int(sizeof(obuf_) - sizeof(int)), 1);
			assert(block_size != 0);
			::new (obuf_) int{ block_size };
			i_ = !i_;
			return { obuf_, sizeof(int) + size_t(block_size) };
		}
		else
			return { obuf_, n };
	}

	finfo stat() const
	{
		finfo info;
		info.flag_ = 0;
		info.reserved = 0;
		info.sizeopt = total_;
		return info;
	}

private:
	static constexpr size_t reqsize = 65536;

	LZ4_stream_t handle_[1];
	char buf_[2][reqsize];
	alignas(int) char obuf_[sizeof(int) + LZ4_COMPRESSBOUND(reqsize)];
	size_t i_ = 0;
	int64_t total_ = 0;
};
}
}

#endif
