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

namespace lip
{

enum class ftype
{
	is_regular_file = 0,
	is_directory = 1,
	is_symlink = 2,
};

enum class feature
{
	// if compressed, the digest field contains the original size
	// if not, the digest field contains a blake2b-224 hash
	lz4_compressed = 0x10,
	executable = 0x100,
	readonly = 0x200,
};

constexpr auto operator|(feature a, feature b)
{
	return feature(int(a) | int(b));
}

struct ptr
{
	int64_t offset;
};

struct descriptor
{
	ptr name;
	uint32_t flag;
	uint32_t digest[7];
	int64_t mtime;
	ptr begin;
	ptr end;
};

static_assert(sizeof(descriptor) == 64, "unsupported");

struct header
{
	char magic[4] = "LIP";
	int32_t epoch = 584755;
};

}

#endif
