#include <fstream>
#include <iostream>
#include <sstream> //std::stringstream
#include <lip/lip.h>
using pread_sig = size_t(char*, size_t, int64_t);

lip::index::index(stdex::signature<pread_sig> f, int64_t filesize)
{
    auto pread_exact = [=](char* p, size_t sz, int64_t from) mutable {
        if (f(p, sz, from) != sz)
            throw std::system_error{ errno,
                                     std::system_category() };
    };

    ptr eof[2];
    // points to location of ptrToTopofIndex in LIP
    ptr endidx = { filesize - int64_t(sizeof(eof)) };

    // this reads the two pointers at the end of LIP: ptrToTopofIndex and ptrToBss
    // eof[0] = ptrToTopofIndex. eof[1] = ptrToBss
    pread_exact(reinterpret_cast<char*>(eof), sizeof(eof), endidx.offset);
    auto blen = size_t(filesize - eof[1].offset);
    bp_.reset(new char[blen]);
    pread_exact(bp_.get(), blen, eof[1].offset);

    eof[0].adjust(bp_.get(), eof[1]);
    first_ = eof[0].pointer_to<fcard>();
    endidx.adjust(bp_.get(), eof[1]);
    last_ = endidx.pointer_to<fcard>();

    std::for_each(first_, last_,
                  [&](fcard& fc) { fc.name.adjust(bp_.get(), eof[1]); });
}


std::string readDataSection(stdex::signature<pread_sig> f, unsigned long size, int64_t from)
{
    char *buffer = new char[size+1];
    if (f(buffer, size, from) != size)
    {
        throw std::system_error{ errno,
                                 std::system_category() };
    }
    std::string data(buffer);
    return data;
}

int main()
{
    std::ifstream inFile;
    inFile.open("archive.lip"); //open the input file containing LIP structure
    if (!inFile.is_open())
    {
        printf("Could not open input \n");
        return 1;
    }

    std::stringstream strStream;
    strStream << inFile.rdbuf(); //read the file
    std::string str = strStream.str(); //str holds the content of the file
    std::cout << "bytes read: " << str.size() << "\n";

    // function to copy from the LIP structure
    auto f = [&](char* p, size_t sz, int64_t from)
    {
        return str.copy(p, sz, size_t(from));
    };

    lip::ptr pointers[2];
    // reads the pointers
    f(reinterpret_cast<char*>(pointers), sizeof(pointers), int64_t(str.size() - sizeof(pointers)));
    int64_t ptrToBss = pointers[0].offset;
    int64_t ptrToTopOfIndex = pointers[1].offset;
    ptrToBss++;
    ptrToTopOfIndex++;

    // reads and constructs the index from LIP structure
    auto const idx = lip::index(f, int64_t(str.size()));

    // reads data section from LIP structure
    int header_size = 8;
    int64_t data_size =  ptrToBss - header_size;
    auto data_section = readDataSection(f, data_size, header_size);
    // print data section here
}
