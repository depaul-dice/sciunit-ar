
#include <assert.h>

#include <stdio.h>
#include <FileSystem/File.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>

#define STUB_PLEASE_REPLACE(x) (x);

// TODO:: add the grabbing of ferror and checking for specfic problems here for
// informational purposes

const char* getMode(const File::Mode fm)
{  // this is quick because it will compile to a jump table rather than nested
   // if's and provides safety to errors in mode selection
	switch (fm)
	{
	case File::Mode::READ: return "r";
	case File::Mode::WRITE: return "w";
	case File::Mode::READ_WRITE: return "rw";
	default: break;
	}
};

File::Error File::Open(File::Handle& fh, const char* const fileName,
                       File::Mode mode)
{

	fh = fopen(fileName, getMode(mode));

	File::Error retval = File::Error::SUCCESS;

	if (fh != 0)
	{
		retval = OPEN_FAIL;
	}

	return retval;
}

File::Error File::Close(File::Handle& fh)
{

	File::Error retval = File::Error::SUCCESS;

	if (!fclose(fh))
	{
		retval = File::Error::CLOSE_FAIL;
	}

	return retval;
}

File::Error File::Write(File::Handle& fh, const void* const buffer,
                        const size_t inSize)
{
	// TODO:: add checks for number of chunks written. also make a version
	// to send an array of elements of size.
	bool ret = fwrite(buffer, inSize, 1, fh);

	File::Error retval = File::Error::SUCCESS;
	if (!ret)
	{
		retval = File::Error::WRITE_FAIL;
	}

	return retval;
}

File::Error File::Read(File::Handle& fh, void* const buffer,
                       const size_t outSize)
{
	// unsigned int read; //could check to see that the bytes that were
	// supposed to be read were all read

	File::Error retval = File::Error::SUCCESS;
	// if (!ReadFile(fh, buffer, inSize, read, 0))
	if (!fread(buffer, outSize, 1, fh))
	{
		retval = File::Error::READ_FAIL;
	}

	return retval;
}

File::Error File::ReadLine(File::Handle& fh, char* const buffer,
                           const int maxSize)
{
	File::Error retval = File::Error::SUCCESS;
	// if (!ReadFile(fh, buffer, inSize, read, 0))

	char* retString = fgets(buffer, maxSize, fh);

	// TOOD:: add error checking for eof indictor and ferror see fgets
	// documentation for details.
	if (retString == nullptr)
	{
		retval = File::Error::READ_FAIL;
	}

	return retval;
}

File::Error File::Seek(File::Handle& fh, File::Location location, long offset)
{
	File::Error retval = File::Error::SUCCESS;

	if (0 != fseek(fh, offset, location))
	{
		retval = File::Error::SEEK_FAIL;
	}

	return retval;
}

File::Error File::Tell(File::Handle& fh, long& offset)
{
	File::Error retval = File::Error::TELL_FAIL;

	offset = ftell(fh);

	if (offset != -1L)
	{
		retval = File::Error::SUCCESS;
	}

	return retval;
}

File::Error File::Flush(File::Handle& fh)
{
	File::Error retval = File::Error::FLUSH_FAIL;

	if (0 == fflush(fh))
	{
		retval = File::Error::SUCCESS;
	}

	return retval;
}

// File::Error File::RemoveDirectory(const char* directoryPath){}

//File::Error File::RemoveDirectoryRealtiveToHandle(File::Handle& fh,                                             const char* directoryPath)
//{  // TODO:: error handling and flesh it out
	//TODO:: if removeContents intterupts it may leak memory from the ftw structure. I need to set it up to be resilient so that on error ftw can cleanup
	//TODO:: removing directories is a bit more complicated if it's written to be 100% portable, I am using unlink() on directories and not all implementations support this.	
	//unlinkat(fh->_fileno, directoryPath, )

//
	File::Error File::MakeDirectory(const char* directoryPath,
	                                File::DirectoryMode mode)
	{
	File::Error retval = File::Error::SUCCESS;

	if (0 != mkdir(directoryPath, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) )
	{
		//TODO:: make a file error enums for directory operations
		retval = File::Error::WRITE_FAIL;
	}

	return retval;
}


//File::Error File::MakeDirectoryRelativeToHandle(
//    File::Handle& fh, const char* directoryPath,
//    File::DirectoryMode mode = File::DirectoryMode::Default)
//{
//	File::Error retval = File::Error::SUCCESS; 
//
//	// TODO:: flesh out with error handling and flesh it out
//	int i = mkdirat(fh->_fileno, directoryPath,
//	        S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
//	if (i != 0)
//	{
//		retval = File::Error::WRITE_FAIL;
//	}
//
//	return retval;
//}

// ---  End of File ---------------
