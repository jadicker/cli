#include "params.h"
#include "paramDefinition.h"
#include "MechSim/Misc/Util.h"

#include <algorithm>

using namespace cli;

CompletionResults Parameters::AutoComplete(ParamContext& ctx,
                                           const std::vector<std::string>& paramTokens,
                                           size_t completionIndex) const
{
    assert(completionIndex < paramTokens.size());
	for (size_t i = 0; i < completionIndex; ++i)
	{
		if (!m_params[i]->Validate(ctx, paramTokens[i]))
		{
            // Param that comes before the completionIndex couldn't be validated
            return {};
		}
	}

    // On the last param, can auto-complete
    // TODO: Why pack the command here?
    return { nullptr,
             completionIndex,
             m_params[completionIndex]->GetAutoCompletions(ctx, paramTokens[completionIndex])
           };
}

Parameters::PreparationResults Parameters::Prepare(ParamContext& ctx,
                                                   const std::vector<std::string>& paramTokens,
                                                   size_t currentIndex)
{
    Parameters::PreparationResults results;

    if (IsFree())
    {
        // Free commands have a single string parameter, all tokens are parsed as one string
        std::string freeString;
        for (size_t i = currentIndex; i < paramTokens.size(); ++i)
        {
            freeString += paramTokens[i];

            if (i != paramTokens.size() - 1)
            {
                freeString += " ";
            }
        }

        if (m_params[0]->Prepare(ctx, freeString))
        {
            results.m_prepared = 1;
        }
        else
        {
            results.m_indicesFailedToParse.push_back(currentIndex);
        }
        return results;
    }

	const size_t size = std::min(paramTokens.size() - currentIndex, m_params.size());
	for (size_t i = 0; i < size; ++i)
	{
        if (m_params[i]->Prepare(ctx, paramTokens[currentIndex + i]))
		{
            results.m_prepared += 1;
        }
        else
        {
            results.m_indicesFailedToParse.push_back(currentIndex + i);
        }
	}
	return results;
}

bool Parameters::IsFree() const
{
    return m_params.size() == 1 &&
        dynamic_pointer_cast<StringParam>(m_params[0]);
}

void Parameters::AddToContext(ParamContext &ctx) const
{
    for (const auto& param : m_params)
    {
        ctx.m_parameters.push_back(param);
    }
}
