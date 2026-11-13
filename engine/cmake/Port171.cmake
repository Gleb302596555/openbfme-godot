# PORT-171 (fork issue 400, P-37): the online tests' temporary folder (tests/OnlineTestTemp.h). Test infrastructure only: no engine source.
#
# The Windows failure "remove_all: The process cannot access the file because it is being used by another process" (online3 rejoin, online16-fix,
# online3 spectators, onlineux1 r2) was the test process's own handle: the tests removed the folder while their RelayCore, which keeps every open
# game's log open, was still alive. onlinetest::TempDir owns the folder: it is removed when the scope ends (after the relay declared after it),
# helper processes registered with it are stopped and waited for first, and a file that is still open fails the test by its name on every platform.
# OnlineTestTemp.cpp is the helper the online test files (cmake/Online2.cmake) share; it is listed here with its own tests.
set(OPENBFME_PORT171_TESTS
    tests/OnlineTestTemp.cpp
    tests/test_port_171.cpp)
