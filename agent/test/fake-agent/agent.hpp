#ifndef YELLA_AGENT_HPP__
#define YELLA_AGENT_HPP__

#include "plugin_message_receiver.hpp"
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
    agent(const std::filesystem::path& plugin, plugin_message_receiver& rcvr);
    ~agent();

    const plugin& current_plugin() const;
    void update();

private:
    static void plugin_send(void* ag, yella_parcel* pcl);

    void load_plugin(const std::filesystem::path& plugin);
    void unload_plugin();

    void* shared_object_;
    yella_plugin_start_func start_func_;
    yella_plugin_status_func status_func_;
    yella_plugin_stop_func stop_func_;
    plugin_message_receiver& rcvr_;
    plugin current_plugin_;
};

inline const plugin& agent::current_plugin() const
{
    return current_plugin_;
}

}

}

#endif
