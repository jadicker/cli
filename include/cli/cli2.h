#pragma once

#include "params.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>


namespace cli
{
    class CliSession;

    enum class ScanResult
    {
        NoneFound,
        Found,
        // Single/all commands failed with bad or missing params
        BadOrMissingParams,
        // Some commands succeeded but there is a partially completed command
        PartialCompletion,
    };

    struct PreparationResult
    {
        ScanResult m_scanResult;
        // Includes the name token
        size_t m_tokensSuccessfullyParsed;
    };

    struct ExecutionResult
    {
        std::vector<std::shared_ptr<Command>> m_commandsScanned;
        std::shared_ptr<Command> m_partialCommand;
        ScanResult m_action = ScanResult::NoneFound;
        size_t m_paramsConsumed = 0;
    };

    class Command;
    class Command final : public std::enable_shared_from_this<Command>
    {
    public:
        using CommandPtr = std::shared_ptr<const Command>;
        using Callback = std::function<void(std::ostream&, const std::vector<CommandPtr>&)>;
        using ExitCallback = std::function<void()>;
        using PromptDisplayFn = std::function<std::string()>;

        enum class ValidationResult
        {
            NoMatch,
            Match,
            Invalid
        };

        Command() = default;

        Command(std::string name, std::string description)
            : m_name(std::move(name))
            , m_desc(std::move(description))
        {

        }

        Command(std::string name,
                std::string description,
                Parameters params,
                Callback onExecute,
                std::shared_ptr<Command> parent = nullptr)
            : m_name(std::move(name))
            , m_desc(std::move(description))
            , m_params(std::move(params))
            , m_onExec(std::move(onExecute))
            , m_parent(std::move(parent))
        {
        }

        Command(std::string name,
                std::string description,
                Parameters params,
                Callback onExecute,
                ExitCallback onExit,
                std::shared_ptr<Command> parent = nullptr)
            : m_name(std::move(name))
            , m_desc(std::move(description))
            , m_params(std::move(params))
            , m_onExec(std::move(onExecute))
            , m_onExit(std::move(onExit))
            , m_parent(std::move(parent))
        {
        }

        // TODO: Move into constructors?
        void SetDisplayFunc(PromptDisplayFn displayFunc) { m_displayFunc = std::move(displayFunc); }

        const std::string& Name() const { return m_name; }
        const std::string& Description() const { return m_desc; }
        std::string GetSignature() const;

        size_t GetParamCount() const { return m_params.size(); }
        // No guarantee that they're valid, check the context you're in!
        const Parameters& GetParameters() const { return m_params; }

        // Count of name and all parameters
        size_t GetTotalTokens() const { return 1 + GetParamCount(); }

        // True if the command only has a single std::string parameter
        bool IsFreeCommand() const
        {
            return m_params.IsFree();
        }

        std::shared_ptr<Command> GetParent() { return m_parent; }
        std::shared_ptr<const Command> GetParent() const { return m_parent; }

        bool HasChildren() const { return !m_children.empty(); }

        std::string GetPromptDisplay() const
        {
            if (m_displayFunc)
            {
                return m_displayFunc();
            }
            return m_name;
        }

        std::shared_ptr<Command> Insert(std::string name,
                                        std::string description,
                                        Parameters params,
                                        Callback onExecute)
        {
            auto command = std::make_shared<Command>(std::move(name),
                                                     std::move(description),
                                                     std::move(params),
                                                     std::move(onExecute),
                                                     shared_from_this());
            m_children.push_back(command);
            return command;
        }

        std::shared_ptr<Command> Insert(std::string name,
                                        std::string description,
                                        Parameters params,
                                        Callback onExecute,
                                        ExitCallback onExit)
        {
            auto command = std::make_shared<Command>(std::move(name),
                                                     std::move(description),
                                                     std::move(params),
                                                     std::move(onExecute),
                                                     std::move(onExit),
                                                     shared_from_this());
            m_children.push_back(command);
            return command;
        }

        std::shared_ptr<Command> Insert(std::string name,
                                        std::string description,
                                        Callback onExecute)
        {
            auto command = std::make_shared<Command>(std::move(name),
                                                     std::move(description),
                                                     Parameters::Null(),
                                                     std::move(onExecute),
                                                     shared_from_this());
            m_children.push_back(command);
            return command;
        }

        std::shared_ptr<Command> Insert(std::string name,
                                        std::string description,
                                        Callback onExecute,
                                        ExitCallback onExit)
        {
            auto command = std::make_shared<Command>(std::move(name),
                                                     std::move(description),
                                                     Parameters::Null(),
                                                     std::move(onExecute),
                                                     std::move(onExit),
                                                     shared_from_this());
            m_children.push_back(command);
            return command;
        }

        void MainHelp(std::ostream& out);

        virtual void Help(std::ostream& out) const;

        detail::AutoCompletion GetCompletion() const;

        std::shared_ptr<const Command> FindChildCommand(const std::string& name) const;

        virtual ValidationResult Validate(const std::vector<std::string>& cmdLine)
        {
            // Always match/follow menus
            return ValidationResult::Match;
        }

        // Same as match command but parses parameters into their value
        PreparationResult Prepare(ParamContext& paramContext,
                                  const std::vector<std::string>& tokens,
                                  size_t currentIndex);

        ExecutionResult ExecuteRecursive(std::ostream& out, const std::vector<std::string>& cmdLineTokens);

        // Filter for ones that start with token, or all if token is ""
        CompletionResults GetAllChildrenCompletions(const std::string& token) const;

        // Care, this uses m_params to execute the command.  Make sure you prepared!
        void Execute(std::ostream& out);
        void Cleanup() const;

        void ScanRecursiveImpl(ParamContext& ctx,
                               const std::vector<std::string>& cmdLineTokens,
                               ExecutionResult& result);

        CompletionResults AutoCompleteImpl(ParamContext& ctx,
                                           const std::vector<std::string>& paramTokens,
                                           size_t paramIndex) const;

        void AddParams(ParamContext& ctx) const;

        // Returns ParamContext containing parameters from all parents
        ParamContext BuildParamContext(std::ostream& out) const;

        // Returns every parent in order from eldest to youngest.  In other words: command scope, in-order
        std::vector<CommandPtr> GetAllCommands() const;

    private:
        std::string m_name;
        std::string m_desc;
        Parameters m_params;
        Callback m_onExec;
        ExitCallback m_onExit;
        PromptDisplayFn m_displayFunc;

        std::shared_ptr<Command> m_parent;
        std::vector<std::shared_ptr<Command>> m_children;
    };

    using ConstCommands = std::vector<Command::CommandPtr>;

    template <typename T>
    std::optional<T> GetPreviousParamOptional(const ConstCommands& commands, size_t skip = 0)
    {
        for (auto iter = commands.rbegin(); iter != commands.rend(); ++iter)
        {
            const auto& parameters = (*iter)->GetParameters();
            if (auto param = ParamContext::StaticPreviousParam<T>(parameters.GetParams(), skip))
            {
                return param;
            }
        }
        return {};
    }

    // Checked version
    template <typename T>
    T GetPreviousParam(const ConstCommands& commands, size_t skip = 0)
    {
        auto optionalResult = GetPreviousParamOptional<T>(commands, skip);
        // It's usually expected certain params have come in parent commands
        assert(optionalResult);
        return *optionalResult;
    }
}
