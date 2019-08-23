#ifndef YELLA_MAIN_WINDOW_HPP__
#define YELLA_MAIN_WINDOW_HPP__

#include "agent.hpp"
#include "ui_main_window.hpp"
#include <QtGui/QMainWindow>

namespace yella
{

namespace console
{

class main_window : public QMainWindow
{
public:
    main_window();

public slots:
    void agent_changed(const agent& ag);

private:
    Ui::main_window ui_;
};

}

}

#endif
