#include "doctest.h"

#include <lip/lip.h>
#include <ostream>

using namespace stdex::literals;

#ifdef _WIN32
#define U(s) L##s
#else
#define U(s) s
#endif

TEST_CASE("gbpath")
{
	lip::gbpath a = U("");
	REQUIRE(a.friendly_name().empty());

	a = U("new");
	REQUIRE(a.friendly_name() == "new"_sv);

	auto b = a;
	REQUIRE(a.friendly_name() == b.friendly_name());

	a.push_back(U("my pace"));
	REQUIRE(a.friendly_name() == "new/my pace"_sv);

	a.pop_back();
	REQUIRE(a.friendly_name() == b.friendly_name());

	a.pop_back();
	REQUIRE(a.friendly_name().empty());

	a.pop_back();
	REQUIRE(a.friendly_name().empty());
}

TEST_CASE("unicode path")
{
#ifdef _WIN32
	lip::gbpath a = (wchar_t const*)u"\u1e31";
	REQUIRE(a.friendly_name() == "\x81\x35\xf3\x33"_sv);

	a.push_back((wchar_t const*)u"\u6d4b\u8bd5");
	REQUIRE(a.friendly_name() == "\x81\x35\xf3\x33/\xb2\xe2\xca\xd4"_sv);
#endif
}
