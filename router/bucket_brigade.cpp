#include "bucket_brigade.hpp"
#include <chucho/log.hpp>

namespace yella
{

namespace router
{

using namespace std::chrono_literals;

bucket_brigade::bucket_brigade(const configuration& cnf)
    : config_(cnf),
      aface_(agent_face::create_agent_face(cnf)),
      mface_(mq_face::create_mq_face(cnf)),
      should_stop_(false)
{
}

void bucket_brigade::death_callback()
{
    CHUCHO_ERROR_L_STR("The faces have died");
    std::lock_guard<std::mutex> lock(guard_);
    should_stop_ = true;
    death_cond_.notify_one();
}

void bucket_brigade::run()
{
    CHUCHO_INFO_L_STR("The bucket brigade is starting");
    mface_->run(aface_.get(), std::bind(&bucket_brigade::death_callback, this));
    aface_->run(mface_.get(), std::bind(&bucket_brigade::death_callback, this));
    std::unique_lock<std::mutex> lock(guard_);
    while (!should_stop_)
        death_cond_.wait(lock);
    CHUCHO_INFO_L_STR("The bucket brigade is ending");
}

void bucket_brigade::termination_handler()
{
    CHUCHO_INFO_L_STR("Stopping on signal");
    std::lock_guard<std::mutex> lock(guard_);
    should_stop_ = true;
    death_cond_.notify_one();
}

}

}