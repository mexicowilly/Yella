#ifndef YELLA_MODEL_HPP__
#define YELLA_MODEL_HPP__

#include "main_window.hpp"
#include "database.hpp"
#include <map>
#include <mutex>
#include <QtCore/QObject>

namespace yella
{

namespace console
{

class model : public QObject
{
public:
    model(const configuration& cnf, database& db, main_window& win);

public slots:
    void file_changed(const parcel& pcl);
    void heartbeat(const parcel& pcl);

signals:
    void agent_changed(const agent& ag);

private:
    Q_OBJECT

    const configuration& config_;
    database& db_;
    std::map<std::string, std::unique_ptr<agent>> agents_;
    std::mutex agent_guard_;
};

}

}

#endif
