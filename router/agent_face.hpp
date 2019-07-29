#ifndef YELLA_AGENT_FACE_HPP__
#define YELLA_AGENT_FACE_HPP__

#include "face.hpp"

namespace yella
{

namespace router
{

class agent_face : public face
{
public:
    static std::shared_ptr<agent_face> create_agent_face(const configuration& cnf);

protected:
    agent_face(const configuration& cnf);
};

}

}

#endif
