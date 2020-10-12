#ifndef YELLA_PARCEL_HPP__
#define YELLA_PARCEL_HPP__

#include "common/parcel.h"
#include <unicode/unistr.h>
#include <memory>
#include <vector>

namespace yella
{

namespace test
{

class parcel
{
public:
    parcel() = default;
    parcel(const yella_parcel& pcl);

    yella_compression compression() const;
    const std::unique_ptr<yella_group_disposition>& group_disposition() const;
    const std::unique_ptr<icu::UnicodeString>& group_id() const;
    std::uint32_t major_sequence() const;
    std::uint32_t minor_sequence() const;
    const std::vector<std::uint8_t>& payload() const;
    const icu::UnicodeString& recipient() const;
    const icu::UnicodeString& sender() const;
    UDate time() const;
    const icu::UnicodeString& type() const;

private:
    UDate time_;
    icu::UnicodeString sender_;
    icu::UnicodeString recipient_;
    icu::UnicodeString type_;
    yella_compression cmp_;
    std::uint32_t major_seq_;
    std::uint32_t minor_seq_;
    std::unique_ptr<icu::UnicodeString> group_id_;
    std::unique_ptr<yella_group_disposition> group_dispos_;
    std::vector<std::uint8_t> payload_;
};

inline yella_compression parcel::compression() const
{
    return cmp_;
}

inline const std::unique_ptr<yella_group_disposition>& parcel::group_disposition() const
{
    return group_dispos_;
}

inline const std::unique_ptr<icu::UnicodeString>& parcel::group_id() const
{
    return group_id_;
}

inline std::uint32_t parcel::major_sequence() const
{
    return major_seq_;
}

inline std::uint32_t parcel::minor_sequence() const
{
    return minor_seq_;
}

inline const std::vector<std::uint8_t>& parcel::payload() const
{
    return payload_;
}

inline const icu::UnicodeString& parcel::recipient() const
{
    return recipient_;
}

inline const icu::UnicodeString& parcel::sender() const
{
    return sender_;
}

inline UDate parcel::time() const
{
    return time_;
}

inline const icu::UnicodeString& parcel::type() const
{
    return type_;
}

}

}

#endif
