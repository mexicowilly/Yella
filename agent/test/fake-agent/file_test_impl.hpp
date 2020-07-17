#ifndef YELLA_FILE_TEST_IMPL_HPP__
#define YELLA_FILE_TEST_IMPL_HPP__

#include "test_impl.hpp"
#include "parcel.hpp"
#include <mutex>

namespace yella
{

namespace test
{

class file_test_impl : public test_impl
{
public:
    file_test_impl(const YAML::Node& doc, const std::filesystem::path& plugin);

    virtual void receive_plugin_message(const yella_parcel& pcl) override;
    virtual void run() override;

private:
    void process_after(const YAML::Node& body);
    void process_before(const YAML::Node& body);
    void process_file_state(const YAML::Node& body);
    void process_monitor_request(const YAML::Node& body);
    void process_monitor_requests(const YAML::Node& before);
    void process_exists(const YAML::Node& before);
    void send_message(const std::uint8_t* const data, std::size_t sz);

    std::filesystem::path working_dir_;
    std::vector<parcel> received_msgs_;
    std::mutex received_guard_;
};

}

}

#endif
