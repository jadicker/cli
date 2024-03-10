#include "param.h"

using namespace cli::v2;

Param::Param(std::string name)
        : m_name(std::move(name))
{}

bool Param::Validate(ParamContext& ctx, const std::string& token) const
{
    return Parse(ctx, token).has_value();
}

std::optional<std::any> Param::ParseWrapper(ParamContext& ctx, const std::string& token) const
{
    auto result = Parse(ctx, token);
    if (result)
    {
        ctx.m_parameters.push_back(shared_from_this());
    }
    return result;
}

