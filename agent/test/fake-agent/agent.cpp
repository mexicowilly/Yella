#include "agent.hpp"
#include "plugin.hpp"
#include "common/settings.h"
#include <chucho/log.hpp>

namespace yella
{

namespace test
{

agent::agent(const std::filesystem::path& plugin, plugin_message_receiver& rcvr)
    : rcvr_(rcvr)
{
    yella_initialize_settings();
    yella_load_settings_doc();
    auto u16 = std::filesystem::temp_directory_path().u16string();
    yella_settings_set_dir(u"agent", u"data-dir", u16.c_str());
    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1k");
    yella_settings_set_uint(u"file", u"send-latency-seconds", 3);
    load_plugin(plugin);
    yella_agent_api api({plugin_send});
    auto cplg = start_func_(&api, this);
    current_plugin_ = *cplg;
    yella_destroy_plugin(cplg);
    CHUCHO_INFO_L("Plugin '" << plugin.string() << "' loaded");
    yella_log_settings();
    yella_destroy_settings_doc();
}

agent::~agent()
{
    stop_func_(current_plugin_.udata());
    CHUCHO_INFO_L_STR("Plugin stopped");
    unload_plugin();
    CHUCHO_INFO_L_STR("Plugin unloaded");
    yella_destroy_settings();
}

void agent::plugin_send(void* ag, yella_parcel* pcl)
{
    reinterpret_cast<agent*>(ag)->rcvr_.receive_plugin_message(*pcl);
}

void agent::update()
{
    auto cplg = status_func_(current_plugin_.udata());
    current_plugin_ = *cplg;
    yella_destroy_plugin(cplg);
}

}

}