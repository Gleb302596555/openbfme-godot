#include "doctest.h"

#include <cstdlib>
#include <string>

namespace {
int run_port268_test(const char *test) {
#ifdef _WIN32
    const char *python = "python";
#else
    const char *python = "python3";
#endif
    // OPENBFME_DOCS_DIR is configured by CMake, independent of the current directory.
    const std::string command = std::string(python) + " \"" + OPENBFME_DOCS_DIR +
        "/../tools/mod/stop_groups_selftest.py\" Grouping." + test;
    return std::system(command.c_str());
}
}

TEST_CASE("PORT-268: citations within a function and stop dependencies cluster") {
    CHECK(run_port268_test("test_same_function_and_reference") == 0);
}

TEST_CASE("PORT-268: unplaced and closed rows remain separate") {
    CHECK(run_port268_test("test_unplaced_and_closed") == 0);
}

TEST_CASE("PORT-268: oversized groups split by function without losing stops") {
    CHECK(run_port268_test("test_large_component_splits_by_function") == 0);
}

TEST_CASE("PORT-268: named classes and cross-group references remain visible") {
    CHECK(run_port268_test("test_class_names_and_split_reference") == 0);
}
