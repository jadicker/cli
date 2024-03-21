#pragma once

#include "param.h"
#include "colorprofile.h"

#include <memory>

namespace cli
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
        std::tuple<Args...> GetParams(std::ostream& out) const
        {
            try
            {
                return Unpack<Args...>(m_params);
            }
            catch (std::bad_any_cast&)
            {
                out << Style::Error("Interpreter Error: Unable to unpack parameters, type mismatch.  Did you expect a const return?");
                return {};
            }
        }

        void AddToContext(ParamContext& ctx) const;

        std::vector<std::shared_ptr<const Param>> GetParams() const
        {
            std::vector<std::shared_ptr<const Param>> params;
            std::copy(m_params.begin(), m_params.end(), std::back_inserter(params));
            return params;
        }

    private:
        std::vector<std::shared_ptr<Param>> m_params;
    };
}