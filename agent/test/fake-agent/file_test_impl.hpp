#ifndef YELLA_FILE_TEST_IMPL_HPP__
#define YELLA_FILE_TEST_IMPL_HPP__

#include "test_impl.hpp"
#include "parcel.hpp"
#include "file_generated.h"
#include <unicode/datefmt.h>
#include <mutex>
#include <bitset>
#include <condition_variable>

namespace yella
{

namespace test
{

class file_test_impl : public test_impl
{
public:
    file_test_impl(const YAML::Node& doc, const std::filesystem::path& plugin);
    ~file_test_impl();

    virtual bool run() override;

protected:
    virtual void receive_plugin_message_impl(const yella_parcel& pcl) override;

private:
    class attribute
    {
    public:
        enum class type
        {
            FILE_TYPE,
            SHA256,
            POSIX_PERMISSIONS,
            USER,
            GROUP,
            SIZE,
            ACCESS_TIME,
            METADATA_CHANGE_TIME,
            MODIFICATION_TIME,
            POSIX_ACL
        };

        virtual ~attribute() = default;

        virtual void emit(YAML::Emitter& e) const;
        type get_type() const { return type_; }

    protected:
        friend bool operator== (const attribute& lhs, const attribute& rhs) { return lhs.equal_to(rhs); }

        attribute(type tp) : type_(tp) { }

        virtual bool equal_to(const attribute& rhs) const;

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

        virtual void emit(YAML::Emitter& e) const override;
        file_type get_file_type() const { return ftype_; }

    protected:
        virtual bool equal_to(const attribute& rhs) const override;

    private:
        file_type ftype_;
    };

    class bytes_attribute : public attribute
    {
    public:
        bytes_attribute(type tp, const std::vector<std::uint8_t>& bytes);
        bytes_attribute(type tp, const fb::file::attr& fba);

        virtual void emit(YAML::Emitter& e) const override;
        const std::vector<std::uint8_t>& get_bytes() const { return bytes_; }

    protected:
        virtual bool equal_to(const attribute& rhs) const override;

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

        virtual void emit(YAML::Emitter& e) const override;
        bool has(perm p) const { return bits_.test(static_cast<std::size_t>(p)); }

    protected:
        virtual bool equal_to(const attribute& rhs) const override;

    private:
        std::bitset<12> bits_;
    };

    class user_group_attribute : public attribute
    {
    public:
        user_group_attribute(type tp, const fb::file::attr& fba);
        user_group_attribute(type tp, const std::filesystem::path& file_name);

        virtual void emit(YAML::Emitter& e) const override;

    protected:
        virtual bool equal_to(const attribute& rhs) const override;

    private:
        std::uint64_t id_;
        std::string name_;
    };

    class unsigned_int_attribute : public attribute
    {
    public:
        unsigned_int_attribute(type tp, const fb::file::attr& fba);
        unsigned_int_attribute(type tp, std::uint64_t val);

        virtual void emit(YAML::Emitter& e) const override;

    protected:
        virtual bool equal_to(const attribute& rhs) const override;

    private:
        std::uint64_t value_;
    };

    class milliseconds_since_epoch_attribute : public attribute
    {
    public:
        milliseconds_since_epoch_attribute(type tp, const fb::file::attr& fba);
        milliseconds_since_epoch_attribute(type tp, const std::filesystem::path& file_name);

        virtual void emit(YAML::Emitter& e) const override;

    protected:
        virtual bool equal_to(const attribute& rhs) const override;

    private:
        void initialize_time_format();

        UDate time_;
        std::unique_ptr<icu::DateFormat> tfmt_;
    };

    class posix_acl_attribute : public attribute
    {
    public:
        class entry
        {
        public:
            enum class permission
            {
                READ = 0,
                WRITE,
                EXECUTE
            };

            enum class type
            {
                USER,
                GROUP,
                MASK,
                USER_OBJ,
                GROUP_OBJ,
                OTHER
            };

            entry(const fb::file::posix_access_control_entry& fbe);
            entry(void* native);

            bool operator== (const entry& rhs) const;
            bool operator< (const entry& rhs) const;

            void emit(YAML::Emitter& e) const;

        private:
            type type_;
            std::bitset<3> permissions_;
            std::uint64_t id_;
            std::string name_;
        };

        posix_acl_attribute(const fb::file::attr& fba);
        posix_acl_attribute(const std::filesystem::path& file_name);

        virtual void emit(YAML::Emitter& e) const override;

    protected:
        virtual bool equal_to(const attribute& rhs) const override;

    private:
        std::set<entry> entries_;
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

        struct attr_type_less
        {
            constexpr bool operator() (const std::unique_ptr<attribute>& lhs, const std::unique_ptr<attribute>& rhs) const { return lhs->get_type() < rhs->get_type(); }
        };

        file_state(const fb::file::file_state& fb);
        file_state(const std::filesystem::path& working_dir, const YAML::Node& body);

        bool operator== (const file_state& other) const;
        bool operator!= (const file_state& other) const { return !operator==(other); }

        const std::set<std::unique_ptr<attribute>, attr_type_less>& get_attributes() const { return attrs_; }
        condition get_condition() const { return cond_; }
        const std::string& get_config_name() const { return config_name_; }
        const std::filesystem::path& get_file_name() const { return file_name_; }
        UDate get_time() const { return time_; }
        std::string to_text() const;

    private:
        void maybe_add_sha256_attr();

        UDate time_;
        std::filesystem::path file_name_;
        std::string config_name_;
        condition cond_;
        std::set<std::unique_ptr<attribute>, attr_type_less> attrs_;
        std::unique_ptr<icu::DateFormat> tfmt_;
    };

    struct file_state_less
    {
        constexpr bool operator() (const file_state& lhs, const file_state& rhs) const
        {
            bool result = false;
            auto cmp = lhs.get_config_name().compare(rhs.get_config_name());
            if (cmp < 0)
            {
                result = true;
            }
            else if (cmp == 0)
            {
                auto cmp2 = lhs.get_file_name().compare(rhs.get_file_name());
                if (cmp2 < 0)
                    result = true;
                else if (cmp2 == 0)
                    result = lhs.get_time() < rhs.get_time();
            }
            return result;
        }
    };

    void clear_plugin_work();
    void process_after(const YAML::Node& body);
    void process_before(const YAML::Node& body);
    void process_exists(const YAML::Node& body);
    void process_grow(const YAML::Node& body);
    void process_monitor_request(const YAML::Node& body);
    void process_pause(const YAML::Node& body);
    void send_message(const std::uint8_t* const data, std::size_t sz);
    template <class rep, class prd>
    bool wait_for_states(std::size_t count, const std::chrono::duration<rep, prd>& max_time);

    std::mutex received_guard_;
    std::multiset<file_state, file_state_less> expected_states_;
    std::size_t received_msg_count_;
    std::multiset<file_state, file_state_less> received_states_;
    std::condition_variable received_cond_;
};

template <class rep, class prd>
bool file_test_impl::wait_for_states(std::size_t count, const std::chrono::duration<rep, prd>& max_time)
{
    std::unique_lock<std::mutex> lock(received_guard_);
    return received_cond_.wait_for(lock, max_time, [this, count]() { return received_states_.size() >= count; }) &&
           received_states_.size() == count;
}

}

}

#endif
