#pragma once

#include "../paramDefinition.h"

namespace MechSim
{
    class Connectable;
    class Module;
}

namespace cli
{
    class ConnectorPortParam : public PODParam<size_t>
    {
    public:
        explicit ConnectorPortParam(std::string name);

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override;

        const char* GetTypeName() const override
        {
            return "ConnectorPort";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override;

        static const MechSim::Connectable* GetActiveConnectable(const ParamContext& ctx);
    };
    NAME_BASIC_TYPE(ConnectorPortParam);

    class ModuleSlotParam : public PODParam<size_t>
    {
    public:
        explicit ModuleSlotParam(std::string name);

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override;

        const char* GetTypeName() const override
        {
            return "ModuleSlot";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override;

    private:
        static const MechSim::Module* GetModule(const ParamContext& ctx);
    };
    NAME_BASIC_TYPE(ModuleSlotParam);

    class ReactorLineParam : public PODParam<size_t>
    {
    public:
        explicit ReactorLineParam(std::string name);

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override;

        const char* GetTypeName() const override
        {
            return "ReactorLine";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override;
    };
    NAME_BASIC_TYPE(ReactorLineParam);
}
