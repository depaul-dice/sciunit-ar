#include "doctest.h"

#include <stdex/hashlib.h>
#include <ostream>

using namespace stdex::literals;

TEST_CASE("blake2b_224")
{
	stdex::hashlib::blake2b_224 h;
	REQUIRE(h.hexdigest() ==
	        "836cc68931c2e4e3e838602eca1902591d216837bafddfe6f0c8cb07"_sv);

	h.update("Hello world\n");
	REQUIRE(h.hexdigest() ==
	        "60397f9d4e828346773896b62a6ccc4e6019f402e887103834b1cf14"_sv);
}
