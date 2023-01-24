#pragma once

#include "colorprofile.h"

#include <limits>

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

	template <typename T>
	class ObjParam final
	{
	public:
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
		explicit operator bool() const { return m_id != std::numeric_limits<size_t>::max(); }
		operator size_t() const { return m_id; }

		bool Create(std::ostream& out, const std::string& paramName, const std::string& idStr)
		{
			// TODO:
			if (auto val = MechSim::ReadInt(idStr); val && Validate(static_cast<size_t>(*val)))
			{
				m_id = static_cast<size_t>(*val);
				return true;
			}
			out << "Id " << idStr << " invalid\n";
			return false;
		}

	protected:
		virtual bool Validate(size_t id) const { return m_id != std::numeric_limits<size_t>::max(); }

		size_t m_id = std::numeric_limits<size_t>::max();
	};

	class MechId : public Id
	{
	public:
		/*
		// TODO:
		MechSim::Mech& operator*() { return *m_object; }
		MechSim::Mech* operator->() { return m_object; }
		*/

		explicit operator bool() const { return m_id == 0; }

		/*
		bool Create(std::ostream& out, const std::string& paramName, const std::string& idStr)
		{
			// TODO:
			if (auto value = MechSim::ReadInt(idStr); value == 0)
			{
				return true;
			}
			out << "Mech id " << idStr << " invalid\n";
			return false;
		}
		*/

		// Only call from safe contexts!
		/*
		// TODO:
		MechSim::Mech& Get() { return *m_object; }

		MechSim::Object* GetObj() { return dynamic_cast<MechSim::Object*>(m_object); }
		const MechSim::Object* GetObj() const { return dynamic_cast<MechSim::Object*>(m_object); }
		*/

	//private:
		//size_t m_id = std::numeric_limits<size_t>::max();

	protected:
		bool Validate(size_t id) const override
		{
			return id == 0;
		}
	};

	class CircuitId : public Id
	{

	protected:
		bool Validate(size_t id) const override
		{
			if (id == std::numeric_limits<size_t>::max())
			{
				return false;
			}

			const auto& mech = MechSim::GetMech();
			if (id >= mech.m_circuits.Size())
			{
				return false;
			}

			return mech.m_circuits.Get(id);
		}
	};
}