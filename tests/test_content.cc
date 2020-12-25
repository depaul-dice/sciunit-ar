#include "doctest.h"
#include "testdata.h"

#include <lip/lip.h>

using namespace stdex::literals;

TEST_CASE("content")
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

	SUBCASE("directory")
	{
		pk.add_directory("tmp", lip::archive_clock::now(), 0, 0, 0, 0);
		pk.finish();
		auto idx = lip::index(f, int64_t(s.size()), nullptr);

		auto s = lip::content(f).retrieve(idx["tmp"]);
		REQUIRE(s.empty());
	}

	SUBCASE("short content")
	{
		pk.add_symlink("link name", lip::archive_clock::now(),
		               "target", 0, 0, 0, 0);
		pk.finish();
		auto idx = lip::index(f, int64_t(s.size()), nullptr);

		auto s = lip::content(f).retrieve(idx["link name"]);
		REQUIRE(s == "target");
	}

	SUBCASE("long content")
	{
		randombuf input(70000);
		pk.add_regular_file(
		    "foo", lip::archive_clock::now(), 0, 0, 0, 0,
		    [&](char* p, size_t sz, std::error_code&) {
			    return static_cast<size_t>(
			        input.sgetn(p, std::streamsize(sz)));
		    });
		pk.finish();
		auto idx = lip::index(f, int64_t(s.size()), nullptr);

		auto fc = idx["foo"];
		REQUIRE_THROWS_AS(lip::content(f).retrieve(fc),
		                  std::invalid_argument);

		size_t total;
		lip::content(f).copy(fc, [&](char const* p, size_t sz) {
			total += sz;
			return sz;
		});
		REQUIRE(total == 70000);
	}
}
