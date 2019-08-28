#ifndef YELLA_MESSAGE_QUEUE_HPP__
#define YELLA_MESSAGE_QUEUE_HPP__

#include "configuration.hpp"
#include "parcel.hpp"
#include "model.hpp"
#include <chucho/loggable.hpp>
#include <QtCore/QObject>
#include <thread>
#include <atomic>

namespace yella
{

namespace console
{

class message_queue : public QObject, public chucho::loggable<message_queue>
{
public:
    static std::unique_ptr<message_queue> create(const configuration& cnf, model& mdl);

    message_queue(const message_queue&) = delete;
    virtual ~message_queue();

    message_queue& operator= (const message_queue&) = delete;

signals:
    void death();
    void file_changed(const parcel& pcl);
    void heartbeat(const parcel& pcl);

protected:
    message_queue(const configuration& cnf, model& mdl);

    const configuration& config_;
    std::vector<std::thread> receivers_;
    std::atomic_bool should_stop_;

private:
    Q_OBJECT
};

}

}

#endif
