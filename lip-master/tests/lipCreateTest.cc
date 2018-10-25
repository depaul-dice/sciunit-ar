#include "doctest.h"
#include "testdata.h"

#include <lip/lip.h>

#include <new>
#include <string.h>
#include <stdex/hashlib.h>
#include <stdio.h>
#include <vvpkg/c_file_funcs.h>

#include <FileSystem/File.h>
#include <unistd.h>

using namespace stdex::literals;
using stdex::hashlib::hexlify;
using namespace std;
using namespace lip;

// TODO::move helper functions below here and above the first test case into a
// place to be shared
bool fileExists(const char* filePath)
{
	bool retval = false;
	FILE* tmp = fopen(filePath, "r");
	if (tmp != 0)
	{
		retval = true;
		fclose(tmp);
	}
	return retval;
};

char cCurrentPath[FILENAME_MAX];

void PrintCurrentPath()
{
	getcwd(cCurrentPath, sizeof(cCurrentPath));
	
	printf("The current working directory is %s\n",cCurrentPath);
}

#define testDataLIP1Root "./tests/testData/Lip1"
#define testDataLIP1Output "./tests/testData/Lip1.lip"
#define testDataLIP2Output "./tests/testData/Lip2.lip"
#define testDataUnpackDirectory "./tests/testData/Output"

// TODO:: write into the CMakeLists.txt the commands to copy the necessary test
// directories from the src to the binary automatically.

// This tests the ability to create files with textOnly objects with no
// directories and no symlinks included to verify this it will also include an
// unpack and a compare to origonal sourceMaterial
TEST_CASE("lipCreate,TextFilesOnly,NoSubDIR,NoSymlink")
{
	PrintCurrentPath();
	//verifying test files

		REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentEqual.txt"));

		REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentMatchedButDiffContent.txt"));

		REQUIRE(fileExists("./tests/testData/Lip1/TextDocumentNotMatched.txt"));
	

	//creating LIP
	
		FILE* fileHandle = fopen(testDataLIP1Output, "w");

		REQUIRE(fileHandle != nullptr);

		lip::archive(vvpkg::to_c_file(fileHandle), testDataLIP1Root);

		fclose(fileHandle);
		REQUIRE(fileExists(testDataLIP1Output));
		
		//reading lip index
		LIP t(testDataLIP1Output);

		LIP::Index* index = t.getIndex();

		REQUIRE(index->getIndexSize() == 4);

		index->dumpIndex();

		fcard* temp = index->getNext();
		fcard* check = temp;

		temp = index->getNext();
		temp = index->getNext();
		temp = index->getNext();
	    temp = index->getNext();
//		REQUIRE(temp == check);

		//unpacking LIP
	    t.UnpackAt(testDataUnpackDirectory);

		REQUIRE(fileExists("./tests/testData/Output/Lip1/TextDocumentEqual.txt"));

	    REQUIRE(fileExists("./tests/testData/Output/Lip1/TextDocumentMatchedButDiffContent.txt"));

	    REQUIRE(fileExists("./tests/testData/Output/Lip1/TextDocumentNotMatched.txt"));

		printf("\nPack/Unpack TestPassed!!!\n");

		//t.UnpackAt()
}

TEST_CASE("lipDIFF,TextFilesOnly,NoSubDIR,NoSymlink")
{
	PrintCurrentPath();
	// verifying test files

	REQUIRE(fileExists("./tests/testData/Lip2/TextDocumentEqual.txt"));
	REQUIRE(fileExists("./tests/testData/Lip2/TextDocumentMatchedButDiffContent.txt"));
	REQUIRE(fileExists("./tests/testData/Lip2/TextDocumentNoMatch"));

	// creating LIP

	FILE* fileHandle = fopen(testDataLIP2Output, "w");

	REQUIRE(fileHandle != nullptr);

	lip::archive(vvpkg::to_c_file(fileHandle), testDataLIP2Root);

	fclose(fileHandle);
	REQUIRE(fileExists(testDataLIP2Output));

	// reading lip index
	LIP lip2(testDataLIP2Output);

	LIP::Index* index = t.getIndex();

	REQUIRE(index->getIndexSize() == 4);

	index->dumpIndex();

	fcard* temp = index->getNext();
	fcard* check = temp;

	temp = index->getNext();
	temp = index->getNext();
	temp = index->getNext();
	temp = index->getNext();
	//		REQUIRE(temp == check);

	// unpacking LIP
	t.UnpackAt(testDataUnpackDirectory);

	REQUIRE(fileExists("./tests/testData/Output/Lip2/TextDocumentEqual.txt"));

	REQUIRE(fileExists("./tests/testData/Output/Lip2/"
	                   "TextDocumentMatchedButDiffContent.txt"));

	REQUIRE(fileExists(
	    "./tests/testData/Output/Lip2/TextDocumentNoMatch.txt"));
	
	//DIFF

	lip1(testDataLIP1Output);

	lip1.diff(lip2);

}
