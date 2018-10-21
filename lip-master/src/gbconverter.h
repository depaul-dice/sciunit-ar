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

#ifndef LIP_GBCONVERTER_H
#define LIP_GBCONVERTER_H

#include <stdexcept>
#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#include <stringapiset.h>
#else
#include <iconv.h>
#include <langinfo.h>
#endif

namespace lip
{
class gbconverter
{
public:
	gbconverter() = default;
	gbconverter(gbconverter const&) = delete;
	gbconverter& operator=(gbconverter const&) = delete;

#ifdef _WIN32
	size_t convert(wchar_t const* from, size_t len, char* to,
	               size_t buflen)
	{
		if (int n = WideCharToMultiByte(54936, WC_ERR_INVALID_CHARS,
		                                from, int(len), to,
		                                int(buflen), nullptr, false))
			return n;
		else
			switch (GetLastError())
			{
			case ERROR_INSUFFICIENT_BUFFER:
				throw std::out_of_range{ "convert size" };
			case ERROR_NO_UNICODE_TRANSLATION:
				throw std::invalid_argument{ "convert" };
			default: __assume(0);
			}
	}
#else
	size_t convert(char const* from, size_t len, char* to, size_t buflen)
	{

		static auto cd = [] {
			auto d = iconv_open("gb18030", nl_langinfo(CODESET));
			if (d == (iconv_t)-1)
				throw std::system_error{
					errno, std::system_category()
				};
			return d;
		}();
		cd_ = cd;

		errno = 0;
		auto left = buflen;
		auto p = const_cast<char*>(from);
		if (iconv(cd_, &p, &len, &to, &left) == 0)
		{
			assert(len == 0);
			return buflen - left;
		}
		else if (errno == E2BIG)
			throw std::out_of_range{ "convert size" };
		else
			throw std::invalid_argument{ "convert" };
	}

	~gbconverter()
	{
		if (cd_ != (iconv_t)-1)
			iconv_close(cd_);
	}

private:
	iconv_t cd_ = (iconv_t)-1;
#endif
};

static gbconverter gb;
}

#endif
