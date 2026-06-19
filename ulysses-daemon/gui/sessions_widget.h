#ifndef ULYSSES_SESSIONS_WIDGET_H
#define ULYSSES_SESSIONS_WIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QLabel>
#include <QTimer>
#include "../gui/dbus_client.h"

namespace Ulysses {

class SessionsWidget : public QWidget {
    Q_OBJECT

public:
    explicit SessionsWidget(DBusClient *client, QWidget *parent = nullptr);

private slots:
    void refreshSessions();
    void endSession();

private:
    void setupUI();

    DBusClient *m_client;
    QListWidget *m_sessionList;
    QPushButton *m_endSessionBtn;
    QLineEdit *m_authTokenEdit;
    QLabel *m_countLabel;
    QTimer m_refreshTimer;
};

} // namespace Ulysses

#endif // ULYSSES_SESSIONS_WIDGET_H
