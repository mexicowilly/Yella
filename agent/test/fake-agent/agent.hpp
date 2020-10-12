#ifndef YELLA_AGENT_HPP__
#define YELLA_AGENT_HPP__

#include "plugin.hpp"
#include <chucho/loggable.hpp>
#include <filesystem>
#include <functional>

namespace yella
{

namespace test
{

class agent : public chucho::loggable<agent>
{
public:
    agent(const std::filesystem::path& plugin, std::function<void(const yella_parcel&)>&& rcvr, const std::filesystem::path& working_dir);
    ~agent();

    const plugin& current_plugin() const;
    void update();

private:
    static void plugin_send(void* ag, yella_parcel* pcl);

    void load_plugin(const std::filesystem::path& plugin);
    void unload_plugin();

    std::filesystem::path working_dir_;
    void* shared_object_;
    yella_plugin_start_func start_func_;
    yella_plugin_status_func status_func_;
    yella_plugin_stop_func stop_func_;
    std::function<void(const yella_parcel&)> rcvr_;
    plugin current_plugin_;
};

inline const plugin& agent::current_plugin() const
{
    return current_plugin_;
}

}

}

#endif
