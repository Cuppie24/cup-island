#pragma once
// Tiny test helper: no framework, no macros. Failures are counted and printed
// with the caller's file:line.

#include <cstdio>
#include <source_location>
#include <string_view>

namespace cup::test {

inline int& failures()
{
    static int count = 0;
    return count;
}

inline void expect(bool ok, std::string_view what, std::source_location at = std::source_location::current())
{
    if (ok) return;
    ++failures();
    std::printf("FAIL %s:%u: %.*s\n", at.file_name(), static_cast<unsigned>(at.line()),
                static_cast<int>(what.size()), what.data());
}

} // namespace cup::test
