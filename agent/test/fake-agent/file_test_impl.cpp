#include "file_test_impl.hpp"
#include "file_generated.h"
#include <chucho/log.hpp>
#include <fstream>

namespace yella
{

namespace test
{

file_test_impl::file_test_impl(const YAML::Node& doc, const std::filesystem::path& plugin)
    : test_impl(doc, plugin),
      working_dir_(std::filesystem::temp_directory_path() / "file-test")
{
    rename_logger(typeid(file_test_impl));
}

void file_test_impl::process_after(const YAML::Node& body)
{
    auto after = body["after"];
    if (after)
    {
        process_file_state(after);

    }
}

void file_test_impl::process_before(const YAML::Node& body)
{
    auto before = body["before"];
    if (before)
    {
        process_exists(before);
        process_monitor_requests(before);
    }
}

void file_test_impl::process_exists(const YAML::Node& before)
{
    auto exists = before["exists"];
    if (exists)
    {
        std::ofstream out(working_dir_ / exists.Scalar());
        out << "file contents";
    }
}

void file_test_impl::process_file_state(const YAML::Node& body)
{
    for (const auto& itor : body)
    {
        if (itor.first.Scalar() == "file.state")
        {
            const auto& cur = itor.second;

        }
    }
}

void file_test_impl::process_monitor_request(const YAML::Node& body)
{
    flatbuffers::FlatBufferBuilder fbld;
    flatbuffers::Offset<fb::plugin::config> fcfg;
    if (body["monitor.request.name"] && body["action"])
    {
        auto name = fbld.CreateString(body["monitor.request.name"].Scalar());
        fb::plugin::configBuilder config_bld(fbld);
        config_bld.add_name(name);
        auto yaction = body["action"];
        for (std::uint8_t i = fb::plugin::config_action_MIN; i <= fb::plugin::config_action_MAX; i++)
        {
            if (strcmp(yaction.Scalar().c_str(), fb::plugin::EnumNameconfig_action(static_cast<fb::plugin::config_action>(i))) == 0)
            {
                config_bld.add_action(static_cast<fb::plugin::config_action>(i));
                break;
            }
        }
        fcfg = config_bld.Finish();
    }
    else
    {
        throw std::runtime_error("The monitor request is missing its config");
    }
    std::vector<std::string> includes;
    for (const auto& cur : body)
    {
        if (cur.first.Scalar() == "include")
            includes.emplace_back(cur.second.Scalar());
    }
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> finc;
    if (!includes.empty())
        finc = fbld.CreateVectorOfStrings(includes);
    std::vector<std::string> excludes;
    for (const auto& cur : body)
    {
        if (cur.first.Scalar() == "exclude")
            excludes.emplace_back(cur.second.Scalar());
    }
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> fexc;
    if (!excludes.empty())
        fbld.CreateVectorOfStrings(excludes);
    auto attrs = body["attrs"];
    flatbuffers::Offset<flatbuffers::Vector<std::uint16_t>> fattrs;
    if (attrs)
    {
        std::vector<std::uint16_t> types;
        for (const auto& cur : attrs)
        {
            for (std::uint16_t i = fb::file::attr_type_MIN; i <= fb::file::attr_type_MAX; i++)
            {
                if (strcmp(cur.first.Scalar().c_str(), fb::file::EnumNameattr_type(static_cast<fb::file::attr_type>(i))) == 0)
                    types.push_back(i);
            }
        }
        if (!types.empty())
            fattrs = fbld.CreateVector<std::uint16_t>(types);
    }
    fb::file::monitor_requestBuilder bld(fbld);
    if (!fcfg.IsNull())
        bld.add_config(fcfg);
    if (!finc.IsNull())
        bld.add_includes(finc);
    if (!fexc.IsNull())
        bld.add_excludes(fexc);
    if (!fattrs.IsNull())
        bld.add_attr_types(fattrs);
    fbld.Finish(bld.Finish());
    send_message(fbld.GetBufferPointer(), fbld.GetSize());
}

void file_test_impl::process_monitor_requests(const YAML::Node& before)
{
    for (const auto& cur : before)
    {
        if (cur.first.Scalar() == "monitor.request")
            process_monitor_request(cur.second);
    }
}

void file_test_impl::receive_plugin_message(const yella_parcel& pcl)
{
    std::lock_guard<std::mutex> locker(received_guard_);
    received_msgs_.emplace_back(pcl);
}

void file_test_impl::run()
{
    for (const auto& cur : doc_)
    {
        CHUCHO_INFO_L_M(lmrk_, "Running: " << cur.first.as<std::string>());
        const auto& body = cur.second;
        process_before(body);
    }
}

void file_test_impl::send_message(const std::uint8_t* const data, std::size_t sz)
{
    const plugin& plg = agent_.current_plugin();
    auto cap = std::find_if(plg.in_caps().begin(), plg.in_caps().end(), [](const plugin::in_cap& cap) { return cap.name == u"yella.file.monitor_request"; });
    if (cap == plg.in_caps().end())
        throw std::runtime_error("This plugin does not support 'yella.file.monitor_request'");
    yella_parcel pcl;
    pcl.cmp = YELLA_COMPRESSION_NONE;
    auto tp(cap->name);
    pcl.type = const_cast<UChar*>(tp.getTerminatedBuffer());
    pcl.payload = const_cast<std::uint8_t*>(data);
    pcl.payload_size = sz;
    cap->handler(&pcl, cap->udata);
    CHUCHO_INFO_L_M(lmrk_, "Sent 'yella.file.monitor_request'");
}

}

}