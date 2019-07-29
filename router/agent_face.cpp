#include "zeromq_agent_face.hpp"

namespace yella
{

namespace router
{

agent_face::agent_face(const configuration& cnf)
    : face(cnf)
{
    rename_logger(typeid(*this));
}

std::shared_ptr<agent_face> agent_face::create_agent_face(const configuration& cnf)
{
    if (cnf.agent_face() == "zeromq")
        return std::make_shared<zeromq_agent_face>(cnf);
    throw std::runtime_error("Unknown agent face type: " + cnf.agent_face());
}

}

}