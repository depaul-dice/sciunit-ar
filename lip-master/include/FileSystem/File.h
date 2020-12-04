#ifndef FILE_H
#define FILE_H

#include <assert.h>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>

class File
{
public:

	typedef FILE* Handle;
	//class Handle;
	
	enum Mode
	{
		READ,
		WRITE,
		READ_WRITE,
		//TODO:: add append and r+ w+ and a+ modes to help with very large LIP IO
	};

	enum DirectoryMode
	{
		//TODO:: implement modes using a default mode for now
		Default,
	};

	enum Location
	{
		BEGIN = SEEK_SET,
		CURRENT = SEEK_CUR,
		END = SEEK_END

	};

	enum Error
	{
		SUCCESS = 0x7C000000,
		OPEN_FAIL,
		CLOSE_FAIL,
		WRITE_FAIL,
		READ_FAIL,
		SEEK_FAIL,
		TELL_FAIL,
		FLUSH_FAIL
	};

public:

	//File Operations
	static File::Error Open(File::Handle& fh, const char* const fileName,
	                        File::Mode mode);
	static File::Error Close(File::Handle& fh);
	static File::Error Write(File::Handle& fh, const void* const buffer,
	                         const int64_t inSize);
	static File::Error Read(File::Handle& fh, void* const _buffer,
	                        const int64_t _size);

	static File::Error Read(File::Handle& fh, void* const _buffer,
	                        const int64_t _size, int64_t& bytesRead);

	static File::Error ReadLine(File::Handle& fh, char* const buffer,
	                            const int maxSize);
	static File::Error Seek(File::Handle& fh, File::Location location,
	                        long offset);
	static File::Error Tell(File::Handle& fh, int64_t& offset);
	static File::Error Flush(File::Handle& fh);

	//Directory operations I may break this out into Directory.h and Directory.cc for cleanliness
	
	//non relative modes are hidden currently I want to encourage users to work relative to thier directory
	
	static File::Error MakeDirectory(const char* directoryPath, File::DirectoryMode mode = File::DirectoryMode::Default);
	//static File::Error MakeDirectoryRelativeToHandle(File::Handle& fh, const char* directoryPath, File::DirectoryMode mode = File::DirectoryMode::Default);

	//static File::Error RemoveDirectory(const char* directoryPath);

	//static File::Error RemoveDirectoryRealtiveToHandle(File::Handle& fh, const char* directoryPath);


	//TODO:: finish this to make the File system handles RAII or make a File wrapper to give execption safety so the handles don't leak.
	/*class Handle
	{
		typedef void* rawFileHandle;

		rawFileHandle fh;

		Handle(const char* const fileName, File::Mode mode)
		{
			fh = fopen(fileName, File::getMode(mode));
		}

		~Handle() { File::Close(fh); }
	};*/
};


class readOnlyFileHandle
{
protected:

	File::Handle handle;

	public:

	readOnlyFileHandle(){}

	readOnlyFileHandle(const char* fileName)
	{
		assert(File::SUCCESS == File::Open(handle, fileName, File::READ));
	}

	virtual void Close() { File::Close(handle); }

	virtual int64_t Read(void* const _buffer, const int64_t _size)
	{
		int64_t bytesRead = 0;
		File::Read(handle, _buffer, _size, bytesRead);
		return bytesRead;
	}

	virtual void Seek(int64_t offset, File::Location fileLocation = File::Location::BEGIN)
	{
		File::Seek(handle, fileLocation, offset);
	}

	virtual int64_t Tell()
	{ 
		int64_t offset;
		File::Tell(handle, offset);
		return offset;
	}

	//TODO:: consider adding bytes read, i am not currently because you can just read till null
	virtual void ReadLine(char* const buffer, const int maxSize)
	{
		File::ReadLine(handle, buffer, maxSize);
	}

};


#endif

// ---  End of File ---------------
