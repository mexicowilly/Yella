#ifndef YELLA_AGENT_FACE_HPP__
#define YELLA_AGENT_FACE_HPP__

#include "configuration.hpp"
#include <chucho/loggable.hpp>

namespace yella
{

namespace router
{

class agent_face : public chucho::loggable<agent_face>
{
public:
    static std::unique_ptr<agent_face> create_agent_face(const configuration& cnf);

    agent_face(const agent_face&) = delete;
    virtual ~agent_face();

    agent_face& operator= (const agent_face&) = delete;

    virtual void send(const std::uint8_t* const msg, std::size_t len) = 0;

protected:
    agent_face(const configuration& cnf);

    const configuration& config_;
};

}

}

#endif
