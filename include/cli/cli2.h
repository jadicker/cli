#pragma once

#include "params.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>


namespace cli::v2
{
    class CliSession;

    enum class ScanResult
    {
        NoneFound,
        Found,
        BadOrMissingParams
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
        using Callback = std::function<void(std::ostream&, const Parameters& params)>;
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
        size_t GetParamCount() const { return m_params.size(); }

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
                                                     v2::Parameters::Null(),
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
                                                     v2::Parameters::Null(),
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
        //size_t PrepareCommand(std::ostream& out, const std::vector<std::string>& tokens, size_t currentIndex);

        // Same as match command but parses parameters into their value
        PreparationResult Prepare(ParamContext& paramContext,
                                  const std::vector<std::string>& tokens,
                                  size_t currentIndex);

        using ScanResult = std::pair<std::vector<std::shared_ptr<const Command>>, ScanResult>;
        ExecutionResult ExecuteRecursive(std::ostream& out, const std::vector<std::string>& cmdLineTokens);

        //CompletionResults AutoComplete(ParamContext& ctx, const std::vector<std::string>& cmdLineTokens);

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
}


#if 0
{
	using namespace cli::v2;
	auto v2Params = Parameters(
		{
		std::make_shared<FloatParam>("first", 5.0f),
		std::make_shared<ObjectIdParam>("second", MechSim::MakeObjectId(1,2,3,4))
		});

	auto [first, second] = v2Params.GetParams<float, MechSim::ObjectId>();
	::OutputDebugStringA((std::string("Param 0: ") + std::to_string(first)).c_str());
	::OutputDebugStringA((std::string("Param 1: ") + second.ToString()).c_str());
	}
#endif