#include "doctest.h"
#include "testdata.h"

#include <lip/lip.h>

using namespace stdex::literals;

TEST_CASE("index")
{
	std::string s;
	lip::packer pk;

	pk.start([&](char const* p, size_t sz) {
		s.append(p, sz);
		return sz;
	});

	auto f = [&](char* p, size_t sz, int64_t from) {
		return s.copy(p, sz, size_t(from));
	};

	SUBCASE("empty")
	{
		pk.finish();
		auto idx = lip::index(f, int64_t(s.size()));

		REQUIRE(idx.empty());
		REQUIRE(idx.begin() == idx.end());
	}

	SUBCASE("with content")
	{
		randombuf input(1000);

		pk.add_symlink("tmp/self", lip::archive_clock::now(),
		               "../tmp");
		pk.add_directory("tmp", lip::archive_clock::now());
		pk.add_symlink("second", lip::archive_clock::now(), "first");
		pk.add_regular_file(
		    "first", lip::archive_clock::now(),
		    [&](char* p, size_t sz, std::error_code&) {
			    return static_cast<size_t>(
			        input.sgetn(p, std::streamsize(sz)));
		    });
		pk.finish();
		auto const idx = lip::index(f, int64_t(s.size()));

		REQUIRE_FALSE(idx.empty());
		REQUIRE(idx.size() == 4);
		REQUIRE(idx[0].arcname == "first"_sv);

		REQUIRE(idx.find("nonexistent"_sv) == idx.end());
		REQUIRE(idx.find("tmp/"_sv) == idx.end());

		auto it = idx.find("tmp"_sv);
		REQUIRE(it->type() == lip::ftype::is_directory);

		it = idx.find("tmp/self"_sv);
		REQUIRE(it->type() == lip::ftype::is_symlink);
		REQUIRE(it->size() == 6);

		it = idx.find("first"_sv);
		REQUIRE(it->type() == lip::ftype::is_regular_file);
		REQUIRE(it->size() == 1000);
		REQUIRE_FALSE(it->is_executable());

		it = idx.find("second"_sv);
		REQUIRE(it->type() == lip::ftype::is_symlink);
		REQUIRE(it->size() == 5);
	}
}
