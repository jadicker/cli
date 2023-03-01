
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
    current(cli.RootMenu()),
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
#ifdef CLI_HISTORY_CMD
    globalScopeMenu->Insert(
        "history",
        [this](std::ostream&) { ShowHistory(); },
        "Show the history"
    );
#endif
}

bool CliSession::Feed(const std::string& cmd, bool silent)
{
    std::vector<std::string> strs;
    detail::split(strs, cmd);
    if (strs.empty()) return false; // just hit enter

    if (!silent)
    {
        history.NewCommand(cmd); // add anyway to history
    }

    try
    {
        // global cmds check
        auto result = current->ScanCmds(strs, *this);

        if (!result.second)
        {
            result = currentGlobalScopeMenu->ScanCmds(strs, *this);
        }

        // root menu recursive cmds check
        if (!result.second)
        {
            result = globalScopeMenu->ScanCmds(strs, *this);
        }

        if (!result.second)
        {
            if (!silent)
            {
                if (result.first.empty())
                {
                    out << "Command '" << Style::Command() << strs[0] << reset << "' not found.\n";
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
            // - 1 because the leaf command isn't a menu
            for (size_t i = 0; i < result.first.size() - 1; ++i)
            {
                Exit();
            }
        }

        return result.second;
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

const Command* CliSession::GetCurrentCommand(const std::string& line) const
{
    std::vector<std::string> strs;
    detail::split(strs, line);

    if (strs.empty())
    {
        return nullptr;
    }

    auto commands = current->GetCurrentCommands(line);
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
    /*
    if (auto* command = current->FindCommand(strs))
    {
        return command;
    }

    // root menu recursive cmds check
    return globalScopeMenu->FindCommand(strs);
    */
}

size_t CliSession::PromptImpl()
{
    if (exit || !current) return 0;

    auto prompt = current->Prompt();
    std::string suffix = "> ";
    out << beforePrompt
        << prompt
        << afterPrompt
        << suffix
        << std::flush;

    return prompt.size() + suffix.size();
}

void CliSession::Help() const
{
    out << "Commands available:\n";
    globalScopeMenu->MainHelp(out);
    current->MainHelp(out);
}

void CliSession::Exit()
{
    if (current = current->GetParent())
    {
        return;
    }

    exitAction(out);
    cli.ExitAction(out);

    auto cmds = history.GetCommands();
    cli.StoreCommands(cmds);

    exit = true; // prevent the prompt to be shown
}

CliSession::CompletionResults CliSession::GetCompletions(std::string currentLine, const size_t param)
{
    std::vector<std::string> params;
    detail::split(params, currentLine);

    auto commands = current->GetCommands(params, param);

    size_t currentParam = 0;
    std::for_each(commands.begin(), commands.end(),
        [&currentParam](const auto& pair) { currentParam += pair.m_paramsFound; });

    if (commands.empty())
    {
        auto completions = current->GetChildCommandCompletions(params.empty() ? "" : params[0]);
        if (completions.empty())
        {
            return {};
        }

        // Rotate completion logic
        current->m_commandAutoCompleteIndex = (current->m_commandAutoCompleteIndex + 1) % completions.size();
        completions.insert(completions.end(), completions.begin(), completions.begin() + current->m_commandAutoCompleteIndex);
        completions.erase(completions.begin(), completions.begin() + current->m_commandAutoCompleteIndex);
        return { nullptr, param, completions };
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
        current->m_commandAutoCompleteIndex = (current->m_commandAutoCompleteIndex + 1) % completions.size();
        completions.insert(completions.end(), completions.begin(), completions.begin() + current->m_commandAutoCompleteIndex);
        completions.erase(completions.begin(), completions.begin() + current->m_commandAutoCompleteIndex);
        return { &commands.back().m_command.get(), param, completions };
    }

    bool valid = true;
    size_t exitsRequired = 0;
    size_t paramOffset = 0;
    for (size_t i = 0; i < commands.size(); ++i)
    {
        Command& cmd = commands[i].m_command.get();

        if (paramOffset + commands[i].m_paramsFound >= params.size())
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
        results.second = false;
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
        results.second = true;
        if (!cmd.Exec(localParams, session))
        {
            // We still executed _something_, it just failed
            break;
        }
        ++executed;
    }

    return results;
}