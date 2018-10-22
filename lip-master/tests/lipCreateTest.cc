#include "doctest.h"
#include "testdata.h"

#include <lip/lip.h>

#include <new>
#include <string.h>
#include <stdex/hashlib.h>
#include <stdio.h>
#include <vvpkg/c_file_funcs.h>

#include <FileSystem/File.h>

using namespace stdex::literals;
using stdex::hashlib::hexlify;
using namespace std;
using namespace lip;
// TODO::move helper functions below here and above the first test case into a
// place to be shared
bool fileExists(const char* filePath)
{
	bool retval = false;
	// This checks if the file exists, if stat is succesfful then = 0 is
	// returned. the FileStatus is discarded
	if (fopen(filePath, "r") != 0)
	{
		retval = true;
	}
	return retval;
};

#define testDataLIP1Root "./tests/testData/Lip1"
#define testDataLIP1Output "./tests/testData/Lip1.lip"

// TODO:: write into the CMakeLists.txt the commands to copy the necessary test
// directories from the src to the binary automatically.

// This tests the ability to create files with textOnly objects with no
// directories and no symlinks included to verify this it will also include an
// unpack and a compare to origonal sourceMaterial
TEST_CASE("lipCreate,TextFilesOnly,NoDIR,NoSymlink")
{

	WHEN("Verifying TestFiles")
	{
		REQUIRE(
		    fileExists("./tests/testData/Lip1/TextDocumentEqual.txt"));
		REQUIRE(fileExists("./tests/testData/Lip1/"
		                   "TextDocumentMatchedButDiffContent.txt"));
		REQUIRE(fileExists(
		    "./tests/testData/Lip1/TextDocumentNotMatched.txt"));
	}

	WHEN("Creating LIP")
	{
		FILE* fileHandle = fopen(testDataLIP1Output, "w");

		REQUIRE(fileHandle != nullptr);

		lip::archive(vvpkg::to_c_file(fileHandle), testDataLIP1Root);

		fclose(fileHandle);
		REQUIRE(fileExists(testDataLIP1Output));

		
	}


	WHEN("Reading LIP Index")
	{ 
		LIP t(testDataLIP1Output);

		LIP::Index* index = t.getIndex();

		REQUIRE(index->getIndexSize() == 4);
	}

	WHEN("Fetching File") 
	{ 
		//lip::getFile()

	}

	WHEN("Unpacking LIP")
	{


	}
}
