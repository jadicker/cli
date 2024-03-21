#pragma once

#include "colorprofile.h"
#include "mechsimtostring.h"
#include "detail/autocomplete.h"

#include "cli/param.h"
#include "cli/paramDefinition.h"

#include <cli/detail/fromstring.h>
#include <limits>
#include <type_traits>

#include "MechSim/Central/Mech.h"
#include "MechSim/Central/Modules.h"
#include "MechSim/Controller/Connectable.h"
#include "MechSim/Game/Game.h"
#include "MechSim/Misc/VectorHandle.h"
#include "MechSim/Instrument/Instrument.h"

namespace cli
{
	inline std::string ParamObjectId(std::string&& paramName)
	{
		std::stringstream str;
		str << Style::ObjectId() << paramName << reset;
		return str.str();
	}

	template <typename T>
	T* GetObj(std::ostream& out, const std::string& paramName, const std::string& objectIdStr)
	{
		if (objectIdStr.empty())
		{
			return nullptr;
		}

		MechSim::ObjectId objectId = MechSim::ObjectId::FromString(objectIdStr);
		auto* object = MechSim::GetObjectRegistry().Get(objectId);
		if (!object)
		{
			out << "Invalid object id "
				<< "'" << Style::Red() << objectIdStr << reset << "'"
                << std::endl;
			return nullptr;
		}

		T* specificObject = dynamic_cast<T*>(object);
		if (!specificObject)
		{
			out << "Object id " << objectId
                << " refers to '" << object->GetName()
				<< "' which is not a '" << paramName << "'\n";
			return nullptr;
		}

		return specificObject;
	}

    namespace ObjFilters
    {
        // All filters return true if obj passes the filter
        struct None
        {
            template <typename T>
            static bool Get(const ParamContext&, const T*)
            {
                return true;
            }
        };

        struct MountableByController
        {
            template <typename T>
            static bool Get(const ParamContext& ctx, const T* obj)
            {
                const auto controller = ctx.GetPreviousParam<MechSim::Controller*>();
                if (!controller)
                {
                    return false;
                }
                return (*controller)->CanControl(obj);
            }
        };

        struct NotInstalled
        {
            template <typename T>
            static bool Get(const ParamContext& ctx, const T* obj)
            {
                return obj->GetId().GetRootId() == MechSim::WorldObjectId.GetRootId() ||
                       obj->GetId().GetRootId() == MechSim::AnonObjectId.GetRootId();
            }
        };

        struct Installed
        {
            template <typename T>
            static bool Get(const ParamContext& ctx, const T* obj)
            {
                return MechSim::ObjectId(obj->GetId().GetRootId()) != MechSim::WorldObjectId &&
                       MechSim::ObjectId(obj->GetId().GetRootId()) != MechSim::AnonObjectId;
            }
        };

        struct IsMech
        {
            template <typename T>
            static bool Get(const ParamContext& ctx, const T* obj)
            {
                return obj->GetId().GetLeaf().second == 0;
            }
        };
    }

    template <typename T, typename FilterFn = ObjFilters::None>
    class FilteredObj : public Param
    {
    public:
        explicit FilteredObj(std::string name) : Param(std::move(name))
        {
        }

        bool Prepare(ParamContext& ctx, const std::string& token) override
        {
            auto result = ParseWrapper(ctx, token);
            if (!result)
            {
                return false;
            }
            m_object = std::any_cast<T*>(*result);
            return true;
        }

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            Completions completions;

            const MechSim::ObjectId filterId = MechSim::ObjectId::FromString(token);

            const auto* mech = MechSim::GetMech();
            auto& reg = MechSim::ObjectRegistry::GetInstance();
            const auto& objects = !mech ?
                                  reg.GetAllObjectsOfTypeWithObject<T>(MechSim::NullObjectId) :
                                  reg.GetAllObjectsOfTypeWithObject<T>(mech->GetId());
            for (const auto& pair : objects)
            {
                if (!FilterFn::Get(ctx, pair.first))
                {
                    continue;
                }

                const MechSim::Object* obj = pair.second;
                if (filterId.IsValid() && !obj->GetId().StartsWith(filterId))
                {
                    continue;
                }

                auto text = obj->GetName();
                const auto& desc = obj->GetDescription();
                if (!desc.empty())
                {
                    text += ": " + obj->GetDescription();
                }
                completions.push_back({ obj->GetId().ToString(), text });
            }

            return completions;
        }

        std::any GetValue() const override { return m_object; }

        const char* GetTypeName() const override
        {
            // TODO: Child types for filters to get the correct name?
            return TypeDesc<T>::Name();
        }

    protected:
        T* m_object = nullptr;

    private:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            T* object = cli::GetObj<T>(ctx.m_out, GetName(), token);

            if (!static_cast<bool>(object) ||
                !FilterFn::template Get<T>(ctx, object))
            {
                object = nullptr;
                return {};
            }

            return object;
        }
    };

    using AnyObjectParam = FilteredObj<MechSim::Object>;
    NAME_TYPE(Object, FilteredObj<MechSim::Object>);
    using PartParam = FilteredObj<MechSim::Part>;
    NAME_TYPE(Part, FilteredObj<MechSim::Part>);
    using MechParam = FilteredObj<MechSim::Mech>;
    NAME_TYPE(Mech, FilteredObj<MechSim::Mech>);
    using PowerableParam = FilteredObj<MechSim::Powerable>;
    NAME_TYPE(PowerableParam, FilteredObj<MechSim::Powerable>);
    using ReadableParam = FilteredObj<MechSim::Readable>;
    NAME_TYPE(Readable, FilteredObj<MechSim::Readable>);

    class PartName : public Param
    {
    public:
        DECLARE_PARAM(PartName, std::string)

        DEFINE_PREPARE(m_value, std::string)

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            using namespace MechSim;

            Completions completions;

            std::vector<std::pair<std::string, const ObjectRegistry::PartClassInfo*>> allParts = GetObjectRegistry().GetAutoCompletions(token);
            for (auto&& pair : allParts)
            {
                completions.push_back({ pair.first, pair.second->m_partInfo->m_className });
            }

            return completions;
        }

        std::any GetValue() const override { return m_value; }

        const char* GetTypeName() const override
        {
            return "PartName";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            const auto* info = MechSim::GetObjectRegistry().FindPartClass(token);
            if (!info)
            {
                ctx.m_out << GetName() << ": received invalid part name '" << token << "'" << std::endl;
                return {};
            }

            return info->m_name;
        }

    private:
        // Part doesn't actually have the class name...
        std::string m_value;
    };
    NAME_BASIC_TYPE(PartName);

    class PartInfoParam : public Param
    {
    public:
        DECLARE_PARAM(PartInfoParam, const MechSim::PartInfo*)

        DEFINE_PREPARE(m_value, const MechSim::PartInfo*)

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            Completions completions;
            // TODO: Auto-complete this?
            return completions;
        }

        std::any GetValue() const override { return m_value; }

        const char* GetTypeName() const override
        {
            return "PartInfoParam";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            auto& reg = MechSim::GetObjectRegistry();
            const MechSim::ObjectRegistry::PartClassInfo* info = reg.FindPartClass(token);
            if (info)
            {
                return info->m_partInfo;
            }

            const auto objectId = MechSim::ObjectId::FromString(token);
            if (auto part = dynamic_cast<const MechSim::Part*>(reg.Get(objectId)))
            {
                const auto* classInfo = reg.GetPartClassInfo(part->GetClass());
                if (!classInfo)
                {
                    ctx.m_out << "No class info found for " << objectId << ", for part name '"
                              << part->GetName() << "'!" << std::endl;
                    return false;
                }

                return classInfo->m_partInfo;
            }

            ctx.m_out << GetName() << ": received invalid part name/object id '" << token << "'" << std::endl;
            return {};
        }

    private:
        const MechSim::PartInfo* m_value = nullptr;
    };
    NAME_BASIC_TYPE(PartInfoParam);

    // Can be an index or a string of chars in {x: 0, y: 1, z: 2, w: 3}
    class InputAxisIds : public Param
    {
    public:
        DECLARE_PARAM(InputAxisIds, std::vector<int>)

        DEFINE_PREPARE(m_value, std::vector<int>)

        Completions GetAutoCompletions(ParamContext& ctx, const std::string& token) const override
        {
            Completions completions;
            // TODO: Auto-complete this?
            return completions;
        }

        std::any GetValue() const override { return m_value; }

        const char* GetTypeName() const override
        {
            return "PartInfoParam";
        }

    protected:
        std::optional<std::any> Parse(const ParamContext& ctx, const std::string& token) const override
        {
            std::vector<int> ids;
            auto index = MechSim::ReadInt(token);
            if (index)
            {
                if (*index < 0 || *index > 3)
                {
                    ctx.m_out << Style::Error(GetName() + ": received invalid input axis, must be 0,1,2, or 3.  Got ")
                              << token << "\n";
                    return {};
                }

                ids.push_back(*index);
                return ids;
            }

            auto pushId = [this, &ids](int id)
            {
                if (std::find(ids.begin(), ids.end(), id) == ids.end())
                {
                    ids.push_back(id);
                }
            };

            for (const char c : token)
            {
                if (c == 'x') { pushId(0); }
                else if (c == 'y') { pushId(1); }
                else if (c == 'z') { pushId(2); }
                else if (c == 'w') { pushId(3); }
                else
                {
                    ctx.m_out << Style::Error(GetName() + ": received invalid input axis name ") << c << "\n";
                    ids.clear();
                    return {};
                }
            }

            return ids;
        }

    private:
        std::vector<int> m_value;
    };
    NAME_BASIC_TYPE(InputAxisIds);
}
