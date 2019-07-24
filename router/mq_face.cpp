#include "rabbit_mq_face.hpp"

namespace yella
{

namespace router
{

mq_face::mq_face(const configuration& cnf)
    : face(cnf)
{
    rename_logger(typeid(*this));
}

std::unique_ptr<mq_face> mq_face::create_mq_face(const configuration& cnf)
{
    if (cnf.mq_face() == "rabbitmq")
        return std::make_unique<rabbit_mq_face>(cnf);
    throw std::runtime_error("Unknown mq face type: " + cnf.mq_face());
}

}

}