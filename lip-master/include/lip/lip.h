/*-
 * Copyright (c) 2018 Zhihao Yuan.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _LIP_LIP_H
#define _LIP_LIP_H

#include <stdint.h>
#include <array>
#include <chrono>
#include <memory>
#include <cerrno>
#include <system_error>

#include <stdex/functional.h>
#include <stdex/string_view.h>

#include "../FileSystem/File.h"
#include <assert.h>
#include <list>

namespace lip
{

namespace chrono = std::chrono;

using stdex::string_view;
using std::error_code;
using read_callback = stdex::signature<size_t(char*, size_t)>;
using write_callback = stdex::signature<size_t(char const*, size_t)>;
using refill_callback = stdex::signature<size_t(char*, size_t, error_code&)>;

enum class ftype
{
	is_regular_file = 0,
	is_directory = 1,
	is_symlink = 2,
};

enum class feature
{
	// if compressed, the sizeopt field contains the original size
	// if not, the digest field contains a blake2b-224 hash
	lz4_compressed = 0x10,
	executable = 0x100,
	readonly = 0x200,  // unimplemented
};

constexpr auto operator|(feature a, feature b)
{
	return feature(int(a) | int(b));
}

struct archive_clock
{
	using duration = chrono::duration<int64_t, std::ratio<1, 10000000>>;
	using rep = duration::rep;
	using period = duration::period;
	using time_point = chrono::time_point<archive_clock, duration>;

	static constexpr bool is_steady = false;

	static time_point now() noexcept;

	template <class T>
	static time_point from(T const&) noexcept;
};

using ftime = archive_clock::time_point;

struct ptr
{
	int64_t offset;

	void adjust(void const* base) &
	{
		auto p = reinterpret_cast<char const*>(base) + offset;
		offset = reinterpret_cast<intptr_t>(p);
	}

	template <class T>
	T* pointer_to() const
	{
		return reinterpret_cast<T*>(offset);
	}

	friend int64_t operator-(ptr x, ptr y) { return x.offset - y.offset; }
};

using fhash = std::array<unsigned char, 28>;

// class fhash
//{
//	unsigned char hash[28];
//};

union finfo
{
	struct
	{
		uint32_t flag;
		fhash digest;
		// unsigned char digest[28];
	};
	struct
	{
		uint32_t flag_;
		uint32_t reserved;
		int64_t sizeopt;
	};
};

struct fcard
{
	union
	{
		ptr name;
		char* arcname;
	};
	finfo info;
	ftime mtime;
	ptr begin;
	ptr end;

	int64_t stored_size() const { return end - begin; }

	int64_t size() const
	{
		if (is_lz4_compressed())
			return info.sizeopt;
		else
			return stored_size();
	}

	ftype type() const { return static_cast<ftype>(info.flag & 0xf); }

	bool isDirectory() const
	{
		return this->type() == ftype::is_directory;
	}

	bool is_lz4_compressed() const
	{
		return (info.flag & int(feature::lz4_compressed)) != 0;
	}

	bool is_executable() const
	{
		return (info.flag & int(feature::executable)) != 0;
	}

	int64_t getSize() { return end - begin; }

	ftime getCreationTime() { return mtime; }

	static bool CheckEquality(fcard& lhs, fcard& rhs)
	{
		// TODO:: expand on this to continue checking various
		// attributes
		bool retval = false;

		if (CheckType(lhs, rhs))
		{
			if (lhs.type() == ftype::is_directory)
			{  // break this into directory equality
				if (CheckName(lhs, rhs))
				{
					retval = true;
				}
			}
			else
			{  // break this into file equality
				if (CheckHash(lhs, rhs))
				{
					retval = true;
				}
			}
		}
		return retval;
	}

private:
	static bool CheckType(fcard& lhs, fcard& rhs) {}

	static bool CheckName(fcard& lhs, fcard& rhs) {}

	static bool CheckHash(fcard& lhs, fcard& rhs)
	{
		// TODO:: must set this from 28 to pull the digest length.
		// because if the lengh of the hash changes it throws
		// everything off
		bool retval = true;
		for (int i = 0; i < 28; i++)
		{
			if (lhs.info.digest[i] != rhs.info.digest[i])
			{
				retval = false;
			}
		}

		return retval;
	}
};

static_assert(sizeof(fcard) == 64, "unsupported");

struct header
{
	char magic[4] = "LIP";
	int32_t epoch = 584755;
};

class packer
{

public:
	packer();
	packer(packer&&) noexcept;
	packer& operator=(packer&&) noexcept;
	~packer();

	void start(write_callback f);
	void add_directory(string_view arcname, ftime);
	void add_symlink(string_view arcname, ftime, string_view target);
	void add_regular_file(string_view arcname, ftime, refill_callback,
	                      feature = {});

	void finish()
	{
		write_bss();
		write_index();
		write_section_pointers();
	}

private:
	struct impl;

	template <class T>
	size_t write_struct(T&& v = {})
	{
		static_assert(std::is_trivially_copyable<
		                  std::remove_reference_t<T>>::value,
		              "not plain");
		return write_buffer(
		    reinterpret_cast<char const*>(std::addressof(v)),
		    sizeof(v));
	}

	size_t write_buffer(char const* p, size_t sz)
	{
		if (write_(p, sz) != sz)
			throw std::system_error{ errno,
				                 std::system_category() };
		return sz;
	}

	void write_bss();
	void write_index();
	void write_section_pointers();

	ptr new_literal(string_view arcname);

	write_callback write_;
	ptr cur_ = {};
	std::unique_ptr<impl> impl_;
};

class gbpath
{
public:
#ifdef _WIN32
	using char_type = wchar_t;
#else
	using char_type = char;
#endif
	using param_type = char_type const*;

	gbpath(param_type);
	gbpath(gbpath const&);
	gbpath& operator=(gbpath const&);
	gbpath(gbpath&&) noexcept;
	gbpath& operator=(gbpath&&) noexcept;
	~gbpath();

	auto friendly_name() const noexcept -> string_view;
	void push_back(param_type);
	void pop_back();

private:
	struct impl;
	std::unique_ptr<impl> impl_;
};

struct archive_options
{
	bool one_level = false;
	feature feat = {};
};

void archive(write_callback, gbpath::param_type src, archive_options = {});

// this is a rewrite to have the equivalent interface but to use
// readOnlyFileHandles instead of normal file handles as we cannot allow the
// lips to be edited so this will eventually just become LIP and the old lip
// class will be deleted once this class is validated
class LIP
{
public:

	class Index
	{
		readOnlyFileHandle* fh;
		fcard* indexPtr;
		uint numCards;

		// TODO:: move the iterator function externally
		fcard* currentPtr;

	public:
		Index() : indexPtr(0), numCards(0), currentPtr(0) {}

		void FillIndex(readOnlyFileHandle * _fh)
		{
			this->fh = _fh;

			// this seeks to the pointer that is under the index
			// that contains the pointer for the top of the index
			fh->Seek(-2l * (long)sizeof(ptr), File::Location::END);

			// finds the offset for the bottom of the index
			int64_t BottomOfIndex = fh->Tell();

			// this gets the pointer to the top of the index
			int64_t indexTopOffset;

			fh->Read(&indexTopOffset, sizeof(ptr));

			// printf("bottom of index at %l and indexTopPtr at
			// %l", BottomOfIndex, indexTopOffset);

			int64_t indexSize = BottomOfIndex - indexTopOffset;

			assert(indexSize % 64 == 0);
			// go to the top of the index so I can read it into a
			// buffer
			fh->Seek(indexTopOffset, File::Location::BEGIN);
		
			// theese lines pull the raw bytes for the index from
			// the file and cast them to an array of fcards that
			// make up the index.
			char* rawIndexBuffer = new char[indexSize];

			fh->Read(rawIndexBuffer, indexSize);

			numCards = indexSize / (long)sizeof(fcard);

			indexPtr = (fcard*)rawIndexBuffer;
		}

		~Index()
		{
			if (indexPtr != nullptr)
				delete indexPtr;
		}

		// returns the number of fcards in the index
		uint getIndexSize() { return numCards; }

		void dumpIndex()
		{
			// This saves the location that the index was on to
			// restore it after the dump just in case the user
			// calls this in the middle of iteration
			fcard* temp = currentPtr;

			currentPtr = indexPtr;

			char name[FILENAME_MAX];

			printf("Beginning Index Dump-------------");
			for (unsigned int i = 0; i < numCards; i++)
			{
				printf("Dumping Fcard-------------\n");
				printf("File size: %ld \n",
				       currentPtr->getSize());

				// TODO:: add debug printing for creation time
				// printf("Creation time %llu \n",
				// currentPtr->getCreationTime());

				// print hash
				printf("File hash = ");
				for (int j = 0; j < 28; j++)
				{
					printf("%02X",
					       currentPtr->info.digest[j]);
				}
				printf("\n");

				// end print hash

				// print name
				fh->Seek(currentPtr->name.offset, File::Location::BEGIN);
				fh->ReadLine(name, FILENAME_MAX);

				printf("Fcard %s\n", name);
				// end print name
				printf("End Card ----------------\n");
				getNext();
			}
			printf("End Index Dump -----------------");
			currentPtr = temp;
		}

		void resetItr() { currentPtr = nullptr; }

		fcard* getNext()
		{
			if (currentPtr == nullptr)
			{
				currentPtr = indexPtr;
			}
			else if ((currentPtr - indexPtr) >= numCards - 1)
			{
				currentPtr = nullptr;
			}
			else
			{
				currentPtr++;
			}
			return currentPtr;
		}

		// TODO:: make a filename buffer class just to add a bit of
		// safety here so a user can know they wont overshoot thier
		// buffer
		void getName(fcard* indexItem, char* filenameBuffer)
		{
			fh->Seek(indexItem->name.offset, File::Location::BEGIN);
			fh->ReadLine(filenameBuffer, FILENAME_MAX);
		}

		char* getFile(fcard* indexOfItemToRetrieve, int64_t& fileSize)
		{
			fileSize = indexOfItemToRetrieve->getSize();
			fh->Seek(indexOfItemToRetrieve->begin.offset,
			        File::Location::BEGIN);
			
			char* fileByteBuffer = new char[fileSize];

			fh->Read(fileByteBuffer, fileSize);
			return fileByteBuffer;
		}
	};

private:

	//File::Handle fh;
	readOnlyFileHandle* fh;
	Index LIPIndex;

public:
	// TODO:: incorporate packing functionality. //LIP's are readonly once
	// archived so the current writer setup makes sense the lip objects are
	// immutable

	// TODO:: incorporate LIP Merging //LIP's don't need to be merged that
	// is done in VVPKG so I don't need to edit that

	//LIPv2();
	LIP(const char* const filePath);
	LIP(readOnlyFileHandle *readOnlyHandle);

	~LIP() {
		if (fh != nullptr)
		{
			fh->Close();
			delete fh;
		}
		
	}

	LIP(const LIP&) = delete;
	LIP operator=(const LIP&) = delete;

	Index* getIndex() { return &LIPIndex; }

	void Unpack() {}

	void UnpackAt(const char* const _filePath)
	{
		// TODO:: optimize the string/char buffer insanity here this is
		// a first pass.

		// TODO:: Zhiaho says lip is sorted in a way that upon
		// iteration you will always visit the parents before any of
		// it's children so I can clean this up substnatially by
		// iterating once and switching behavior based on directory or
		// file

		std::string filePathPrefix = _filePath;
		// this removes a trailing slash if it's on the provided path
		// because I keep the / from the files
		if (filePathPrefix.back() == '/')
		{
			filePathPrefix.pop_back();
		}

		std::string concatenatedFilePath = "";

		std::list<std::string> directoryList;

		// makes base directory
		File::MakeDirectory(_filePath);

		LIPIndex.resetItr();
		fcard* currentItem = LIPIndex.getNext();

		char filePathBuffer[FILENAME_MAX];

		// iterate through all fcards and grab the pathnames of all
		// directories
		while (currentItem != nullptr)
		{
			if (currentItem->isDirectory())
			{
				LIPIndex.getName(currentItem, filePathBuffer);

				// filePathToClean = filePathBuffer;

				directoryList.push_front(
				    std::string(filePathBuffer));
			}

			currentItem = LIPIndex.getNext();
		}

		// sort directories by length so that higher directories are
		// created first
		// TODO:: confirm that default sort sorts strings by string
		// length
		directoryList.sort();

		// now directoryList has list of every directory that must be
		// created for the files to be unpacked successfully

		// grab the garbage path from the beginning of the root file of
		// the lip
		std::string filePathPrefixToRemoveFromLIP =
		    directoryList.front();

		filePathPrefixToRemoveFromLIP.erase(
		    filePathPrefixToRemoveFromLIP.rfind('/'),
		    filePathPrefixToRemoveFromLIP.npos);

		// Create all required directories
		std::string toAdd;
		while (directoryList.size() > 0)
		{
			toAdd = directoryList.front();

			toAdd.erase(0, filePathPrefixToRemoveFromLIP.length());

			toAdd = filePathPrefix + toAdd;

			File::MakeDirectory(toAdd.c_str());

			directoryList.pop_front();
		}

		// unpack all files
		LIPIndex.resetItr();
		currentItem = LIPIndex.getNext();

		File::Handle unpackHandle;
		std::string filePathToClean = "";

		while (currentItem != nullptr)
		{
			if (!currentItem->isDirectory())
			{
				LIPIndex.getName(currentItem, filePathBuffer);

				filePathToClean = filePathBuffer;

				filePathToClean.erase(
				    0, filePathPrefixToRemoveFromLIP.length());

				concatenatedFilePath =
				    filePathPrefix + filePathToClean;

				File::Open(unpackHandle,
				           concatenatedFilePath.c_str(),
				           File::WRITE);

				int64_t fileSize;

				char* fileBytes =
				    LIPIndex.getFile(currentItem, fileSize);

				File::Write(unpackHandle, fileBytes, fileSize);

				File::Close(unpackHandle);
				delete fileBytes;
			}

			currentItem = LIPIndex.getNext();
		}
	}

	// Note:: Theese Match classses are likley not gonna stay around I'm
	// just playing around with some ideas right now.
	//the main problem here is it maintains pointers to indexes and fcards and if the diff's are closed the indexes and thier fcards are deleted so it becomes useless. I need to come up with a better solution. If the diff data is to be exported
	class PossibbleMatch
	{

	protected:
		// TODO:: change index1 to lhsIndex etc...
		Index* index1;
		fcard* card1;
		Index* index2;
		fcard* card2;

		std::string matchName;

	public:
		PossibbleMatch(Index* i1, fcard* c1, Index* i2, fcard* c2,
		               std::string _matchName)
		    : index1(i1), card1(c1), index2(i2), card2(c2),
		      matchName(_matchName)
		{
		}

		PossibbleMatch(const PossibbleMatch& rhs)
		{
			index1 = rhs.index1;
			card1 = rhs.card1;
			index2 = rhs.index2;
			card2 = rhs.card2;
			matchName = rhs.matchName;
		}

		const char* const Name() { return matchName.c_str(); }

		bool hashMatch()
		{
			return card1->info.digest == card2->info.digest;
		}
	};

	class PerfectMatch : public PossibbleMatch
	{
	public:
		PerfectMatch(const PossibbleMatch& pMatch)
		    : PossibbleMatch(pMatch)
		{
		}
	};

	class PartialMatch : public PossibbleMatch
	{
	public:
		PartialMatch(const PossibbleMatch& pMatch)
		    : PossibbleMatch(pMatch)
		{
		}

		char* getLhsFile(int64_t& fileSize)
		{
			return index1->getFile(card1, fileSize);
		}

		char* getRhsFile(int64_t& fileSize)
		{
			return index2->getFile(card2, fileSize);
		}
	};

	// class DiffData
	//{ //TODO:: flesh this out and use it in the diff pipeline and
	// possibbly use as a return value to the program , may be helpful to
	// transform the data because if the lips are closed the indexes
	// disappear and name data for lhsUnmatched and rhsUnmatched disappears
	// public:

	//	std::list<fcard*> lhsUnmatched;
	//	std::list<fcard*> rhsUnmatched;
	//	std::list<PossibbleMatch> PossibbleMatches;
	//	std::list<PartialMatch> PartialMatches;
	//	std::list<PerfectMatch> PerfectMatches;
	//};

	// TODO:: setup returns for lhsAll,rhsAll, Partial Matches and Perfect
	// Matches so they can be handled programatically

	// General
	// algorithim::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ for
	// each card in lhsAll look for name match in rhsAll pop both and add
	// create an entry on the possibble match list

	// NOTE:: for the sake of name testing I'm removing the leading
	// stuff before the root directory and also the name of the
	// root directory itself allowing LIP1 and LIP2 to have name
	// matches even if in different directories with different
	// roots

	// check each possibble match for full match via hash

	// put hash matches on a full match list as they are identical
	// files

	// put name matches that fail hash match onto Partial Match
	// list

	// output data

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	bool diff(LIP& rhs)
	{
		// TODO:: clean this up and break it out into multiple calls
		// this
		// monolithic beast is too messy... but it is functional.

		bool perfectMatch = true;

		std::list<fcard*> lhsAll;
		std::list<fcard*> rhsAll;
		std::list<PossibbleMatch> PossibbleMatches;
		std::list<PartialMatch> PartialMatches;
		std::list<PerfectMatch> PerfectMatches;

		// load up every fcard from the index into a list representing
		// the files on the left hand side of the diff
		this->LIPIndex.resetItr();
		fcard* cardPtr = this->LIPIndex.getNext();

		while (cardPtr != nullptr)
		{
			lhsAll.push_back(cardPtr);
			cardPtr = this->LIPIndex.getNext();
		}

		// load up every fcard from the index into a list representing
		// the files on the right hand side of the diff
		rhs.LIPIndex.resetItr();
		cardPtr = rhs.LIPIndex.getNext();

		while (cardPtr != nullptr)
		{
			rhsAll.push_back(cardPtr);
			cardPtr = rhs.LIPIndex.getNext();
		}

		// Debug
		// confirms the names of all the files in the directory
		char fileNameBuff[FILENAME_MAX];
		printf("lhsALL contents-------\n\n");
		for (fcard* l : lhsAll)
		{
			this->LIPIndex.getName(l, fileNameBuff);
			printf("cardName: %s\n", fileNameBuff);
		}
		printf("end lhsAll contents------\n\n");

		printf("rhsALL contents-------\n\n");
		for (fcard* l : rhsAll)
		{
			rhs.LIPIndex.getName(l, fileNameBuff);
			printf("cardName: %s\n", fileNameBuff);
		}
		printf("end rhsAll contents------\n\n");

		// End Debug

		// get the file path prefixes including root directory to
		// remove from each file so they can be compared properly

		// TODO:: probably want to include special notice if the root
		// directories don't match

		int lhsFilePrefixLength(0);
		int rhsFilePrefixLength(0);

		// std::string lhsPrefix(fileNameBuff);
		this->LIPIndex.getName(lhsAll.front(), fileNameBuff);

		std::string filePrefix(fileNameBuff);

		lhsFilePrefixLength = filePrefix.length();

		rhs.LIPIndex.getName(rhsAll.front(), fileNameBuff);

		filePrefix = fileNameBuff;
		rhsFilePrefixLength = filePrefix.length();

		// End Debug
		std::string lhsFixed;
		std::string rhsFixed;

		// TODO:: special behavior needed for root maches as currently
		// thier names are entirely deleted

		// this just checks the two roots against eachother. they are
		// garunteed to be the first two cards

		// TODO:: optimization: cache all the fixed file paths
		// for both indexes so it doesn't continuously have to
		// be recalculated.

		// fix the file name for the left hand side to be
		// compared to each rhs element
		fcard* lhsRootDir = lhsAll.front();
		lhsAll.pop_front();
		this->LIPIndex.getName(lhsRootDir, fileNameBuff);

		lhsFixed = fileNameBuff;

		fcard* rhsRootDir = rhsAll.front();
		rhsAll.pop_front();

		rhs.LIPIndex.getName(rhsRootDir, fileNameBuff);
		rhsFixed = fileNameBuff;

		if (lhsFixed.compare(rhsFixed) == 0)
		{

			PossibbleMatches.push_back(PossibbleMatch(
			    &this->LIPIndex, lhsRootDir, &rhs.LIPIndex,
			    rhsRootDir, lhsFixed));
		}
		else
		{
			// TODO:: whatever it is I should do if the root of the
			// directories don't match I have to touchbase with
			// what should be done about this.
		}

		// find name matches
		bool matchfound;
		for (auto lhsIterator = lhsAll.begin();
		     lhsIterator != lhsAll.end();)
		{
			matchfound = false;

			// TODO:: optimization: cache all the fixed file paths
			// for both indexes so it doesn't continuously have to
			// be recalculated.

			// fix the file name for the left hand side to be
			// compared to each rhs element
			this->LIPIndex.getName(*lhsIterator, fileNameBuff);
			lhsFixed = fileNameBuff;
			lhsFixed.erase(0, lhsFilePrefixLength);

			for (auto rhsIterator = rhsAll.begin();
			     rhsIterator != rhsAll.end();)
			{

				rhs.LIPIndex.getName(*rhsIterator,
				                     fileNameBuff);
				rhsFixed = fileNameBuff;
				rhsFixed.erase(0, rhsFilePrefixLength);

				if (lhsFixed.compare(rhsFixed) == 0)
				{
					matchfound = true;
					PossibbleMatches.push_back(
					    PossibbleMatch(
					        &this->LIPIndex, *lhsIterator,
					        &rhs.LIPIndex, *rhsIterator,
					        lhsFixed));

					lhsIterator =
					    lhsAll.erase(lhsIterator);
					rhsIterator =
					    rhsAll.erase(rhsIterator);
					break;
				}
				// this only increments if the iterator hasn't
				// been adjusted by the erase
				++rhsIterator;
			}
			// only increment if the iterator hasn't been adjusted
			// by the erase
			if (matchfound != true)
			{
				++lhsIterator;
			}
		}

		// CheckPossibble Matches
		for (auto possibbleMatchIterator = PossibbleMatches.begin();
		     possibbleMatchIterator != PossibbleMatches.end();)
		{

			if (possibbleMatchIterator->hashMatch())
			{
				PerfectMatches.push_back(
				    (PerfectMatch)*possibbleMatchIterator);
			}
			else
			{
				PartialMatches.push_back(
				    (PartialMatch)*possibbleMatchIterator);
				perfectMatch = false;
			}
			possibbleMatchIterator =
			    PossibbleMatches.erase(possibbleMatchIterator);
		}

		// print out all remaining files in lhsAll

		this->LIPIndex.resetItr();
		fcard* ptr = this->LIPIndex.getNext();

		this->LIPIndex.getName(ptr, fileNameBuff);

		std::string LipFileName(fileNameBuff);

		LipFileName.erase(0, LipFileName.rfind('/'));

		if (lhsAll.size() > 0)
		{
			printf("Unmatched files found in %s\n",
			       LipFileName.c_str());
			for (auto item : lhsAll)
			{
				this->LIPIndex.getName(item, fileNameBuff);
				printf("File with cardName: %s\n",
				       fileNameBuff);
				perfectMatch = false;
			}
			printf("\n");
		}

		rhs.LIPIndex.resetItr();
		ptr = rhs.LIPIndex.getNext();

		rhs.LIPIndex.getName(ptr, fileNameBuff);

		LipFileName = fileNameBuff;

		LipFileName.erase(0, LipFileName.rfind('/'));

		// print out all remaining files in rhsAll
		if (rhsAll.size() > 0)
		{
			printf("Unmatched files found in %s\n",
			       LipFileName.c_str());
			for (auto item : rhsAll)

			{
				rhs.LIPIndex.getName(item, fileNameBuff);
				printf("File with cardName: %s\n",
				       fileNameBuff);
				perfectMatch = false;
			}
			printf("\n");
		}

		// print out full matches
		if (PerfectMatches.size() > 0)
		{
			printf("Perfect Matches found\n");
			for (auto item : PerfectMatches)
			{
				printf("Perfect match found on file %s\n",
				       item.Name());
			}
			printf("\n");
		}

		// print out partial matches
		if (PartialMatches.size() > 0)
		{
			char lhsTempName[L_tmpnam];
			char rhsTempName[L_tmpnam];

			File::Handle lhsTmpFile;
			File::Handle rhsTmpFile;

			int64_t lhsFileSize;
			int64_t rhsFileSize;

			printf("Partial Matched Files found\n");
			for (auto item : PartialMatches)
			{
				printf("Partial match found on file %s\n",
				       item.Name());
				// call to linux diff make internal call to the
				// diffutils source code to wrap it completely
				// for programatic control right now Ihave to
				// make the files physically to diff them so it
				// do it in temp files

				// TODO:: there is a potential problem with
				// using tmpnam it's specfically noted that it
				// cause problems Solution is to create two
				// files to use and then remove them via my api
				// but I don't support file deletion just yet
				// so I'm using tmpnam

				std::tmpnam(lhsTempName);

				File::Open(lhsTmpFile, lhsTempName,
				           File::Mode::WRITE);

				char* lhsFileBytes =
				    item.getLhsFile(lhsFileSize);

				File::Write(lhsTmpFile, lhsFileBytes,
				            lhsFileSize);
				File::Flush(lhsTmpFile);
				delete lhsFileBytes;

				std::tmpnam(rhsTempName);

				File::Open(rhsTmpFile, rhsTempName,
				           File::Mode::WRITE);

				char* rhsFileBytes =
				    item.getRhsFile(rhsFileSize);

				File::Write(rhsTmpFile, rhsFileBytes,
				            rhsFileSize);
				File::Flush(rhsTmpFile);
				delete rhsFileBytes;

				std::string diffCmd("diff ");
				diffCmd += lhsTempName;
				diffCmd += " ";
				diffCmd += rhsTempName;

				/*printf("Diff command is %s\n",
				       diffCmd.c_str());*/

				system(diffCmd.c_str());

				File::Close(lhsTmpFile);
				File::Close(rhsTmpFile);
				remove(lhsTempName);
				remove(rhsTempName);

				// use tmp files take file name and rfind(.)
				// and take that position to npos to retrieve
				// the file extension and make a tmp file with
				// that extension and copy the block data into
				// it to do a diff
			}
			printf("\n");
		}

		// return the list of partial matches for further diffing
	}
};

// TODO:: make this easy serializable and deserializable so that it can be
// pulled to and from disk to a nice little structure the packer struct is cool
// and all but it could be much more strightforward to add things to the LIP if
// i had an existing lip on disk and I wanted to append I could load it into a
// LIP class and then add things I'm not focusing on this functionality now
// because it isn't explicitly needed. The goal is though to deal with the LIP
// class as an abstraction of the file itself and expose any necessary methods
// and hide all implementation details behind a nice clean interface

//class LIP
//{
//public:
//	class Index
//	{
//		File::Handle fh;
//		fcard* indexPtr;
//		uint numCards;
//
//		// TODO:: move the iterator function externally
//		fcard* currentPtr;
//
//	public:
//		Index() : fh(0), indexPtr(0), numCards(0), currentPtr(0) {}
//
//		void FillIndex(File::Handle _fh)
//		{
//			this->fh = _fh;
//			// this seeks to the pointer that is under the index
//			// that contains the pointer for the top of the index
//			File::Seek(this->fh, File::Location::END,
//			           -2l * (long)sizeof(ptr));
//
//			// finds the offset for the bottom of the index
//			long BottomOfIndex = 0;
//			File::Tell(this->fh, BottomOfIndex);
//
//			// this gets the pointer to the top of the index
//			int64_t indexTopOffset;
//			File::Read(this->fh, &indexTopOffset, sizeof(ptr));
//
//			// printf("bottom of index at %l and indexTopPtr at
//			// %l", BottomOfIndex, indexTopOffset);
//
//			int64_t indexSize = BottomOfIndex - indexTopOffset;
//
//			assert(indexSize % 64 == 0);
//			// go to the top of the index so I can read it into a
//			// buffer
//			File::Seek(this->fh, File::Location::BEGIN,
//			           indexTopOffset);
//
//			// theese lines pull the raw bytes for the index from
//			// the file and cast them to an array of fcards that
//			// make up the index.
//			char* rawIndexBuffer = new char[indexSize];
//
//			File::Read(this->fh, rawIndexBuffer, indexSize);
//
//			numCards = indexSize / (long)sizeof(fcard);
//
//			indexPtr = (fcard*)rawIndexBuffer;
//		}
//
//		~Index()
//		{
//			if (indexPtr != nullptr)
//				delete indexPtr;
//		}
//
//		// returns the number of fcards in the index
//		uint getIndexSize() { return numCards; }
//
//		void dumpIndex()
//		{
//			// This saves the location that the index was on to
//			// restore it after the dump just in case the user
//			// calls this in the middle of iteration
//			fcard* temp = currentPtr;
//
//			currentPtr = indexPtr;
//
//			char name[FILENAME_MAX];
//
//			printf("Beginning Index Dump-------------");
//			for (unsigned int i = 0; i < numCards; i++)
//			{
//				printf("Dumping Fcard-------------\n");
//				printf("File size: %ld \n",
//				       currentPtr->getSize());
//
//				// TODO:: add debug printing for creation time
//				// printf("Creation time %llu \n",
//				// currentPtr->getCreationTime());
//
//				// print hash
//				printf("File hash = ");
//				for (int j = 0; j < 28; j++)
//				{
//					printf("%02X",
//					       currentPtr->info.digest[j]);
//				}
//				printf("\n");
//
//				// end print hash
//
//				// print name
//				File::Seek(fh, File::Location::BEGIN,
//				           currentPtr->name.offset);
//				File::ReadLine(fh, name, FILENAME_MAX);
//
//				printf("Fcard %s\n", name);
//				// end print name
//				printf("End Card ----------------\n");
//				getNext();
//			}
//			printf("End Index Dump -----------------");
//			currentPtr = temp;
//		}
//
//		void resetItr() { currentPtr = nullptr; }
//
//		fcard* getNext()
//		{
//			if (currentPtr == nullptr)
//			{
//				currentPtr = indexPtr;
//			}
//			else if ((currentPtr - indexPtr) >= numCards - 1)
//			{
//				currentPtr = nullptr;
//			}
//			else
//			{
//				currentPtr++;
//			}
//			return currentPtr;
//		}
//
//		// TODO:: make a filename buffer class just to add a bit of
//		// safety here so a user can know they wont overhoot thier
//		// buffer
//		void getName(fcard* indexItem, char* filenameBuffer)
//		{
//			File::Seek(fh, File::Location::BEGIN,
//			           indexItem->name.offset);
//			File::ReadLine(fh, filenameBuffer, FILENAME_MAX);
//		}
//
//		char* getFile(fcard* indexOfItemToRetrieve, int64_t& fileSize)
//		{
//			fileSize = indexOfItemToRetrieve->getSize();
//			File::Seek(fh, File::Location::BEGIN,
//			           indexOfItemToRetrieve->begin.offset);
//
//			char* fileByteBuffer = new char[fileSize];
//
//			File::Read(fh, fileByteBuffer, fileSize);
//			return fileByteBuffer;
//		}
//	};
//
//private:
//	File::Handle fh;
//	Index LIPIndex;
//
//public:
//	// TODO:: incorporate packing functionality. //LIP's are readonly once
//	// archived so the current writer setup makes sense the lip objects are
//	// immutable
//
//	// TODO:: incorporate LIP Merging //LIP's don't need to be merged that
//	// is done in VVPKG so I don't need to edit that
//
//	LIP();
//	LIP(const char* const filePath);
//	~LIP() { File::Close(fh); }
//
//	LIP(const LIP&) = delete;
//	LIP operator=(const LIP&) = delete;
//
//	Index* getIndex() { return &LIPIndex; }
//
//	void Unpack() {}
//
//	void UnpackAt(const char* const _filePath)
//	{
//		// TODO:: optimize the string/char buffer insanity here this is
//		// a first pass.
//
//		// TODO:: Zhiaho says lip is sorted in a way that upon
//		// iteration you will always visit the parents before any of
//		// it's children so I can clean this up substnatially by
//		// iterating once and switching behavior based on directory or
//		// file
//
//		std::string filePathPrefix = _filePath;
//		// this removes a trailing slash if it's on the provided path
//		// because I keep the / from the files
//		if (filePathPrefix.back() == '/')
//		{
//			filePathPrefix.pop_back();
//		}
//
//		std::string concatenatedFilePath = "";
//
//		std::list<std::string> directoryList;
//
//		// makes base directory
//		File::MakeDirectory(_filePath);
//
//		LIPIndex.resetItr();
//		fcard* currentItem = LIPIndex.getNext();
//
//		char filePathBuffer[FILENAME_MAX];
//
//		// iterate through all fcards and grab the pathnames of all
//		// directories
//		while (currentItem != nullptr)
//		{
//			if (currentItem->isDirectory())
//			{
//				LIPIndex.getName(currentItem, filePathBuffer);
//
//				// filePathToClean = filePathBuffer;
//
//				directoryList.push_front(
//				    std::string(filePathBuffer));
//			}
//
//			currentItem = LIPIndex.getNext();
//		}
//
//		// sort directories by length so that higher directories are
//		// created first
//		// TODO:: confirm that default sort sorts strings by string
//		// length
//		directoryList.sort();
//
//		// now directoryList has list of every directory that must be
//		// created for the files to be unpacked successfully
//
//		// grab the garbage path from the beginning of the root file of
//		// the lip
//		std::string filePathPrefixToRemoveFromLIP =
//		    directoryList.front();
//
//		filePathPrefixToRemoveFromLIP.erase(
//		    filePathPrefixToRemoveFromLIP.rfind('/'),
//		    filePathPrefixToRemoveFromLIP.npos);
//
//		// Create all required directories
//		std::string toAdd;
//		while (directoryList.size() > 0)
//		{
//			toAdd = directoryList.front();
//
//			toAdd.erase(0, filePathPrefixToRemoveFromLIP.length());
//
//			toAdd = filePathPrefix + toAdd;
//
//			File::MakeDirectory(toAdd.c_str());
//
//			directoryList.pop_front();
//		}
//
//		// unpack all files
//		LIPIndex.resetItr();
//		currentItem = LIPIndex.getNext();
//
//		File::Handle unpackHandle;
//		std::string filePathToClean = "";
//
//		while (currentItem != nullptr)
//		{
//			if (!currentItem->isDirectory())
//			{
//				LIPIndex.getName(currentItem, filePathBuffer);
//
//				filePathToClean = filePathBuffer;
//
//				filePathToClean.erase(
//				    0, filePathPrefixToRemoveFromLIP.length());
//
//				concatenatedFilePath =
//				    filePathPrefix + filePathToClean;
//
//				File::Open(unpackHandle,
//				           concatenatedFilePath.c_str(),
//				           File::WRITE);
//
//				int64_t fileSize;
//
//				char* fileBytes =
//				    LIPIndex.getFile(currentItem, fileSize);
//
//				File::Write(unpackHandle, fileBytes, fileSize);
//
//				File::Close(unpackHandle);
//				delete fileBytes;
//			}
//
//			currentItem = LIPIndex.getNext();
//		}
//	}
//
//	// Note:: Theese Match classses are likley not gonna stay around I'm
//	// just playing around with some ideas right now.
//	class PossibbleMatch
//	{
//
//	protected:
//		// TODO:: change index1 to lhsIndex etc...
//		Index* index1;
//		fcard* card1;
//		Index* index2;
//		fcard* card2;
//
//		std::string matchName;
//
//	public:
//		PossibbleMatch(Index* i1, fcard* c1, Index* i2, fcard* c2,
//		               std::string _matchName)
//		    : index1(i1), card1(c1), index2(i2), card2(c2),
//		      matchName(_matchName)
//		{
//		}
//
//		PossibbleMatch(const PossibbleMatch& rhs)
//		{
//			index1 = rhs.index1;
//			card1 = rhs.card1;
//			index2 = rhs.index2;
//			card2 = rhs.card2;
//			matchName = rhs.matchName;
//		}
//
//		const char* const Name() { return matchName.c_str(); }
//
//		bool hashMatch()
//		{
//			return card1->info.digest == card2->info.digest;
//		}
//	};
//
//	class PerfectMatch : public PossibbleMatch
//	{
//	public:
//		PerfectMatch(const PossibbleMatch& pMatch)
//		    : PossibbleMatch(pMatch)
//		{
//		}
//	};
//
//	class PartialMatch : public PossibbleMatch
//	{
//	public:
//		PartialMatch(const PossibbleMatch& pMatch)
//		    : PossibbleMatch(pMatch)
//		{
//		}
//
//		char* getLhsFile(int64_t& fileSize)
//		{
//			return index1->getFile(card1, fileSize);
//		}
//
//		char* getRhsFile(int64_t& fileSize)
//		{
//			return index2->getFile(card2, fileSize);
//		}
//	};
//
//	// class DiffData
//	//{ //TODO:: flesh this out and use it in the diff pipeline and
//	// possibbly use as a return value to the program , may be helpful to
//	// transform the data because if the lips are closed the indexes
//	// disappear and name data for lhsUnmatched and rhsUnmatched disappears
//	// public:
//
//	//	std::list<fcard*> lhsUnmatched;
//	//	std::list<fcard*> rhsUnmatched;
//	//	std::list<PossibbleMatch> PossibbleMatches;
//	//	std::list<PartialMatch> PartialMatches;
//	//	std::list<PerfectMatch> PerfectMatches;
//	//};
//
//	// TODO:: setup returns for lhsAll,rhsAll, Partial Matches and Perfect
//	// Matches so they can be handled programatically
//
//	// General
//	// algorithim::~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ for
//	// each card in lhsAll look for name match in rhsAll pop both and add
//	// create an entry on the possibble match list
//
//	// NOTE:: for the sake of name testing I'm removing the leading
//	// stuff before the root directory and also the name of the
//	// root directory itself allowing LIP1 and LIP2 to have name
//	// matches even if in different directories with different
//	// roots
//
//	// check each possibble match for full match via hash
//
//	// put hash matches on a full match list as they are identical
//	// files
//
//	// put name matches that fail hash match onto Partial Match
//	// list
//
//	// output data
//
//	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
//	void diff(LIP& rhs)
//	{
//		// TODO:: clean this up and break it out into multiple calls
//		// this
//		// monolithic beast is too messy... but it is functional.
//
//		std::list<fcard*> lhsAll;
//		std::list<fcard*> rhsAll;
//		std::list<PossibbleMatch> PossibbleMatches;
//		std::list<PartialMatch> PartialMatches;
//		std::list<PerfectMatch> PerfectMatches;
//
//		// load up every fcard from the index into a list representing
//		// the files on the left hand side of the diff
//		this->LIPIndex.resetItr();
//		fcard* cardPtr = this->LIPIndex.getNext();
//
//		while (cardPtr != nullptr)
//		{
//			lhsAll.push_back(cardPtr);
//			cardPtr = this->LIPIndex.getNext();
//		}
//
//		// load up every fcard from the index into a list representing
//		// the files on the right hand side of the diff
//		rhs.LIPIndex.resetItr();
//		cardPtr = rhs.LIPIndex.getNext();
//
//		while (cardPtr != nullptr)
//		{
//			rhsAll.push_back(cardPtr);
//			cardPtr = rhs.LIPIndex.getNext();
//		}
//
//		// Debug
//		// confirms the names of all the files in the directory
//		char fileNameBuff[FILENAME_MAX];
//		printf("lhsALL contents-------\n\n");
//		for (fcard* l : lhsAll)
//		{
//			this->LIPIndex.getName(l, fileNameBuff);
//			printf("cardName: %s\n", fileNameBuff);
//		}
//		printf("end lhsAll contents------\n\n");
//
//		printf("rhsALL contents-------\n\n");
//		for (fcard* l : rhsAll)
//		{
//			rhs.LIPIndex.getName(l, fileNameBuff);
//			printf("cardName: %s\n", fileNameBuff);
//		}
//		printf("end rhsAll contents------\n\n");
//
//		// End Debug
//
//		// get the file path prefixes including root directory to
//		// remove from each file so they can be compared properly
//
//		// TODO:: probably want to include special notice if the root
//		// directories don't match
//
//		int lhsFilePrefixLength(0);
//		int rhsFilePrefixLength(0);
//
//		// std::string lhsPrefix(fileNameBuff);
//		this->LIPIndex.getName(lhsAll.front(), fileNameBuff);
//
//		std::string filePrefix(fileNameBuff);
//
//		lhsFilePrefixLength = filePrefix.length();
//
//		rhs.LIPIndex.getName(rhsAll.front(), fileNameBuff);
//
//		filePrefix = fileNameBuff;
//		rhsFilePrefixLength = filePrefix.length();
//
//		// End Debug
//		std::string lhsFixed;
//		std::string rhsFixed;
//
//		// TODO:: special behavior needed for root maches as currently
//		// thier names are entirely deleted
//
//		// this just checks the two roots against eachother. they are
//		// garunteed to be the first two cards
//
//		// TODO:: optimization: cache all the fixed file paths
//		// for both indexes so it doesn't continuously have to
//		// be recalculated.
//
//		// fix the file name for the left hand side to be
//		// compared to each rhs element
//		fcard* lhsRootDir = lhsAll.front();
//		lhsAll.pop_front();
//		this->LIPIndex.getName(lhsRootDir, fileNameBuff);
//
//		lhsFixed = fileNameBuff;
//
//		fcard* rhsRootDir = rhsAll.front();
//		rhsAll.pop_front();
//
//		rhs.LIPIndex.getName(rhsRootDir, fileNameBuff);
//		rhsFixed = fileNameBuff;
//
//		if (lhsFixed.compare(rhsFixed) == 0)
//		{
//
//			PossibbleMatches.push_back(PossibbleMatch(
//			    &this->LIPIndex, lhsRootDir, &rhs.LIPIndex,
//			    rhsRootDir, lhsFixed));
//		}
//		else
//		{
//			// TODO:: whatever it is I should do if the root of the
//			// directories don't match I have to touchbase with
//			// what should be done about this.
//		}
//
//		// find name matches
//		bool matchfound;
//		for (auto lhsIterator = lhsAll.begin();
//		     lhsIterator != lhsAll.end();)
//		{
//			matchfound = false;
//
//			// TODO:: optimization: cache all the fixed file paths
//			// for both indexes so it doesn't continuously have to
//			// be recalculated.
//
//			// fix the file name for the left hand side to be
//			// compared to each rhs element
//			this->LIPIndex.getName(*lhsIterator, fileNameBuff);
//			lhsFixed = fileNameBuff;
//			lhsFixed.erase(0, lhsFilePrefixLength);
//
//			for (auto rhsIterator = rhsAll.begin();
//			     rhsIterator != rhsAll.end();)
//			{
//
//				rhs.LIPIndex.getName(*rhsIterator,
//				                     fileNameBuff);
//				rhsFixed = fileNameBuff;
//				rhsFixed.erase(0, rhsFilePrefixLength);
//
//				if (lhsFixed.compare(rhsFixed) == 0)
//				{
//					matchfound = true;
//					PossibbleMatches.push_back(
//					    PossibbleMatch(
//					        &this->LIPIndex, *lhsIterator,
//					        &rhs.LIPIndex, *rhsIterator,
//					        lhsFixed));
//
//					lhsIterator =
//					    lhsAll.erase(lhsIterator);
//					rhsIterator =
//					    rhsAll.erase(rhsIterator);
//					break;
//				}
//				// this only increments if the iterator hasn't
//				// been adjusted by the erase
//				++rhsIterator;
//			}
//			// only increment if the iterator hasn't been adjusted
//			// by the erase
//			if (matchfound != true)
//			{
//				++lhsIterator;
//			}
//		}
//
//		// CheckPossibble Matches
//		for (auto possibbleMatchIterator = PossibbleMatches.begin();
//		     possibbleMatchIterator != PossibbleMatches.end();)
//		{
//
//			if (possibbleMatchIterator->hashMatch())
//			{
//				PerfectMatches.push_back(
//				    (PerfectMatch)*possibbleMatchIterator);
//			}
//			else
//			{
//				PartialMatches.push_back(
//				    (PartialMatch)*possibbleMatchIterator);
//			}
//			possibbleMatchIterator =
//			    PossibbleMatches.erase(possibbleMatchIterator);
//		}
//
//		// print out all remaining files in lhsAll
//
//		this->LIPIndex.resetItr();
//		fcard* ptr = this->LIPIndex.getNext();
//
//		this->LIPIndex.getName(ptr, fileNameBuff);
//
//		std::string LipFileName(fileNameBuff);
//
//		LipFileName.erase(0, LipFileName.rfind('/'));
//
//		if (lhsAll.size() > 0)
//		{
//			printf("Unmatched files found in %s\n",
//			       LipFileName.c_str());
//			for (auto item : lhsAll)
//			{
//				this->LIPIndex.getName(item, fileNameBuff);
//				printf("File with cardName: %s\n",
//				       fileNameBuff);
//			}
//			printf("\n");
//		}
//
//		rhs.LIPIndex.resetItr();
//		ptr = rhs.LIPIndex.getNext();
//
//		rhs.LIPIndex.getName(ptr, fileNameBuff);
//
//		LipFileName = fileNameBuff;
//
//		LipFileName.erase(0, LipFileName.rfind('/'));
//
//		// print out all remaining files in rhsAll
//		if (rhsAll.size() > 0)
//		{
//			printf("Unmatched files found in %s\n",
//			       LipFileName.c_str());
//			for (auto item : rhsAll)
//
//			{
//				rhs.LIPIndex.getName(item, fileNameBuff);
//				printf("File with cardName: %s\n",
//				       fileNameBuff);
//			}
//			printf("\n");
//		}
//
//		// print out full matches
//		if (PerfectMatches.size() > 0)
//		{
//			printf("Perfect Matches found\n");
//			for (auto item : PerfectMatches)
//			{
//				printf("Perfect match found on file %s\n",
//				       item.Name());
//			}
//			printf("\n");
//		}
//
//		// print out partial matches
//		if (PartialMatches.size() > 0)
//		{
//			char lhsTempName[L_tmpnam];
//			char rhsTempName[L_tmpnam];
//
//			File::Handle lhsTmpFile;
//			File::Handle rhsTmpFile;
//
//			int64_t lhsFileSize;
//			int64_t rhsFileSize;
//
//			printf("Partial Matched Files found\n");
//			for (auto item : PartialMatches)
//			{
//				printf("Partial match found on file %s\n",
//				       item.Name());
//				// call to linux diff make internal call to the
//				// diffutils source code to wrap it completely
//				// for programatic control right now Ihave to
//				// make the files physically to diff them so it
//				// do it in temp files
//
//				// TODO:: there is a potential problem with
//				// using tmpnam it's specfically noted that it
//				// cause problems Solution is to create two
//				// files to use and then remove them via my api
//				// but I don't support file deletion just yet
//				// so I'm using tmpnam
//
//				std::tmpnam(lhsTempName);
//
//				File::Open(lhsTmpFile, lhsTempName,
//				           File::Mode::WRITE);
//
//				char* lhsFileBytes =
//				    item.getLhsFile(lhsFileSize);
//
//				File::Write(lhsTmpFile, lhsFileBytes,
//				            lhsFileSize);
//				File::Flush(lhsTmpFile);
//				delete lhsFileBytes;
//
//				std::tmpnam(rhsTempName);
//
//				File::Open(rhsTmpFile, rhsTempName,
//				           File::Mode::WRITE);
//
//				char* rhsFileBytes =
//				    item.getRhsFile(rhsFileSize);
//
//				File::Write(rhsTmpFile, rhsFileBytes,
//				            rhsFileSize);
//				File::Flush(rhsTmpFile);
//				delete rhsFileBytes;
//
//				std::string diffCmd("diff ");
//				diffCmd += lhsTempName;
//				diffCmd += " ";
//				diffCmd += rhsTempName;
//
//				/*printf("Diff command is %s\n",
//				       diffCmd.c_str());*/
//
//				system(diffCmd.c_str());
//
//				File::Close(lhsTmpFile);
//				File::Close(rhsTmpFile);
//				remove(lhsTempName);
//				remove(rhsTempName);
//
//				// use tmp files take file name and rfind(.)
//				// and take that position to npos to retrieve
//				// the file extension and make a tmp file with
//				// that extension and copy the block data into
//				// it to do a diff
//			}
//			printf("\n");
//		}
//
//		// return the list of partial matches for further diffing
//	}
//};
//}
}
#endif

// bonus documentation as I go
// so the very first thing that's written by the packer is the header structure
// it writes LIP\0 in the first 4 bytes of the file so it can be used as a
// check to make sure the path fed is a LIP