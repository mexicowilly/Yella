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
      aface_deaths_left_(cnf.max_agent_face_deaths()),
      mface_deaths_left_(cnf.max_mq_face_deaths()),
      should_stop_(false)
{
}

void bucket_brigade::aface_death_callback()
{
    std::lock_guard<std::mutex> lock(guard_);
    if (--aface_deaths_left_ == 0)
    {
        CHUCHO_ERROR_L("The agent face has died for the last time");
        death_cond_.notify_one();
    }
    else
    {
        CHUCHO_ERROR_L("The agent face has " << aface_deaths_left_ << " fatalities left");
        aface_ = agent_face::create_agent_face(config_);
        aface_->run(mface_, std::bind(&bucket_brigade::aface_death_callback, this));
    }
}

void bucket_brigade::mface_death_callback()
{
    std::lock_guard<std::mutex> lock(guard_);
    if (--mface_deaths_left_ == 0)
    {
        CHUCHO_ERROR_L("The MQ face has died for the last time");
        death_cond_.notify_one();
    }
    else
    {
        CHUCHO_ERROR_L("The MQ face has " << mface_deaths_left_ << " fatalities left");
        mface_ = mq_face::create_mq_face(config_);
        mface_->run(aface_, std::bind(&bucket_brigade::mface_death_callback, this));
    }
}

void bucket_brigade::run()
{
    CHUCHO_INFO_L_STR("The bucket brigade is starting");
    mface_->run(aface_, std::bind(&bucket_brigade::mface_death_callback, this));
    aface_->run(mface_, std::bind(&bucket_brigade::aface_death_callback, this));
    std::unique_lock<std::mutex> lock(guard_);
    while (!should_stop_ && aface_deaths_left_ > 0 && mface_deaths_left_ > 0)
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