// OpenBFME unit tests. GPL-3.0.
// PORT-171 (fork issue 400, P-37): OnlineTestTemp.h.

#include "OnlineTestTemp.h"

#include "doctest.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <stdexcept>
#include <system_error>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <fcntl.h>
#include <sys/param.h>
#include <unistd.h>
#endif

namespace onlinetest
{
namespace
{
#if defined(__linux__) || defined(__APPLE__)
// "data/a.log" when `file` is inside `root`, "" otherwise (both absolute, symbolic links resolved)
std::string insideRoot(const std::string &root, const std::string &file)
{
	if (file.size() <= root.size() + 1 || file.compare(0, root.size(), root) != 0 || file[root.size()] != '/')
	{
		return std::string();
	}
	return file.substr(root.size() + 1);
}
#endif
} // namespace

bool canSeeOpenFiles()
{
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
	return true;
#else
	return false;
#endif
}

std::vector<std::string> openFilesUnder(const std::filesystem::path &dir)
{
	std::vector<std::string> open;
	std::error_code ec;
#if defined(_WIN32)
	// DeleteFile needs every handle of the file to share deletion. An open that asks for DELETE access and shares everything is refused with
	// ERROR_SHARING_VIOLATION exactly when some handle does not: the file remove_all would fail on, found without touching it.
	for (std::filesystem::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
	{
		std::error_code kind;
		if (!it->is_regular_file(kind))
		{
			continue;
		}
		const HANDLE h = CreateFileW(it->path().c_str(), DELETE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, nullptr);
		if (h == INVALID_HANDLE_VALUE)
		{
			if (GetLastError() == ERROR_SHARING_VIOLATION)
			{
				open.push_back(it->path().lexically_relative(dir).generic_string());
			}
		}
		else
		{
			CloseHandle(h);
		}
	}
#elif defined(__linux__)
	const std::string root = std::filesystem::weakly_canonical(dir, ec).string();
	if (ec || root.empty())
	{
		return open;
	}
	// the descriptors of this process: /proc/self/fd/<n> is a link to the open file (an unlinked one reads "<path> (deleted)" and is not in the folder)
	for (std::filesystem::directory_iterator it("/proc/self/fd", ec), end; !ec && it != end; it.increment(ec))
	{
		std::error_code link;
		const std::filesystem::path target = std::filesystem::read_symlink(it->path(), link);
		if (link)
		{
			continue; // the iterator's own descriptor is gone by now
		}
		const std::string inside = insideRoot(root, target.string());
		if (!inside.empty())
		{
			open.push_back(inside);
		}
	}
#elif defined(__APPLE__)
	const std::string root = std::filesystem::weakly_canonical(dir, ec).string();
	if (ec || root.empty())
	{
		return open;
	}
	const int limit = getdtablesize();
	for (int fd = 0; fd < limit; ++fd)
	{
		char file[MAXPATHLEN];
		if (fcntl(fd, F_GETPATH, file) == -1)
		{
			continue;
		}
		std::error_code exists;
		if (!std::filesystem::exists(std::filesystem::path(file), exists))
		{
			continue; // unlinked: F_GETPATH keeps the last name
		}
		const std::string inside = insideRoot(root, file);
		if (!inside.empty())
		{
			open.push_back(inside);
		}
	}
#else
	(void)dir;
#endif
	std::sort(open.begin(), open.end());
	open.erase(std::unique(open.begin(), open.end()), open.end());
	return open;
}

TempDir::TempDir(const std::string &name)
{
	static std::atomic<unsigned> counter{ 0 };
	const std::filesystem::path base = std::filesystem::temp_directory_path();
	const long long stamp = (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	for (;;)
	{
		const std::filesystem::path candidate = base / (name + "_" + std::to_string(stamp) + "_" + std::to_string(counter.fetch_add(1)));
		// true only for a folder this call made: one that is already there belongs to another test or another process
		if (std::filesystem::create_directory(candidate))
		{
			m_path = candidate;
			return;
		}
	}
}

TempDir::~TempDir()
{
	// the holders are for remove() / removeOrFail(): by now what they refer to (objects declared after the folder) is destroyed, and destroyed means
	// released (a TestChild's destructor stops its process and waits). Anything that still has a file open is found below.
	m_holders.clear();
	removeOrFail();
}

void TempDir::holds(std::string who, std::function<std::string()> release)
{
	m_holders.push_back(Holder{ std::move(who), std::move(release) });
}

void TempDir::remove()
{
	if (m_removed)
	{
		return;
	}
	m_removed = true; // one attempt, one report
	std::string failure;
	for (auto it = m_holders.rbegin(); it != m_holders.rend(); ++it)
	{
		std::string why;
		try
		{
			why = it->release();
		}
		catch (const std::exception &e)
		{
			why = e.what();
		}
		if (!why.empty())
		{
			failure += "\n  " + it->who + ": " + why;
		}
	}
	m_holders.clear();
	for (const std::string &file : openFilesUnder(m_path))
	{
		failure += "\n  still open: " + file + " (close it, or destroy / shut down what writes it, before the folder is removed)";
	}
	std::error_code ec;
	std::filesystem::remove_all(m_path, ec);
	if (ec)
	{
		failure += "\n  the folder is still there: " + ec.message();
	}
	if (!failure.empty())
	{
		throw std::runtime_error("the temporary folder " + m_path.string() + " was not released:" + failure);
	}
}

void TempDir::removeOrFail() noexcept
{
	const bool first = !m_removed;
	try
	{
		remove();
	}
	catch (const std::exception &e)
	{
		// not thrown out of a destructor; the test case fails all the same, with the names
		FAIL_CHECK(e.what());
	}
	catch (...)
	{
		FAIL_CHECK("the temporary folder " << m_path.string() << " was not released");
	}
	if (!first)
	{
		// remove() already failed and said so; what held the folder then (an object destroyed since) may have let go: nothing is left behind
		std::error_code ec;
		std::filesystem::remove_all(m_path, ec);
	}
}
} // namespace onlinetest
