#pragma once

#include <string>

namespace textutil
{
    struct Utf8StringInfo
    {
        size_t charCount = 0;
        size_t extraBytes = 0;	// Bytes used by utf8 extension
    };

    Utf8StringInfo GetUtf8Info(const std::string& str);

    std::string Pad(const std::string& str, size_t count);

    size_t get_n_chars_from_back_utf8(const std::string& str, size_t n);
}