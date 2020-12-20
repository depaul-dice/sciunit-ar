#include "doctest.h"
#include "testdata.h"

#include <lip/lip.h>
#include <vvpkg/c_file_funcs.h>
#include <vvpkg/fd_funcs.h>
#include <stdex/defer.h>

#ifdef _WIN32
#define U(s) L##s
#else
#define U(s) s
#endif

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
		               "../tmp", 0, 0, 0, 0);
		pk.add_directory("tmp", lip::archive_clock::now(), 0, 0, 0, 0);
		pk.add_symlink("second", lip::archive_clock::now(), "first", 0, 0, 0, 0);
		pk.add_regular_file(
		    "first", lip::archive_clock::now(), 0, 0, 0, 0,
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

	SUBCASE("real files")
	{
		char fn[] = "lip__test_index.tmp";
		std::unique_ptr<FILE, vvpkg::c_file_deleter> fp(
		    vvpkg::xfopen(fn, "wb"));
		lip::archive(vvpkg::to_c_file(fp.get()), U("3rdparty"));
		fp.reset();

		auto fd = vvpkg::xopen_for_read(fn);
		defer(vvpkg::xclose(fd));

		auto idx = lip::index(vvpkg::from_seekable_descriptor(fd),
		                      vvpkg::xfstat(fd).st_size);

		REQUIRE_FALSE(idx.empty());

		auto it = idx.find("nonexistent"_sv);
		REQUIRE(it == idx.end());

		it = idx.find("3rdparty"_sv);
		REQUIRE(it->type() == lip::ftype::is_directory);

		it = idx.find("3rdparty/include"_sv);
		REQUIRE(it->type() == lip::ftype::is_directory);

		it = idx.find("3rdparty/include/cedar"_sv);
		REQUIRE(it->type() == lip::ftype::is_directory);

		it = idx.find("3rdparty/include/cedar/COPYING"_sv);
		REQUIRE(it->type() == lip::ftype::is_regular_file);
		REQUIRE(it->size() == 1311);
		REQUIRE_FALSE(it->is_executable());

		::remove(fn);
	}
}
