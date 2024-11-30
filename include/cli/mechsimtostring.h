#pragma once

#include <ostream>

#include "colorprofile.h"

#include "MechSim/Misc/ObjectId.h"
#include "MechSim/Central/Mech.h"
#include "MechSim/Game/Agent.h"

namespace MechSim
{
    class Agent;
}

namespace cli
{
    inline std::ostream& operator<<(std::ostream& os, const MechSim::ObjectId& objectId)
    {
        os << cli::Style::ObjectId() << objectId.ToString(false) << "(" << GetObjectName(objectId) << ")"
           << cli::reset;
        return os;
    }

	/*
    inline std::ostream& operator<<(std::ostream& os, const MechSim::Object& object)
    {
        os << cli::Style::Object() << object.GetClass() << cli::reset << " " << object.GetId() << cli::reset;
        return os;
    }
    */

    inline std::ostream& operator<<(std::ostream& os, const MechSim::ObjectHandleAny& handle)
    {
		const auto* object = handle.Get<MechSim::Object>();
        os << *object;
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MechSim::Mech& mech)
    {
        os << cli::Style::Mech() << mech.GetName() << cli::reset << " " << mech.GetId() << cli::reset;
        return os;
    }

    template<typename T, typename IndexType>
    inline std::ostream& operator<<(std::ostream& os, const Util::VectorHandle<T, IndexType>& vectorHandle)
    {
        os << vectorHandle.GetIndex() << cli::reset;
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MechSim::Object* object)
    {
        if (!object)
        {
            os << cli::Style::Object() << "Invalid" << cli::reset;
        }
        else
        {
            os << *object;
        }
        return os;
    }

    inline std::ostream& operator<<(std::ostream& os, const MechSim::Agent& agent)
    {
        os << agent.GetName();
        return os;
    }
}