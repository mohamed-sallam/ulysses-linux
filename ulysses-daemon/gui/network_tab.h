#ifndef ULYSSES_NETWORK_TAB_H
#define ULYSSES_NETWORK_TAB_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QGroupBox>
#include <QTextEdit>
#include <QTimer>
#include "../gui/dbus_client.h"

namespace Ulysses {

class NetworkTab : public QWidget {
    Q_OBJECT

public:
    explicit NetworkTab(DBusClient *client, QWidget *parent = nullptr);

private slots:
    void refreshStatus();
    void createGroup();
    void joinGroup();
    void leaveGroup();

private:
    void setupUI();

    DBusClient *m_client;

    QLabel *m_statusLabel;
    QLabel *m_groupUuidLabel;
    QLabel *m_deviceIdLabel;

    QPushButton *m_createGroupBtn;
    QPushButton *m_joinGroupBtn;
    QPushButton *m_leaveGroupBtn;

    // QR data display (text-based for now)
    QGroupBox *m_qrGroup;
    QTextEdit *m_qrDataDisplay;

    // Peers
    QListWidget *m_peersList;

    QTimer m_refreshTimer;
};

} // namespace Ulysses

#endif // ULYSSES_NETWORK_TAB_H
