#pragma once

#include "cli2.h"

#include "detail/autocomplete.h"
#include "detail/history.h"

#include "historystorage.h"
#include "volatilehistorystorage.h"

#include <fstream>
#include <iostream>
#include <functional>
#include <memory>

class ConsoleTestRunner;
class TerminalTests;

namespace cli
{
    using Completions = std::vector<detail::AutoCompletion>;

    // this class provides a global output stream
    class OutStream : public std::basic_ostream<char>, public std::streambuf
    {
    public:
        OutStream() : std::basic_ostream<char>(this)
        {
        }

        // std::streambuf overrides
        std::streamsize xsputn(const char* s, std::streamsize n) override
        {
            for (auto os : ostreams)
                os->rdbuf()->sputn(s, n);
            return n;
        }
        int overflow(int c) override
        {
            for (auto os : ostreams)
                *os << static_cast<char>(c);
            return c;
        }

        void Register(std::ostream& o)
        {
            ostreams.push_back(&o);
        }

        void UnRegister(std::ostream& o)
        {
            ostreams.erase(std::remove(ostreams.begin(), ostreams.end(), &o), ostreams.end());
        }

    private:
        std::vector<std::ostream*> ostreams;
    };

    class Cli
    {
        friend class ConsoleTestRunner;

    public:
        ~Cli() = default;
        // disable value semantics
        Cli(const Cli&) = delete;
        Cli& operator = (const Cli&) = delete;
        // enable move semantics
        Cli(Cli&&) = default;
        Cli& operator = (Cli&&) = default;

        /**
         * @brief Construct a new Cli object having a given root menu that contains the first level commands available.
         *
         * @param _rootMenu is the @c Menu containing the first level commands available to the user.
         * @param historyStorage is the policy for the storage of the cli commands history. You must pass an istance of
         * a class derived from @c HistoryStorage. The library provides these policies:
         *   - @c VolatileHistoryStorage
         *   - @c FileHistoryStorage it's a persistent history. I.e., the command history is preserved after your application
         *     is restarted.
         *
         * However, you can develop your own, just derive a class from @c HistoryStorage .
         */
        Cli(std::unique_ptr<HistoryStorage> historyStorage = std::make_unique<VolatileHistoryStorage>()) :
            globalHistoryStorage(std::move(historyStorage)),
            exitAction{}
        {
        }

        void SetRootMenu(std::shared_ptr<Command> inRootMenu)
        {
            rootMenu = inRootMenu;
        }

        /**
         * @brief Add a global exit action that is called every time a session (local or remote) gets the "exit" command.
         *
         * @param action the function to be called when a session exits, taking a @c std::ostream& parameter to write on that session console.
         */
        void ExitAction(const std::function< void(std::ostream&)>& action) { exitAction = action; }

        /**
         * @brief Add an handler that will be called when a @c std::exception (or derived) is thrown inside a command handler.
         * If an exception handler is not set, the exception will be logget on the session output stream.
         *
         * @param handler the function to be called when an exception is thrown, taking a @c std::ostream& parameter to write on that session console
         * and the exception thrown.
         */
        void StdExceptionHandler(const std::function< void(std::ostream&, const std::string& cmd, const std::exception&) >& handler)
        {
            exceptionHandler = handler;
        }

        /**
         * @brief Get a global out stream object that can be used to print on every session currently connected (local and remote)
         *
         * @return TerminalOutStream& the reference to the global out stream writing on every session console.
         */
        static OutStream& cout()
        {
            return *CoutPtr();
        }

    private:
        friend class CliSession;

        static std::shared_ptr<OutStream> CoutPtr()
        {
            static std::shared_ptr<OutStream> s = std::make_shared<OutStream>();
            return s;
        }

        Command* RootMenu() { return rootMenu.get(); }

        void ExitAction(std::ostream& out)
        {
            if (exitAction)
                exitAction(out);
        }

        void StdExceptionHandler(std::ostream& out, const std::string& cmd, const std::exception& e)
        {
            if (exceptionHandler)
                exceptionHandler(out, cmd, e);
            else
                out << e.what() << '\n';
        }

        void StoreCommands(const std::vector<std::string>& cmds)
        {
            globalHistoryStorage->Store(cmds);
        }

        std::vector<std::string> GetCommands() const
        {
            return globalHistoryStorage->Commands();
        }

    private:
        std::unique_ptr<HistoryStorage> globalHistoryStorage;
        std::shared_ptr<Command> rootMenu;
        std::function<void(std::ostream&)> exitAction;
        std::function<void(std::ostream&, const std::string& cmd, const std::exception&)> exceptionHandler;
    };

    class CliSession
    {
        friend class ConsoleTestRunner;
		friend class ::TerminalTests;

    public:
        CliSession(Cli& _cli, std::ostream& _out, std::size_t historySize = 100);
        virtual ~CliSession() noexcept { coutPtr->UnRegister(out); }

        // disable value semantics
        CliSession(const CliSession&) = delete;
        CliSession& operator = (const CliSession&) = delete;
        // disable move semantics
        CliSession(CliSession&&) = delete;
        CliSession& operator = (CliSession&&) = delete;

        void SetSilent(bool silent)
        {
            m_silent = silent;
        }

        bool Feed(const std::string& cmd,
                  bool dontSaveCommand = false,
                  bool printCmd = false,
                  bool silentOutput = false);

        void RunProgram(const std::string& name, const std::vector<std::string>& program);

        void Prompt()
        {
            SetPromptSize(PromptImpl());
        }

        std::string GetCurrentName() const
        {
            assert(m_current);
            return m_current->Name();
        }

        void SetRootMenu(const std::shared_ptr<Command>& inRootMenu)
        {
            //currentGlobalScopeMenu.reset(new Command(""));
            //currentGlobalScopeMenu->TransferRootCommands(*inRootMenu);
            cli.SetRootMenu(inRootMenu);

            m_current = inRootMenu;
            m_rootMenu = m_current;
        }

        void Current(const std::shared_ptr<Command>& menu)
        {
            if (m_testingExecution)
            {
                return;
            }

            m_menuParamIndex = 0;
            m_current = menu;
        }

        void PushTop()
        {
            m_top = m_current;
        }

        // Pop to m_top
        void Pop();

        std::ostream& GetOutStream();

        void Help() const;

        void Exit();

        void ExitAction(const std::function<void(std::ostream&)>& action)
        {
            exitAction = action;
        }

        void ShowHistory() const { history.Show(out); }

        std::string PreviousCmd(const std::string& line)
        {
            return history.Previous(line);
        }

        std::string NextCmd()
        {
            return history.Next();
        }

        CompletionResults GetCompletions(const std::string& currentLine, size_t param);

        void ResetCompletions() { m_menuParamIndex = 0; }

        virtual void SetPromptSize(size_t size) {}

    private:
        size_t PromptImpl();

        CompletionResults GetCompletionsForCommand(Command* command,
                                                   const std::string& currentLine,
                                                   size_t param);

        Cli& cli;
        std::shared_ptr<OutStream> coutPtr;
        std::shared_ptr<Command> m_current = nullptr;

        // Marker that can be set, used for popping scopes
        std::shared_ptr<Command> m_top = nullptr;

        // Param index to use for command matching (rather than param matching)
        Completions m_previousCompletions;
        size_t m_menuParamIndex = 0;

        // Globals attached to current
        std::shared_ptr<Command> currentGlobalScopeMenu;
        // Globals attached to the CLI
        std::shared_ptr<Command> globalScopeMenu;
        std::shared_ptr<Command> m_rootMenu;
        std::shared_ptr<const Command> m_exitCommand = nullptr;
        std::ofstream m_nullOut;
        std::ostream& out;
        std::function<void(std::ostream&)> exitAction = [](std::ostream&) {};
        cli::detail::History history;
        bool exit{ false }; // to prevent the prompt after exit command
        bool m_silent = false;
        bool m_testingExecution = false;
    };
}
