#ifndef FILE_H
#define FILE_H

// Make the assumption of c-char strings, not UNICODE
// 32 bit files, not supporting 64 bits yet

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
	static File::Error Open(File::Handle& fh, const char* const fileName,
	                        File::Mode mode);
	static File::Error Close(File::Handle& fh);
	static File::Error Write(File::Handle& fh, const void* const buffer,
	                         const size_t inSize);
	static File::Error Read(File::Handle& fh, void* const _buffer,
	                        const size_t _size);
	static File::Error Seek(File::Handle& fh, File::Location location,
	                        int offset);
	static File::Error Tell(File::Handle& fh, unsigned int& offset);
	static File::Error Flush(File::Handle& fh);

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

//class RAIIHandle
//{
//	File::Handle fh;
//
//	RAIIHandle(const char* const fileName, File::Mode mode) { fh = fopen(fileName, File::getMode(mode)); }
//
//	~RAIIHandle() { File::Close(fh); }
//};

#endif

// ---  End of File ---------------
