#ifndef YELLA_PLUGIN_MESSAGE_RECEIVER_HPP__
#define YELLA_PLUGIN_MESSAGE_RECEIVER_HPP__

#include "plugin.h"

namespace yella
{

namespace test
{

class plugin_message_receiver
{
public:
    virtual ~plugin_message_receiver() { }

    virtual void receive_plugin_message(const yella_parcel& pcl) = 0;
};

}

}

#endif
