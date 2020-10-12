#include "parcel.hpp"

namespace yella
{

namespace test
{

parcel::parcel(const yella_parcel& pcl)
    : time_(pcl.time),
    sender_(pcl.sender),
    recipient_(pcl.recipient),
    type_(pcl.type),
    cmp_(pcl.cmp),
    major_seq_(pcl.seq.major),
    minor_seq_(pcl.seq.minor),
    group_id_(pcl.grp == nullptr ? nullptr : std::make_unique<icu::UnicodeString>(pcl.grp->identifier)),
    group_dispos_(pcl.grp == nullptr ? nullptr : std::make_unique<yella_group_disposition>(pcl.grp->disposition))
{
    if (pcl.payload_size > 0)
        payload_.assign(pcl.payload, pcl.payload + pcl.payload_size);
}

}

}

