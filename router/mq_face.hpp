#ifndef YELLA_MQ_FACE_HPP__
#define YELLA_MQ_FACE_HPP__

#include "configuration.hpp"
#include <chucho/loggable.hpp>

namespace yella
{

namespace router
{

class mq_face : public chucho::loggable<mq_face>
{
public:
    static std::unique_ptr<mq_face> create_agent_face(const configuration& cnf);

    mq_face(const mq_face&) = delete;
    virtual ~mq_face();

    mq_face& operator= (const mq_face&) = delete;

    virtual void send(const std::uint8_t* const msg, std::size_t len) = 0;

protected:
    mq_face(const configuration& cnf);

    const configuration& config_;
};

}

}

#endif
