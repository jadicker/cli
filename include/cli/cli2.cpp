#include "cli2.h"
#include "cliSession2.h"
#include "colorprofile.h"

#include <MechSim/Misc/Event.h>
#include <MechSim/Common.h>

using namespace cli;

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

CompletionResults Command::GetAllChildrenCompletions(const std::string& token) const
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

void Command::MainHelp(std::ostream& out)
{
    for (const auto& child : m_children)
    {
        child->Help(out);
    }
}

void Command::Help(std::ostream& out) const
{
    out << " - " << m_name << "\n\t" << m_desc << "\n";
}

detail::AutoCompletion Command::GetCompletion() const
{
    return { m_name, m_desc };
}

std::shared_ptr<const Command> Command::FindChildCommand(const std::string& name) const
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

PreparationResult Command::Prepare(ParamContext& paramContext,
                                       const std::vector<std::string>& tokens,
                                       size_t currentIndex)
{
    if (tokens.empty() || currentIndex >= tokens.size() || tokens[currentIndex] != m_name)
    {
        return { ScanResult::NoneFound, 0 };
    }

    auto results = m_params.Prepare(paramContext, tokens, currentIndex + 1);
    if (!results.m_indicesFailedToParse.empty())
    {
        paramContext.m_out << "Error: Bad param(s) for " << GetSignature() << std::endl;
        for (size_t i : results.m_indicesFailedToParse)
        {
            const size_t paramIndex = i - (currentIndex + 1);
            paramContext.m_out << "  " << m_params.GetParams()[paramIndex]->GetName() << ":   '" << tokens[i] << "'" << std::endl;
        }
    }

    size_t tokensPrepared = results.m_prepared + 1;
    ScanResult scanResult = (tokensPrepared == GetTotalTokens()) ? ScanResult::Found : ScanResult::BadOrMissingParams;
    return { scanResult, tokensPrepared };
}

void Command::Cleanup() const
{
    if (m_onExit)
    {
        m_onExit();
    }
}

void Command::ScanRecursiveImpl(ParamContext& ctx,
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
        if (prepareResult.m_scanResult == ScanResult::Found)
        {
            result.m_commandsScanned.push_back(command);
            // Note: execution already modifies the current index
            command->ScanRecursiveImpl(ctx, cmdLineTokens, result);
            break;
        }
        else if (prepareResult.m_scanResult == ScanResult::BadOrMissingParams)
        {
            result.m_partialCommand = command;
            break;
        }
    }
}

ExecutionResult Command::ExecuteRecursive(std::ostream& out,
                                              const std::vector<std::string>& cmdLineTokens)
{
    // This command is the current context when doing recursive execution
    ParamContext ctx = BuildParamContext(out);
    ExecutionResult results;
    ScanRecursiveImpl(ctx, cmdLineTokens, results);

    if (results.m_commandsScanned.empty())
    {
        results.m_action = ScanResult::NoneFound;
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

        if (results.m_partialCommand)
        {
            results.m_action = ScanResult::PartialCompletion;
        }
        else if (results.m_paramsConsumed == maxTokens)
        {
            results.m_action = ScanResult::Found;
        }
        else
        {
            // > 0 bad params
            results.m_action = ScanResult::BadOrMissingParams;
        }
    }

    // TODO: Move this?
    MechSim::EventManager::GetInstance().Flush();

    return results;
}

void Command::Execute(std::ostream& out)
{
    m_onExec(out, GetAllCommands());
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

std::vector<Command::CommandPtr> Command::GetAllCommands() const
{
    std::vector<CommandPtr> commands;
    auto command = shared_from_this();
    //std::shared_ptr<CommandPtr> command = thisCommand;
    do
    {
        commands.push_back(command);
        command = command->GetParent();
    }
    while (command);

    std::reverse(commands.begin(), commands.end());
    return commands;
}

std::string Command::GetSignature() const
{
    std::stringstream out;
    out << Name() << "(";

    const auto& params = m_params.GetParams();
    for (size_t i = 0; i < params.size(); ++i)
    {
        const auto& param = params[i];
        out << *param;
        if (i != params.size() - 1)
        {
            out << ", ";
        }
    }

    out << ")";

    return out.str();
}
