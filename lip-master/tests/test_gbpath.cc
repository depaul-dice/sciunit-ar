#include "doctest.h"

#include <lip/lip.h>
#include <ostream>

using namespace stdex::literals;

#ifdef _WIN32
#define U(s) L##s
#define LU(s) ((wchar_t const*)u##s)
#else
#include <locale.h>
#define U(s) s
#define LU(s) u8##s
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

#ifdef _WIN32
TEST_CASE("unicode path")
#else
TEST_CASE("unicode path" *
          doctest::skip(setlocale(LC_CTYPE, "en_US.UTF-8") == nullptr))
#endif
{
	lip::gbpath a = LU("\u1e31");
	REQUIRE(a.friendly_name() == "\x81\x35\xf3\x33"_sv);

	a.push_back(LU("\u6d4b\u8bd5"));
	REQUIRE(a.friendly_name() == "\x81\x35\xf3\x33/\xb2\xe2\xca\xd4"_sv);
}
