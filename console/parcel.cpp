#include "parcel.hpp"
#include "parcel_generated.h"

namespace
{

constexpr yella::console::parcel::compression cmp_from_fb(yella::fb::compression fb)
{
    if (fb == yella::fb::compression_NONE)
        return yella::console::parcel::compression::NONE;
    return yella::console::parcel::compression::LZ4;
}

constexpr yella::console::parcel::group_disposition gdis_from_fb(yella::fb::group_disposition fgd)
{
    if (fgd == yella::fb::group_disposition_MORE)
        return yella::console::parcel::group_disposition::MORE;
    return yella::console::parcel::group_disposition::END;
}

}

namespace yella
{

namespace console
{

parcel::parcel(const std::uint8_t* const raw)
{
    auto fp = yella::fb::Getparcel(raw);
    when_ = std::chrono::system_clock::from_time_t(fp->seconds_since_epoch());
    if (fp->sender() == nullptr)
        throw std::invalid_argument("The parcel's sender must be set");
    sender_ = fp->sender()->str();
    if (fp->recipient() == nullptr)
        throw std::invalid_argument("The parcel's recipient must be set");
    recipient_ = fp->recipient()->str();
    if (fp->type() == nullptr)
        throw std::invalid_argument("The parcel's type must be set");
    type_ = fp->type()->str();
    compression_ = cmp_from_fb(fp->cmp());
    auto seq = fp->seq();
    if (seq == nullptr)
        throw std::invalid_argument("The parcel's sequence must be set");
    sequence_.major = seq->major();
    sequence_.major = seq->major();
    auto grp = fp->grp();
    if (grp != nullptr)
    {
        if (grp->id() == nullptr)
            throw std::invalid_argument("The parcel's group ID is not set");
        group_ = std::make_shared<group>();
        group_->id = grp->id()->str();
        group_->disposition = gdis_from_fb(grp->disposition());
    }
    auto pl = fp->payload();
    if (pl == nullptr)
        throw std::invalid_argument("The parcel's payload must be set");
    payload_.assign(pl->begin(), pl->end());
}

}

}
