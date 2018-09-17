#include "doctest.h"
#include "testdata.h"

#include <lip/lip.h>

#include <new>
#include <string.h>
#include <stdex/hashlib.h>

using namespace stdex::literals;
using stdex::hashlib::hexlify;

TEST_CASE("packer")
{
	std::string s;
	lip::packer pk;

	auto f = [&](char const* p, size_t sz) {
		s.append(p, sz);
		return sz;
	};

	pk.start(f);

	REQUIRE(s.size() == 8);
	REQUIRE(s == "LIP\0\x33\xec\x08\00"_sv);

	WHEN("adding content")
	{
		pk.add_symlink("tmp/self", lip::archive_clock::now(),
		               "../tmp");
		pk.add_directory("tmp", lip::archive_clock::now());
		pk.finish();

		auto cs = sizeof(lip::fcard);
		REQUIRE(s.size() == (32 + cs * 2 + 16));

		std::unique_ptr<char[]> p{ new char[s.size()] };
		auto dir = ::new (p.get() + 32) lip::fcard;
		auto sym = ::new (p.get() + 32 + cs) lip::fcard;
		auto indexp = ::new (p.get() + s.size() - 16) lip::ptr;
		auto bssp = ::new (p.get() + s.size() - 8) lip::ptr;
		memcpy(p.get(), s.data(), s.size());

		dir->name.adjust(p.get());
		dir->begin.adjust(p.get());
		dir->end.adjust(p.get());
		sym->name.adjust(p.get());
		sym->begin.adjust(p.get());
		sym->end.adjust(p.get());
		indexp->adjust(p.get());
		bssp->adjust(p.get());

		REQUIRE(indexp->pointer_to<lip::fcard>() == dir);
		REQUIRE(bssp->pointer_to<char>() == (p.get() + 8));

		auto contentof = [](lip::fcard const& fc) {
			return std::string(fc.begin.pointer_to<char>(),
			                   fc.end.pointer_to<char>());
		};

		REQUIRE(dir->arcname == "tmp"_sv);
		REQUIRE(sym->arcname == "tmp/self"_sv);
		REQUIRE(sym->mtime <= dir->mtime);
		REQUIRE(sym->size() == 6);
		REQUIRE(contentof(*sym) == "../tmp"_sv);
		REQUIRE(dir->size() == 0);
		REQUIRE(contentof(*dir).empty());
		REQUIRE(sym->digest != dir->digest);
		REQUIRE(hexlify(sym->digest) == "12e0296f8b9dba8f7f0be0614c67d"
		                                "108c160cba9ff496e256d98b1c2");
		REQUIRE(sym->type() == lip::ftype::is_symlink);
		REQUIRE(dir->type() == lip::ftype::is_directory);
		REQUIRE_FALSE(dir->is_executable());
	}

	WHEN("adding file")
	{
		randombuf input(70000);

		pk.add_symlink("second", lip::archive_clock::now(), "first");
		pk.add_regular_file(
		    "first", lip::archive_clock::now(),
		    [&](char* p, size_t sz, std::error_code&) {
			    return static_cast<size_t>(
			        input.sgetn(p, std::streamsize(sz)));
		    },
		    lip::feature::executable);
		pk.finish();

		auto cs = sizeof(lip::fcard);
		REQUIRE(s.size() == (70032 + cs * 2 + 16));

		lip::ptr last[2];
		memcpy(last, &*s.begin() + s.size() - 16, 16);
		REQUIRE(last[0].offset == 70032);
		REQUIRE(last[1].offset == 70008);

		std::unique_ptr<char[]> p{
			new char[s.size() - size_t(last[1].offset)]
		};
		auto indexp = last[0];
		indexp.adjust(p.get() - last[1].offset);

		::new (indexp.pointer_to<lip::fcard>()) lip::fcard;
		::new (indexp.pointer_to<lip::fcard>() + 1) lip::fcard;
		memcpy(p.get(), s.data() + last[1].offset,
		       s.size() - size_t(last[0].offset));
		auto& first = *indexp.pointer_to<lip::fcard>();
		first.name.adjust(p.get() - last[1].offset);

		REQUIRE(first.arcname == "first"_sv);
		REQUIRE(first.type() == lip::ftype::is_regular_file);
		REQUIRE(first.is_executable());
		REQUIRE(first.size() == 70000);
	}

	WHEN("movable")
	{
		auto pk2 = std::move(pk);
		pk2.add_directory("tmp", lip::archive_clock::now());

		THEN("move constructible")
		{
			pk2.finish();
			REQUIRE(s.size() == 32 + 64);
		}

		THEN("move assignable")
		{
			pk = std::move(pk2);
			pk.finish();
			REQUIRE(s.size() == 32 + 64);
		}
	}

	WHEN("empty")
	{
		pk.finish();

		REQUIRE(s.size() == 24);
		REQUIRE(s[8] == s[16]);
	}
}
