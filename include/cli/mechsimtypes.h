#pragma once

#include "colorprofile.h"
#include "detail/autocomplete.h"

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
	inline std::ostream& operator<<(std::ostream& os, MechSim::ObjectId objectId)
	{
		os << Style::ObjectId() << objectId.ToString(false) << " (" << GetObjectName(objectId) << ")"
		   << reset;
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const MechSim::Object& object)
	{
		os << Style::Object() << object.GetClass() << reset << " " << object.GetId() << reset;
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, MechSim::ObjectHandleAny object)
	{
		os << *object.Get<MechSim::Object>();
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const MechSim::Mech& mech)
	{
		os << Style::Mech() << mech.GetName() << reset << " " << mech.GetId() << reset;
		return os;
	}

	template <typename T, typename IndexType>
	inline std::ostream& operator<<(std::ostream& os, const Util::VectorHandle<T, IndexType>& vectorHandle)
	{
		os << vectorHandle.GetIndex() << reset;
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const MechSim::Object* object)
	{
		if (!object)
		{
			os << Style::Object() << "Invalid" << reset;	
		}
		else
		{
			os << *object;
		}
		return os;
	}

	inline std::string ParamObjectId(std::string&& paramName)
	{
		std::stringstream str;
		str << Style::ObjectId() << paramName << reset;
		return str.str();
	}

	template <typename T>
	T* GetObj(std::ostream& out, const std::string& paramName, const std::string& objectIdStr)
	{
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
		// Return true if this obj passes the filter
		using FilterFn = std::function<bool(const MechSim::Object* obj)>;

		struct None
		{
			static FilterFn Get()
			{
				return [](const MechSim::Object*) { return true; };
			}
		};

		template <typename ObjInterface>
		struct IsA
		{
			static FilterFn Get()
			{
				return [](const MechSim::Object* obj)
					{
						const auto* powerable = dynamic_cast<const ObjInterface*>(obj);
						return static_cast<bool>(powerable);
					};
			}
		};

		struct MountableByController
		{
			static FilterFn Get()
			{
				return [](const MechSim::Object* obj)
				{
					if (!MechSim::Game::GetInstance().m_controllerToMount)
					{
						return false;
					}
					return MechSim::Game::GetInstance().m_controllerToMount->CanControl(obj);
				};
			}
		};

		struct NotInstalled
		{
			static FilterFn Get()
			{
				return [](const MechSim::Object* obj)
				{
					return obj->GetId().GetRootId() == MechSim::WorldObjectId
						|| obj->GetId().GetRootId() == MechSim::AnonObjectId;
				};
			}
		};

		struct IsMech
		{
			static FilterFn Get()
			{
				return [](const MechSim::Object* obj)
				{
					return obj->GetId().GetLeaf().second == 0;
				};
			}
		};
	}

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

			if (!filter::Get()(GetObj()))
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

	using Powerable = FilteredObjParam<MechSim::Powerable, ObjFilters::IsA<MechSim::Powerable>>;
	using Readable = FilteredObjParam<MechSim::Readable, ObjFilters::IsA<MechSim::Readable>>;
	using Joystick = FilteredObjParam<MechSim::Joystick, ObjFilters::None>;
	using Part = FilteredObjParam<MechSim::Part, ObjFilters::None>;
	
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
		const MechSim::Part* m_part;
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

	class ModuleSlotId : public Id
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			const auto& slots = MechSim::Game::GetInstance().m_activeModule->DescribeSlots();
			size_t id = 0;
			for (const std::string& slot : slots)
			{
				results.push_back({ std::to_string(id++), slot });
			}
			return results;
		}
	protected:
		bool Validate(size_t id) const override
		{
			return id < MechSim::Game::GetInstance().m_activeModule->GetSlotCount();
		}
	};

	class MechId : public Id
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			const auto& allRootObjectIds = MechSim::ObjectRegistry::GetInstance().GetAllRootObjects();
			for (const auto& mechId : allRootObjectIds)
			{
				results.push_back({ mechId.ToString(), "Mech description here..."});
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
			const auto& allRootObjectIds = MechSim::ObjectRegistry::GetInstance().GetAllRootObjects();
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
				str << "Plug " << i << " (" << plugs[i].m_voltage << "V)";
				results.push_back({ std::to_string(i), str.str() });
			}
			return results;
		}

	protected:
		bool Validate(size_t id) const override
		{
			return id < MechSim::GetMech()->GetReactor()->ConnectionCount;
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
				if (!plugs[i].m_handle)
				{
					continue;
				}

				std::stringstream str;
				str << "Plug " << i << " (" << plugs[i].m_voltage << "V)";
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

	template <typename T, typename Enable>
	struct ParamAutoComplete;

	template <typename T, typename Enable = void>
	struct ParamAutoComplete
	{
		static AutoCompleter Get()
		{
			return AutoCompleter({});
		}
	};

	template <typename T>
	struct ParamAutoComplete<T,
							 std::enable_if_t<std::is_base_of<MechSim::Object, std::decay_t<T>>::value>
							>
	{
		static AutoCompleter Get()
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

	template <typename T, typename FilterFnObj>
	struct ParamAutoComplete<FilteredObjParam<T, FilterFnObj>>
	{
		static AutoCompleter Get()
		{
			AutoCompleter::Completions completions;
			using ObjectType = typename ObjParam<T>::type;

			const auto* mech = MechSim::GetMech();
			const auto& objects = !mech ? MechSim::GetAllObjectsOfType<ObjectType>() :
				MechSim::ObjectRegistry::GetInstance().GetAllObjectsOfType<ObjectType>(mech->GetId());
			auto filter = FilterFnObj::Get();
			for (const auto* obj : objects)
			{
				if (filter(obj))
				{
					auto text = obj->GetName();
					const auto& desc = obj->GetDescription();
					if (!desc.empty())
					{
						text += ": " + obj->GetDescription();
					}
					completions.push_back({ obj->GetId().ToString(), text });
				}
			}
			return AutoCompleter(std::move(completions));
		}
	};

	template <typename T>
	struct ParamAutoComplete<T,
							 std::enable_if_t<std::is_base_of<Id, std::decay_t<T>>::value>
							>
	{
		static AutoCompleter Get()
		{
			AutoCompleter::Completions completions = T::GetCompletions();
			return AutoCompleter(std::move(completions));
		}
	};

	template <>
	struct ParamAutoComplete<PartName>
	{
		static AutoCompleter Get()
		{
			AutoCompleter::Completions completions;
			const auto& rawCompletions = MechSim::GetObjectRegistry().GetAutoCompletions("");
			for (const auto& completion : rawCompletions)
			{
				completions.push_back({ completion.first, completion.second->GetName() });
			}
			// TODO: Pass param string to Get() to filter
			return AutoCompleter(std::move(completions));
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

#define NAME_BASIC_TYPE(typeName) template <> struct TypeDesc<##typeName##> { static const char* Name() { return #typeName; } };

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
	DEFINE_BASIC_FROM_STRING(ConnectorPort);

	// Specially defined
	NAME_BASIC_TYPE(Powerable);

#undef DEFINE_BASIC_FROM_STRING
#undef NAME_BASIC_TYPE
}
