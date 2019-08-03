#ifndef YELLA_PARCEL_HPP__
#define YELLA_PARCEL_HPP__

#include <chrono>
#include <string>
#include <memory>
#include <vector>

namespace yella
{

namespace console
{

class parcel
{
public:
    enum class compression
    {
        NONE,
        LZ4
    };
    
    enum class group_disposition
    {
        MORE,
        END
    };
    
    struct sequence
    {
        std::uint32_t major;
        std::uint32_t minor;
    };
    
    struct group
    {
        std::string id;
        group_disposition disposition;
    };

    parcel(const std::uint8_t* const raw);

    compression comp() const;
    const std::unique_ptr<group>& grp() const;
    const std::vector<std::uint8_t>& payload() const;
    const std::string& recipient() const;
    const std::string& sender() const;
    const sequence& seq() const;
    const std::string& type() const;
    std::chrono::system_clock::time_point when() const;

private:
    std::chrono::system_clock::time_point when_;
    std::string sender_;
    std::string recipient_;
    std::string type_;
    compression compression_;
    sequence sequence_;
    std::unique_ptr<group> group_;
    std::vector<std::uint8_t> payload_;
};

inline parcel::compression parcel::comp() const
{
    return compression_;
}
inline const std::unique_ptr<parcel::group>& parcel::grp() const
{
    return group_;
}

inline const std::vector<std::uint8_t>& parcel::payload() const
{
    return payload_;
}

inline const std::string& parcel::recipient() const
{
    return recipient_;
}

inline const std::string& parcel::sender() const
{
    return sender_;
}

inline const parcel::sequence& parcel::seq() const
{
    return sequence_;
}

inline const std::string& parcel::type() const
{
    return type_;
}

inline std::chrono::system_clock::time_point parcel::when() const
{
    return when_;
}

}

}

#endif
