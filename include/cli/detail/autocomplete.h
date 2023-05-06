#pragma once

#include <string>
#include <vector>
#include <algorithm>

namespace cli
{
namespace detail
{
	class AutoCompletion
	{
	public:
		std::string text;
		std::string description;

		bool operator!=(const AutoCompletion& other) const
		{
			return text != other.text && description != other.description;
		}
		bool operator==(const AutoCompletion& other) const
		{
			return !(*this != other);
		}
	};

	inline std::vector<std::string> GetTextCompletions(const std::vector<AutoCompletion>& completions)
	{
		std::vector<std::string> result;
		result.reserve(completions.size());
		std::for_each(completions.begin(), completions.end(),
			[&result](const AutoCompletion& completion)
			{
				result.push_back(completion.text);
			});
		return result;
	}
}
}