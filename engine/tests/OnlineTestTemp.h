// OpenBFME unit tests. GPL-3.0.
// PORT-171 (fork issue 400, P-37): the temporary folder of an online test and its removal, in one place.
//
// The cause of "remove_all: The process cannot access the file because it is being used by another process" on Windows was never another process: the
// tests called std::filesystem::remove_all(dir) while their own RelayCore was still alive, and a RelayCore keeps the log of every open game open
// (<game>.obfmerlog.part for writing, and a second handle once a LOG_REQUEST read it back; RelayCore.cpp hello() / logRequest()). Windows refuses to
// delete a file with such a handle and words the refusal as "another process"; Linux and macOS unlink it, so the tests were green there.
//
// TempDir removes its folder when it goes out of scope. Declared before the relay (or the helper process) that writes into it, it is destroyed after
// them, so their files are closed by then; and a failed REQUIRE (doctest throws) no longer leaves the folder behind. The removal:
//   1. releases what the test registered with holds() (a helper process is stopped and waited for), the last registered first;
//   2. looks for files in the folder that are still open (openFilesUnder) and names them;
//   3. removes the folder.
// Whatever did not let go fails the test by its name. There is no retry and no swallowed error: a leak is a failure on every platform, not on Windows only.
#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace onlinetest
{
// true where openFilesUnder can see open files: Windows, Linux, macOS
bool canSeeOpenFiles();

// The files under `dir` (paths relative to it, '/' separated, sorted) that are still open.
//   Windows: every file some handle of any process holds without FILE_SHARE_DELETE (what the C runtime's fopen and the standard streams do): exactly
//            the files DeleteFile refuses with ERROR_SHARING_VIOLATION.
//   Linux, macOS: every file the test process itself has a descriptor for (the OS would unlink it; the test must close it all the same, or the test
//            fails on Windows only). A helper process is not seen here: register it with TempDir::holds.
std::vector<std::string> openFilesUnder(const std::filesystem::path &dir);

class TempDir
{
public:
	// creates <the system's temporary folder>/<name>_<time>_<n>, a folder that did not exist (two tests, two processes: two folders)
	explicit TempDir(const std::string &name);
	TempDir(const TempDir &) = delete;
	TempDir &operator=(const TempDir &) = delete;
	// removes the folder if remove() did not (the open-file check and the removal, without the holders); a failure fails the running test case
	// (doctest FAIL_CHECK: a destructor does not throw)
	~TempDir();

	const std::filesystem::path &path() const { return m_path; }
	operator const std::filesystem::path &() const & { return m_path; }
	// `std::filesystem::path dir = tempDir(..)` would remove the folder at the end of that statement
	operator const std::filesystem::path &() const && = delete;
	std::string string() const { return m_path.string(); }
	template <class Part> std::filesystem::path operator/(const Part &part) const { return m_path / part; }

	// Something other than an object of this scope keeps files of the folder open (a helper process, an object that lives longer than the folder):
	// `release` makes it let go and returns "" or why it did not. Called once, by remove() / removeOrFail(), which the test calls while what the holder
	// refers to is alive. The destructor does not call it: the objects declared after the folder are destroyed by then.
	void holds(std::string who, std::function<std::string()> release);
	// A helper process the test started (TestChild): SIGTERM, then at most `limit` for it to exit. One that is still running then is killed, and the
	// removal fails with `who`.
	template <class Child> void holdsProcess(std::string who, Child &child, std::chrono::milliseconds limit = std::chrono::seconds(60))
	{
		holds(std::move(who), [&child, limit]() {
			return child.stopWithin(limit) ? std::string()
				: "the helper process did not exit within " + std::to_string((long long)limit.count()) + " ms of SIGTERM (it was killed)";
		});
	}

	// The removal (steps 1-3 above). Throws std::runtime_error that names every holder that did not release and every file still open. A second call
	// does nothing.
	void remove();
	// remove(), a failure reported as the destructor does
	void removeOrFail() noexcept;
	bool removed() const { return m_removed; }

private:
	struct Holder
	{
		std::string who;
		std::function<std::string()> release;
	};
	std::filesystem::path m_path;
	std::vector<Holder> m_holders;
	bool m_removed = false;
};
} // namespace onlinetest
