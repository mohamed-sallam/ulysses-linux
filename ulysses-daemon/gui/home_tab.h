#ifndef ULYSSES_HOME_TAB_H
#define ULYSSES_HOME_TAB_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include "../gui/dbus_client.h"

namespace Ulysses {

class HomeTab : public QWidget {
    Q_OBJECT

public:
    explicit HomeTab(DBusClient *client, QWidget *parent = nullptr);

private slots:
    void refreshBlocks();
    void refreshSessions();
    void startBlock();
    void stopSession();

private:
    void setupUI();

    DBusClient *m_client;
    QListWidget *m_blocksList;
    QListWidget *m_sessionsList;
};

} // namespace Ulysses

#endif // ULYSSES_HOME_TAB_H
