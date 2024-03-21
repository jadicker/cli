#pragma once

#include "detail/autocomplete.h"

#include <any>
#include <iostream>
#include <optional>
#include <memory>
#include <string>
#include <vector>

// Used to unpack parameters from the variable `const auto& params`
#define PARAMS(...) params.back()->GetParameters().template GetParams<__VA_ARGS__>(out)

namespace cli
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
        static std::optional<T> StaticPreviousParam(const std::vector<std::shared_ptr<const Param>>& params,
                                                    size_t& skip)
        {
            for (auto iter = params.rbegin(); iter != params.rend(); ++iter)
            {
                if (skip > 0)
                {
                    --skip;
                    continue;
                }

                std::any value = (*iter)->GetValue();
                // Also note that this cast is against a pointer to get back a nullptr on failure
                // instead of throwing an exception.
                if (auto* param = std::any_cast<T>(&value))
                {
                    return *param;
                }
            }
            return {};
        }

        template <typename T>
        std::optional<T> GetPreviousParam(size_t skip = 0) const
        {
            return StaticPreviousParam<T>(m_parameters, skip);
        }
    };

    inline std::ostream& operator<<(std::ostream& os, const Param& param)
    {
        os << param.GetTypeName() << " " << param.GetName();
        return os;
    }
}
