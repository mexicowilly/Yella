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
}

int console::run(int argc, char* argv[])
{
    QApplication app(argc, argv);
    main_window_.show();
    return app.exec();
}

}

}
