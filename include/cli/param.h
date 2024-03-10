#pragma once

#include "detail/autocomplete.h"

#include <any>
#include <optional>
#include <memory>
#include <string>
#include <vector>

// Used to unpack parameters from the variable `const auto& params`
#define PARAMS(...) params.template GetParams<__VA_ARGS__>()

namespace cli
{
	namespace v2
	{
		class Command;

		using Completions = std::vector<detail::AutoCompletion>;

		struct CompletionResults
		{
			std::shared_ptr<const Command> m_command;
			size_t m_completionParamIndex = std::numeric_limits<size_t>::max();
			Completions m_completions;
		};

        struct ParamContext;

        class Param;
		class Param : public std::enable_shared_from_this<Param>
		{
		public:
			explicit Param(std::string name);

			virtual ~Param() = default;

			// Returns false if validation fails, result is stored internally
			virtual bool Prepare(ParamContext& ctx, const std::string& token) = 0;

			// Same as parse, but only validates.
			bool Validate(ParamContext& ctx, const std::string& token) const;

			virtual Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const = 0;

			virtual std::any GetValue() const = 0;

			const std::string& GetName() const { return m_name; }
            virtual const char* GetTypeName() const = 0;

        protected:
            std::optional<std::any> ParseWrapper(ParamContext& ctx, const std::string& token) const;

		private:
            virtual std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const = 0;

			std::string m_name;
		};

        // TODO: Pass prior commands/parameters for advanced auto-complete
        struct ParamContext
        {
            std::ostream& m_out;
            // In-order parameters already parsed
            std::vector<std::shared_ptr<const Param>> m_parameters;

            template <typename T>
            T GetPreviousParam() const
            {
                for (auto iter = m_parameters.rbegin(); iter != m_parameters.rend(); ++iter)
                {
                    std::any value = (*iter)->GetValue();
                    // Also note that this cast is against a pointer to get back a nullptr on failure
                    // instead of throwing an exception.
                    if (auto** module = std::any_cast<T>(&value))
                    {
                        return *module;
                    }
                }
                return nullptr;
            }
        };
	}
}
