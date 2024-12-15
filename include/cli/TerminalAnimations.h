#pragma once

#include <memory>
#include <vector>
#include <cassert>

#include "utf8.h"

#include "MechSim/Terminal/Console.h"
#include "MechSim/Misc/Timeline.h"


class AnimatedTerminalTests;

#define TERMINAL_BUFFER_CHECKED 1

#if TERMINAL_BUFFER_CHECKED
#define terminalAssert(expr) assert((expr))
#else
#define terminalAssert(expr)
#endif

namespace cli
{

inline std::vector<uint32_t> Utf8StrToUtf32Str(const std::string& utf8Str)
{
	std::vector<uint32_t> utf32Str;
	utf8::utf8to32(utf8Str.begin(), utf8Str.end(), std::back_inserter(utf32Str));

	return utf32Str;
}

inline uint32_t Utf8CharToUtf32Char(const std::string& utf8Char)
{
	assert(MechSim::Str::Utf8Len(utf8Char) == 1);

	auto utf32Buffer = Utf8StrToUtf32Str(utf8Char);

	assert(utf32Buffer.size() == 1);
	return utf32Buffer[0];
}

inline std::string Utf32CharToUtf8Char(uint32_t utf32Char)
{
	std::string utf8Str;
	std::vector<uint32_t> utf32Wrapper(1);
	utf32Wrapper[0] = utf32Char;
	utf8::utf32to8(utf32Wrapper.begin(), utf32Wrapper.end(), std::back_inserter(utf8Str));
	return utf8Str;
}

inline std::string Utf32StrToUtf8Str(const std::vector<uint32_t>& utf32Str)
{
	std::string utf8Str;
	utf8::utf32to8(utf32Str.begin(), utf32Str.end(), std::back_inserter(utf8Str));
	return utf8Str;
}

inline std::string Utf32BufferToUtf8Str(const std::vector<uint32_t>& utf32Str, size_t begin, size_t end)
{
	terminalAssert(begin < end);
	terminalAssert(end <= utf32Str.size());

	std::string utf8Str;
	utf8::utf32to8(utf32Str.begin() + begin, utf32Str.begin() + end, std::back_inserter(utf8Str));
	return utf8Str;
}

struct TerminalPos
{
	size_t row;
	size_t col;
};

class TerminalBuffer
{
public:
	TerminalBuffer(size_t width, size_t height, bool transparentClear = true)
		: m_transparentClear(transparentClear)
	{
		Resize(width, height);
	}

	void Resize(size_t width, size_t height)
	{
		m_width = width;
		m_height = height;
		m_utf32Buffer.resize(width * height);
		m_opacities.resize(width * height);
		Clear();
	}

	size_t GetIndex(size_t row, size_t col) const
	{
		return GetIndex({row, col});
	}

	size_t GetIndex(TerminalPos pos) const
	{
		size_t index = pos.row * m_width + pos.col;
		terminalAssert(index < m_utf32Buffer.size());
		return index;
	}

	uint32_t GetChar(size_t row, size_t col) const
	{
		return m_utf32Buffer[GetIndex(row, col)];
	}

	uint32_t GetChar(TerminalPos pos) const
	{
		return GetChar(pos.row, pos.col);
	}

	uint8_t GetOpacity(TerminalPos pos) const
	{
		return m_opacities[GetIndex(pos.row, pos.col)];
	}

	std::string GetUtf8Line(size_t row) const
	{
		auto b = GetIndex(row, 0);
		auto e = b + m_width;

		return Utf32BufferToUtf8Str(m_utf32Buffer, b, e);
	}

	std::string GetUtf8Buffer() const
	{
		return Utf32BufferToUtf8Str(m_utf32Buffer, 0, m_utf32Buffer.size());
	}

	std::string GetUtf8String(size_t row, size_t colBegin, size_t colEnd) const
	{
		auto b = GetIndex(row, colBegin);
		auto e = b + colEnd - colBegin;

		return Utf32BufferToUtf8Str(m_utf32Buffer, b, e);
	}

	void SetChar(size_t row, size_t col, const std::string& utf8Char)
	{
		auto utf32Char = Utf8CharToUtf32Char(utf8Char);
		m_utf32Buffer[GetIndex(row, col)] = utf32Char;
	}

	void SetOpacity(TerminalPos pos, uint8_t opacity)
	{
		m_opacities[GetIndex(pos)] = opacity;
	}

	void SetChar(size_t row, size_t col, uint32_t c)
	{
		m_utf32Buffer[GetIndex(row, col)] = c;
	}

	size_t ClipCol(size_t col) const
	{
		return col >= m_width ? m_width - 1 :  col;
	}

	bool SetStr(size_t row, size_t col, const std::vector<uint32_t>& str)
	{
		// String cannot wrap into the next line, stricter than end of buffer
		if (GetIndex(row, col) + str.size() > m_utf32Buffer.size()) {
			return false;
		}

		std::memcpy(&m_utf32Buffer[GetIndex(row, col)], &str[0], str.size() * sizeof(uint32_t));
		return true;
	}

	void SetStrClipped(size_t row, size_t col, const std::vector<uint32_t>& str)
	{
		terminalAssert(col < m_width);
		terminalAssert(row < m_height);

		const size_t maxWidth = (m_width - col);
		const size_t clippedWidth = std::min(str.size(), maxWidth);

		const size_t b = GetIndex(row, col);
		for (size_t i = 0; i < str.size(); ++i)
		{
			m_utf32Buffer[b + i] = str[i];
		}
	}

	void SetStrClipped(size_t row, size_t col, const std::string& str)
	{
		SetStrClipped(row, col, Utf8StrToUtf32Str(str));
	}

	static const uint32_t utf32Space()
	{
		static const uint32_t utf32Space = Utf8CharToUtf32Char(" ");
		return utf32Space;
	}

	void Clear()
	{
		std::fill(m_utf32Buffer.begin(), m_utf32Buffer.end(), utf32Space());
		std::fill(m_opacities.begin(), m_opacities.end(), m_transparentClear ? 0x00 : 0xFF);
	}

private:
	size_t m_width;
	size_t m_height;
	bool m_transparentClear;

	// Row-wise buffer
	std::vector<uint32_t> m_utf32Buffer;
	std::vector<uint8_t> m_opacities;
};

struct TerminalBufferData
{
	TerminalPos pos = { 0, 0 };
	size_t bufferIndex = 0u;
	TerminalBuffer* buffer = nullptr;
	const std::vector<const TerminalBuffer*> allBuffers;
};

class Animation
{
public:
	Animation(TerminalBufferData data)
		: m_data(data)
	{
	}

	virtual ~Animation() {}

	virtual void Update(const float dt) = 0;

	TerminalPos GetPos() const { return m_data.pos; }

	bool Complete() const { return m_complete; }

protected:
	bool m_complete = false;
	TerminalBufferData m_data;
};

class ScrollAnimation : public Animation
{
public:
	ScrollAnimation(TerminalBufferData data, const std::string& utf8Str, float duration)
		: Animation(std::move(data))
		, m_utf32Str(Utf8StrToUtf32Str(utf8Str))
		, m_time(0.0f)
		, m_duration(duration)
	{
		const size_t lastColumnIndex = m_data.pos.col + m_utf32Str.size() - 1;
		m_endCol = m_data.buffer->ClipCol(lastColumnIndex);
		m_outputSize = (m_endCol + 1) - m_data.pos.col;
	}

	void Update(const float dt) override
	{
		m_time += dt;
		Tick(m_time / m_duration);

		if (m_time >= m_duration)
		{
			m_complete = true;
			m_time = m_duration;
		}
	}

private:
	void Tick(const float t)
	{
		// t is 0..1
		const size_t chars = static_cast<size_t>(static_cast<float>(m_utf32Str.size()) * t);

		const size_t spaces = std::min(m_utf32Str.size() - chars, m_outputSize);
		std::vector<uint32_t> str;
		str.resize(m_outputSize, TerminalBuffer::utf32Space());
		for (size_t i = 0; i < spaces; ++i)
		{
			m_data.buffer->SetOpacity({m_data.pos.row, m_data.pos.col + i}, 0x00);
		}

		size_t i = 0;
		for (auto iter = str.begin() + spaces; iter != str.end(); ++iter)
		{
			m_data.buffer->SetOpacity({m_data.pos.row, m_data.pos.col + spaces + i}, 0xFF);
			*iter = m_utf32Str[i++];
		}
		m_data.buffer->SetStrClipped(m_data.pos.row, m_data.pos.col, str);
	}

	std::vector<uint32_t> m_utf32Str;
	size_t m_endCol;
	size_t m_outputSize;

	float m_time;
	const float m_duration;
};

class AnimatedTerminal
{
public:
	AnimatedTerminal(size_t width, size_t height)
		: m_width(width)
		, m_height(height)
	{
		AddLayer(false);
	}

	void AddLayer(bool transparentClear = true)
	{
		m_layers.push_back(TerminalBuffer(m_width, m_height, transparentClear));
	}

	std::string ResolveBuffer()
	{
		return m_layers[0].GetUtf8Buffer();
	}

	// Append utf8 string str at current cursor position and clip to row
	void AppendClipped(const std::string& str)
	{
		GetPrimary().SetStrClipped(m_cursorRow, m_cursorCol, str);
	}

	// Append utf8 string str at current cursor position.  Returns a string of
	// all characters that need to be written out/"fell off the terminal".
	std::string Append(const std::string& str)
	{
		auto utf32Str = Utf8StrToUtf32Str(str);
		size_t requiredSize = m_cursorCol + utf32Str.size();
		size_t lines = requiredSize / m_width;
		size_t finalCol = m_width - 1;

		if (requiredSize > lines * m_width) {
			finalCol = requiredSize - (lines * m_width);
			++lines;
		}

		std::string writeOut;

		size_t linesToTrim = m_height - lines;
		size_t bufferLinesToTrim = std::min(linesToTrim, m_height);
		for (size_t i = 0; i < bufferLinesToTrim; ++i) {
			writeOut += GetPrimary().GetUtf8Line(i);
		}

		if (linesToTrim > bufferLinesToTrim) {
			size_t trimChars = (linesToTrim - bufferLinesToTrim) * m_width;
			writeOut += Utf32BufferToUtf8Str(utf32Str, 0, trimChars);
			utf32Str.erase(utf32Str.begin(), utf32Str.begin() + trimChars);
		}

		GetPrimary().SetStr(m_cursorRow, m_cursorCol, utf32Str);
		m_cursorRow = m_cursorRow + lines - 1;
		m_cursorCol = finalCol;

		return writeOut;
	}

	void CursorAdvance(size_t chars)
	{
		const size_t rows = chars / m_width;
		const size_t col = m_cursorCol + chars - (rows * m_width);
		const size_t row = std::min(m_cursorRow + rows, m_height);

		m_cursorCol = col;
		m_cursorRow = row;
	}

	void Clear()
	{
		m_cursorCol = 0;
		m_cursorRow = 0;

		for (auto& buffer : m_layers)
		{
			buffer.Clear();
		}
	}

	const std::vector<const TerminalBuffer*> GetAllBuffers() const
	{
		// TODO: This is so fucking stupid
		std::vector<const TerminalBuffer*> result;
		result.reserve(m_layers.size());
		for (const auto& layer : m_layers)
		{
			result.push_back(&layer);
		}

		return result;
	}

	uint32_t GetUtf32Char(TerminalPos pos) const
	{
		for (auto iter = m_layers.rbegin(); iter != m_layers.rend(); ++iter)
		{
			constexpr uint8_t opaque = 0xF;
			if (iter->GetOpacity(pos) > opaque)
			{
				return iter->GetChar(pos);
			}
		}

		return TerminalBuffer::utf32Space();
	}


	std::string GetUtf8Line(size_t row) const
	{
		std::vector<uint32_t> line;
		line.reserve(m_width);
		for (size_t col = 0; col < m_width; ++col)
		{
			line.push_back(GetUtf32Char({row, col}));
		}
		return Utf32StrToUtf8Str(line);
	}

private:
	TerminalBuffer& GetPrimary() { return m_layers[0]; }

	std::vector<TerminalBuffer> m_layers;

	size_t m_width;
	size_t m_height;

	size_t m_cursorRow = 0;
	size_t m_cursorCol = 0;

	friend class AnimatedTerminalTests;
};



}
