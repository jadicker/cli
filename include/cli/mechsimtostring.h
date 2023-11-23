#pragma once

#include "colorprofile.h"

#include "MechSim/Central/Mech.h"

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
}
