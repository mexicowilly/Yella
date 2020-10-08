#include "agent.hpp"
#include "plugin.hpp"
#include "common/settings.h"
#include <chucho/log.hpp>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace yella
{

namespace test
{

agent::agent(const std::filesystem::path& plugin, std::function<void(const yella_parcel&)>&& rcvr, const std::filesystem::path& working_dir)
    : working_dir_(working_dir),
      rcvr_(std::move(rcvr))
{
    std::filesystem::create_directories(working_dir);
    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "file" << YAML::Value;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "send-latency-seconds" << YAML::Value << 3;
    emitter << YAML::EndMap;
    emitter << YAML::EndMap;
    auto cfg_name = working_dir_ / "config.yaml";
    std::ofstream cfg(cfg_name);
    cfg << emitter.c_str();
    cfg.close();
    yella_initialize_settings();
    yella_settings_set_text(u"agent", u"config-file", cfg_name.u16string().c_str());
    yella_settings_set_byte_size(u"agent", u"max-message-size", u"1k");
    yella_settings_set_dir(u"agent", u"data-dir", working_dir_.u16string().c_str());
    yella_load_settings_doc();
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
    std::error_code ec;
    std::filesystem::remove_all(working_dir_);
}

void agent::plugin_send(void* ag, yella_parcel* pcl)
{
    reinterpret_cast<agent*>(ag)->rcvr_(*pcl);
}

void agent::update()
{
    auto cplg = status_func_(current_plugin_.udata());
    current_plugin_ = *cplg;
    yella_destroy_plugin(cplg);
}

}

}