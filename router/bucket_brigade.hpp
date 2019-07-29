#ifndef YELLA_BUCKET_BRIGADE_HPP__
#define YELLA_BUCKET_BRIGADE_HPP__

#include <condition_variable>
#include "agent_face.hpp"
#include "mq_face.hpp"

namespace yella
{

namespace router
{

class bucket_brigade : public chucho::loggable<bucket_brigade>
{
public:
    bucket_brigade(const configuration& cnf);

    void run();
    void termination_handler();

private:
    void aface_death_callback();
    void mface_death_callback();

    const configuration& config_;
    std::shared_ptr<agent_face> aface_;
    std::shared_ptr<mq_face> mface_;
    std::size_t aface_deaths_left_;
    std::size_t mface_deaths_left_;
    std::mutex guard_;
    std::condition_variable death_cond_;
    bool should_stop_;
};

}

}

#endif
