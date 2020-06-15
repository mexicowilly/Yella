#include "console.hpp"
#include <QtGui/QApplication>

namespace yella
{

namespace console
{

console::console(const configuration& cnf)
    : config_(cnf),
      db_(database::create(cnf)),
      model_(cnf, *db_, main_window_),
      mq_(message_queue::create(cnf, model_))
{
    QObject::connect(&model_, SIGNAL(agent_changed(const agent&)),
                     &main_window_, SLOT(agent_changed(const agent&)));
    QObject::connect(mq_.get(), SIGNAL(death()),
                     qApp, SLOT(quit()));
    main_window_.show();
}

}

}
