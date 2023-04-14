#include "cli/colorprofile.h"

std::string cli::Style::Color(unsigned char red, unsigned char green, unsigned char blue)
{
    std::string colorStr;
    colorStr.reserve(19);
    colorStr += "\033[38;2;";
    colorStr += std::to_string(red);
    colorStr += ";";
    colorStr += std::to_string(green);
    colorStr += ";";
    colorStr += std::to_string(blue);
    colorStr += "m";
    return colorStr;
}

std::string cli::Style::BGColor(unsigned char red, unsigned char green, unsigned char blue)
{
    std::string colorStr;
    colorStr.reserve(19);
    colorStr += "\033[48;2;";
    colorStr += std::to_string(red);
    colorStr += ";";
    colorStr += std::to_string(green);
    colorStr += ";";
    colorStr += std::to_string(blue);
    colorStr += "m";
    return colorStr;
}

std::string cli::FormatColor(const ColorTable& colorTable,
    const std::vector<std::string>& textLines,
    const std::vector<std::string>& colorLines)
{
    assert(textLines.size() == colorLines.size());

    struct ColorRun
    {
        size_t startIndex;
        size_t length;
        std::optional<ColorHelper> color;
    };

    auto GetColorRun = [](const ColorTable& colorTable, const std::string& color, size_t startIndex) -> ColorRun
    {
        assert(startIndex < color.size());

        ColorRun colorRun{ startIndex, 0 };
        char c = color[startIndex];
        auto colorIter = colorTable.find(c);
        if (colorIter != colorTable.end())
        {
            colorRun.color = colorIter->second;
        }

        size_t i;
        for (i = startIndex + 1; i < color.size(); ++i)
        {
            if (color[i] != c)
            {
                break;
            }
        }

        colorRun.length = i - startIndex;
        return colorRun;
    };

    auto GetNextChars = [](const std::string& str, size_t start, size_t chars) -> std::string_view
    {
        return std::string_view(str.data() + start, chars);
    };

    // TODO: Could optimize a reservation here
    std::string out;
    // Tracks utf8 index
    size_t textIndex;
    for (size_t row = 0; row < textLines.size(); ++row)
    {
        textIndex = 0;
        const std::string& textLine = textLines[row];
        const std::string& colorLine = colorLines[row];

        // Color line is required to be ANSI
        for (size_t c = 0; c < colorLine.size();)
        {
            auto colorRun = GetColorRun(colorTable, colorLine, c);

            out += colorRun.color ? Style::Color(*colorRun.color) : Style::Reset();

            size_t utf8Length =
                GetLength(std::string_view(textLine.data() + textIndex, textLine.size() - textIndex),
                    colorRun.length);

            std::string_view text = GetNextChars(textLine, textIndex, utf8Length);

            textIndex += text.length();

            out += text;
            c += colorRun.length;
        }

        out += Style::Reset();
        out += '\n';
    }

    return out;
}

std::string cli::FormatColor(const ColorTable& colorTable, const std::vector<std::string>& lines)
{
    // There should be an even number of lines
    assert(lines.size() % 2 == 0);

    std::vector<std::string> textLines;
    std::vector<std::string> colorLines;

    for (size_t i = 0; i < lines.size(); i += 2)
    {
        textLines.push_back(lines[i]);
        colorLines.push_back(lines[i + 1]);
    }

    return FormatColor(colorTable, textLines, colorLines);
}