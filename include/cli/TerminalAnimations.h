#pragma once

#include <memory>
#include <vector>
#include <cassert>

#include "utf8.h"

#include "MechSim/Terminal/Console.h"
#include "MechSim/Misc/Timeline.h"
#include "MechSim/Misc/TypeWrapper.h"


class AnimatedTerminalTests;

#define TERMINAL_BUFFER_CHECKED 1

#if TERMINAL_BUFFER_CHECKED
#define terminalAssert(expr) assert((expr))
#else
#define terminalAssert(expr)
#endif

using MechSim::TypeWrapper;

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

	void SetOpacity(TerminalPos pos, uint8_t opacity, size_t len = 1)
	{
		const size_t maxLen = m_opacities.size() - GetIndex(pos);
		len = std::min(maxLen, len);
		const size_t end = GetIndex(pos) + len;
		for (size_t i = GetIndex(pos); i < end; ++i)
		{
			m_opacities[i] = opacity;
		}
	}

	void SetOpaque(TerminalPos pos, size_t len)
	{
		auto length = ClipCol(pos.col + len - 1) + 1;
		const size_t startOffset = GetIndex(pos);
		const size_t endOffset = startOffset + length;

		for (auto iter = m_opacities.begin() + startOffset; iter != m_opacities.begin() + endOffset; ++iter)
		{
			*iter = 0xFF;
		}
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

	void SetStrClipped(TerminalPos pos, const std::vector<uint32_t>& str)
	{
		terminalAssert(pos.col < m_width);
		terminalAssert(pos.row < m_height);

		const size_t clippedEnd = ClipCol(pos.col + str.size() - 1) + 1;
		const size_t clippedLen = clippedEnd - pos.col;

		const size_t b = GetIndex(pos);
		for (size_t i = 0; i < clippedLen; ++i)
		{
			m_utf32Buffer[b + i] = str[i];
		}
	}

	void SetStrClipped(TerminalPos pos, const std::string& str)
	{
		SetStrClipped(pos, Utf8StrToUtf32Str(str));
	}

	void CopyStr(TerminalPos dest, const std::vector<uint32_t>& str, size_t start, size_t end)
	{
		terminalAssert(end >= start);
		const size_t bufferStart = GetIndex(dest);
		size_t len = end - start;
		len = std::min(len, m_utf32Buffer.size() - bufferStart);
		for (size_t i = bufferStart, j = start; i < bufferStart + len; ++i, ++j)
		{
			m_utf32Buffer[i] = str[j];
		}
	}

	static const uint32_t utf32Space()
	{
		static const uint32_t utf32Space = Utf8CharToUtf32Char(" ");
		return utf32Space;
	}

	size_t GetWidth() const
	{
		return m_width;
	}

	void Clear()
	{
		std::fill(m_utf32Buffer.begin(), m_utf32Buffer.end(), utf32Space());
		std::fill(m_opacities.begin(), m_opacities.end(), m_transparentClear ? 0x00 : 0xFF);
	}

	// The assumption is that scrolling in the terminal's scroll back region has already occurred
	// at the composition level, so this just clears the lines and moves them to the end.
	void Scroll(size_t lines)
	{
		terminalAssert(lines < m_height);

		const size_t scrollCopyStart = GetIndex(lines, 0);
		std::copy(m_utf32Buffer.begin() + scrollCopyStart, m_utf32Buffer.end(), m_utf32Buffer.begin());
		std::copy(m_opacities.begin() + scrollCopyStart, m_opacities.end(), m_opacities.begin());

		const size_t clearStart = GetIndex(m_height - lines, 0);
		std::fill(m_utf32Buffer.begin() + clearStart, m_utf32Buffer.end(), utf32Space());
		std::fill(m_opacities.begin() + clearStart, m_opacities.end(), m_transparentClear ? 0x00 : 0xFF);
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
	TerminalBufferData(TerminalBuffer* writeBuffer, const std::vector<const TerminalBuffer*> inAllBuffers)
		: buffer(writeBuffer)
		, allBuffers(inAllBuffers)
	{}

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

class AnimatedTerminal
{
public:
	AnimatedTerminal(size_t width, size_t height)
			: m_width(width)
			  , m_height(height)
	{
		AddLayer(false);
	}

	TerminalBufferData GetBufferData(TerminalPos pos, size_t writeLayer)
	{
		terminalAssert(writeLayer < m_layers.size());

		TerminalBufferData data(&m_layers[writeLayer], GetAllBuffers());
		data.pos = pos;
		data.bufferIndex = writeLayer;
		return data;
	}

	size_t GetCharacterCount() const
	{
		return m_width * m_height;
	}

	void SetCursor(TerminalPos pos)
	{
		terminalAssert(pos.row < m_height);
		terminalAssert(pos.col < m_width);
		m_cursorRow = pos.row;
		m_cursorCol = pos.col;
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
		GetPrimary().SetStrClipped({ m_cursorRow, m_cursorCol}, str);
	}

	// Append utf8 string str at current cursor position.  Returns a string of
	// all characters that need to be written out/"fell off the terminal".
	std::string Append(const std::string& str, size_t layer = 0)
	{
		terminalAssert(layer < m_layers.size());

		auto utf32Str = Utf8StrToUtf32Str(str);
		const size_t requiredSize = m_cursorCol + utf32Str.size();
		size_t lines = requiredSize / m_width;

		const size_t availableLines = (m_height - m_cursorRow);

		size_t finalCol = m_width;

		if (requiredSize > lines * m_width)
		{
			finalCol = requiredSize - (lines * m_width);
			++lines;
		}

		// Write out first partial line to get it out of the way
		size_t inputStartIndex = 0;
		if (m_cursorCol > 0)
		{
			const size_t startTrimLen = m_width - m_cursorCol;
			m_layers[layer].CopyStr({m_cursorRow, m_cursorCol}, utf32Str, 0, startTrimLen);
			m_layers[layer].SetOpacity({m_cursorRow, m_cursorCol}, 0xFF, startTrimLen);
			m_cursorCol = 0;
			++m_cursorRow;
			inputStartIndex = startTrimLen;
		}

		const size_t finalLinesInBuffer = std::min(m_height, m_cursorRow + lines);

		std::string scrolledOutput;
		if (lines > availableLines)
		{
			scrolledOutput = Scroll(std::min(lines - availableLines, m_height));
			// If there aren't enough rows to write our input
			if (m_height < lines)
			{
				inputStartIndex += (lines - m_height) * m_width;
			}
		}

		size_t charLen = utf32Str.size() - inputStartIndex;
		m_layers[layer].CopyStr({m_cursorRow, m_cursorCol}, utf32Str, inputStartIndex, utf32Str.size());
		m_layers[layer].SetOpacity({m_cursorRow, m_cursorCol}, 0xFF, charLen);

		m_cursorRow = finalLinesInBuffer - 1;
		m_cursorCol = finalCol;
		if (m_cursorCol >= m_width)
		{
			terminalAssert(m_cursorCol < 2 * m_width);
			++m_cursorRow;
			m_cursorCol = m_cursorCol - m_width;
		}

		return scrolledOutput;
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

	std::vector<const TerminalBuffer*> GetAllBuffers() const
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

	std::vector<std::string> GetUtf8Lines() const
	{
		std::vector<std::string> lines;
		for (size_t i = 0; i < m_height; ++i)
		{
			lines.push_back(GetUtf8Line(i));
		}
		return lines;
	}

	std::shared_ptr<Animation> AddAnimation(std::shared_ptr<Animation> anim)
	{
		m_animations.push_back(anim);
		return m_animations.back();
	}

	void Update(float dt)
	{
		for (auto iter = m_animations.begin(); iter != m_animations.end();)
		{
			auto animation = *iter;
			if (animation->Complete())
			{
				iter = m_animations.erase(iter);
				continue;
			}

			animation->Update(dt);
			++iter;
		}
	}

	std::string Scroll(size_t lines)
	{
		terminalAssert(lines < m_height);
		std::string scrolled;
		for (size_t i = 0; i < lines; ++i)
		{
			scrolled += GetUtf8Line(i);
		}

		for (auto& layer : m_layers)
		{
			layer.Scroll(lines);
		}

		m_cursorRow -= lines;
		return scrolled;
	}

private:
	TerminalBuffer& GetPrimary() { return m_layers[0]; }

	std::vector<TerminalBuffer> m_layers;
	std::vector<std::shared_ptr<Animation>> m_animations;

	size_t m_width;
	size_t m_height;

	size_t m_cursorRow = 0;
	size_t m_cursorCol = 0;

	friend class AnimatedTerminalTests;
};

class ScrollAnimation : public Animation
{
public:
	ScrollAnimation(AnimatedTerminal& term,
					TerminalPos pos,
					size_t layer,
					const std::string& utf8Str,
					float duration)
		: Animation(term.GetBufferData(pos, layer))
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
		m_data.buffer->SetStrClipped(m_data.pos, str);
	}

	std::vector<uint32_t> m_utf32Str;
	size_t m_endCol;
	size_t m_outputSize;

	float m_time;
	const float m_duration;
};

class TickerTapeAnimation : public Animation
{
public:
	// Continuously loop the ticker tape anim
	using Loop = TypeWrapper<bool>;
	// Pad to start with blank output
	using Pad = TypeWrapper<bool>;

	TickerTapeAnimation(AnimatedTerminal& term,
						TerminalPos pos,
						size_t layer,
						const std::string& utf8Str,
						float charsPerSecond,
						Loop loop,
						Pad pad)
			: Animation(term.GetBufferData(pos, layer))
			  , m_utf32Str(Utf8StrToUtf32Str(utf8Str))
			  , m_time(0.0f)
			  , m_secondsPerChar(1.0f / charsPerSecond)
			  , m_loop(loop)
			  , m_pad(pad)
			  , m_charsScrolled(0)
	{
		;
		m_endCol = m_data.buffer->ClipCol(m_data.buffer->GetWidth() - m_data.pos.col - 1);
		m_outputSize = (m_endCol + 1) - m_data.pos.col;

		if (m_pad)
		{
			std::vector<uint32_t> padding;
			padding.resize(m_outputSize, TerminalBuffer::utf32Space());
			m_utf32Str.insert(m_utf32Str.begin(), padding.begin(), padding.end());
		}
		m_data.buffer->SetOpaque(m_data.pos, m_utf32Str.size());
	}

	void Update(const float dt) override
	{
		m_time += dt;
		while (m_time >= m_secondsPerChar)
		{
			m_utf32Str.push_back(m_utf32Str[0]);
			m_utf32Str.erase(m_utf32Str.begin(), m_utf32Str.begin() + 1);

			m_time -= m_secondsPerChar;
			++m_charsScrolled;

			UpdateBuffer();
		};

		if (!m_loop && m_charsScrolled == m_utf32Str.size())
		{
			m_complete = true;
		}
	}

private:
	void UpdateBuffer()
	{
		m_data.buffer->SetStrClipped(m_data.pos, m_utf32Str);
	}

	std::vector<uint32_t> m_utf32Str;
	size_t m_endCol;
	size_t m_outputSize;
	size_t m_charsScrolled;

	float m_time;
	float m_secondsPerChar;
	Loop m_loop;
	Pad m_pad;
};

}
