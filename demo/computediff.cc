#include <fstream>
#include <iostream>
#include <sstream> //std::stringstream
#include <lip/lip.h>
#include <vvpkg/c_file_funcs.h>
#include <vvpkg/fd_funcs.h>

#ifdef _WIN32
#define U(s) L##s
#define UF "%ls"
#define UNF "%.*ls"
#else
#define U(s) s
#define UF "%s"
#define UNF "%.*s"
#endif

using pread_sig = size_t(char*, size_t, int64_t);
using param_type = lip::gbpath::param_type;
using view_type = stdex::basic_string_view<lip::gbpath::char_type>;


struct args
{
    args(int argc, param_type* argv)
    {
        auto p = argv;
        if (++p == argv + argc)
        {
            err:
            fprintf(stderr,
                    "usage: " UF
                    " [ctx]f [-C <dir>] [--lz4] [--one-level] "
                    "<archive-file> [<directory>]\n",
                    argv[0]);
            exit(2);
        }

        cmd = *p;

        for (;;)
        {
            if (++p == argv + argc)
                goto err;
            if ((*p)[0] == U('-'))
            {
                view_type vp = *p;
                if (vp == U("-C"))
                {
                    if (++p == argv + argc)
                        goto err;
                    cd = *p;
                }
                else if (vp == U("--lz4"))
                    opts.feat =
                            lip::feature::lz4_compressed;
                else if (vp == U("--one-level"))
                    opts.one_level = true;
                else
                    goto err;
            }
            else
                break;
        }

        archive_file = *p;
        ++p;
        directory = *p;
    }

    view_type cmd;
    lip::archive_options opts;
    param_type cd = nullptr, archive_file, directory;
};


std::string readDataSection(stdex::signature<pread_sig> f, unsigned long size, int64_t from)
{
    std::unique_ptr<char[]> buffer(new char[size+1]);
    if (f(buffer.get(), size, from) != size)
    {
        throw std::system_error{ errno,
                                 std::system_category() };
    }
    std::string data(buffer.get());
    return data;
}

bool compareFileHashes(param_type name1, param_type name2)
{
    // read the file hashes from the vvpkg sqlite database
    // database design is as follows:
    // filename1, hash1
    // filename2, hash2
    return true;
}

void iterateIndex(const lip::index& index1, const lip::index& index2)
{
    std::vector<std::string> entries_only_in_e1;
    std::vector<std::string> entries_only_in_e2;
    std::vector<std::string> entries_with_diff_size;
    std::vector<std::string> entries_with_diff_permissions;
    std::vector<std::string> entries_with_diff_mtime;

    for(auto fc : index1)
    {
        const lip::fcard *fcard_ = index2.find_by_name(fc.arcname);
        if(fcard_ != index2.end())
        {
            bool content_match = compareFileHashes(fc.arcname, fcard_->arcname);
            if(fc.size_ != fcard_->size_)
            {
                entries_with_diff_size.emplace_back(fc.arcname);
            }
            if(fc.permissions != fcard_->permissions)
            {
                entries_with_diff_permissions.emplace_back(fc.arcname);
            }
            if(fc.mtime != fcard_->mtime)
            {
                entries_with_diff_mtime.emplace_back(fc.arcname);
            }
        }
        else
        {
            // files only in e1
            entries_only_in_e1.emplace_back(fc.arcname);
        }
    }

    // files only in e2
    for(auto fc: index2)
    {
        const lip::fcard *fcard_ = index1.find_by_name(fc.arcname);
        if(fcard_ == index1.end())
        {
            entries_only_in_e2.emplace_back(fc.arcname);
        }
    }

    std::string dir1 = index1.begin()->arcname;
    std::string dir2 = index2.begin()->arcname;
    std::string output;
    output.reserve(200);
    output += "Differences between executions " + dir1 + " and " + dir2 + "\n\n";
    output += "Entries only in " + dir1 + ":\n";
    for(const auto& fn: entries_only_in_e1)
    {
        output += fn + "\n";
    }
    output += "\n";
    output += "Entries only in " + dir2 + ":\n";
    for(const auto& fn: entries_only_in_e2)
    {
        output += fn + "\n";
    }
    output += "\n";
    output += "Entries with changed size:\n";
    for(const auto& fn: entries_with_diff_size)
    {
        output += fn + "\n";
    }
    output += "\n";
    output += "Entries with changed modified time:\n";
    for(const auto& fn: entries_with_diff_mtime)
    {
        output += fn + "\n";
    }
    output += "\n";
    output += "Entries with changed permissions:\n";
    for(const auto& fn: entries_with_diff_permissions)
    {
        output += fn + "\n";
    }
    output += "\n";

    std::cout << output;
}

std::string loadArchive(const std::string& filename)
{
    std::ifstream inFile;
    //open the input file containing LIP archive
    inFile.open(filename);
    if (!inFile)
    {
        printf("Could not open input \n");
        return "";
    }

    std::stringstream strStream;
    strStream << inFile.rdbuf(); //read the file
    std::string str = strStream.str(); //str holds the content of the file
    std::cout << "bytes read: " << str.size() << "\n";

    return str;
}

std::string readDataSection(int64_t ptrToBss, stdex::signature<pread_sig> f)
{
    // read data section from LIP structure
    const int header_size = 8;
    auto data_size = static_cast<unsigned long>(ptrToBss - header_size);
    auto data_section = readDataSection(f, data_size, header_size);
    std::cout << "data section has bytes: " << data_size << std::endl;
    std::cout << data_section;
    std::ofstream out("data.txt");
    out << data_section;
    out.close();

    return data_section;
}

int main(int argc, char* argv[])
{
//    args a(argc, const_cast<param_type*>(argv));

    auto str_check = [](const std::string& str)
    {
        if(str.empty())
        {
            printf("Could not load archive");
        }
    };
    std::string str;
    // function to copy from the LIP archive
    auto f = [&](char* p, size_t sz, int64_t from)
    {
        return str.copy(p, sz, size_t(from));
    };
    str = loadArchive("archive_test2.lip");
    str_check(str);
    lip::ptr pointers[2];
    auto const idx1 = lip::index(f, int64_t(str.size()), pointers);
    std::string data1 = readDataSection(pointers[1].offset, f);

    str.clear();
    str = loadArchive("archive_test.lip");
    str_check(str);
    auto const idx2 = lip::index(f, int64_t(str.size()), pointers);
    std::string data2 = readDataSection(pointers[1].offset, f);

    iterateIndex(idx1, idx2);
}

