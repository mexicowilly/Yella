#ifndef YELLA_FACE_HPP__
#define YELLA_FACE_HPP__

#include "configuration.hpp"
#include <chucho/loggable.hpp>

namespace yella
{

namespace router
{

class face : public chucho::loggable<face>
{
public:
    face(const face&) = delete;
    virtual ~face() { }

    face& operator= (const face&) = delete;

    virtual void run(std::shared_ptr<face> other_face) { other_face_ = other_face; }
    virtual void send(const std::uint8_t* const msg, std::size_t len) = 0;

protected:
    face(const configuration& cnf) : config_(cnf) { }

    const configuration& config_;
    std::shared_ptr<face> other_face_;
};


}

}

#endif
