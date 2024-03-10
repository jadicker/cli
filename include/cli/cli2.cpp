#include "cli2.h"
#include "cliSession2.h"
#include "colorprofile.h"

#include <MechSim/Misc/Event.h>
#include <MechSim/Common.h>

using namespace cli;
using namespace cli::v2;

namespace
{
    void BuildParamContextRecursive(ParamContext &ctx, const Command *command)
    {
        if (!command)
        {
            return;
        }

        BuildParamContextRecursive(ctx, command->GetParent().get());
        command->AddParams(ctx);
    }
}

CompletionResults Command::AutoCompleteImpl(ParamContext& ctx,
                                            const std::vector<std::string>& paramTokens,
                                            size_t paramIndex) const
{
    if (paramIndex >= GetTotalTokens())
    {
        return {};
    }

    // Try to complete the name
    if (paramIndex == 0)
    {
        CompletionResults results;
        for (const auto& child : m_children)
        {
            // TODO: to lower, or not to lower?  Tests currently assume no tolower
            //auto lowerParam = Util::ToLower(paramTokens.back());
            if (child->Name().starts_with(paramTokens.back()))
            {
                results.m_completions.push_back({ child->Name(), child->Description() });
            }
        }
        return results;
    }

    std::vector<std::string> paramOnlyTokens;
    std::copy(paramTokens.begin() + 1, paramTokens.end(), std::back_inserter(paramOnlyTokens));
    return m_params.AutoComplete(ctx, paramOnlyTokens, paramIndex - 1);
}

CompletionResults v2::Command::GetAllChildrenCompletions(const std::string& token) const
{
    CompletionResults result;
    for (const auto& child : m_children)
    {
        if (token.empty() || child->Name().starts_with(token))
        {
            result.m_completions.push_back(child->GetCompletion());
        }
    }
    return result;
}

void v2::Command::MainHelp(std::ostream& out)
{
    for (const auto& child : m_children)
    {
        child->Help(out);
    }
}

void v2::Command::Help(std::ostream& out) const
{
    out << " - " << m_name << "\n\t" << m_desc << "\n";
}

detail::AutoCompletion v2::Command::GetCompletion() const
{
    return { m_name, m_desc };
}

std::shared_ptr<const Command> v2::Command::FindChildCommand(const std::string& name) const
{
    for (const auto& cmd : m_children)
    {
        if (cmd->Name() == name)
        {
            return cmd;
        }
    }
    return nullptr;
}

PreparationResult v2::Command::Prepare(ParamContext& paramContext,
                                       const std::vector<std::string>& tokens,
                                       size_t currentIndex)
{
    if (tokens.empty() || currentIndex >= tokens.size() || tokens[currentIndex] != m_name)
    {
        return { v2::ScanResult::NoneFound, 0 };
    }

    auto results = m_params.Prepare(paramContext, tokens, currentIndex + 1);
    if (!results.m_indicesFailedToParse.empty())
    {
        paramContext.m_out << Style::Red() << "Error: failed to parse command.  Bad param(s):" << std::endl;
        for (size_t i : results.m_indicesFailedToParse)
        {
            paramContext.m_out << "\t'" << tokens[i] << "'" << std::endl;
        }
    }

    size_t tokensPrepared = results.m_prepared + 1;
    v2::ScanResult scanResult = (tokensPrepared == GetTotalTokens()) ? v2::ScanResult::Found : v2::ScanResult::BadOrMissingParams;
    return { scanResult, tokensPrepared };
}

void v2::Command::Cleanup() const
{
    if (m_onExit)
    {
        m_onExit();
    }
}

void v2::Command::ScanRecursiveImpl(ParamContext& ctx,
                                    const std::vector<std::string>& cmdLineTokens,
                                    ExecutionResult& result)
{
    if (result.m_paramsConsumed >= cmdLineTokens.size())
    {
        return;
    }

    // This command is the current context when doing recursive execution
    for (auto& command : m_children)
    {
        auto prepareResult = command->Prepare(ctx, cmdLineTokens, result.m_paramsConsumed);
        result.m_paramsConsumed += prepareResult.m_tokensSuccessfullyParsed;
        result.m_action = prepareResult.m_scanResult;
        if (prepareResult.m_scanResult == v2::ScanResult::Found)
        {
            result.m_commandsScanned.push_back(command);
            // Note: execution already modifies the current index
            command->ScanRecursiveImpl(ctx, cmdLineTokens, result);
            break;
        }
        else if (prepareResult.m_scanResult == v2::ScanResult::BadOrMissingParams)
        {
            result.m_partialCommand = command;
            break;
        }
    }
}

ExecutionResult v2::Command::ExecuteRecursive(std::ostream& out,
                                              const std::vector<std::string>& cmdLineTokens)
{
    // This command is the current context when doing recursive execution
    ParamContext ctx = BuildParamContext(out);
    ExecutionResult results;
    ScanRecursiveImpl(ctx, cmdLineTokens, results);

    if (results.m_commandsScanned.empty())
    {
        results.m_action = v2::ScanResult::NoneFound;
    }
    else
    {
        // Execute all scanned commands
        for (auto& cmd : results.m_commandsScanned)
        {
            cmd->Execute(ctx.m_out);
        }

        size_t maxTokens = 0;
        for (const auto& command : results.m_commandsScanned)
        {
            maxTokens += command->GetTotalTokens();
        }

        if (results.m_paramsConsumed == maxTokens)
        {
            results.m_action = v2::ScanResult::Found;
        }
        else
        {
            // > 0 bad params
            results.m_action = v2::ScanResult::BadOrMissingParams;
        }
    }

    // TODO: Move this?
    MechSim::EventManager::GetInstance().Flush();

    return results;
}

#if 0
v2::CompletionResults Command::AutoComplete(ParamContext& ctx,
                                            const std::vector<std::string>& cmdLineTokens)
{
    ExecutionResult scanResult(ctx.m_out);
    ScanRecursiveImpl(ctx.m_out, cmdLineTokens, scanResult);

    if (scanResult.m_action == v2::ScanResult::Found)
    {
        // Found everything, nothing left to do
        return {};
    }

    const Command* completionCommand = this;
    if (scanResult.m_partialCommand)
    {
        completionCommand = scanResult.m_partialCommand.get();
    }
    else if (!scanResult.m_commandsScanned.empty())
    {
        completionCommand = scanResult.m_commandsScanned.back().get();
    }

    // 0 or more commands were executed.  It doesn't really matter why execution of the last
    // command failed, auto-complete will figure out what to do
    size_t paramIndex = scanResult.m_paramsConsumed;

    return completionCommand->AutoCompleteImpl(ctx, cmdLineTokens, paramIndex);
}
#endif

void Command::Execute(std::ostream& out)
{
    m_onExec(out, m_params);
}

void Command::AddParams(ParamContext &ctx) const
{
    m_params.AddToContext(ctx);
}

ParamContext Command::BuildParamContext(std::ostream& out) const
{
    ParamContext ctx(out);
    BuildParamContextRecursive(ctx, this);
    return ctx;
}
