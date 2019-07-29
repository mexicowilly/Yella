#ifndef YELLA_MQ_FACE_HPP__
#define YELLA_MQ_FACE_HPP__

#include "face.hpp"

namespace yella
{

namespace router
{

class mq_face : public face
{
public:
    static std::shared_ptr<mq_face> create_mq_face(const configuration& cnf);

protected:
    mq_face(const configuration& cnf);
};

}

}

#endif
