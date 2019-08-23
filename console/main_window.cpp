#include "main_window.hpp"

namespace yella
{

namespace console
{

main_window::main_window()
    : QMainWindow(nullptr)
{
    ui_.setupUi(this);
}

void main_window::agent_changed(const agent& ag)
{
}

}

}
