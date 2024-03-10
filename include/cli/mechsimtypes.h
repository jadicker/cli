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
			out << Style::Red() << "Invalid object id " << reset
				<< "'" << objectIdStr << "' for '" << paramName << "'\n";
			return nullptr;
		}

		T* specificObject = dynamic_cast<T*>(object);
		if (!specificObject)
		{
			out << Style::Error("Object id ") << reset 
				<< objectId << " refers to '" << object->GetName()
				<< "' which is not a '" << paramName << "'\n";
			return nullptr;
		}

		return specificObject;
	}

	class AutoCompleter
	{
	public:
		using Completions = std::vector<detail::AutoCompletion>;

		AutoCompleter(Completions&& completions)
		{
			m_completions = std::move(completions);
			m_current = 0;
		}

		bool HasValues() const { return !m_completions.empty(); }
		size_t Size() const { return m_completions.size(); }

		const detail::AutoCompletion* Current()
		{
			if (!HasValues()) { return {}; }
			return &m_completions[m_current];
		}

		// Peek 0 returns the next element.  Wraps around, but returns nullptr when i >= size() -1
		const detail::AutoCompletion* Peek(size_t i)
		{
			const size_t size = Size();
			if (i >= size - 1)
			{
				return nullptr;
			}

			return &m_completions[(m_current + 1 + i) % size];
		}

		Completions GetAutoCompletions(size_t cur, const std::string& paramStr) const
		{
			size_t count = m_completions.size();
			Completions completions;
			completions.reserve(count);

			auto isCompletionValid = [&paramStr, this](size_t i)
				{
					return paramStr.empty() || m_completions[i].text.find(paramStr) == 0;
				};

			if (isCompletionValid(cur))
			{
				completions.push_back(m_completions[cur]);
			}

			for (size_t i = (cur + 1) % count; i != cur; i = (i + 1) % count)
			{
				if (isCompletionValid(i))
				{
					completions.push_back(m_completions[i]);
				}
			}
			return completions;
		}

	private:
		size_t m_current;
		Completions m_completions;
	};

	// TODO: Maybe push towards the version with error reporting
	template <typename T>
	inline const T* GetObjRaw(const std::string& objectIdStr)
	{
		MechSim::ObjectId objectId = MechSim::ObjectId::FromString(objectIdStr);
		return MechSim::GetObjectRegistry().Get(objectId);
	}

	namespace ObjFilters
	{
		// All filters return true if obj passes the filter
		struct None
		{
			template <typename T>
			static bool Get(const v2::ParamContext&, const T*)
			{
				return true;
			}
		};

		struct MountableByController
		{
			template <typename T>
			static bool Get(const v2::ParamContext& ctx, const T* obj)
			{
                const auto* controller = ctx.GetPreviousParam<MechSim::Controller*>();
				if (!controller)
				{
					return false;
				}
				return controller->CanControl(obj);
			}
		};

		struct NotInstalled
		{
			template <typename T>
			static bool Get(const v2::ParamContext& ctx, const T* obj)
			{
				return obj->GetId().GetRootId() == MechSim::WorldObjectId.GetRootId() ||
					obj->GetId().GetRootId() == MechSim::AnonObjectId.GetRootId();
			}
		};

		struct Installed
		{
			template <typename T>
			static bool Get(const v2::ParamContext& ctx, const T* obj)
			{
				return MechSim::ObjectId(obj->GetId().GetRootId()) != MechSim::WorldObjectId &&
					MechSim::ObjectId(obj->GetId().GetRootId()) != MechSim::AnonObjectId;
			}
		};

		struct IsMech
		{
			template <typename T>
			static bool Get(const v2::ParamContext& ctx, const T* obj)
			{
				return obj->GetId().GetLeaf().second == 0;
			}
		};
	}

#if 0
	template <typename T, typename FilterFnObj>
	class FilteredObjParam final
	{
	public:
		using type = T;
		using filter = FilterFnObj;

		T& operator*() { return *m_object; }
		T* operator->() { return m_object; }

		explicit operator bool() const { return m_object; }

		bool Create(std::ostream& out, const std::string& paramName, const std::string& objectIdStr)
		{
			m_object = cli::GetObj<T>(out, paramName, objectIdStr);
			if (!static_cast<bool>(m_object))
			{
				m_object = nullptr;
				return false;
			}

			if (!filter::template Get<T>(m_object))
			{
				m_object = nullptr;
				return false;
			}

			return true;
		}

		// Only call from safe contexts!
		T& Get() { return *m_object; }

		MechSim::ObjectId GetId() const
		{
			const auto* obj = GetObj();
			return obj ? obj->GetId() : MechSim::NullObjectId;
		}

		MechSim::ObjectGUID GetGUID() const
		{
			const auto* obj = GetObj();
			return obj ? obj->GetGUID() : MechSim::ObjectGUID();
		}

		MechSim::Object* GetObj() { return dynamic_cast<MechSim::Object*>(m_object); }
		const MechSim::Object* GetObj() const { return dynamic_cast<MechSim::Object*>(m_object); }

	private:
		T* m_object = nullptr;
	};

	template <typename T, typename FilterFnObj>
	inline std::ostream& operator<<(std::ostream& os, const FilteredObjParam<T, FilterFnObj>& objParam)
	{
		if (!objParam.GetObj())
		{
			os << Style::Object() << reset << " (" << objParam.GetId() << ")" << reset;
			return os;
		}

		os << Style::Object() << objParam.GetObj()->GetName() << reset << " (" << objParam.GetId() << ")" << reset;
		return os;
	}

	template <typename T>
	using ObjParam = FilteredObjParam<T, ObjFilters::None>;

	using Powerable = ObjParam<MechSim::Powerable>;
	using Readable = ObjParam<MechSim::Readable>;
	using Joystick = ObjParam<MechSim::Joystick>;
	using Part = ObjParam<MechSim::Part>;
#endif

	class PartName
	{
	public:
		bool Create(std::ostream& out, const std::string& paramName, const std::string& partName)
		{
			const MechSim::ObjectRegistry::PartClassInfo* info = MechSim::GetObjectRegistry().FindPartClass(partName);
			if (!info)
			{
				m_name.clear();
				out << paramName << ": received invalid part name '" << partName << "'" << std::endl;
				return false;
			}

			m_name = info->m_name;
			return true;
		}

		std::string GetName() const { return m_name; }

	private:
		// Part doesn't actually have the class name...
		std::string m_name;
	};

	class PartGUID
	{
	public:
		bool Create(std::ostream& out, const std::string& paramName, const std::string& partNameOrObjectId)
		{
			m_input = partNameOrObjectId;

			auto& reg = MechSim::GetObjectRegistry();
			const MechSim::ObjectRegistry::PartClassInfo* info = reg.FindPartClass(partNameOrObjectId);
			if (info)
			{
				m_partInfo = info->m_partInfo;
				return true;
			}

			const auto objectId = MechSim::ObjectId::FromString(partNameOrObjectId);
			if (auto part = dynamic_cast<const MechSim::Part*>(reg.Get(objectId)))
			{
				const auto* classInfo = reg.GetPartClassInfo(part->GetClass());
				if (!classInfo)
				{
					out << "No class info found for " << objectId << ", for part name '"
						<< part->GetName() << "'!" << std::endl;
					return false;
				}

				m_partInfo = classInfo->m_partInfo;
				return true;
			}

			out << paramName << ": received invalid part name/object id '" << partNameOrObjectId << "'" << std::endl;
			return false;
		}

		const MechSim::PartInfo* GetPartInfo() const { return m_partInfo; }
		const std::string& GetInputParam() const { return m_input; }

	private:
		const MechSim::PartInfo* m_partInfo = nullptr;
		std::string m_input;
	};

	class Id
	{
	public:
		Id() = default;

		explicit operator bool() const { return Validate(m_id); }
		operator size_t() const { return m_id; }

		bool Create(std::ostream& out, const std::string& paramName, const std::string& idStr)
		{
			if (auto val = MechSim::ReadInt(idStr); val && Validate(static_cast<size_t>(*val)))
			{
				m_id = static_cast<size_t>(*val);
				return true;
			}
			out << Style::Error(paramName + ": received invalid id ") << idStr << "\n";
			return false;
		}

	protected:
		Id(size_t id) : m_id(id) {}

		virtual bool Validate(size_t id) const { return m_id != std::numeric_limits<size_t>::max(); }

		size_t m_id = std::numeric_limits<size_t>::max();
	};

	class MechId : public Id
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			const auto& allRootObjectIds = MechSim::ObjectRegistry::GetInstance().GetAllRootObjectIds();
			for (const auto& mechId : allRootObjectIds)
			{
				results.push_back({ mechId.ToString(false), "Mech description here..."});
			}
			return results;
		}

		MechSim::Mech* GetMech() const
		{
			return MechSim::FindObject<MechSim::Mech>(m_id);
		}

	protected:
		bool Validate(size_t id) const override
		{
			AutoCompleter::Completions results;
			const auto& allRootObjectIds = MechSim::ObjectRegistry::GetInstance().GetAllRootObjectIds();
			MechSim::ObjectId objId = MechSim::MakeObjectId(static_cast<MechSim::ObjectIdElementType>(id));
			auto iter = std::find_if(allRootObjectIds.begin(), allRootObjectIds.end(),
				[objId](const auto& mechId) { return mechId == objId; });

			return iter != allRootObjectIds.end();
		}
	};

	class ReactorLine : public Id
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			const auto& plugs = MechSim::GetMech()->GetReactor()->GetPlugs();
			for (size_t i = 0; i < plugs.size(); ++i)
			{
				std::stringstream str;
				str << "Plug " << i << " (" << plugs[i].GetVoltage() << "V)";
				results.push_back({ std::to_string(i), str.str() });
			}
			return results;
		}

	protected:
		bool Validate(size_t id) const override
		{
			return id < MechSim::GetMech()->GetReactor()->kConnectionCount;
		}
	};

	class ValidReactorLine : public ReactorLine
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			const auto& plugs = MechSim::GetMech()->GetReactor()->GetPlugs();
			for (size_t i = 0; i < plugs.size(); ++i)
			{
				std::stringstream str;
				str << "Plug " << i << " (" << plugs[i].GetVoltage() << "V)";
				results.push_back({ std::to_string(i), str.str() });
			}
			return results;
		}
	};

	class ConnectorPort : public Id
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			if (auto connectable = GetActiveConnectable())
			{
				connectable->VisitAllConnectors([&results](const MechSim::Connector* connector) -> bool
					{
						results.push_back({ std::to_string(results.size()), connector->GetDescription() });
						return true;
					});
			}
			return results;
		}

	protected:
		bool Validate(size_t id) const override
		{
			if (auto connectable = GetActiveConnectable())
			{
				return id < connectable->GetPortCount();
			}

			return false;
		}

		static const MechSim::Connectable* GetActiveConnectable()
		{
			auto& game = MechSim::Game::GetInstance();
			if (game.m_menuObjects.empty())
			{
				return nullptr;
			}

			return dynamic_cast<const MechSim::Connectable*>(game.m_menuObjects.back());
		}
	};

	// Can be an index or a string of chars in {x: 0, y: 1, z: 2, w: 3}
	class InputAxisIds
	{
	public:
		InputAxisIds() = default;

		const std::vector<int>& GetAxes() const { return m_ids; }

		bool Create(std::ostream& out, const std::string& paramName, const std::string& idStr)
		{
			auto index = MechSim::ReadInt(idStr);
			if (index)
			{
				if (*index < 0 || *index > 3)
				{
					out << Style::Error(paramName + ": received invalid input axis, must be 0,1,2, or 3.  Got ") << idStr << "\n";
					return false;
				}

				m_ids.push_back(*index);
				return true;
			}

			auto pushId = [this](int id)
				{
					if (std::find(m_ids.begin(), m_ids.end(), id) == m_ids.end())
					{
						m_ids.push_back(id);
					}
				};

			for (const char c : idStr)
			{
				if (c == 'x') { pushId(0); }
				else if (c == 'y') { pushId(1); }
				else if (c == 'z') { pushId(2); }
				else if (c == 'w') { pushId(3); }
				else
				{
					out << Style::Error(paramName + ": received invalid input axis name ") << c << "\n";
					m_ids.clear();
					return false;
				}
			}

			return !m_ids.empty();
		}

	private:
		std::vector<int> m_ids;
	};

	// Not sure of a better way to do this yet
#define DEFINE_CLAMPED_FLOAT(name /* class name */, min /* float */, max /* float */) \
	class name \
	{ \
	public: \
		constexpr float GetMin() const { return (min); } \
		constexpr float GetMax() const { return (max); } \
		\
		operator float() const { return m_val; } \
		\
		bool Create(std::ostream& out, const std::string& paramName, const std::string& str) \
		{ \
			float val = detail::FromString<float>::get(out, paramName, str); \
			if (val < GetMin() || val > GetMax()) \
			{ \
				out << paramName << ": received invalid float " << str << "\n"; \
				return false; \
			} \
			m_val = val; \
			return true; \
		} \
	\
	private: \
		float m_val = 0.0f; \
	};

	DEFINE_CLAMPED_FLOAT(Axis, -1.0f, 1.0f);
	DEFINE_CLAMPED_FLOAT(TimelineSeconds, 0.0f, 30.0f);

	template <typename T, typename Enable>
	struct ParamAutoComplete;

	// Will wipe token if an auto-completion already happened
	template <typename T, typename Enable = void>
	struct ParamAutoComplete
	{
		static AutoCompleter Get(std::string& token)
		{
			return AutoCompleter({});
		}
	};

	template <typename T>
	struct ParamAutoComplete<T,
							 std::enable_if_t<std::is_base_of<MechSim::Object, std::decay_t<T>>::value>
							>
	{
		static AutoCompleter Get(std::string& token)
		{
			AutoCompleter::Completions completions;
			const auto& objects = MechSim::GetAllObjects<T>();
			for (const auto* obj : objects)
			{
				completions.push_back({ detail::FromString<T>::get(*obj), obj->GetDescription() });
			}
			return AutoCompleter(std::move(completions));
		}
	};

#if 0
	template <typename T, typename FilterFnObj>
	struct ParamAutoComplete<FilteredObjParam<T, FilterFnObj>>
	{
		static AutoCompleter Get(std::string& token)
		{
			AutoCompleter::Completions completions;

			const MechSim::ObjectId filterId = MechSim::ObjectId::FromString(token);

			const auto* mech = MechSim::GetMech();
			auto& reg = MechSim::ObjectRegistry::GetInstance();
			const auto& objects = !mech ?
				reg.GetAllObjectsOfTypeWithObject<T>(MechSim::NullObjectId) :
				reg.GetAllObjectsOfTypeWithObject<T>(mech->GetId());
			for (const auto& pair : objects)
			{
				if (!FilterFnObj::Get(pair.first))
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

			if (filterId.IsValid())
			{
				token.clear();
			}

			return AutoCompleter(std::move(completions));
		}
	};

	template <typename T>
	struct ParamAutoComplete<T,
							 std::enable_if_t<std::is_base_of<Id, std::decay_t<T>>::value>
							>
	{
		static AutoCompleter Get(std::string& token)
		{
			AutoCompleter::Completions completions = T::GetCompletions();
			return AutoCompleter(std::move(completions));
		}
	};

	template <>
	struct ParamAutoComplete<PartName>
	{
		static AutoCompleter Get(std::string& token)
		{
			AutoCompleter::Completions completions;
			const auto& rawCompletions = MechSim::GetObjectRegistry().GetAutoCompletions(token);
			for (const auto& completion : rawCompletions)
			{
				completions.push_back({ completion.first, completion.second->GetName() });
			}
			// TODO: Pass param string to Get() to filter
			return AutoCompleter(std::move(completions));
		}
	};

	template <>
	struct ParamAutoComplete<InputAxisIds>
	{
		static AutoCompleter Get(std::string& token)
		{
			// TODO: Would be amazing to have auto-complete on the actual instrument, but could be hard
			return AutoCompleter(AutoCompleter::Completions(
				{
					{ "0", "x axis" }, 
					{ "1", "y axis" },
					{ "2", "z axis" },
				}));
		}
	};

	namespace detail
	{
		template <typename T, typename Filter>
		struct FromString<FilteredObjParam<T, Filter>>
		{
			static FilteredObjParam<T, Filter> get(std::ostream& out, const std::string& paramName, const std::string& s)
			{
				FilteredObjParam<T, Filter> result;
				if (!result.Create(out, paramName, s))
				{
					throw bad_conversion();
				}
				return result;
			}
		};
	}

	// TODO: Also a dep in cli.h right now, should be in another header
	template <typename T> struct TypeDesc { static const char* Name() { return ""; } };

#define NAME_BASIC_TYPE(typeName) template <> struct TypeDesc< typeName > { static const char* Name() { return #typeName; } };

	// TODO: Clean this up to be one template, if possible
#define DEFINE_BASIC_FROM_STRING(typeName) \
	namespace detail { \
		template <> \
		struct FromString<typeName> \
		{ \
			static typeName get(std::ostream& out, const std::string& param, const std::string& s) \
			{ \
				typeName result; \
				if (!result.Create(out, param, s)) \
				{ throw bad_conversion(); } \
				return result; \
			} \
		};\
	} \
	\
	NAME_BASIC_TYPE(typeName);

	// Uses default Create path for FromString
	DEFINE_BASIC_FROM_STRING(MechId);
	DEFINE_BASIC_FROM_STRING(ModuleSlotId);
	DEFINE_BASIC_FROM_STRING(ReactorLine);
	DEFINE_BASIC_FROM_STRING(ValidReactorLine);
	DEFINE_BASIC_FROM_STRING(PartName);
	DEFINE_BASIC_FROM_STRING(PartGUID);
	DEFINE_BASIC_FROM_STRING(Joystick);
	DEFINE_BASIC_FROM_STRING(Axis);
	DEFINE_BASIC_FROM_STRING(TimelineSeconds);
	DEFINE_BASIC_FROM_STRING(ConnectorPort);
	DEFINE_BASIC_FROM_STRING(InputAxisIds);

	// Specially defined
	NAME_BASIC_TYPE(Powerable);

#undef DEFINE_BASIC_FROM_STRING
#undef NAME_BASIC_TYPE

#endif

    namespace v2
    {
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
                return v2::TypeDesc<T>::Name();
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

        using AnyObjectParam = v2::FilteredObj<MechSim::Object>;
        using PartParam = v2::FilteredObj<MechSim::Part>;
        using MechParam = v2::FilteredObj<MechSim::Mech>;
        using PowerableParam = v2::FilteredObj<MechSim::Powerable>;
        using Readable = v2::FilteredObj<MechSim::Readable>;

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
    }
}
