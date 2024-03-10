#include "idparams.h"

#include "MechSim/Controller/Connector.h"
#include "MechSim/Central/Modules.h"
#include "MechSim/Central/Mech.h"

using namespace cli::v2;

ConnectorPortParam::ConnectorPortParam(std::string name)
        : PODParam(std::move(name))
{
}

Completions ConnectorPortParam::GetAutoCompletions(ParamContext& ctx, const std::string& token) const
{
    Completions results;
    if (const auto* connectable = ctx.GetPreviousParam<MechSim::Connectable*>())
    {
        connectable->VisitAllConnectors(
                [&results](const MechSim::Connector* connector) -> bool
                {
                    results.push_back({std::to_string(results.size()), connector->GetDescription()});
                    return true;
                });
    }
    return results;
}

std::optional<std::any> ConnectorPortParam::Parse(const ParamContext& ctx, const std::string& token) const
{
    const auto* connectablePart = ctx.GetPreviousParam<MechSim::Part*>();
    const auto* connectable = dynamic_cast<const MechSim::Connectable*>(connectablePart);
    if (!connectable)
    {
        return {};
    }

    auto id = PODParam::ParseImpl(ctx, token);
    if (!id)
    {
        return {};
    }

    if (*id >= connectable->GetPortCount())
    {
        return {};
    }

    return *id;
}

const MechSim::Connectable* ConnectorPortParam::GetActiveConnectable(const ParamContext& ctx)
{
    return ctx.GetPreviousParam<MechSim::Connectable*>();
}

ModuleSlotParam::ModuleSlotParam(std::string name)
        : PODParam(std::move(name))
{
}

Completions ModuleSlotParam::GetAutoCompletions(ParamContext& ctx, const std::string& token) const
{
    Completions results;
    const auto* module = GetModule(ctx);
    assert(module);

    const auto& slots = module->DescribeSlots();
    size_t id = 0;
    for (const std::string& slot: slots)
    {
        results.push_back({std::to_string(id), slot});
        ++id;
    }
    return results;
}

const MechSim::Module* ModuleSlotParam::GetModule(const ParamContext& ctx)
{
    return ctx.GetPreviousParam<MechSim::Module*>();
}

std::optional<std::any> ModuleSlotParam::Parse(const ParamContext& ctx, const std::string& token) const
{
    auto id = PODParam::ParseImpl(ctx, token);
    if (!id)
    {
        return {};
    }

    const MechSim::Module* module = GetModule(ctx);
    assert(module);
#if 0
    if (!module)
    {
        // TODO: Emit error, command structure is no good
        //ctx.m_out << ""
        return {};
    }
#endif

    if (*id >= module->GetSlotCount())
    {
        return {};
    }

    return *id;
}

ReactorLineParam::ReactorLineParam(std::string name)
        : PODParam(std::move(name))
{
}

Completions ReactorLineParam::GetAutoCompletions(ParamContext& ctx, const std::string& token) const
{
    Completions results;
    const auto* reactor = ctx.GetPreviousParam<MechSim::Reactor*>();
    assert(reactor);
    const auto& plugs = reactor->GetPlugs();
    for (size_t i = 0; i < plugs.size(); ++i)
    {
        std::stringstream str;
        str << "Plug " << i << " (" << plugs[i].GetVoltage() << "V)";
        results.push_back({std::to_string(i), str.str()});
    }
    return results;
}

std::optional<std::any> ReactorLineParam::Parse(const ParamContext& ctx, const std::string& token) const
{
    auto id = PODParam::ParseImpl(ctx, token);
    if (!id)
    {
        return {};
    }

    if (*id >= MechSim::Reactor::kConnectionCount)
    {
        return {};
    }

    return *id;
}

