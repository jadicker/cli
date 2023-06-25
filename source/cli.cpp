
#include "../include/cli/cli.h"

using namespace cli;

bool Command::Exec(const std::vector<std::string>& cmdLine, CliSession& session)
{
    if (!IsEnabled())
        return false;
    if (cmdLine[0] == Name())
    {
        // check also for subcommands
        std::vector<std::string > subCmdLine(cmdLine.begin() + 1, cmdLine.end());
        for (auto& cmd : *cmds)
            if (cmd->Exec(subCmdLine, session)) return true;

        if (HasChildren())
        {
            session.Current(this);
            return true;
        }
    }
    return false;
}

CliSession::CliSession(Cli& _cli, std::ostream& _out, std::size_t historySize) :
    cli(_cli),
    coutPtr(Cli::CoutPtr()),
    m_current(cli.RootMenu()),
    m_rootMenu(m_current),
    globalScopeMenu(std::make_unique< Command >()),
    out(_out),
    history(historySize)
{
    history.LoadCommands(cli.GetCommands());

    coutPtr->Register(out);

    // TODO: Transfer 
    //globalScopeMenu->TransferRootCommands(*cli.RootMenu());

    globalScopeMenu->Insert(
        "help",
        [this](std::ostream&) { Help(); },
        "This help message"
    );
    globalScopeMenu->Insert(
        "exit",
        [this](std::ostream&) { Exit(); },
        "Quit the session"
    );

    m_exitCommand = globalScopeMenu->GetCommand("exit");

#ifdef CLI_HISTORY_CMD
    globalScopeMenu->Insert(
        "history",
        [this](std::ostream&) { ShowHistory(); },
        "Show the history"
    );
#endif
}

bool CliSession::Feed(const std::string& cmd, bool silent, bool printCmd)
{
    if (printCmd)
    {
        out << cmd << "\n";
    }

    auto* currentCommand = m_current;

    std::vector<std::string> strs;
    detail::split(strs, cmd);
    if (strs.empty()) return false; // just hit enter

    if (!silent)
    {
        history.NewCommand(cmd); // add anyway to history
    }

    try
    {
        auto result = m_current->ScanCmds(strs, *this);

        if (result.second == Command::ScanResultAction::NoneFound)
        {
            result = cli.rootMenu->ScanCmds(strs, *this);
        }

        // root menu recursive cmds check
        if (result.second == Command::ScanResultAction::NoneFound)
        {
            result = globalScopeMenu->ScanCmds(strs, *this);
        }

        if (result.second != Command::ScanResultAction::Executed)
        {
            if (!silent)
            {
                if (result.first.empty())
                {
                    out << Style::Red() << "Command " << reset
                        << "'" << Style::Command() << strs[0] << reset
                        << "' not found.\n";
                }
                else if (result.first.size() == 1)
                {
                    result.first.back()->Help(out);
                }
                else
                {
                    out << "Which '" << Style::Command() << strs[0] << reset << "'?  Could be:\n";
                    for (const auto& cmd : result.first)
                    {
                        out << "\t";
                        cmd->Help(out);
                    }
                }
            }
        }
        else
        {
            // There's only 2 cases in which we don't go back to our previous menu:
            //  - if the leaf command that executed has children, meaning it's a submenu.
            //  - exit was called
            if (!result.first.back()->HasChildren() && result.first.back() != m_exitCommand)
            {
                m_current = currentCommand;
            }
        }

        return result.second == Command::ScanResultAction::Executed;
    }
    catch (const std::exception& e)
    {
        cli.StdExceptionHandler(out, cmd, e);
    }
    catch (...)
    {
        if (!silent)
        {
            out << "Cli. Unknown exception caught handling command line \""
                << cmd
                << "\"\n";
        }
    }

    return false;
}

void CliSession::RunProgram(const std::string& name, const std::vector<std::string>& program, bool silent)
{
    out << "Executing program " << name << "...\n";

    for (const auto& line : program)
    {
        Prompt();
        Feed(line, silent, true);
    }
}

const Command* CliSession::GetCurrentCommand(const std::string& line) const
{
    std::vector<std::string> strs;
    detail::split(strs, line);

    if (strs.empty())
    {
        return nullptr;
    }

    auto commands = m_current->GetCurrentCommands(line);
    if (!commands.empty())
    {
        return &commands.back().m_command.get();
    }

    commands = globalScopeMenu->GetCurrentCommands(line);
    if (!commands.empty())
    {
        return &commands.back().m_command.get();
    }

    return nullptr;
}

size_t CliSession::PromptImpl()
{
    if (exit || !m_current) return 0;

    std::vector<Command*> commands;
    Command* cur = m_current;
    do
    {
        commands.push_back(cur);
        cur = cur->GetParent();
    } while (cur);

    std::string prompt;
    for (auto iter = commands.rbegin(); iter != commands.rend(); ++iter)
    {
        // More interesting symbols that work ○•⁃◘◙π
        prompt += u8">" + (*iter)->Name();
    }

    // TODO: To have unicode characters in the prompt, all substring work in terminal.h
    //       will have to be fixed
    //std::string suffix = u8"  ╰╴>  ";
    std::string suffix = "  \\-> ";
    // Now that it's utf8, size() won't give us chars
    size_t suffixChars = 6;
    out << beforePrompt
        << prompt
        << afterPrompt << std::endl
        << ColorHelper{52, 144, 111} << suffix << afterPrompt
        << std::flush;

    return suffixChars;
}

void CliSession::Help() const
{
    out << "Commands available:\n";
    globalScopeMenu->MainHelp(out);
    m_current->MainHelp(out);
}

void CliSession::Exit()
{
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

CliSession::CompletionResults CliSession::GetCompletionsImpl(Command* command,
    std::string currentLine,
    const size_t param,
    const bool recursive /* = true */)
{
    std::vector<std::string> params;
    detail::split(params, currentLine);

    auto commands = command->GetCommands(params, param);
    /*
    if (commands.empty())
    {
        return { nullptr, param, {} };
    }
    */

    if (!recursive && commands.empty())
    {
        return {};
    }

    size_t currentParam = 0;
    std::for_each(commands.begin(), commands.end(),
        [&currentParam](const auto& pair) { currentParam += pair.m_paramsFound; });

    if (commands.empty())
    {
        auto completions = command->GetChildCommandCompletions(params.empty() ? "" : params[0]);
        if (completions.empty())
        {
            return {};
        }

        if (m_previousCompletions != completions)
        {
            m_menuParamIndex = 0;
            m_previousCompletions = completions;
        }

        auto paramIndex = m_menuParamIndex;
        m_menuParamIndex = (m_menuParamIndex + 1) % completions.size();

        if (paramIndex > 0)
        {
            completions.insert(completions.end(), completions.begin(), completions.begin() + paramIndex);
            completions.erase(completions.begin(), completions.begin() + paramIndex);
        }

        return { nullptr, paramIndex, completions };
    }

    // It's not possible to do menu auto-complete within the menu command as it doesn't know about
    // its peers.  If the second value - param count provided - is 0, then do this work.
    if (param >= commands.back().m_paramsFound &&
        commands.back().m_paramsFound == commands.back().m_command.get().GetParamCount())
    {
        auto completions = commands.back().m_command.get().GetChildCommandCompletions(
            currentParam >= params.size() ? "" : params[currentParam]);

        if (completions.empty())
        {
            return {};
        }

        // Rotate completion logic
        auto paramIndex = m_menuParamIndex;
        m_menuParamIndex = (m_menuParamIndex + 1) % completions.size();
        
        if (paramIndex > 0)
        {
            completions.insert(completions.end(), completions.begin(), completions.begin() + paramIndex);
            completions.erase(completions.begin(), completions.begin() + paramIndex);
        }

        return { &commands.back().m_command.get(), paramIndex, completions };
    }

    bool valid = true;
    size_t exitsRequired = 0;
    size_t paramOffset = 0;
    for (size_t i = 0; i < commands.size(); ++i)
    {
        Command& cmd = commands[i].m_command.get();

        // Guard against attempting to call the function with not enough parameters
        if (paramOffset + cmd.GetParamCount() >= params.size())
        {
            break;
        }

        std::vector<std::string> localParams;
        localParams.assign(params.begin() + paramOffset,
            params.begin() + paramOffset + commands[i].m_paramsFound);
        paramOffset += cmd.GetParamCount();

        if (!cmd.Exec(localParams, *this))
        {
            valid = false;
            break;
        }

        ++exitsRequired;
    }

    AutoCompleter::Completions results;
    // TODO: This validity logic might be wrong
    if (valid)
    {
        results = commands.back().m_command.get().GetParamCompletion(param >= params.size() ? "" : params[param],
            (param - paramOffset));
    }

    for (size_t i = 0; i < exitsRequired; ++i)
    {
        Exit();
    }

    if (!valid)
    {
        return { nullptr, param, {} };
    }

    return { &commands.back().m_command.get(), param, results };
}

CliSession::CompletionResults CliSession::GetCompletions(std::string currentLine, const size_t param)
{
    auto completions = GetCompletionsImpl(m_current, currentLine, param);
    if (completions.m_completions.empty())
    {
        if (m_rootMenu)
        {
            completions = GetCompletionsImpl(m_rootMenu, currentLine, param);
        }

        if (completions.m_completions.empty())
        {
            completions = GetCompletionsImpl(globalScopeMenu.get(), currentLine, param, false);
        }
    }
    return completions;
}

detail::AutoCompletion CliSession::GetCurrentCommandCompletion(const std::string& line) const
{
    const auto* command = GetCurrentCommand(line);
    if (!command)
    {
        return {};
    }

    return { command->Name(), command->Description() };
}

CmdHandler Command::Insert(std::unique_ptr<Command>&& cmd)
{
    std::shared_ptr<Command> scmd(std::move(cmd));
    scmd->parent = this;
    CmdHandler c(scmd, cmds);
    cmds->push_back(scmd);
    return c;
}

CmdHandler Command::Insert(std::string&& menuName)
{
    std::shared_ptr<Command> smenu = std::make_shared<Command>(std::move(menuName));
    CmdHandler c(smenu, cmds);
    smenu->parent = this;
    cmds->push_back(smenu);
    return c;
}

Command::ScanResult Command::ScanCmds(const std::vector<std::string>& cmdLine, CliSession& session)
{
    if (!IsEnabled())
    {
        return {};
    }

    auto commands = GetCommands(cmdLine, 0);
    size_t requiredParams = 0;
    for (const auto& cmdPair : commands)
    {
        requiredParams += cmdPair.m_command.get().GetParamCount();
    }

    Command::ScanResult results;
    if (requiredParams > cmdLine.size())
    {
        for (const auto& command : commands)
        {
            results.first.push_back(&command.m_command.get());
        }
        results.second = ScanResultAction::BadParams;
        return results;
    }

    size_t executed = 0;
    size_t paramOffset = 0;
    for (size_t i = 0; i < commands.size(); ++i)
    {
        Command& cmd = commands[i].m_command.get();

        std::vector<std::string> localParams;
        localParams.assign(cmdLine.begin() + paramOffset,
            cmdLine.begin() + paramOffset + commands[i].m_paramsFound);

        paramOffset += commands[i].m_paramsFound;

        results.first.push_back(&cmd);
        results.second = ScanResultAction::Executed;
        if (!cmd.Exec(localParams, session))
        {
            // We still executed _something_, it just failed
            break;
        }
        ++executed;
    }

    return results;
}