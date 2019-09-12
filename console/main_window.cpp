#include "main_window.hpp"
#include <QtCore/QDateTime>

namespace yella
{

namespace console
{

main_window::main_window()
    : QMainWindow(nullptr)
{
    ui_.setupUi(this);
    QObject::connect(ui_.agent_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
                     this, SLOT(current_item_changed(QTreeWidgetItem*, QTreeWidgetItem*)));
    QObject::connect(ui_.quit_action, SIGNAL(triggered()),
                     qApp, SLOT(quit()));
}

void main_window::agent_changed(const agent& ag)
{
    auto found = agents_.find(ag.id());
    if (found == agents_.end())
    {
        auto item = std::make_unique<QTreeWidgetItem>(ui_.agent_tree, QStringList(QString::fromUtf8(ag.id().c_str())));
        item->setData(0, Qt::UserRole, QVariant::fromValue(const_cast<void*>(static_cast<const void*>(&ag))));
        found = agents_.insert(std::make_pair(ag.id(), item.release())).first;
    }
    if (ui_.agent_tree->currentItem() == found->second)
        update_detail(found->second);
}

void main_window::current_item_changed(QTreeWidgetItem* cur, QTreeWidgetItem* prev)
{
    update_detail(cur);
}

void main_window::update_detail(QTreeWidgetItem* cur)
{
    if (cur != nullptr)
    {
        ui_.detail_table->clear();
        if (cur->parent() == nullptr)
        {
            agent* ag = static_cast<agent*>(cur->data(0, Qt::UserRole).value<void*>());
            ui_.detail_table->setRowCount(10);
            ui_.detail_table->setItem(0, 0, new QTableWidgetItem(QString::fromUtf8("UUID")));
            ui_.detail_table->setItem(0, 1, new QTableWidgetItem(QString::fromUtf8(ag->id().c_str())));
            ui_.detail_table->setItem(1, 0, new QTableWidgetItem(QString::fromUtf8("Host")));
            ui_.detail_table->setItem(1, 1, new QTableWidgetItem(QString::fromUtf8(ag->host().c_str())));
            QStringList sl;
            for (const auto& addr : ag->ip_addresses())
                sl << QString::fromUtf8(addr.c_str());
            ui_.detail_table->setItem(2, 0, new QTableWidgetItem(QString::fromUtf8("IP Addresses")));
            ui_.detail_table->setItem(2, 1, new QTableWidgetItem(sl.join(QString::fromUtf8(","))));
            ui_.detail_table->setItem(3, 0, new QTableWidgetItem(QString::fromUtf8("Machine")));
            ui_.detail_table->setItem(3, 1, new QTableWidgetItem(QString::fromUtf8(ag->os().machine().c_str())));
            ui_.detail_table->setItem(4, 0, new QTableWidgetItem(QString::fromUtf8("Operating System")));
            ui_.detail_table->setItem(4, 1, new QTableWidgetItem(QString::fromUtf8(ag->os().system().c_str())));
            ui_.detail_table->setItem(5, 0, new QTableWidgetItem(QString::fromUtf8("Operating System Version")));
            ui_.detail_table->setItem(5, 1, new QTableWidgetItem(QString::fromUtf8(ag->os().version().c_str())));
            ui_.detail_table->setItem(6, 0, new QTableWidgetItem(QString::fromUtf8("Operating System Release")));
            ui_.detail_table->setItem(6, 1, new QTableWidgetItem(QString::fromUtf8(ag->os().release().c_str())));
            ui_.detail_table->setItem(7, 0, new QTableWidgetItem(QString::fromUtf8("In Capabilities")));
            sl.clear();
            for (const auto& cap : ag->in_caps())
            {
                QString str = QString::fromUtf8("{");
                str += QString::fromUtf8(cap.name().c_str());
                str += QString::fromUtf8(",");
                str += QString::number(cap.version());
                str += QString::fromUtf8("}");
                sl << str;
            }
            ui_.detail_table->setItem(7, 1, new QTableWidgetItem(sl.join(QString::fromUtf8(","))));
            sl.clear();
            for (const auto& cap : ag->in_caps())
            {
                QString str = QString::fromUtf8("{");
                str += QString::fromUtf8(cap.name().c_str());
                str += QString::fromUtf8(",");
                str += QString::number(cap.version());
                str += QString::fromUtf8("}");
                sl << str;
            }
            ui_.detail_table->setItem(7, 1, new QTableWidgetItem(sl.join(QString::fromUtf8(","))));
            ui_.detail_table->setItem(8, 0, new QTableWidgetItem(QString::fromUtf8("Out Capabilities")));
            sl.clear();
            for (const auto& cap : ag->out_caps())
            {
                QString str = QString::fromUtf8("{");
                str += QString::fromUtf8(cap.name().c_str());
                str += QString::fromUtf8(",");
                str += QString::number(cap.version());
                str += QString::fromUtf8(",{");
                QStringList configs;
                for (const auto& config : cap.configurations())
                    configs << QString::fromUtf8(config.c_str());
                str += configs.join(QString::fromUtf8(","));
                str += QString::fromUtf8("}}");
                sl << str;
            }
            ui_.detail_table->setItem(8, 1, new QTableWidgetItem(sl.join(QString::fromUtf8(","))));
            ui_.detail_table->setItem(9, 0, new QTableWidgetItem(QString::fromUtf8("Last Time")));
            QDateTime dt = QDateTime::fromTime_t(std::chrono::system_clock::to_time_t(ag->when()));
            ui_.detail_table->setItem(9, 1, new QTableWidgetItem(dt.toString(QString::fromUtf8("yyyy-MM-dd hh:mm:ss"))));
        }
    }
}

}

}
