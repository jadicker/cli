#pragma once

#include "param.h"

#include <memory>

namespace cli
{
	namespace v2
	{
		class Parameters final
		{
		public:
            struct PreparationResults
            {
                bool Success() const { return m_indicesFailedToParse.empty() && m_prepared > 0; }

                size_t m_prepared = 0;
                std::vector<size_t> m_indicesFailedToParse;
            };

			Parameters() = default;

			Parameters(std::vector<std::shared_ptr<Param>> params)
				: m_params(std::move(params))
			{}

			Parameters(std::initializer_list<std::shared_ptr<Param>> params)
				: m_params{ params }
			{}

			static const Parameters& Null()
			{
				static Parameters nullParams;
				return nullParams;
			}

			size_t size() const
			{
				return m_params.size();
			}

            // Only for string
            bool IsFree() const;

            CompletionResults AutoComplete(ParamContext& ctx,
                                           const std::vector<std::string>& paramTokens,
                                           size_t completionIndex) const;

			// Same as match but parses.
            PreparationResults Prepare(ParamContext& ctx, const std::vector<std::string>& paramTokens, size_t currentIndex);

			// User calls
			//  auto [myFloat, myObj] = myParams.GetParams<float, ObjectId>();
			template <typename ...Args>
			std::tuple<Args...> GetParams() const
			{
				return Unpack<Args...>(m_params);
			}

            void AddToContext(ParamContext& ctx) const;

		private:
			std::vector<std::shared_ptr<Param>> m_params;
		};
	}
}