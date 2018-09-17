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
