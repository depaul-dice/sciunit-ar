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

#include <lip/lip.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include <stack>
#include <utility>
#include <new>

namespace lip
{

class file_descriptor;

class directory
{
public:
	explicit directory(char const* root) : directory(opendir(root)) {}

	auto cd(char const* dirname) -> directory;
	auto open(char const* basename, int flags) -> file_descriptor;

	auto readlink(int64_t sz, char const* basename) -> std::string
	{
		std::string buf;
		buf.resize(size_t(sz + 1));

		auto n = readlinkat(native_handle(), basename, &*buf.begin(),
		                    buf.size());
		if (n == -1)
			throw std::system_error{ errno,
				                 std::system_category() };
		else if (n != sz)
			throw std::runtime_error{ "racy symlink access" };

		buf.resize(size_t(sz));
		return buf;
	}

	feature is_executable(char const* basename) const
	{
		int r = faccessat(native_handle(), basename, X_OK, AT_EACCESS);
		if (r == -1)
		{
			if (errno == EACCES)
			{
				errno = 0;
				return {};
			}
			else
				throw std::system_error{
					errno, std::system_category()
				};
		}

		return feature::executable;
	}

	DIR* get() const { return d_.get(); }

	int native_handle() const { return dirfd(get()); }

private:
	explicit directory(DIR* r) : d_(r)
	{
		if (d_ == nullptr)
		{
			throw std::system_error{ errno,
				                 std::system_category() };
		}
	}

	struct dir_close
	{
		void operator()(DIR* d) const { closedir(d); }
	};

	std::unique_ptr<DIR, dir_close> d_;
};

class file_descriptor
{
public:
	auto get_reader() const
	{
		return [fd = fd_](char* p, size_t sz, error_code& ec) {
			auto n = read(fd, p, sz);
			if (n == -1)
				ec.assign(errno, std::system_category());
			return size_t(n);
		};
	}

	file_descriptor(file_descriptor&& other) noexcept : fd_(other.fd_)
	{
		other.fd_ = -1;
	}

	file_descriptor& operator=(file_descriptor&& other) noexcept
	{
		this->~file_descriptor();
		return *::new (static_cast<void*>(this)) auto(
		    std::move(other));
	}

	~file_descriptor()
	{
		if (fd_ != -1)
			close(fd_);
	}

	int release()
	{
		auto fd = fd_;
		fd_ = -1;
		return fd;
	}

	int native_handle() const { return fd_; }

private:
	friend class directory;

	explicit file_descriptor(int fd) : fd_(fd)
	{
		if (fd_ == -1)
			throw std::system_error{ errno,
				                 std::system_category() };
	}

	int fd_;
};

inline auto directory::cd(char const* dirname) -> directory
{
	auto file = open(dirname, O_RDONLY | O_NONBLOCK | O_DIRECTORY);
	directory d(fdopendir(file.native_handle()));
	file.release();
	return d;
}

inline auto directory::open(char const* basename, int flags) -> file_descriptor
{
	return file_descriptor(
	    openat(native_handle(), basename, flags | O_CLOEXEC));
}

inline bool is_dots(char const* dirname)
{
	using namespace stdex::literals;
	return dirname == "."_sv || dirname == ".."_sv;
}

void archive(std::function<write_sig> f, gbpath::param_type src,
             archive_options opts)
{
	std::stack<std::pair<directory, gbpath>> stk;
	packer pk;
	struct stat st;

	stk.emplace(directory(src), src);
	if (fstat(stk.top().first.native_handle(), &st) == -1)
		throw std::system_error{ errno, std::system_category() };

	pk.start(std::move(f));
	pk.add_directory(stk.top().second.friendly_name(),
	                 archive_clock::from(st.st_mtim));

	while (not stk.empty())
	{
		auto d = std::move(stk.top());
		stk.pop();

		errno = 0;
		while (auto entryp = readdir(d.first.get()))
		{
			switch (entryp->d_type)
			{
			case DT_DIR:
				if (is_dots(entryp->d_name))
					continue;
			case DT_UNKNOWN:
			case DT_REG:
			case DT_LNK: break;
			default: continue;
			}

			if (fstatat(d.first.native_handle(), entryp->d_name,
			            &st, AT_SYMLINK_NOFOLLOW) == -1)
				throw std::system_error{
					errno, std::system_category()
				};

			if (S_ISDIR(st.st_mode) &&
			    (opts.one_level || is_dots(entryp->d_name)))
				continue;

			d.second.push_back(entryp->d_name);

			switch (st.st_mode & S_IFMT)
			{
			case S_IFDIR:
				stk.emplace(d.first.cd(entryp->d_name),
				            d.second);
				pk.add_directory(
				    d.second.friendly_name(),
				    archive_clock::from(st.st_mtim));
				break;
			case S_IFREG:
			{
				auto to_copy =
				    d.first.open(entryp->d_name, O_RDONLY);
				pk.add_regular_file(
				    d.second.friendly_name(),
				    archive_clock::from(st.st_mtim),
				    to_copy.get_reader(),
				    d.first.is_executable(entryp->d_name) |
				        opts.feat);
				break;
			}
			case S_IFLNK:
				pk.add_symlink(
				    d.second.friendly_name(),
				    archive_clock::from(st.st_mtim),
				    d.first.readlink(st.st_size,
				                     entryp->d_name));
				break;
			}

			d.second.pop_back();
		}
		if (errno)
			throw std::system_error{ errno,
				                 std::system_category() };
	}

	pk.finish();
}
}
