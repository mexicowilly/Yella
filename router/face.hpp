#ifndef YELLA_FACE_HPP__
#define YELLA_FACE_HPP__

#include "configuration.hpp"
#include <chucho/loggable.hpp>
#include <functional>

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

    virtual void run(face* other_face,
                     std::function<void()> callback_of_death)
    {
        other_face_ = other_face;
        callback_of_death_ = callback_of_death;
    }
    virtual void send(const std::uint8_t* const msg, std::size_t len) = 0;

protected:
    face(const configuration& cnf) : config_(cnf) { }

    const configuration& config_;
    face* other_face_;
    std::function<void()> callback_of_death_;
};


}

}

#endif
