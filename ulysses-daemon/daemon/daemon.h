#ifndef ULYSSES_DAEMON_H
#define ULYSSES_DAEMON_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusError>
#include <QTimer>
#include <QDebug>
#include "../common/constants.h"
#include "../common/database.h"
#include "session_manager.h"
#include "hardening.h"
#include "enforcement_manager.h"
#include "network_manager.h"

namespace Ulysses {

class Daemon : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.ulysses.Daemon")

public:
    explicit Daemon(char **argv, int argc, QObject *parent = nullptr);
    ~Daemon() override;

    bool registerOnBus();

public slots:
    QString StartSession(const QString &block_id, const QString &lock_type, const QString &lock_param);
    bool EndSession(const QString &session_id, const QString &auth_token);

    QString CreateBlock(const QString &block_json);
    bool UpdateBlock(const QString &block_json);
    bool DeleteBlock(const QString &block_id);
    QString GetBlocks();

    QString CreateList(const QString &list_json);
    bool UpdateList(const QString &list_json);
    bool DeleteList(const QString &list_id);
    QString GetLists();

    QString CreateTrigger(const QString &trigger_json);
    bool DeleteTrigger(const QString &uuid);
    QString FireTrigger(const QString &uuid);
    QString GetTriggers();

    QString GetActiveSessions();
    QString GetStatus();

    // Network D-Bus methods
    QString CreateNetworkGroup();
    bool JoinNetworkGroup(const QString &groupUuid, const QString &groupSecretHex, const QString &saltHex);
    void LeaveNetworkGroup();
    QString GetNetworkStatus();
    QString GetPeers();

signals:
    void SessionStarted(const QString &session_json);
    void SessionEnded(const QString &session_id);
    void TriggerFired(const QString &trigger_uuid, const QString &session_id);
    void BlocksChanged();
    void ListsChanged();
    void TriggersChanged();

private:
    SessionManager *m_sessionMgr;
    Hardening *m_hardening;
    EnforcementManager *m_enforcement;
    NetworkManager *m_network;
};

} // namespace Ulysses

#endif // ULYSSES_DAEMON_H
