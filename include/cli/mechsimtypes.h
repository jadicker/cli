#pragma once

#include "colorprofile.h"
#include "detail/autocomplete.h"

#include <limits>
#include <type_traits>

#include "MechSim/Central/Central.h"
#include "MechSim/Central/Mech.h"
#include "MechSim/Game/Game.h"
#include "MechSim/Misc/ObjectUtils.h"
#include "MechSim/Misc/VectorHandle.h"
#include "MechSim/Instrument/Instrument.h"

namespace cli
{
	inline std::ostream& operator<<(std::ostream& os, MechSim::ObjectId objectId)
	{
		os << Style::ObjectId() << objectId.ToString(false) << reset;
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const MechSim::Object& object)
	{
		os << Style::Object() << object.GetName() << " (" << object.GetId() << ") " << reset;
		return os;
	}

	inline std::ostream& operator<<(std::ostream& os, const MechSim::Mech& mech)
	{
		os << Style::Mech() << mech.GetName() << " (" << mech.GetId() << ") " << reset;
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
			out << "Invalid object id '" << objectIdStr << "' for '" << paramName << "'\n";
			return nullptr;
		}

		T* specificObject = dynamic_cast<T*>(object);
		if (!specificObject)
		{
			out << "Object id " << objectId << " refers to '" << object->GetName()
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

		Completions GetAutoCompletions(size_t cur) const
		{
			size_t count = m_completions.size();
			Completions completions;
			completions.reserve(count);
			completions.push_back(m_completions[cur]);

			for (size_t i = (cur + 1) % count; i != cur; i = (i + 1) % count)
			{
				completions.push_back(m_completions[i]);
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

		struct Powerable
		{
			static FilterFn Get()
			{
				return [](const MechSim::Object* obj)
				{
					return !!dynamic_cast<const MechSim::Powerable*>(obj);
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
			return static_cast<bool>(m_object);
		}

		// Only call from safe contexts!
		T& Get() { return *m_object; }

		MechSim::ObjectId GetId() const { return m_object ? m_object->GetId() : MechSim::NullId; }

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
			os << Style::Object() << " (" << objParam.GetId() << ")" << reset;
			return os;
		}

		os << Style::Object() << objParam.GetObj()->GetName() << " (" << objParam.GetId() << ")" << reset;
		return os;
	}

	template <typename T>
	using ObjParam = FilteredObjParam<T, ObjFilters::None>;

	using Powerable = FilteredObjParam<MechSim::Object, ObjFilters::Powerable>;

	using Joystick = FilteredObjParam<MechSim::Joystick, ObjFilters::None>;
	
	class PartName
	{
	public:
		bool Create(std::ostream& out, const std::string& paramName, const std::string& partName)
		{
			m_part = MechSim::GetObjectRegistry().FindMechClass<MechSim::Part>(partName);
			if (!m_part)
			{
				m_name.clear();
				out << paramName << ": received invalid part name '" << partName << "'\n";
				return false;
			}

			m_name = partName;
			return true;
		}

		std::string GetName() const { return m_name; }

	private:
		// Part doesn't actually have the class name...
		std::string m_name;
		const MechSim::Part* m_part;
	};

	class Id
	{
	public:
		Id() = default;

		explicit operator bool() const { return m_id != std::numeric_limits<size_t>::max(); }
		operator size_t() const { return m_id; }

		bool Create(std::ostream& out, const std::string& paramName, const std::string& idStr)
		{
			if (auto val = MechSim::ReadInt(idStr); val && Validate(static_cast<size_t>(*val)))
			{
				m_id = static_cast<size_t>(*val);
				return true;
			}
			out << paramName << ": received invalid id " << idStr << "\n";
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
		explicit operator bool() const { return m_id == 0; }

		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			for (const auto& mech : MechSim::Game::GetInstance().m_shop.GetMechs())
			{
				results.push_back({ mech->GetId().ToString(), "Mech desc here (I don't think they exist yet)" });
			}
			return results;
		}

	protected:
		bool Validate(size_t id) const override
		{
			return id == 0;
		}
	};

	class CircuitId : public Id
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			auto& circuits = MechSim::GetMech().GetAllCircuits();
			for (size_t i = 0; i < circuits.size(); ++i)
			{
				const auto& circuit = circuits[i];
				std::stringstream str;
				str << i;
				results.push_back({ str.str(), "Circuit desc here"});
			}
			return results;
		}

	protected:
		bool Validate(size_t id) const override
		{
			if (id == std::numeric_limits<size_t>::max())
			{
				return false;
			}

			const auto& mech = MechSim::GetMech();
			if (id >= mech.GetAllCircuits().size())
			{
				return false;
			}

			return true;
		}
	};

	class ReactorPlug : public Id
	{
	public:
		static AutoCompleter::Completions GetCompletions()
		{
			AutoCompleter::Completions results;
			const auto& plugs = MechSim::GetMech().GetReactor()->GetPlugs();
			for (size_t i = 0; i < plugs.size(); ++i)
			{
				std::stringstream str;
				str << i << " (" << plugs[i].m_voltage << ")";
				results.push_back({ std::to_string(i), str.str() });
			}
			return results;
		}

	protected:
		bool Validate(size_t id) const override
		{
			return id < MechSim::GetMech().GetReactor()->ConnectionCount;
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
			const auto& objects = MechSim::GetAllObjects<ObjectType>();
			auto filter = FilterFnObj::Get();
			for (const auto* obj : objects)
			{
				if (filter(obj))
				{
					completions.push_back({ obj->GetId().ToString(), obj->GetDescription() });
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
				result.Create(out, paramName, s);
				return result;
			}
		};

		// TODO: Clean this up to be one template, if possible
#define DEFINE_BASIC_FROM_STRING(typeName) \
		template <> \
		struct FromString<typeName> \
		{ \
			static typeName get(std::ostream& out, const std::string& param, const std::string& s) \
			{ \
				typeName result; \
				result.Create(out, param, s); \
				return result; \
			} \
		}

		DEFINE_BASIC_FROM_STRING(MechId);
		DEFINE_BASIC_FROM_STRING(CircuitId);
		DEFINE_BASIC_FROM_STRING(ReactorPlug);
		DEFINE_BASIC_FROM_STRING(PartName);
		DEFINE_BASIC_FROM_STRING(Joystick);
		DEFINE_BASIC_FROM_STRING(Axis);
#if 0
		template <>
		struct FromString<MechId>
		{
			static MechId get(std::ostream& out, const std::string& param, const std::string& s)
			{
				MechId result;
				result.Create(out, param, s);
				return result;
			}
		};

		template <>
		struct FromString<CircuitId>
		{
			static CircuitId get(std::ostream& out, const std::string& param, const std::string& s)
			{
				CircuitId result;
				result.Create(out, param, s);
				return result;
			}
		};

		template <>
		struct FromString<ReactorPlug>
		{
			static ReactorPlug get(std::ostream& out, const std::string& param, const std::string& s)
			{
				ReactorPlug result;
				result.Create(out, param, s);
				return result;
			}
		};

		template <>
		struct FromString<PartName>
		{
			static PartName get(std::ostream& out, const std::string& param, const std::string& s)
			{
				PartName result;
				result.Create(out, param, s);
				return result;
			}
		};
#endif
	}
}