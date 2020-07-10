#include "agent.hpp"
#include <dlfcn.h>

namespace yella
{

namespace test
{

void agent::load_plugin(const std::filesystem::path& plugin)
{
    shared_object_ = dlopen(plugin.c_str(), RTLD_LAZY);
    if(shared_object_ == nullptr)
        throw std::runtime_error(std::string("The shared object '") + plugin.string() + "' could not be loaded: " + dlerror());
    start_func_ = reinterpret_cast<yella_plugin_start_func>(dlsym(shared_object_, "plugin_start"));
    if(start_func_ == nullptr)
        throw std::runtime_error(std::string("The function 'plugin_start' could not be found: ") + dlerror());
    status_func_ = reinterpret_cast<yella_plugin_status_func>(dlsym(shared_object_, "plugin_status"));
    if(status_func_ == nullptr)
        throw std::runtime_error(std::string("The function 'plugin_status' could not be found: ") + dlerror());
    stop_func_ = reinterpret_cast<yella_plugin_stop_func>(dlsym(shared_object_, "plugin_stop"));
    if(stop_func_ == nullptr)
        throw std::runtime_error(std::string("The function 'plugin_stop' could not be found: ") + dlerror());
}

void agent::unload_plugin()
{
    dlclose(shared_object_);
}

}

}
