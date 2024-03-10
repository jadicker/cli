#include "cli/textutil.h"

using namespace textutil;

Utf8StringInfo textutil::GetUtf8Info(const std::string& str)
{
    Utf8StringInfo info;
    bool onExtendedChar = false;

    for (size_t i = 0; i < str.size(); i++)
    {
        if ((0x80 & str[i]) > 0)
        {
            if (onExtendedChar)
            {
                info.extraBytes++;
            }
            else
            {
                onExtendedChar = true;
                info.charCount++;
            }

            continue;
        }
        onExtendedChar = false;

        info.charCount++;
    }

    return info;
}

std::string textutil::Pad(const std::string& str, size_t count)
{
    std::string out;
    out.reserve(count);
    for (size_t i = 0; i < count; ++i)
    {
        out += str;
    }
    return out;
}

size_t textutil::get_n_chars_from_back_utf8(const std::string& str, size_t n)
{
    for (int i = static_cast<int>(str.size()) - 1; i >= 0; --i)
    {
        if ((0x80 & str[i]) > 0)
        {
            continue;
        }

        --n;
        if (n == 0)
        {
            return i;
        }
    }

    return 0;
}