#include "file_test_impl.hpp"
#include "expected_exception.hpp"
#include "common/file.h"
#include "common/compression.h"
#include <chucho/log.hpp>
#include <openssl/evp.h>
#include <flatbuffers/flatbuffers.h>
#include <fstream>
#include <thread>
#include <iomanip>

namespace
{

static void digest_callback(const uint8_t* const buf, size_t sz, void* udata)
{
    EVP_DigestUpdate(reinterpret_cast<EVP_MD_CTX*>(udata), buf, sz);
}

}

namespace yella
{

namespace test
{

using namespace std::chrono_literals;

file_test_impl::file_test_impl(const YAML::Node& doc, const std::filesystem::path& plugin)
    : test_impl(doc, plugin),
      received_msg_count_(0)
{
    rename_logger(typeid(file_test_impl));
}

file_test_impl::~file_test_impl()
{
    destroy_agent();
}

void file_test_impl::clear_plugin_work()
{
    agent_->update();
    const plugin& plg = agent_->current_plugin();
    for (const auto& in_cap : plg.in_caps())
    {
        for (const auto& cfg : in_cap.configs)
        {
            flatbuffers::FlatBufferBuilder fbld;
            std::string utf8;
            cfg.toUTF8String(utf8);
            auto name = fbld.CreateString(utf8);
            fb::plugin::configBuilder cbld(fbld);
            cbld.add_action(fb::plugin::config_action_REPLACE_ALL);
            cbld.add_name(name);
            auto config = cbld.Finish();
            fb::file::monitor_requestBuilder bld(fbld);
            bld.add_config(config);
            fbld.Finish(bld.Finish());
            send_message(fbld.GetBufferPointer(), fbld.GetSize());
        }
    }
}

void file_test_impl::process_after(const YAML::Node& body)
{
    auto after = body["after"];
    if (after)
    {
        for (const auto& itor : after)
        {
            const auto& p = itor.begin();
            if (p->first.Scalar() == "file.state")
                expected_states_.emplace(working_dir_, p->second);
        }
        CHUCHO_INFO_L_M(lmrk_, "Expected state count: " << expected_states_.size());
        if (!wait_for_states(expected_states_.size(), 5s * expected_states_.size()))
        {
            std::lock_guard<std::mutex> lock(received_guard_);
            throw expected("number of received states", expected_states_.size(), received_states_.size());
        }
        auto rcv = received_states_.begin();
        for (const auto& exp : expected_states_)
        {
            if (exp != *rcv)
                throw expected("file state (ignore timestamp)", exp.to_text(), rcv->to_text());
            ++rcv;
        }
    }
}

void file_test_impl::process_before(const YAML::Node& body)
{
    auto before = body["before"];
    if (before)
    {
        for (const auto& itor : before)
        {
            // This is a completely weird API for getting a map's key/value pair
            const auto& p = itor.begin();
            if (p->first.Scalar() == "exists")
                process_exists(p->second);
            else if (p->first.Scalar() == "monitor.request")
                process_monitor_request(p->second);
            else if (p->first.Scalar() == "pause")
                process_pause(p->second);
            else if (p->first.Scalar() == "grow")
                process_grow(p->second);
        }
    }
    YAML::Emitter emitter;
    emitter << YAML::Flow;
    emitter << YAML::BeginSeq;
    for (auto lgr : chucho::logger::get_existing_loggers())
        emitter << lgr->get_name();
    emitter << YAML::EndSeq;
    CHUCHO_INFO_L_M(lmrk_, "Loggers: " << emitter.c_str());
}

void file_test_impl::process_exists(const YAML::Node& body)
{
    std::ofstream out(working_dir_ / body.Scalar());
    out << "file contents";
}

void file_test_impl::process_grow(const YAML::Node& body)
{
    std::ofstream out(working_dir_ / body.Scalar(), std::ios::ate | std::ios::out);
    out << "Here's some text!";
}

void file_test_impl::process_monitor_request(const YAML::Node& body)
{
    flatbuffers::FlatBufferBuilder fbld;
    flatbuffers::Offset<fb::plugin::config> fcfg;
    if (body["config.name"] && body["action"])
    {
        auto name = fbld.CreateString(body["config.name"].Scalar());
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
        {
            auto full = working_dir_ / cur.second.Scalar();
            includes.emplace_back(full.string());
        }
    }
    flatbuffers::Offset<flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>> finc;
    if (!includes.empty())
        finc = fbld.CreateVectorOfStrings(includes);
    std::vector<std::string> excludes;
    for (const auto& cur : body)
    {
        if (cur.first.Scalar() == "exclude")
        {
            auto full = working_dir_ / cur.second.Scalar();
            excludes.emplace_back(full.string());
        }
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
                if (strcmp(cur.Scalar().c_str(), fb::file::EnumNameattr_type(static_cast<fb::file::attr_type>(i))) == 0)
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

void file_test_impl::process_pause(const YAML::Node& body)
{
    try
    {
        auto num = std::stoul(body.Scalar());
        std::this_thread::sleep_for(std::chrono::seconds(num));
    }
    catch (...)
    {
        CHUCHO_WARN_L_M(lmrk_, "Could not convert '" + body.Scalar() + "' to a number");
    }
}

void file_test_impl::receive_plugin_message_impl(const yella_parcel& pcl)
{
    parcel p(pcl);
    icu::UnicodeString type(u"yella.fb.file.file_states");
    if (p.type() != type)
        throw expected("parcel type", type, p.type());
    if (p.compression() != YELLA_COMPRESSION_LZ4)
        throw expected("compression type", YELLA_COMPRESSION_LZ4, p.compression());
    if (p.group_id().get() != nullptr)
        throw expected("group id", nullptr, p.group_id().get());
    if (p.group_disposition().get() != nullptr)
        throw expected("group disposition", nullptr, p.group_disposition().get());
    std::string rec;
    p.recipient().toUTF8String(rec);
    std::string sen;
    p.sender().toUTF8String(sen);
    std::string type8;
    p.type().toUTF8String(type8);
    CHUCHO_INFO_L_M(lmrk_, "Received message of type '" << type8 << "' from '" << sen << "' to '" << rec << "' (" << p.major_sequence() << ", " << p.minor_sequence() << ")");
    std::size_t sz = p.payload().size();
    auto bytes = yella_lz4_decompress(&p.payload()[0], &sz);
    std::lock_guard<std::mutex> locker(received_guard_);
    received_msg_count_++;
    for (auto st : *flatbuffers::GetRoot<yella::fb::file::file_states>(bytes)->states())
    {
        auto it = received_states_.emplace(*st);
        CHUCHO_INFO_L_M(lmrk_, "Received file state: " << it->to_text());
    }
    free(bytes);
    received_cond_.notify_one();
}

bool file_test_impl::run()
{
    bool result = true;
    try
    {
        for (const auto& cur : doc_)
        {
            CHUCHO_INFO_L_M(lmrk_, "Running: " << cur.first.as<std::string>());
            const auto& body = cur.second;
            process_before(body);
            process_after(body);
            CHUCHO_INFO_L_STR_M(lmrk_, "Clearing plugin configs");
            clear_plugin_work();
            CHUCHO_INFO_L_STR_M(lmrk_, "Removing test data");
            std::filesystem::remove_all(working_dir_);
            std::filesystem::create_directory(working_dir_);
        }
    }
    catch (const expected& exp)
    {
        CHUCHO_ERROR_L_STR_M(lmrk_, exp.what());
        result = false;
    }
    catch (const std::exception& e)
    {
        CHUCHO_ERROR_L_M(lmrk_, "Unexpected error: " << e.what());
        result = false;
    }
    return result;
}

void file_test_impl::send_message(const std::uint8_t* const data, std::size_t sz)
{
    const plugin& plg = agent_->current_plugin();
    auto cap = std::find_if(plg.in_caps().begin(), plg.in_caps().end(), [](const plugin::in_cap& cap) { return cap.name == u"file.monitor_request"; });
    if (cap == plg.in_caps().end())
        throw std::runtime_error("This plugin does not support 'file.monitor_request'");
    yella_parcel pcl;
    pcl.cmp = YELLA_COMPRESSION_NONE;
    auto tp(cap->name);
    pcl.type = const_cast<UChar*>(tp.getTerminatedBuffer());
    pcl.payload = const_cast<std::uint8_t*>(data);
    pcl.payload_size = sz;
    cap->handler(&pcl, cap->udata);
    CHUCHO_INFO_L_M(lmrk_, "Sent 'file.monitor_request'");
}

void file_test_impl::attribute::emit(YAML::Emitter& e) const
{
    e << YAML::Key;
    switch (type_)
    {
    case type::ACCESS_TIME:
        e << "ACCESS_TIME";
        break;
    case type::FILE_TYPE:
        e << "FILE_TYPE";
        break;
    case type::GROUP:
        e << "GROUP";
        break;
    case type::METADATA_CHANGE_TIME:
        e << "METADATA_CHANGE_TIME";
        break;
    case type::MODIFICATION_TIME:
        e << "MODIFICATION_TIME";
        break;
    case type::POSIX_PERMISSIONS:
        e << "POSIX_PERMISSIONS";
        break;
    case type::SHA256:
        e << "SHA256";
        break;
    case type::SIZE:
        e << "SIZE";
        break;
    case type::USER:
        e << "USER";
        break;
    case type::POSIX_ACL:
        e << "POSIX_ACL";
        break;
    }
    e << YAML::Value;
}

bool file_test_impl::attribute::equal_to(const attribute& rhs) const
{
    return type_ == rhs.type_;
}

file_test_impl::file_type_attribute::file_type_attribute(file_type ftype)
    : attribute(type::FILE_TYPE),
      ftype_(ftype)
{
}

file_test_impl::file_type_attribute::file_type_attribute(const fb::file::attr& fba)
    : attribute(type::FILE_TYPE)
{
    assert(fba.type() == fb::file::attr_type_FILE_TYPE);
    switch (fba.ftype())
    {
    case fb::file::file_type_BLOCK_SPECIAL:
        ftype_ = file_type::BLOCK_SPECIAL;
        break;
    case fb::file::file_type_CHARACTER_SPECIAL:
        ftype_ = file_type::CHARACTER_SPECIAL;
        break;
    case fb::file::file_type_DIRECTORY:
        ftype_ = file_type::DIRECTORY;
        break;
    case fb::file::file_type_FIFO:
        ftype_ = file_type::FIFO;
        break;
    case fb::file::file_type_REGULAR:
        ftype_ = file_type::REGULAR;
        break;
    case fb::file::file_type_SOCKET:
        ftype_ = file_type::SOCKET;
        break;
    case fb::file::file_type_SYMBOLIC_LINK:
        ftype_ = file_type::SYMBOLIC_LINK;
        break;
    case fb::file::file_type_WHITEOUT:
        ftype_ = file_type::SYMBOLIC_LINK;
        break;
    }
}

void file_test_impl::file_type_attribute::emit(YAML::Emitter& e) const
{
    attribute::emit(e);
    switch (ftype_)
    {
    case file_type::BLOCK_SPECIAL:
        e << "BLOCK_SPECIAL";
        break;
    case file_type::CHARACTER_SPECIAL:
        e << "CHARACTER SPECIAL";
        break;
    case file_type::DIRECTORY:
        e << "DIRECTORY";
        break;
    case file_type::FIFO:
        e << "FIFO";
        break;
    case file_type::REGULAR:
        e << "REGULAR";
        break;
    case file_type::SOCKET:
        e << "SOCKET";
        break;
    case file_type::SYMBOLIC_LINK:
        e << "SYMBOLIC_LINK";
        break;
    case file_type::WHITEOUT:
        e << "WHITEOUT";
        break;
    }
}

bool file_test_impl::file_type_attribute::equal_to(const attribute& rhs) const
{
    auto fta = dynamic_cast<const file_type_attribute*>(&rhs);
    return fta != nullptr && attribute::equal_to(rhs) && ftype_ == fta->ftype_;
}

file_test_impl::bytes_attribute::bytes_attribute(type tp, const std::vector<std::uint8_t>& bytes)
    : attribute(tp),
      bytes_(bytes)
{
}

file_test_impl::bytes_attribute::bytes_attribute(type tp, const fb::file::attr& fba)
    : attribute(tp),
      bytes_(fba.bytes()->begin(), fba.bytes()->end())
{
    assert(tp == type::SHA256);
}

void file_test_impl::bytes_attribute::emit(YAML::Emitter& e) const
{
    attribute::emit(e);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (auto byte : bytes_)
        stream << std::setw(2) << static_cast<int>(byte);
    e << stream.str();
}

bool file_test_impl::bytes_attribute::equal_to(const attribute& rhs) const
{
    auto ba = dynamic_cast<const bytes_attribute*>(&rhs);
    return ba != nullptr && attribute::equal_to(rhs) && bytes_ == ba->bytes_;
}

file_test_impl::posix_permissions_attribute::posix_permissions_attribute(const fb::file::attr& fba)
    : attribute(type::POSIX_PERMISSIONS)
{
    const auto& fbits = fba.psx_permissions();
    auto perm = fbits->owner();
    if (perm != nullptr)
    {
        if (perm->read())
            bits_.set(static_cast<std::size_t>(perm::OWNER_READ));
        if (perm->write())
            bits_.set(static_cast<std::size_t>(perm::OWNER_WRITE));
        if (perm->execute())
            bits_.set(static_cast<std::size_t>(perm::OWNER_EXECUTE));
    }
    perm = fbits->group();
    if (perm != nullptr)
    {
        if (perm->read())
            bits_.set(static_cast<std::size_t>(perm::GROUP_READ));
        if (perm->write())
            bits_.set(static_cast<std::size_t>(perm::GROUP_WRITE));
        if (perm->execute())
            bits_.set(static_cast<std::size_t>(perm::GROUP_EXECUTE));
    }
    perm = fbits->other();
    if (perm != nullptr)
    {
        if (perm->read())
            bits_.set(static_cast<std::size_t>(perm::OTHER_READ));
        if (perm->write())
            bits_.set(static_cast<std::size_t>(perm::OTHER_WRITE));
        if (perm->execute())
            bits_.set(static_cast<std::size_t>(perm::OTHER_EXECUTE));
    }
    if (fbits->set_uid())
        bits_.set(static_cast<std::size_t>(perm::SET_UID));
    if (fbits->set_gid())
        bits_.set(static_cast<std::size_t>(perm::SET_GID));
    if (fbits->sticky())
        bits_.set(static_cast<std::size_t>(perm::STICKY));
}

void file_test_impl::posix_permissions_attribute::emit(YAML::Emitter& e) const
{
    attribute::emit(e);
    std::ostringstream stream;
    stream << (has(perm::OWNER_READ) ? 'r' : '-');
    stream << (has(perm::OWNER_WRITE) ? 'w' : '-');
    if (has(perm::SET_UID))
        stream << (has(perm::OWNER_EXECUTE) ? 's' : 'S');
    else
        stream << (has(perm::OWNER_EXECUTE) ? 'x' : '-');
    stream << (has(perm::GROUP_READ) ? 'r' : '-');
    stream << (has(perm::GROUP_WRITE) ? 'w' : '-');
    if (has(perm::SET_GID))
        stream << (has(perm::GROUP_EXECUTE) ? 's' : 'S');
    else
        stream << (has(perm::GROUP_EXECUTE) ? 'x' : '-');
    stream << (has(perm::OTHER_READ) ? 'r' : '-');
    stream << (has(perm::OTHER_WRITE) ? 'w' : '-');
    if (has(perm::STICKY))
        stream << (has(perm::OTHER_EXECUTE) ? 't' : 'T');
    else
        stream << (has(perm::OTHER_EXECUTE) ? 'x' : '-');
    e << stream.str();
}

bool file_test_impl::posix_permissions_attribute::equal_to(const attribute& rhs) const
{
    auto ppa = dynamic_cast<const posix_permissions_attribute*>(&rhs);
    return ppa != nullptr && attribute::equal_to(rhs) && bits_ == ppa->bits_;
}

file_test_impl::user_group_attribute::user_group_attribute(type tp, const fb::file::attr& fba)
    : attribute(tp)
{
    const auto& ug = fba.usr_grp();
    id_ = ug->id();
    name_ = ug->name()->str();
}

void file_test_impl::user_group_attribute::emit(YAML::Emitter& e) const
{
    attribute::emit(e);
    e << YAML::BeginMap;
    e << YAML::Key << "id" << YAML::Value << id_;
    e << YAML::Key << "name" << YAML::Value << name_;
    e << YAML::EndMap;
}

bool file_test_impl::user_group_attribute::equal_to(const attribute& rhs) const
{
    auto uga = dynamic_cast<const user_group_attribute*>(&rhs);
    return uga != nullptr &&
           attribute::equal_to(rhs) &&
           id_ == uga->id_ &&
           name_ == uga->name_;
}

file_test_impl::unsigned_int_attribute::unsigned_int_attribute(type tp, const fb::file::attr& fba)
    : attribute(tp),
      value_(fba.unsigned_int())
{
}

file_test_impl::unsigned_int_attribute::unsigned_int_attribute(type tp, std::uint64_t val)
    : attribute(tp),
      value_(val)
{
}

void file_test_impl::unsigned_int_attribute::emit(YAML::Emitter& e) const
{
    attribute::emit(e);
    e << value_;
}

bool file_test_impl::unsigned_int_attribute::equal_to(const attribute& rhs) const
{
    auto uia = dynamic_cast<const unsigned_int_attribute*>(&rhs);
    return uia != nullptr && attribute::equal_to(rhs) && value_ == uia->value_;
}

file_test_impl::milliseconds_since_epoch_attribute::milliseconds_since_epoch_attribute(type tp, const fb::file::attr& fba)
    : attribute(tp),
      time_(fba.milliseconds_since_epoch())
{
    initialize_time_format();
}

void file_test_impl::milliseconds_since_epoch_attribute::emit(YAML::Emitter& e) const
{
    attribute::emit(e);
    icu::UnicodeString utd;
    tfmt_->format(time_, utd);
    utd.findAndReplace(u" ", u"T");
    std::string td;
    utd.toUTF8String(td);
    e << td;
}

bool file_test_impl::milliseconds_since_epoch_attribute::equal_to(const attribute& rhs) const
{
    auto msea = dynamic_cast<const milliseconds_since_epoch_attribute*>(&rhs);
    return msea != nullptr && attribute::equal_to(rhs) && time_ == msea->time_;
}

void file_test_impl::milliseconds_since_epoch_attribute::initialize_time_format()
{
    UErrorCode ec = U_ZERO_ERROR;
    tfmt_.reset(icu::DateFormat::createInstanceForSkeleton(u"yyyyMMddHHmmssSSS", icu::Locale::getRoot(), ec));
    tfmt_->setTimeZone(*icu::TimeZone::getGMT());
    if (!U_SUCCESS(ec))
        throw std::runtime_error("Error creating time formatter");
}

file_test_impl::posix_acl_attribute::entry::entry(const fb::file::posix_access_control_entry& fbe)
    : id_(std::numeric_limits<std::uint64_t>::max())
{
    switch (fbe.type())
    {
    case fb::file::posix_access_control_entry_type_USER:
        type_ = type::USER;
        break;
    case fb::file::posix_access_control_entry_type_GROUP:
        type_ = type::GROUP;
        break;
    case fb::file::posix_access_control_entry_type_MASK:
        type_ = type::MASK;
        break;
    case fb::file::posix_access_control_entry_type_USER_OBJ:
        type_ = type::USER_OBJ;
        break;
    case fb::file::posix_access_control_entry_type_GROUP_OBJ:
        type_ = type::GROUP_OBJ;
        break;
    case fb::file::posix_access_control_entry_type_OTHER:
        type_ = type::OTHER;
        break;
    }
    auto perms = fbe.permission();
    if (perms != nullptr)
    {
        permissions_.set(static_cast<std::size_t>(permission::READ), perms->read());
        permissions_.set(static_cast<std::size_t>(permission::WRITE), perms->write());
        permissions_.set(static_cast<std::size_t>(permission::EXECUTE), perms->execute());
    }
    auto usr_grp = fbe.usr_grp();
    if (usr_grp != nullptr)
    {
        id_ = usr_grp->id();
        if (usr_grp->name() != nullptr)
            name_ = usr_grp->name()->str();
    }
}

bool file_test_impl::posix_acl_attribute::entry::operator== (const entry& rhs) const
{
    return type_ == rhs.type_ &&
           permissions_ == rhs.permissions_ &&
           id_ == rhs.id_ &&
           name_ == rhs.name_;
}

bool file_test_impl::posix_acl_attribute::entry::operator< (const entry& rhs) const
{
    std::int64_t diff = static_cast<std::int64_t>(type_) - static_cast<std::int64_t>(rhs.type_);
    if (diff == 0)
    {
        diff = static_cast<int64_t>(id_) - static_cast<int64_t>(rhs.id_);
        if (diff == 0)
            diff = name_.compare(rhs.name_);
    }
    return diff < 0;
}

void file_test_impl::posix_acl_attribute::entry::emit(YAML::Emitter& e) const
{
    e << YAML::BeginMap;
    e << YAML::Key << "type" << YAML::Value;
    std::string id_value;
    switch (type_)
    {
    case type::USER:
        e << "USER";
        id_value = name_ + '(' + std::to_string(id_) + ')';
        e << YAML::Key << "id" << YAML::Value << id_value;
        break;
    case type::GROUP:
        e << "GROUP";
        id_value = name_ + '(' + std::to_string(id_) + ')';
        e << YAML::Key << "id" << YAML::Value << id_value;
        break;
    case type::MASK:
        e << "MASK";
        break;
    case type::USER_OBJ:
        e << "USER_OBJ";
        break;
    case type::GROUP_OBJ:
        e << "GROUP_OBJ";
        break;
    case type::OTHER:
        e << "OTHER";
        break;
    }
    e << YAML::Key << "permissions" << YAML::Value;
    std::string perm;
    perm += (permissions_.test(static_cast<std::size_t>(permission::READ)) ? 'r' : '-');
    perm += (permissions_.test(static_cast<std::size_t>(permission::WRITE)) ? 'w' : '-');
    perm += (permissions_.test(static_cast<std::size_t>(permission::EXECUTE)) ? 'x' : '-');
    e << perm;
    e << YAML::EndMap;
}

file_test_impl::posix_acl_attribute::posix_acl_attribute(const fb::file::attr& fba)
    : attribute(type::POSIX_ACL)
{
    if (fba.psx_acl() != nullptr)
    {
        for (auto cur : *fba.psx_acl())
            entries_.emplace(*cur);
    }
}

void file_test_impl::posix_acl_attribute::emit(YAML::Emitter& e) const
{
    attribute::emit(e);
    e << YAML::BeginSeq;
    for (const auto& ent : entries_)
        ent.emit(e);
    e << YAML::EndSeq;
}

bool file_test_impl::posix_acl_attribute::equal_to(const attribute& rhs) const
{
    auto pacla = dynamic_cast<const posix_acl_attribute*>(&rhs);
    return pacla != nullptr && attribute::equal_to(rhs) && entries_ == pacla->entries_;
}

file_test_impl::file_state::file_state(const fb::file::file_state& fb)
    : time_(fb.milliseconds_since_epoch()),
      file_name_(fb.file_name()->str()),
      config_name_(fb.config_name()->str())
{
    UErrorCode ec = U_ZERO_ERROR;
    tfmt_.reset(icu::DateFormat::createInstanceForSkeleton(u"yyyyMMddHHmmssSSS", icu::Locale::getRoot(), ec));
    tfmt_->setTimeZone(*icu::TimeZone::getGMT());
    if (!U_SUCCESS(ec))
        throw std::runtime_error("Error creating time formatter");
    if (fb.cond() == fb::file::condition_ADDED)
        cond_ = condition::ADDED;
    else if (fb.cond() == fb::file::condition_CHANGED)
        cond_ = condition::CHANGED;
    else if (fb.cond() == fb::file::condition_REMOVED)
        cond_ = condition::REMOVED;
    for (auto cur : *fb.attrs())
    {
        switch (cur->type())
        {
        case fb::file::attr_type_FILE_TYPE:
            attrs_.emplace(std::make_unique<file_type_attribute>(*cur));
            break;
        case fb::file::attr_type_SHA256:
            attrs_.emplace(std::make_unique<bytes_attribute>(attribute::type::SHA256, *cur));
            break;
        case fb::file::attr_type_POSIX_PERMISSIONS:
            attrs_.emplace(std::make_unique<posix_permissions_attribute>(*cur));
            break;
        case fb::file::attr_type_USER:
            attrs_.emplace(std::make_unique<user_group_attribute>(attribute::type::USER, *cur));
            break;
        case fb::file::attr_type_GROUP:
            attrs_.emplace(std::make_unique<user_group_attribute>(attribute::type::GROUP, *cur));
            break;
        case fb::file::attr_type_SIZE:
            attrs_.emplace(std::make_unique<unsigned_int_attribute>(attribute::type::SIZE, *cur));
            break;
        case fb::file::attr_type_ACCESS_TIME:
            attrs_.emplace(std::make_unique<milliseconds_since_epoch_attribute>(attribute::type::ACCESS_TIME, *cur));
            break;
        case fb::file::attr_type_METADATA_CHANGE_TIME:
            attrs_.emplace(std::make_unique<milliseconds_since_epoch_attribute>(attribute::type::METADATA_CHANGE_TIME, *cur));
            break;
        case fb::file::attr_type_MODIFICATION_TIME:
            attrs_.emplace(std::make_unique<milliseconds_since_epoch_attribute>(attribute::type::MODIFICATION_TIME, *cur));
            break;
        case fb::file::attr_type_POSIX_ACL:
            attrs_.emplace(std::make_unique<posix_acl_attribute>(*cur));
            break;
        }
    }
}

file_test_impl::file_state::file_state(const std::filesystem::path& working_dir, const YAML::Node& body)
    : time_(0)
{
    UErrorCode ec = U_ZERO_ERROR;
    tfmt_.reset(icu::DateFormat::createInstanceForSkeleton(u"yyyyMMddHHmmssSSS", icu::Locale::getRoot(), ec));
    tfmt_->setTimeZone(*icu::TimeZone::getGMT());
    if (!U_SUCCESS(ec))
        throw std::runtime_error("Error creating time formatter");
    auto n = body["config.name"];
    if (n)
        config_name_ = n.Scalar();
    n = body["file.name"];
    if (n)
        file_name_ = working_dir / n.Scalar();
    n = body["condition"];
    if (n)
    {
        if (n.Scalar() == "ADDED")
            cond_ = condition::ADDED;
        else if (n.Scalar() == "CHANGED")
            cond_ = condition::CHANGED;
        else if (n.Scalar() == "REMOVED")
            cond_ = condition::REMOVED;
    }
    if (std::filesystem::exists(file_name_))
    {
        n = body["attrs"];
        if (n.IsSequence())
        {
            bool should_get_sha256 = false;
            for (const auto& cur : n)
            {
                if (cur.Scalar() == "POSIX_PERMISSIONS")
                    attrs_.emplace(std::make_unique<posix_permissions_attribute>(file_name_));
                else if (cur.Scalar() == "FILE_TYPE")
                    attrs_.emplace(std::make_unique<file_type_attribute>(file_name_));
                else if (cur.Scalar() == "USER")
                    attrs_.emplace(std::make_unique<user_group_attribute>(attribute::type::USER, file_name_));
                else if (cur.Scalar() == "GROUP")
                    attrs_.emplace(std::make_unique<user_group_attribute>(attribute::type::GROUP, file_name_));
                else if (cur.Scalar() == "SIZE")
                    attrs_.emplace(std::make_unique<unsigned_int_attribute>(attribute::type::SIZE, std::filesystem::file_size(file_name_)));
                else if (cur.Scalar() == "MODIFICATION_TIME")
                    attrs_.emplace(std::make_unique<milliseconds_since_epoch_attribute>(attribute::type::MODIFICATION_TIME, file_name_));
                else if (cur.Scalar() == "METADATA_CHANGE_TIME")
                    attrs_.emplace(std::make_unique<milliseconds_since_epoch_attribute>(attribute::type::METADATA_CHANGE_TIME, file_name_));
                else if (cur.Scalar() == "ACCESS_TIME")
                    attrs_.emplace(std::make_unique<milliseconds_since_epoch_attribute>(attribute::type::ACCESS_TIME, file_name_));
                else if (cur.Scalar() == "SHA256")
                    should_get_sha256 = true;
                else if (cur.Scalar() == "POSIX_ACL")
                    attrs_.emplace(std::make_unique<posix_acl_attribute>(file_name_));
            }
            if (should_get_sha256)
                maybe_add_sha256_attr();
        }
    }
}

bool file_test_impl::file_state::operator== (const file_state& other) const
{
    return file_name_ == other.file_name_ &&
           config_name_ == other.config_name_ &&
           cond_ == other.cond_ &&
           attrs_.size() == other.attrs_.size() &&
           std::equal(attrs_.begin(),
                      attrs_.end(),
                      other.attrs_.begin(),
                      [] (const std::unique_ptr<attribute>& lhs, const std::unique_ptr<attribute>& rhs) { return *lhs == *rhs; });
}

std::string file_test_impl::file_state::to_text() const
{
    icu::UnicodeString utd;
    tfmt_->format(time_, utd);
    utd.findAndReplace(u" ", u"T");
    std::string td;
    utd.toUTF8String(td);
    YAML::Emitter emitter;
    emitter << YAML::Flow;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "time" << YAML::Value << td;
    emitter << YAML::Key << "config" << YAML::Value << config_name_;
    emitter << YAML::Key << "file" << YAML::Value << file_name_;
    emitter << YAML::Key << "condition" << YAML::Value;
    switch (cond_)
    {
    case condition::ADDED:
        emitter << "ADDED";
        break;
    case condition::CHANGED:
        emitter << "CHANGED";
        break;
    case condition::REMOVED:
        emitter << "REMOVED";
        break;
    }
    emitter << YAML::Key << "attributes" << YAML::Value;
    emitter << YAML::BeginMap;
    for (const auto& attr : attrs_)
        attr->emit(emitter);
    emitter << YAML::EndMap;
    emitter << YAML::EndMap;
    return emitter.c_str();
}

void file_test_impl::file_state::maybe_add_sha256_attr()
{
    if (std::filesystem::is_regular_file(file_name_) || std::filesystem::is_symlink(file_name_))
    {
        auto ctx = EVP_MD_CTX_new();
        EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
        // This method is used because otherwise it's a pain in the ass to get the file
        // contents without following symlinks
        yella_apply_function_to_file_contents(file_name_.u16string().c_str(), digest_callback, ctx);
        unsigned char md[EVP_MAX_MD_SIZE];
        unsigned md_len;
        EVP_DigestFinal_ex(ctx, md, &md_len);
        EVP_MD_CTX_free(ctx);
        std::vector<std::uint8_t> md_bytes(md, md + md_len);
        attrs_.emplace(std::make_unique<bytes_attribute>(attribute::type::SHA256, md_bytes));
    }
}

}

}