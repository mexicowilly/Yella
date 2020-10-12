#ifndef YELLA_PLUGIN_HPP__
#define YELLA_PLUGIN_HPP__

#include "plugin/plugin.h"
#include <chucho/loggable.hpp>
#include <unicode/unistr.h>
#include <vector>

namespace yella
{

namespace test
{

class plugin : public chucho::loggable<plugin>
{
public:
    struct in_cap
    {
        icu::UnicodeString name;
        int version;
        std::vector<icu::UnicodeString> configs;
        yella_in_cap_handler handler;
        void* udata;
    };

    struct out_cap
    {
        icu::UnicodeString name;
        int version;
    };

    plugin();
    plugin(const yella_plugin& plg);

    const std::vector<in_cap>& in_caps() const;
    const icu::UnicodeString& name() const;
    const std::vector<out_cap>& out_caps() const;
    void* udata() const;
    const icu::UnicodeString& version() const;

private:
    icu::UnicodeString name_;
    icu::UnicodeString version_;
    std::vector<in_cap> in_caps_;
    std::vector<out_cap> out_caps_;
    void* udata_;
};

inline const std::vector<plugin::in_cap>& plugin::in_caps() const
{
    return in_caps_;
}

inline const icu::UnicodeString& plugin::name() const
{
    return name_;
}

inline const std::vector<plugin::out_cap>& plugin::out_caps() const
{
    return out_caps_;
}

inline void* plugin::udata() const
{
    return udata_;
}

inline const icu::UnicodeString& plugin::version() const
{
    return version_;
}

}

}

#endif
