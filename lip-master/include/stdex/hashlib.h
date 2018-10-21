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

#ifndef _STDEX_HASHLIB_H
#define _STDEX_HASHLIB_H

#if defined(HASHLIB_HAS_OPENSSL)
#include <openssl/sha.h>
#include <openssl/md5.h>
#endif

#include <array>
#include <string>
#include <algorithm>
#include <iterator>
#include <cstring>
#include <stdexcept>
#include <iosfwd>
#include <stdex/string_view.h>

#include "blake2.h"

namespace stdex
{
namespace hashlib
{

namespace detail
{

#if defined(HASHLIB_HAS_OPENSSL)
struct sha1_provider
{
	using context_type = SHA_CTX;
	static constexpr size_t digest_size = SHA_DIGEST_LENGTH;
	static constexpr size_t block_size = SHA_CBLOCK;

	static void init(context_type* ctx) { SHA1_Init(ctx); }

	static void update(context_type* ctx, void const* data, size_t len)
	{
		SHA1_Update(ctx, data, len);
	}

	static void final(unsigned char* md, context_type* ctx)
	{
		SHA1_Final(md, ctx);
	}
};

struct sha256_provider
{
	using context_type = SHA256_CTX;
	static constexpr size_t digest_size = SHA256_DIGEST_LENGTH;
	static constexpr size_t block_size = SHA256_CBLOCK;

	static void init(context_type* ctx) { SHA256_Init(ctx); }

	static void update(context_type* ctx, void const* data, size_t len)
	{
		SHA256_Update(ctx, data, len);
	}

	static void final(unsigned char* md, context_type* ctx)
	{
		SHA256_Final(md, ctx);
	}
};

struct sha512_provider
{
	using context_type = SHA512_CTX;
	static constexpr size_t digest_size = SHA512_DIGEST_LENGTH;
	static constexpr size_t block_size = SHA512_CBLOCK;

	static void init(context_type* ctx) { SHA512_Init(ctx); }

	static void update(context_type* ctx, void const* data, size_t len)
	{
		SHA512_Update(ctx, data, len);
	}

	static void final(unsigned char* md, context_type* ctx)
	{
		SHA512_Final(md, ctx);
	}
};

struct md5_provider
{
	using context_type = MD5_CTX;
	static constexpr size_t digest_size = MD5_DIGEST_LENGTH;
	static constexpr size_t block_size = MD5_CBLOCK;

	static void init(context_type* ctx) { MD5_Init(ctx); }

	static void update(context_type* ctx, void const* data, size_t len)
	{
		MD5_Update(ctx, data, len);
	}

	static void final(unsigned char* md, context_type* ctx)
	{
		MD5_Final(md, ctx);
	}
};
#endif

struct blake2b_224_provider
{
	using context_type = blake2b_state;
	static constexpr size_t digest_size = 28;
	static constexpr size_t block_size = BLAKE2B_BLOCKBYTES;

	static void init(context_type* ctx) { blake2b_init(ctx, digest_size); }

	static void update(context_type* ctx, void const* data, size_t len)
	{
		blake2b_update(ctx, data, len);
	}

	static void final(unsigned char* md, context_type* ctx)
	{
		blake2b_final(ctx, md, digest_size);
	}
};

template <size_t N, typename OutIt>
inline OutIt hexlify_to(std::array<unsigned char, N> const& md, OutIt it)
{
	auto half_to_hex = [](int c)
	{
		return char((c > 9) ? c + 'a' - 10 : c + '0');
	};

	std::for_each(md.begin(), md.end(), [&](unsigned char c)
	    {
		*it = half_to_hex((c >> 4) & 0xf);
		++it;
		*it = half_to_hex(c & 0xf);
		++it;
	    });

	return it;
}

// only accept lower case hexadecimal
template <size_t N, typename OutIt>
inline OutIt unhexlify_to(stdex::string_view hs, OutIt first)
{
	auto hex_to_half = [](char c) -> int
	{
		if ('0' <= c and c <= '9')
			return c - '0';
		if ('a' <= c and c <= 'f')
			return c - 'a' + 10;

		throw std::invalid_argument("not a hexadecimal");
	};

	if (hs.size() != N * 2)
		throw std::invalid_argument("unexpected hexadecimal length");

	auto it = begin(hs);

#if !(defined(_MSC_VER) && _MSC_VER < 1800)
	return
#endif
		std::generate_n(first, N, [&]() -> int
	    {
		auto v = hex_to_half(*it) << 4;
		++it;
		v ^= hex_to_half(*it);
		++it;

		return v;
	    });
#if defined(_MSC_VER) && _MSC_VER < 1800
	return std::next(first, N);
#endif
}

}

template <size_t N>
inline std::string hexlify(std::array<unsigned char, N> const& md)
{
	std::string s;
	s.resize(N * 2);
	detail::hexlify_to(md, begin(s));

	return s;
}

template <size_t N>
inline auto unhexlify(stdex::string_view hs) -> std::array<unsigned char, N>
{
	std::array<unsigned char, N> md;
	detail::unhexlify_to<N>(hs, md.begin());

	return md;
}

template <typename HashProvider>
struct hasher
{
	static constexpr size_t digest_size = HashProvider::digest_size;
	static constexpr size_t block_size = HashProvider::block_size;

	using context_type = typename HashProvider::context_type;
	using digest_type = std::array<unsigned char, digest_size>;

	hasher() { HashProvider::init(&ctx_); }

	explicit hasher(char const* s)
	{
		HashProvider::init(&ctx_);
		update(s);
	}

	explicit hasher(char const* s, size_t n)
	{
		HashProvider::init(&ctx_);
		update(s, n);
	}

	template <typename StringLike>
	explicit hasher(StringLike const& bytes)
	{
		HashProvider::init(&ctx_);
		update(bytes);
	}

	void update(char const* s) { update(s, strlen(s)); }

	void update(char const* s, size_t n)
	{
		HashProvider::update(&ctx_, s, n);
	}

	template <typename StringLike>
	void update(StringLike const& bytes)
	{
		update(bytes.data(), bytes.size());
	}

	auto digest() const -> digest_type
	{
		digest_type md;
		auto tmp_ctx = ctx_;

		HashProvider::final(md.data(), &tmp_ctx);

		return md;
	}

	auto hexdigest() const -> std::string { return hexlify(digest()); }

private:
	context_type ctx_;
};

template <typename HashProvider>
inline bool operator==(hasher<HashProvider> const& a,
                       hasher<HashProvider> const& b)
{
	return a.digest() == b.digest();
}

template <typename HashProvider>
inline bool operator!=(hasher<HashProvider> const& a,
                       hasher<HashProvider> const& b)
{
	return !(a == b);
}

template <typename CharT, typename Traits, typename HashProvider>
inline auto operator<<(std::basic_ostream<CharT, Traits>& out,
                       hasher<HashProvider> const& h) -> decltype(out)
{
	typedef stdex::basic_string_view<CharT, Traits> inserter_type;

	std::array<CharT, hasher<HashProvider>::digest_size * 2> buf;
	detail::hexlify_to(h.digest(), buf.begin());

	return out << inserter_type(buf.data(), buf.size());
}

#if defined(HASHLIB_HAS_OPENSSL)
using md5 = hasher<detail::md5_provider>;
using sha1 = hasher<detail::sha1_provider>;
using sha256 = hasher<detail::sha256_provider>;
using sha512 = hasher<detail::sha512_provider>;
#endif
using blake2b_224 = hasher<detail::blake2b_224_provider>;

}
}

#endif
