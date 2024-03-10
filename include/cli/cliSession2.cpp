#include "cliSession2.h"

#include "cli2.h"
#include "colorprofile.h"
#include "detail/split.h"

#include <MechSim/Misc/Util.h>

#include <algorithm>
#include <numeric>

using namespace cli;

v2::CliSession::CliSession(Cli& _cli, std::ostream& _out, std::size_t historySize) :
    cli(_cli),
    coutPtr(Cli::CoutPtr()),
    m_current(cli.RootMenu()),
    globalScopeMenu(std::make_shared<Command>()),
    m_rootMenu(m_current),
    out(_out),
    history(historySize)
{
    history.LoadCommands(cli.GetCommands());

    coutPtr->Register(out);

    globalScopeMenu->Insert(
        "help",
        "This help message",
        Parameters::Null(),
        [this](std::ostream&, const Parameters&) { Help(); });

    globalScopeMenu->Insert(
        "exit",
        "Quit the session",
        Parameters::Null(),
        [this](std::ostream&, const Parameters&) { Exit(); });

    m_exitCommand = globalScopeMenu->FindChildCommand("exit");

#ifdef CLI_HISTORY_CMD
    globalScopeMenu->Insert(
        "history",
        [this](std::ostream&) { ShowHistory(); },
        "Show the history"
    );
#endif
}

std::ostream& v2::CliSession::GetOutStream()
{
    if (m_silent)
    {
        return m_nullOut;
    }
    return out;
}

bool v2::CliSession::Feed(const std::string& cmd, bool dontSaveCommand, bool printCmd, bool silentOutput)
{
    bool silence = m_silent;
    MechSim::ScopedLambda silentScope(
        [this, silentOutput]()
        {
            if (silentOutput)
            {
                m_silent = true;
            }
        },
        [this, silence]()
        {
            m_silent = silence;
        });

    if (printCmd)
    {
        GetOutStream() << cmd << "\n";
    }

    auto currentCommand = m_current;

    std::vector<std::string> strs;
    detail::split(strs, cmd);
    if (strs.empty()) return false; // just hit enter

    if (!dontSaveCommand)
    {
        history.NewCommand(cmd); // add anyway to history
    }

    try
    {
        auto result = m_current->ExecuteRecursive(GetOutStream(), strs);

        if (result.m_action == ScanResult::NoneFound)
        {
            result = cli.rootMenu->ExecuteRecursive(GetOutStream(), strs);
        }

        if (result.m_action == ScanResult::NoneFound)
        {
            result = globalScopeMenu->ExecuteRecursive(GetOutStream(), strs);
        }

        if (result.m_action != ScanResult::Found)
        {
            if (result.m_action == ScanResult::NoneFound)
            {
                GetOutStream() << Style::Error("Command '")
                    << Style::Command() << strs[0] << reset
                    << Style::Error("' not found.") << std::endl;
            }
            else
            {
                // Bad parameters
                GetOutStream() << "Bad parameters, cannot execute commands." << std::endl;
                result.m_commandsScanned.back()->Help(GetOutStream());
            }
            // Overloads not allowed
        }
        else
        {
            // There's only 2 cases in which we don't go back to our previous menu:
            //  - if the leaf command that executed has children, meaning it's a submenu.
            //  - exit was called
            auto& commandsExecuted = result.m_commandsScanned;
            assert(!commandsExecuted.empty());
            // Exit sets m_current
            if (commandsExecuted.back() != m_exitCommand)
            {
                if (!commandsExecuted.back()->HasChildren())
                {
                    for (auto iter = commandsExecuted.rbegin(); iter != commandsExecuted.rend(); ++iter)
                    {
                        if (*iter == currentCommand)
                        {
                            break;
                        }
                        (*iter)->Cleanup();
                    }
                    m_current = currentCommand;
                }
                else
                {
                    m_current = commandsExecuted.back();
                }
            }

            bool endedInFreeCommand = false;
            size_t usedParams = 0;
            for (const auto& resultCmd : result.m_commandsScanned)
            {
                if (resultCmd->IsFreeCommand())
                {
                    endedInFreeCommand = true;
                    break;
                }

                usedParams += resultCmd->GetTotalTokens();
            }

            if (!endedInFreeCommand && strs.size() > usedParams)
            {
                GetOutStream() << Style::Error("Couldn't find command '" + strs[usedParams] + "'.")
                    << "  Discarding remainder of command line: '";
                for (size_t i = usedParams; i < strs.size(); ++i)
                {
                    GetOutStream() << strs[i];
                    if (i < strs.size() - 1)
                    {
                        GetOutStream() << " ";
                    }
                }
                GetOutStream() << "'." << std::endl;
            }
        }

        return result.m_action == ScanResult::Found;
    }
    catch (const std::exception& e)
    {
        cli.StdExceptionHandler(GetOutStream(), cmd, e);
    }
    catch (...)
    {
        GetOutStream() << "Cli. Unknown exception caught handling command line \""
            << cmd
            << "\"\n";
    }

    return false;
}

void v2::CliSession::RunProgram(const std::string& name, const std::vector<std::string>& program)
{
    GetOutStream() << "Executing program " << name << "...\n";

    for (const auto& line : program)
    {
        Prompt();
        Feed(line, true);
    }
}

size_t v2::CliSession::PromptImpl()
{
    if (exit || !m_current) return 0;

    std::vector<std::shared_ptr<const Command>> commands;
    std::shared_ptr<const Command> cur = m_current;
    do
    {
        commands.push_back(cur);
        cur = cur->GetParent();
    } while (cur);

    std::string prompt;
    for (auto iter = commands.rbegin(); iter != commands.rend(); ++iter)
    {
        // More interesting symbols that work ○•⁃◘◙π
        prompt += U8(">") + (*iter)->GetPromptDisplay();
    }

    // TODO: To have unicode characters in the prompt, all substring work in terminal.h
    //       will have to be fixed
    //std::string suffix = U8("  ╰╴>  ");
    std::string suffix = "  \\-> ";
    // Now that it's utf8, size() won't give us chars
    size_t suffixChars = 6;
    GetOutStream() << beforePrompt
        << prompt
        << afterPrompt << std::endl
        << ColorHelper{ 52, 144, 111 } << suffix << afterPrompt
        << std::flush;

    return suffixChars;
}

void v2::CliSession::Help() const
{
    out << "Commands available:\n";
    globalScopeMenu->MainHelp(out);
    m_current->MainHelp(out);
}

void v2::CliSession::Exit()
{
    m_current->Cleanup();
    m_current = m_current->GetParent();
    if (m_current)
    {
        return;
    }

    exitAction(out);
    cli.ExitAction(out);

    auto cmds = history.GetCommands();
    cli.StoreCommands(cmds);

    exit = true; // prevent the prompt to be shown
}

v2::CompletionResults v2::CliSession::GetCompletionsForCommand(Command* command,
                                                               const std::string& currentLine,
                                                               const size_t param)
{
    if (currentLine.empty())
    {
        return command->GetAllChildrenCompletions("");
    }

    ParamContext paramContext = command->BuildParamContext(GetOutStream());
    std::vector<std::string> cmdLineTokens;
    detail::split(cmdLineTokens, currentLine);

    ExecutionResult scanResult;
    command->ScanRecursiveImpl(paramContext, cmdLineTokens, scanResult);

    if (scanResult.m_action == ScanResult::Found)
    {
        // Found everything, nothing left to do
        return {};
    }

    const Command* completionCommand = command;
    if (scanResult.m_partialCommand)
    {
        completionCommand = scanResult.m_partialCommand.get();
    }
    else if (!scanResult.m_commandsScanned.empty())
    {
        completionCommand = scanResult.m_commandsScanned.back().get();
    }

    // Specifically excludes partial execution; we want the name in here
    size_t tokensExecuted = 0;
    for (const auto& scannedCommand : scanResult.m_commandsScanned)
    {
        tokensExecuted += scannedCommand->GetTotalTokens();
    }

    std::vector<std::string> commandParams;
    std::copy(cmdLineTokens.begin() + tokensExecuted, cmdLineTokens.end(),
              std::back_inserter(commandParams));
    assert(param >= tokensExecuted);
    size_t relativeParam = param - tokensExecuted;

    // It's ok to auto-complete the next, non-existent parameter
    if (relativeParam == commandParams.size())
    {
        commandParams.emplace_back("");
    }

    return completionCommand->AutoCompleteImpl(paramContext, commandParams, relativeParam);
}

v2::CompletionResults v2::CliSession::GetCompletions(const std::string& currentLine, const size_t param)
{
    v2::CompletionResults completions = GetCompletionsForCommand(m_current.get(), currentLine, param);
    if (completions.m_completions.empty())
    {
        if (m_rootMenu)
        {
            completions = GetCompletionsForCommand(m_rootMenu.get(), currentLine, param);
        }

        if (completions.m_completions.empty())
        {
            completions = GetCompletionsForCommand(globalScopeMenu.get(), currentLine, param);
        }
    }

    auto& stringCompletions = completions.m_completions;
    if (stringCompletions.empty())
    {
        return completions;
    }

    // Rotate completion logic
    auto paramIndex = m_menuParamIndex;
    m_menuParamIndex = (m_menuParamIndex + 1) % stringCompletions.size();

    if (paramIndex > 0 && paramIndex <= stringCompletions.size())
    {
        stringCompletions.insert(stringCompletions.end(),
                                 stringCompletions.begin(),
                                 stringCompletions.begin() + paramIndex);
        stringCompletions.erase(stringCompletions.begin(),
                                stringCompletions.begin() + paramIndex);
    }

    return completions;
}

void v2::CliSession::Pop()
{
    if (!m_top || !m_current)
    {
        return;
    }

    std::shared_ptr<Command> cur = m_current;
    while (cur != m_top && cur != nullptr)
    {
        cur->Cleanup();
        cur = cur->GetParent();
    }
    m_current = m_top;
    m_top = nullptr;
}
