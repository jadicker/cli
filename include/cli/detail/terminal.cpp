#include "terminal.h"

namespace cli::detail
{
	void Terminal::SetLine(const std::string& newLine)
	{
		*out << beforeInput
			 << std::string(GetInputPosition(), '\b') << newLine
			 << afterInput << std::flush;

		// if newLine is shorter than currentLine, we have
		// to clear the rest of the string
		if (newLine.size() < m_currentLine.size())
		{
			*out << std::string(m_currentLine.size() - newLine.size(), ' ');
			// and go back
			*out << std::string(m_currentLine.size() - newLine.size(), '\b') << std::flush;
		}

		m_currentLine = newLine;
		m_position = m_currentLine.size() + m_promptSize;
		m_autoCompleteStart = std::string::npos;
	}

	void Terminal::BackUpToPosition(size_t newPosition)
	{
		if (newPosition >= m_position)
		{
			return;
		}

		*out << std::string(m_position - newPosition, '\b') << std::flush;
		m_position = newPosition;
	}

	void Terminal::GoBackToPosition(size_t posX, size_t posY)
	{
		assert(posY <= m_cursorY);
		Up(m_cursorY - posY);
		Advance(posX);
	}

	void Terminal::CompleteLine()
	{
		const auto pos = static_cast<std::string::difference_type>(m_position - m_promptSize);

		*out << beforeInput
			 << std::string(m_currentLine.begin() + pos, m_currentLine.end())
			 << afterInput << std::flush;
		m_position = m_promptSize + m_currentLine.size();
	}

	void Terminal::InsertText(std::string str)
	{
		if (str.empty())
		{
			return;
		}

		std::string& currentLine = m_cursorY == 0 ? m_currentLine : m_nextLines[m_cursorY - 1];
		size_t position = m_cursorY == 0 ? GetInputPosition() : m_position;

		const size_t remainingWidth = m_terminalWidth - m_position;
		size_t lineWidth = position + str.size();
		if (lineWidth > remainingWidth)
		{
			str.resize(remainingWidth - 4);
			str += " ..";
		}

		currentLine.insert(position, str.c_str());
		m_position += str.size();
		*out << str << std::flush;
	}

	void Terminal::TrimEnd(size_t toPosition, bool moveBack)
	{
		size_t oldSize = m_currentLine.size();
		if (oldSize <= toPosition)
		{
			return;
		}
		m_currentLine.erase(toPosition);
		m_position = toPosition;
		if (moveBack)
		{
			*out << std::string(oldSize - toPosition, '\b') << std::flush;
		}
	}

	std::pair<Symbol, std::string> Terminal::Keypressed(std::pair<KeyType, char> k)
	{
		switch (k.first)
		{
			case KeyType::eof:
				return std::make_pair(Symbol::eof, std::string{});
				break;
			case KeyType::backspace:
			{
				// Clear auto-complete even if we don't have any input
				if (m_autoCompleteStart != std::string::npos && m_position <= m_autoCompleteStart)
				{
					ClearAutoComplete();
					m_autoCompleteStart = std::string::npos;
					break;
				}

				if (GetInputPosition() == 0)
				{
					break;
				}

				--m_position;

				const auto pos = static_cast<std::string::difference_type>(GetInputPosition());
				// remove the char from buffer
				m_currentLine.erase(m_currentLine.begin() + pos);
				// go back to the previous char
				*out << '\b';
				// output the rest of the line
				*out << beforeInput;
				*out << std::string(m_currentLine.begin() + pos, m_currentLine.end());
				*out << afterInput;
				// remove last char
				*out << ' ';
				// go back to the original position
				*out << std::string(m_currentLine.size() - GetInputPosition() + 1, '\b') << std::flush;

				break;
			}
			case KeyType::up:
				return std::make_pair(Symbol::up, std::string{});
				break;
			case KeyType::down:
				return std::make_pair(Symbol::down, std::string{});
				break;
			case KeyType::left:
				if (GetInputPosition() > 0)
				{
					*out << '\b' << std::flush;
					--m_position;
				}
				break;
			case KeyType::right:
				if (GetInputPosition() < m_currentLine.size())
				{
					*out << beforeInput
						 << m_currentLine[GetInputPosition()]
						 << afterInput << std::flush;
					++m_position;
				}
				break;
			case KeyType::ret:
			{
				size_t lastValidPosition = GetInputPosition();
				if (lastValidPosition >= m_currentLine.size())
				{
					lastValidPosition = m_currentLine.size() - 1;
				}

				auto cmd = m_currentLine;

				TryFinishAutoComplete();

				*out << "\r\n";

				m_currentLine.clear();

				// TODO: Not actually accurate until prompt is issued again.  In practice this is ok?
				m_position = m_promptSize;

				return std::make_pair(Symbol::command, cmd);
			}
				break;
			case KeyType::ascii:
			{
				const char c = static_cast<char>(k.second);
				if (c == '\t')
				{
					return std::make_pair(Symbol::tab, std::string());
				}
				else
				{
					if (m_autoCompleteStart != std::string::npos)
					{
						const char autoCompleteChar = ' ';
						if (m_position < m_autoCompleteStart)
						{
							ClearAutoComplete();
						}
						else if (c == autoCompleteChar)
						{
							if (TryFinishAutoComplete())
							{
								break;
							}
						}
						else if(m_autoCompleteStart == m_position)
						{
							// Typing mid-token, need to increment
							++m_autoCompleteStart;
						}
					}

					const auto pos = static_cast<std::string::difference_type>(GetInputPosition());

					// output the new char:
					*out << beforeInput << c;
					// and the rest of the string:
					*out << std::string(m_currentLine.begin() + pos, m_currentLine.end())
						 << afterInput;

					// go back to the original position
					*out << std::string(m_currentLine.size() - GetInputPosition(), '\b') << std::flush;

					// update the buffer and cursor position:
					m_currentLine.insert(m_currentLine.begin() + pos, c);
					++m_position;
				}
				break;
			}
			case KeyType::canc:
			{
				if (GetInputPosition() == m_currentLine.size())
					break;

				const auto pos = static_cast<std::string::difference_type>(GetInputPosition());

				// output the rest of the line
				*out << std::string(m_currentLine.begin() + pos + 1, m_currentLine.end());
				// remove last char
				*out << ' ';
				// go back to the original position
				*out << std::string(m_currentLine.size() - GetInputPosition(), '\b') << std::flush;
				// remove the char from buffer
				m_currentLine.erase(m_currentLine.begin() + pos);
				break;
			}
			case KeyType::end:
			{
				CompleteLine();
				break;
			}
			case KeyType::home:
			{
				BackUpToPosition(m_promptSize);
				break;
			}
			case KeyType::ignored:
				// TODO
				break;
		}

		return std::make_pair(Symbol::nothing, std::string());
	}

	size_t Terminal::GetParamIndex(const std::string&) const
	{
		return GetParamInfo(m_currentLine, GetInputPosition()).index;
	}

	void Terminal::SetLineStart(size_t start)
	{
		auto difference = static_cast<int>(m_promptSize) - static_cast<int>(start);
		m_position -= difference;

		m_promptSize = start;
	}

	void Terminal::TestFill2Lines()
	{
		size_t start = m_promptSize + m_currentLine.size();
		*out << std::string(m_terminalWidth - start, '=') << std::flush;
		*out << std::string(m_terminalWidth, '+') << std::flush;
		*out << std::string(m_terminalWidth * 2 - start, '\b') << std::flush;

		// Front of previous line
		Up(1);

		// Advance back to position
		Advance(m_promptSize + m_position);

		*out << std::flush;
	}

	std::pair<std::string, size_t> Terminal::PrepareAutoCompletedLine(const std::string& line, size_t position)
	{
		auto token = GetToken(line, position);
		if (!token)
		{
			return { "", 0 };
		}

		if (position >= line.size())
		{
			auto params = detail::split(line);
			if (!std::isspace(line.back()))
			{
				return { line + " ", params.size() };
			}
			else
			{
				return { line, params.size() };
			}
		}

		size_t cursor = position;
		if (cursor >= line.size() && std::isspace(line.back()) ||
			std::isspace(line[cursor]))
		{
			std::string newLine = line.substr(0, cursor);
			std::vector<std::string> params;
			detail::split(params, newLine);
			return { newLine, params.size() };
		}

		auto lineToCursor = line.substr(0, position);
		std::vector<std::string> params;
		detail::split(params, lineToCursor);
		if (params.empty())
		{
			return { lineToCursor, 0 };
		}

		const bool onNewParam = std::isspace(line[cursor]);
		const size_t currentParam = onNewParam ? params.size() : params.size() - 1;
		return { lineToCursor, currentParam };
	}

	std::pair<std::string, size_t> Terminal::GetAutoCompleteLine() const
	{
		if (m_currentLine.empty())
		{
			return { "", 0 };
		}

		auto paramInfo = GetParamInfo(m_currentLine, GetInputPosition());
		return { m_currentLine.substr(0, GetInputPosition()), paramInfo.index };
	}

	Terminal::ParamInfo Terminal::GetParamInfo(const std::string& line, const size_t pos)
	{
		if (line.empty())
		{
			return {0, 0, 0};
		}

		if (pos < line.size() && std::isspace(line[pos]))
		{
			return {};
		}

		// pos is at the very last param
		if (pos >= line.size())
		{
			size_t index = detail::split(line).size();
			if (index == 0)
			{
				// In case of all whitespace
				return { pos, pos, 0 };
			}

			if (std::isspace(line.back()))
			{
				return { pos, pos, index };
			}

			size_t i = line.size() - 1;
			while (i > 0)
			{
				if (std::isspace(line[i]))
				{
					return { i + 1, line.size() - 1, index - 1 };
				}
				--i;
			}

			return { 0, line.size() - 1, index - 1 };
		}

		// Note: eats leading whitespace
		bool onWhitespace = static_cast<bool>(std::isspace(line[0]));
		size_t paramStart = 0;
		int index = -1;
		for (size_t i = 1; i < line.size(); ++i)
		{
			const bool spaceMatches = static_cast<bool>(std::isspace(line[i])) == onWhitespace;
			if (!spaceMatches)
			{
				if (!onWhitespace)
				{
					++index;
					if (i > pos)
					{
						return { paramStart, i - 1, static_cast<size_t>(index) };
					}
				}
				else
				{
					paramStart = i;
				}

				onWhitespace = !onWhitespace;
			}
		}

		// We can't be on a space because we haven't passed pos yet and pos is not on a space.
		// The only remaining case is that pos is in the last token, so return it
		return { paramStart, line.size() - 1, static_cast<size_t>(++index) };
	}

	void Terminal::Advance(size_t size)
	{
		if (size == 0) { return; }

		std::stringstream token;
		token << "\033[" << size << "C";
		*out << token.str();

		m_position += size;

		if (GetInputPosition() > m_currentLine.size())
		{
			m_currentLine.append(std::string(GetInputPosition() - m_currentLine.size(), ' '));
		}
	}

	void Terminal::Reverse(size_t size)
	{
		if (size == 0) { return; }
		if (size > m_position) { size = m_position; }

		*out << std::string(size, '\b');
		m_position -= size;
	}

	void Terminal::Up(size_t lines)
	{
		// BUG:  If you scroll down into "empty buffer space" and run this it
		//       scrolls back up in an undesirable way
		// It's possible this isn't worth fixing...
		if (lines == 0 || m_cursorY == 0) { return; }

		std::stringstream token;
		token << "\033[" << lines << "F";
		*out << token.str();

		// Moves to start of line
		m_position = 0;
		m_cursorY -= lines;
	}

	void Terminal::Down(size_t lines)
	{
		if (lines == 0) { return; }

		std::stringstream token;
		token << "\033[" << lines << "E";
		*out << token.str();

		// Moves to start of line
		m_position = 0;
		m_cursorY += lines;
	}

	void Terminal::ClearAhead(size_t size)
	{
		if (size == std::numeric_limits<size_t>::max())
		{
			size = m_currentLine.size() - GetInputPosition();
		}

		size_t position = GetInputPosition();
		if (m_currentLine.size() < position + size)
		{
			return;
		}

		m_currentLine.erase(position, size);
		*out << std::string(size, ' ') << std::string(size, '\b') << std::flush;
	}

	void Terminal::ClearBehind(size_t size)
	{
		if (size == 0) { return; }
		if (size > GetInputPosition()) { size = GetInputPosition(); }

		*out << std::string(size, '\b') << std::string(size, ' ') << std::string(size, '\b');
		m_currentLine.erase(GetInputPosition() - size, size);
		m_position -= size;
	}

	std::optional<std::pair<size_t, size_t>> Terminal::GetToken(const std::string& line, size_t position)
	{
		if (line.empty())
		{
			return {};
		}

		// If we don't have a space at the end and we're past the line by one, we are
		// auto-completing with a filter
		const bool autoCompleteWithFilter = position == line.size() && line.back() != ' ';
		if ((!autoCompleteWithFilter && position >= line.size()) ||
			(position < line.size() && std::isspace(line[position])))
		{
			// This position is not on a param
			return {};
		}

		if (autoCompleteWithFilter)
		{
			// Walk back onto the last param, which we're 1 past than by definition
			position--;
		}

		size_t start = position;
		while (start > 0 && !std::isspace(line[start]))
		{
			--start;
		}

		if (std::isspace(line[start]))
		{
			++start;
		}

		size_t end = position;
		while (end < line.size() && !std::isspace(line[end]))
		{
			++end;
		}

		if (end < line.size() && std::isspace(line[end]))
		{
			--end;
		}

		return std::pair<size_t, size_t>{ start, end - start };
	}

	void Terminal::ClearAutoComplete()
	{
		if (m_autoCompleteStart == std::string::npos)
		{
			return;
		}

		auto token = GetToken(GetInputPosition());
		if (!token)
		{
			// Nothing to clear...
			return;
		}

		const size_t tokenEnd = token->first + token->second;
		if (tokenEnd > GetInputPosition())
		{
			ClearAhead(tokenEnd - GetInputPosition());
		}

		ClearToCurrent();
	}

	bool Terminal::TryFinishAutoComplete()
	{
		if (m_autoCompleteStart == std::string::npos ||
			m_position >= m_currentLine.size() + m_promptSize)
		{
			return false;
		}

		auto token = GetToken(GetInputPosition());
		ClearNextLines();
		size_t endPos = token->first + token->second;

		// Re-print in regular text
		Reverse(m_position - (token->first + m_promptSize));
		*out << beforeInput
			 << std::string(m_currentLine.begin() + token->first,
							m_currentLine.begin() + endPos)
			 << afterInput << std::flush;
		m_position = m_promptSize + endPos;

		// Space to next param
		// TODO: Use parameter completion to know if you're done or not I guess
		InsertText(" ");

		m_autoCompleteStart = std::string::npos;

		return true;
	}

	void Terminal::ClearNextLines()
	{
		size_t oldPosition = m_position;
		bool clearedLines = m_nextLines.size() > 0;
		while (m_nextLines.size() > 0)
		{
			assert(m_cursorY <= (GetLineCount() - 1));
			size_t dist = (GetLineCount() - 1) - m_cursorY;
			Down(dist);
			*out << std::string(m_terminalWidth, ' ');
			Up(dist);
			m_nextLines.pop_back();
		}

		// m_position stays constant this whole time
		if (clearedLines)
		{
			Advance(oldPosition);
		}
	}

	void Terminal::ClearToCurrent()
	{
		ClearNextLines();
		if (!m_currentLine.empty() && !std::isspace(m_currentLine.back()))
		{
			auto lastSpace = std::find_if(m_currentLine.rbegin(), m_currentLine.rend(),
										  [](char c) { return std::isspace(c); });
			if (lastSpace != m_currentLine.rend())
			{
				int dist = static_cast<int>(std::distance(m_currentLine.rbegin(), lastSpace));
				dist = std::min<int>(dist, static_cast<int>(m_currentLine.size()) - static_cast<int>(GetInputPosition()));
				if (dist >= 1)
				{
					Advance((m_promptSize + m_currentLine.size()) - m_position);
					ClearBehind(dist);
				}
			}
		}

		*out << std::flush;
	}

	void Terminal::ClearCurrentLine()
	{
		ClearNextLines();
		Reverse(m_position - m_promptSize);
		ClearAhead();

		*out << std::flush;
	}

	void Terminal::CreateLines(size_t count)
	{
		SavePosition save(this);
		for (size_t i = 0; i < count; ++i)
		{
			*out << "\r\n";
		}
		m_cursorY += count;
	}

	void Terminal::AddLine(const std::string& line)
	{
		Down(1);
		*out << line << std::flush;
		m_nextLines.push_back(line);
		m_position = line.size();
	}

	void Terminal::Reset()
	{
		if (m_silent)
		{
			m_currentLine = "";
			m_nextLines.clear();
			m_autoCompleteStart = std::string::npos;
			m_position = 0;
			m_cursorY = 0;
			m_promptSize = 0;
			m_silent = false;
		}
		// TODO: Do we need a non-silent reset?
	}

	size_t ParamList::Print(Terminal& t, size_t paramStartPos, size_t index, size_t style)
	{
		if (m_params.empty())
		{
			// TODO: Show no valid/additional completions
			return 0;
		}

		if (m_params.size() == 1)
		{
			t.AddLine(std::string(paramStartPos, ' '));
			t.InsertText("^> ");
			t.InsertText(m_params[0].description);
			return 1;
		}

		if (style == 0)
		{
			// ^ [a b c d] going forward
			//size_t width = (t.m_terminalWidth - paramStartPos) + 2;

			t.AddLine(std::string(paramStartPos, ' '));
			t.InsertText("^ [");

			*t.out << rang::fg::blue;

			std::string completionSuffix;
			for (size_t i = 1; i < m_params.size(); ++i)
			{
				completionSuffix += m_params[i].text;
				if (i != m_params.size() - 1)
				{
					completionSuffix += "  ";
				}
			}

			t.InsertText(completionSuffix);

			*t.out << rang::style::reset;

			t.InsertText("]");
		}
		else if (style == 1)
		{
			//size_t clampedIndex = index % m_params.size();
			//m_params.insert(m_params.end(), m_params.begin(), m_params.begin() + clampedIndex);
			//m_params.erase(m_params.begin(), m_params.begin() + clampedIndex);

			// prompt> cmd parm
			//  [.. b c d] ^ <desc text>
			// ^          ^ > subtract these from the width
			size_t width = paramStartPos - 3;

			size_t paramCount = 0;
			std::vector<std::string> paramList;
			size_t spaceSize = 1;
			size_t remainingSize = width - 2; // Subtract '[' and ']'
			// This much remaining room is needed to squeeze one last param in.  Includes the space
			constexpr size_t minFinalTokenWidth = 6;
			{
				// See how many we can fit with 1 space
				for (size_t i = 1; i < m_params.size(); ++i)
				{
					const size_t size = m_params[i].text.size() +
										(paramCount > 1 ? spaceSize : 0);
					if (remainingSize < size)
					{
						break;
					}

					++paramCount;
					remainingSize -= size;
				}

				for (size_t i = 0; i < paramCount; ++i)
				{
					paramList.push_back(m_params[i + 1].text);
				}

				if (paramCount < (m_params.size() - 1) &&
					paramCount > 0 &&   // lastCertainParam is invalid with no params
					remainingSize >= minFinalTokenWidth)
				{
					// Cram in an abbreviated param, we have a param and room.  Leave room for a space.
					std::string finalParam = m_params[paramCount].text;
					finalParam.replace(minFinalTokenWidth - 3, 2, "..");
					finalParam = finalParam.substr(0, minFinalTokenWidth - 1);

					// Sneak in extra first param
					remainingSize -= (finalParam.size() + spaceSize);
					paramList.push_back(std::move(finalParam));
				}
				else if (paramCount > 0 && remainingSize > 0)
				{
					// Fiddle with spacing instead, we can't fit anything else in
					size_t oldSpaceSize = spaceSize;
					spaceSize = oldSpaceSize + (remainingSize / paramCount);
					if (spaceSize > oldSpaceSize)
					{
						remainingSize = remainingSize - ((paramCount - 1) * (spaceSize - oldSpaceSize));
					}
				}
			}

			std::reverse(paramList.begin(), paramList.end());

			// Extra space at the start
			t.AddLine(std::string(remainingSize + 1, ' '));
			t.InsertText("[");
			*t.out << rang::fg::blue;

			for (size_t i = 0; i < paramList.size(); ++i)
			{
				t.InsertText(paramList[i]);
				if (i != paramList.size() - 1)
				{
					t.InsertText(std::string(spaceSize, ' '));
				}
			}
			*t.out << rang::style::reset;
			t.InsertText("] ");

			t.InsertText("^> ");
			t.InsertText(m_params[0].description);
			return 1;
		}
		else if (style == 2)
		{
#if 0
			size_t remainingSize = t.GetTerminalWidth(); // Subtract '[' and ']'

            std::string completionSuffix;
            for (size_t i = 1; i < m_params.size(); ++i)
            {
                completionSuffix += m_params[i].text;
                if (i != m_params.size() - 1)
                {
                    completionSuffix += "  ";
                }
            }

            // ^ [a b c d] going forward
            //size_t width = (t.m_terminalWidth - paramStartPos) + 2;

            t.AddLine(std::string(paramStartPos, ' '));
            t.InsertText("^ [");

            *t.out << rang::fg::blue;

            std::string completionSuffix;
            for (size_t i = 1; i < m_params.size(); ++i)
            {
                completionSuffix += m_params[i].text;
                if (i != m_params.size() - 1)
                {
                    completionSuffix += "  ";
                }
            }

            t.InsertText(completionSuffix);

            *t.out << rang::style::reset;

            t.InsertText("]");
#endif
		}

		return 0;
	}

	void Terminal::SetCompletions(size_t param, const std::vector<AutoCompletion>& completions, const std::string& cmdDesc)
	{
		assert(!completions.empty());

		ParamInfo paramInfo = GetParamInfo(m_currentLine, GetInputPosition());
		if (m_autoCompleteStart == std::string::npos)
		{
			m_autoCompleteStart = m_position;
		}

		ClearAutoComplete();

		// Uses 2 additional lines, make sure we have room
		CreateLines(2);

		auto firstCompletion = completions[0].text;
		std::string paramToken = m_currentLine.substr(paramInfo.startPos,
													  (GetInputPosition() - paramInfo.startPos) + 1);
		bool paramTokenStartsCompletion = completions[0].text.find(paramToken) == 0;
		if (paramTokenStartsCompletion &&
			m_autoCompleteStart >= (paramInfo.startPos + m_promptSize))
		{
			const size_t autoCompleteStartIndex = (m_autoCompleteStart - m_promptSize) - paramInfo.startPos;
			if (firstCompletion.size() > autoCompleteStartIndex)
			{
				firstCompletion = firstCompletion.substr(autoCompleteStartIndex);
			}
		}


		//paramToken = transformForStupidObjectIdCase(paramToken, completions[0].text);

		if (!paramTokenStartsCompletion)
		{
			// Our filter failed, so wipe it
			// TODO: This is a choice, instead we could accept nothing and leave the param as-is
			ClearBehind(paramToken.size());
			m_autoCompleteStart = m_position;
		}

		*out << rang::fg::yellow;
		InsertText(firstCompletion);
		*out << rang::style::reset;

		ParamList paramList(completions);
		size_t linesPrinted = paramList.Print(*this, m_autoCompleteStart, paramInfo.index, 1);

		// Add description line
		AddLine(std::string(m_autoCompleteStart, ' '));
		InsertText(cmdDesc);

		Up(1 + linesPrinted);
		Advance(m_autoCompleteStart);

		*out << std::flush;
	}

}