#include "doctest.h"
#include "testdata.h"

#include <lip/lip.h>

#include <stdex/string_view.h>

using namespace stdex::literals;

TEST_CASE("packer")
{
	std::string s;
	lip::packer pk;

	pk.start([&](char const* p, size_t sz) {
		s.append(p, sz);
		return sz;
	});

	REQUIRE(s.size() == 8);
	REQUIRE(s == "LIP\0\x33\xec\x08\00"_sv);

	pk.finish();

	REQUIRE(s.size() == 24);
	REQUIRE(s[8] == s[16]);
}
