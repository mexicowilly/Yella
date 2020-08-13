#ifndef YELLA_FILE_TEST_IMPL_HPP__
#define YELLA_FILE_TEST_IMPL_HPP__

#include "test_impl.hpp"
#include "parcel.hpp"
#include "file_generated.h"
#include <mutex>
#include <bitset>

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
    class attribute
    {
    public:
        enum class type
        {
            FILE_TYPE,
            SHA256,
            POSIX_PERMISSIONS
        };

        virtual ~attribute() = default;

        type get_type() const { return type_; }

    protected:
        attribute(type tp) : type_(tp) { }

        type type_;
    };

    class file_type_attribute : public attribute
    {
    public:
        enum class file_type
        {
            BLOCK_SPECIAL,
            CHARACTER_SPECIAL,
            DIRECTORY,
            FIFO,
            SYMBOLIC_LINK,
            REGULAR,
            SOCKET,
            WHITEOUT
        };

        file_type_attribute(file_type ftype);
        file_type_attribute(const fb::file::attr& fba);
        file_type_attribute(const std::filesystem::path& file_name);

        file_type get_file_type() const { return ftype_; }

    private:
        file_type ftype_;
    };

    class bytes_attribute : public attribute
    {
    public:
        bytes_attribute(type tp, const std::vector<std::uint8_t>& bytes);
        bytes_attribute(type tp, const fb::file::attr& fba);

        const std::vector<std::uint8_t>& get_bytes() const { return bytes_; }

    private:
        std::vector<std::uint8_t> bytes_;
    };

    class posix_permissions_attribute : public attribute
    {
    public:
        enum class perm
        {
            OWNER_READ = 0,
            OWNER_WRITE,
            OWNER_EXECUTE,
            GROUP_READ,
            GROUP_WRITE,
            GROUP_EXECUTE,
            OTHER_READ,
            OTHER_WRITE,
            OTHER_EXECUTE,
            SET_UID,
            SET_GID,
            STICKY
        };

        posix_permissions_attribute(const std::filesystem::path& file_name);
        posix_permissions_attribute(const fb::file::attr& fba);

        bool has(perm p) { return bits_.test(static_cast<std::size_t>(p)); }

    private:
        std::bitset<12> bits_;
    };

    class file_state
    {
    public:
        enum class condition
        {
            ADDED,
            CHANGED,
            REMOVED
        };

        file_state(const std::filesystem::path& fname, const std::vector<attribute::type>& attrs);
        file_state(const fb::file::file_state& fb);
        file_state(const YAML::Node& body);

        const std::vector<std::unique_ptr<attribute>>& get_attributes() const { return attrs_; }
        condition get_condition() const { return cond_; }
        const std::string& get_config_name() const { return config_name_; }
        const std::filesystem::path& get_file_name() const { return file_name_; }

    private:
        void maybe_add_sha256_attr();

        std::filesystem::path file_name_;
        std::string config_name_;
        condition cond_;
        std::vector<std::unique_ptr<attribute>> attrs_;
    };

    void process_after(const YAML::Node& body);
    void process_before(const YAML::Node& body);
    void process_file_state(const YAML::Node& body);
    void process_monitor_request(const YAML::Node& body);
    void process_monitor_requests(const YAML::Node& before);
    void process_exists(const YAML::Node& before);
    void send_message(const std::uint8_t* const data, std::size_t sz);

    std::filesystem::path working_dir_;
    std::mutex received_guard_;
    std::vector<file_state> expected_states_;
    std::size_t received_msg_count_;
    std::vector<file_state> received_states_;
};

}

}

#endif
