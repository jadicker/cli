#pragma once

#include "colorprofile.h"

#include <limits>
#include <type_traits>

#include "MechSim/Central/Central.h"
#include "MechSim/Central/Mech.h"
#include "MechSim/Game/Game.h"
#include "MechSim/Misc/ObjectUtils.h"

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

	// TODO: Maybe push towards the version with error reporting
	template <typename T>
	inline const T* GetObjRaw(const std::string& objectIdStr)
	{
		MechSim::ObjectId objectId = MechSim::ObjectId::FromString(objectIdStr);
		return MechSim::GetObjectRegistry().Get(objectId);
	}

	template <typename T>
	class ObjParam final
	{
	public:
		using type = T;

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

		MechSim::Object* GetObj() { return dynamic_cast<MechSim::Object*>(m_object); }
		const MechSim::Object* GetObj() const { return dynamic_cast<MechSim::Object*>(m_object); }

	private:
		T* m_object = nullptr;
	};

	template <typename T>
	inline std::ostream& operator<<(std::ostream& os, const ObjParam<T>& object)
	{
		os << Style::Object() << typeid(T).name() << reset;
		return os;
	}

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

		static Id Create(const std::string& idStr)
		{
			if (auto val = MechSim::ReadInt(idStr); val)
			{
				return Id(static_cast<size_t>(*val));
			}
			return Id();
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

		// TODO: How to not duplicate this?
		static MechId Create(const std::string& idStr)
		{
			if (auto val = MechSim::ReadInt(idStr); val)
			{
				MechId id;
				id.m_id = *val;
				return id;
			}
			return MechId();
		}

		static std::vector<std::string> GetCompletions()
		{
			std::vector<std::string> results;
			for (const auto& mech : MechSim::Game::GetInstance().m_shop.GetMechs())
			{
				results.push_back(mech->GetId().ToString());
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
		// TODO: How to not duplicate this?
		static CircuitId Create(const std::string& idStr)
		{
			if (auto val = MechSim::ReadInt(idStr); val)
			{
				CircuitId id;
				id.m_id = *val;
				return id;
			}
			return CircuitId();
		}

		static std::vector<std::string> GetCompletions()
		{
			std::vector<std::string> results;
			auto& circuits = MechSim::GetMech().m_circuits;
			for (size_t i = 0; i < circuits.size(); ++i)
			{
				const auto& circuit = circuits[i];
				std::stringstream str;
				str << i;
				results.push_back(str.str());
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
			if (id >= mech.m_circuits.size())
			{
				return false;
			}

			return true;
		}
	};


	template <typename InT, typename Enable>
	class ObjectAutoCompleter;

	template <typename InT>
	class ObjectAutoCompleter<InT, void>
	{
	public:
		ObjectAutoCompleter() = default;
		bool HasValues() const { return false; }
		size_t Size() const { return 0; }

		std::vector<std::string> GetAutoCompletions(size_t cur) const
		{
			return {};
		}
	};

	class AutoCompleter
	{
	public:
		AutoCompleter(std::vector<std::string>&& completions)
		{
			/*
			const auto& objects = MechSim::GetAllObjects<T>();
			for (const auto* obj : objects)
			{
				m_completions.push_back(detail::from_string<T>(*m_objects[i]))
			}
			*/
			m_completions = std::move(completions);
			m_current = 0;
		}

		bool HasValues() const { return !m_completions.empty(); }
		size_t Size() const { return m_completions.size(); }

		const std::string* Current()
		{
			if (!HasValues()) { return {}; }
			return &m_completions[m_current];
		}

		// Peek 0 returns the next element.  Wraps around, but returns nullptr when i >= size() -1
		const std::string* Peek(size_t i)
		{
			const size_t size = Size();
			if (i >= size - 1)
			{
				return nullptr;
			}

			return &m_completions[(m_current + 1 + i) % size];
		}

		std::vector<std::string> GetAutoCompletions(size_t cur) const
		{
			size_t count = m_completions.size();
			std::vector<std::string> completions;
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
		std::vector<std::string> m_completions;
	};

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
							 typename std::enable_if_t<std::is_base_of<MechSim::Object, std::decay_t<T>>::value>
							>
	{
		static AutoCompleter Get()
		{
			std::vector<std::string> completions;
			const auto& objects = MechSim::GetAllObjects<T>();
			for (const auto* obj : objects)
			{
				completions.push_back(detail::from_string<T>(*obj));
			}
			return AutoCompleter(std::move(completions));
		}
	};

	template <typename T>
	struct ParamAutoComplete<ObjParam<T>>
	{
		static AutoCompleter Get()
		{
			std::vector<std::string> completions;
			using ObjectType = typename ObjParam<T>::type;
			const auto& objects = MechSim::GetAllObjects<ObjectType>();
			for (const auto* obj : objects)
			{
				completions.push_back(obj->GetId().ToString());
			}
			return AutoCompleter(std::move(completions));
		}
	};

	template <typename T>
	struct ParamAutoComplete<T,
							 typename std::enable_if_t<std::is_base_of<Id, std::decay_t<T>>::value>
							>
	{
		static AutoCompleter Get()
		{
			std::vector<std::string> completions = T::GetCompletions();
			return AutoCompleter(std::move(completions));
		}
	};

	namespace detail
	{
		/*
		// Partial function specialization won't work
		template <typename T>
		inline ObjParam<T> from_string(const std::string& s)
		{
			return GetObjRaw<T>(s);
		}
		*/

		template <>
		inline MechId from_string(const std::string& s)
		{
			return MechId::Create(s);
		}

		template <>
		inline CircuitId from_string(const std::string& s)
		{
			return CircuitId::Create(s);
		}
	}
}
