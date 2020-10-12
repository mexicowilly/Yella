#include "plugin.hpp"

namespace yella
{

namespace test
{

plugin::plugin()
    : udata_(nullptr)
{
}

plugin::plugin(const yella_plugin& plg)
    : name_(plg.name),
      version_(plg.version),
      udata_(plg.udata)
{
    for (std::size_t i = 0; i < yella_ptr_vector_size(plg.in_caps); i++)
    {
        auto ccap = reinterpret_cast<yella_plugin_in_cap*>(yella_ptr_vector_at(plg.in_caps, i));
        in_cap cap;
        cap.name = ccap->name;
        cap.version = ccap->version;
        cap.handler = ccap->handler;
        cap.udata = ccap->udata;
        for (std::size_t j = 0; j < yella_ptr_vector_size(ccap->configs); j++)
            cap.configs.emplace_back(reinterpret_cast<UChar*>(yella_ptr_vector_at(ccap->configs, j)));
        in_caps_.push_back(cap);
    }
    for (std::size_t i = 0; i < yella_ptr_vector_size(plg.out_caps); i++)
    {
        auto ccap = reinterpret_cast<yella_plugin_out_cap*>(yella_ptr_vector_at(plg.out_caps, i));
        out_cap cap;
        cap.name = ccap->name;
        cap.version = ccap->version;
        out_caps_.push_back(cap);
    }
}

}

}