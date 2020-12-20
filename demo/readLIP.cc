#include <fstream>
#include <iostream>
#include <sstream> //std::stringstream
#include <lip/lip.h>
#include <vvpkg/c_file_funcs.h>
#include <vvpkg/fd_funcs.h>

#define U(s) s

using pread_sig = size_t(char*, size_t, int64_t);
using param_type = lip::gbpath::param_type;
using view_type = stdex::basic_string_view<lip::gbpath::char_type>;


//lip::index::index(stdex::signature<pread_sig> f, int64_t filesize)
//{
//    auto pread_exact = [=](char* p, size_t sz, int64_t from) mutable {
//        if (f(p, sz, from) != sz)
//            throw std::system_error{ errno,
//                                     std::system_category() };
//    };
//
//    ptr eof[2];
//    // points to location of ptrToTopofIndex in LIP
//    ptr endidx = { filesize - int64_t(sizeof(eof)) };
//
//    // this reads the two pointers at the end of LIP: ptrToTopofIndex and ptrToBss
//    // eof[0] = ptrToTopofIndex. eof[1] = ptrToBss
//    pread_exact(reinterpret_cast<char*>(eof), sizeof(eof), endidx.offset);
//    auto blen = size_t(filesize - eof[1].offset);
//    bp_.reset(new char[blen]);
//    pread_exact(bp_.get(), blen, eof[1].offset);
//
//    eof[0].adjust(bp_.get(), eof[1]);
//    first_ = eof[0].pointer_to<fcard>();
//    endidx.adjust(bp_.get(), eof[1]);
//    last_ = endidx.pointer_to<fcard>();
//
//    std::for_each(first_, last_,
//                  [&](fcard& fc) { fc.name.adjust(bp_.get(), eof[1]); });
//}


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

lip::fcard findFIle(param_type filename, lip::index index)
{
    lip::fcard fcard_;
    try
    {
        fcard_ = index[filename];
    }
    catch(std::out_of_range &ex)
    {

    }
    return fcard_;
}

lip::fcard findDir(param_type dirname, lip::index index)
{
    lip::fcard fcard_;
    try
    {
        fcard_ = index[dirname];
    }
    catch(std::out_of_range &ex)
    {

    }
    return fcard_;
}

lip::fcard findSymlink(param_type symlinkname, lip::index index)
{
    lip::fcard fcard_;
    try
    {
        fcard_ = index[symlinkname];
    }
    catch(std::out_of_range &ex)
    {

    }
    return fcard_;
}

void generateLIP(param_type filename, param_type dirname, lip::archive_options opts)
{
    FILE* fp;
    std::unique_ptr<FILE, vvpkg::c_file_deleter> to_open;

    if (filename == view_type(U("-")))
    {
        vvpkg::xstdout_fileno();
        fp = stdout;
    }
    else
    {
        to_open.reset(vvpkg::xfopen(filename, U("wb")));
        fp = to_open.get();
    }

    lip::archive(vvpkg::to_c_file(fp), dirname, opts);
}


bool compareFileHashes(char *string)
{
    // read the file hashes from the vvpkg sqlite database
    // database design is as follows:
    // filename1, hash1
    // filename2, hash2
    return true;
}

//void iterateIndex(lip::index index1, lip::index index2)
//{
//    std::for_each(index1.begin(), index1.end(),
//                  [&](lip::fcard& fc)
//                  {
//                        // check the type of entry here: file, dir, symlink
//                        const lip::fcard *fcard_ = index2.find(fc.arcname);
//                        // if found, compare their data sections here first
//                        bool match = compareFileHashes(fc.arcname);
//                        if(match)
//                        {
//                            // compare the rest of metadata here
//                        }
//                  });
//}

int main()
{
    std::ifstream inFile;
    //open the input file containing LIP structure
    inFile.open("/home/raza/CLionProjects/lip/demo/archive.lip");
    if (!inFile)
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

    auto const idx = lip::index(f, int64_t(str.size()));
    exit(0);

    lip::ptr pointers[2];
    // reads the pointers
    f(reinterpret_cast<char*>(pointers), sizeof(pointers), int64_t(str.size() - sizeof(pointers)));
    int64_t ptrToBss = pointers[0].offset;
    int64_t ptrToTopOfIndex = pointers[1].offset;
    ptrToBss++;
    ptrToTopOfIndex++;

    // reads and constructs the index from LIP structure
//    auto const idx = lip::index(f, int64_t(str.size()));

    // reads data section from LIP structure
    int header_size = 8;
    int64_t data_size =  ptrToBss - header_size;
    auto data_section = readDataSection(f, data_size, header_size);
    // print data section here
}
