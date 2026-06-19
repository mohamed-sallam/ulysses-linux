#ifndef ULYSSES_DBUS_CLIENT_H
#define ULYSSES_DBUS_CLIENT_H

#include <QObject>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include "../common/constants.h"

namespace Ulysses {

class DBusClient : public QObject {
    Q_OBJECT

public:
    explicit DBusClient(QObject *parent = nullptr)
        : QObject(parent)
    {
        m_iface = new QDBusInterface(DBUS_SERVICE_NAME, DBUS_OBJECT_PATH,
                                     DBUS_INTERFACE, QDBusConnection::sessionBus(), this);

        QDBusConnection::sessionBus().connect(
            DBUS_SERVICE_NAME, DBUS_OBJECT_PATH, DBUS_INTERFACE,
            "SessionStarted", this, SIGNAL(sessionStarted(QString)));
        QDBusConnection::sessionBus().connect(
            DBUS_SERVICE_NAME, DBUS_OBJECT_PATH, DBUS_INTERFACE,
            "SessionEnded", this, SIGNAL(sessionEnded(QString)));
        QDBusConnection::sessionBus().connect(
            DBUS_SERVICE_NAME, DBUS_OBJECT_PATH, DBUS_INTERFACE,
            "BlocksChanged", this, SIGNAL(blocksChanged()));
        QDBusConnection::sessionBus().connect(
            DBUS_SERVICE_NAME, DBUS_OBJECT_PATH, DBUS_INTERFACE,
            "ListsChanged", this, SIGNAL(listsChanged()));
        QDBusConnection::sessionBus().connect(
            DBUS_SERVICE_NAME, DBUS_OBJECT_PATH, DBUS_INTERFACE,
            "TriggersChanged", this, SIGNAL(triggersChanged()));
    }

    bool isConnected() const { return m_iface->isValid(); }

    // Lists
    QString createList(const QString &json) {
        QDBusReply<QString> r = m_iface->call("CreateList", json);
        return r.isValid() ? r.value() : "";
    }
    bool updateList(const QString &json) {
        QDBusReply<bool> r = m_iface->call("UpdateList", json);
        return r.isValid() && r.value();
    }
    bool deleteList(const QString &id) {
        QDBusReply<bool> r = m_iface->call("DeleteList", id);
        return r.isValid() && r.value();
    }
    QString getLists() {
        QDBusReply<QString> r = m_iface->call("GetLists");
        return r.isValid() ? r.value() : "[]";
    }

    // Blocks
    QString createBlock(const QString &json) {
        QDBusReply<QString> r = m_iface->call("CreateBlock", json);
        return r.isValid() ? r.value() : "";
    }
    bool updateBlock(const QString &json) {
        QDBusReply<bool> r = m_iface->call("UpdateBlock", json);
        return r.isValid() && r.value();
    }
    bool deleteBlock(const QString &id) {
        QDBusReply<bool> r = m_iface->call("DeleteBlock", id);
        return r.isValid() && r.value();
    }
    QString getBlocks() {
        QDBusReply<QString> r = m_iface->call("GetBlocks");
        return r.isValid() ? r.value() : "[]";
    }

    // Triggers
    QString createTrigger(const QString &json) {
        QDBusReply<QString> r = m_iface->call("CreateTrigger", json);
        return r.isValid() ? r.value() : "";
    }
    bool deleteTrigger(const QString &uuid) {
        QDBusReply<bool> r = m_iface->call("DeleteTrigger", uuid);
        return r.isValid() && r.value();
    }
    QString getTriggers() {
        QDBusReply<QString> r = m_iface->call("GetTriggers");
        return r.isValid() ? r.value() : "[]";
    }
    QString fireTrigger(const QString &uuid) {
        QDBusReply<QString> r = m_iface->call("FireTrigger", uuid);
        return r.isValid() ? r.value() : "";
    }

    // Sessions
    QString startSession(const QString &blockId, const QString &lockType, const QString &lockParam) {
        QDBusReply<QString> r = m_iface->call("StartSession", blockId, lockType, lockParam);
        return r.isValid() ? r.value() : "";
    }
    bool endSession(const QString &sessionId, const QString &authToken) {
        QDBusReply<bool> r = m_iface->call("EndSession", sessionId, authToken);
        return r.isValid() && r.value();
    }
    QString getActiveSessions() {
        QDBusReply<QString> r = m_iface->call("GetActiveSessions");
        return r.isValid() ? r.value() : "[]";
    }
    QString getStatus() {
        QDBusReply<QString> r = m_iface->call("GetStatus");
        return r.isValid() ? r.value() : "{}";
    }

    // Network
    QString createNetworkGroup() {
        QDBusReply<QString> r = m_iface->call("CreateNetworkGroup");
        return r.isValid() ? r.value() : "";
    }
    bool joinNetworkGroup(const QString &groupUuid, const QString &secretHex, const QString &saltHex) {
        QDBusReply<bool> r = m_iface->call("JoinNetworkGroup", groupUuid, secretHex, saltHex);
        return r.isValid() && r.value();
    }
    void leaveNetworkGroup() {
        m_iface->call("LeaveNetworkGroup");
    }
    QString getNetworkStatus() {
        QDBusReply<QString> r = m_iface->call("GetNetworkStatus");
        return r.isValid() ? r.value() : "{}";
    }
    QString getPeers() {
        QDBusReply<QString> r = m_iface->call("GetPeers");
        return r.isValid() ? r.value() : "[]";
    }

signals:
    void sessionStarted(const QString &sessionJson);
    void sessionEnded(const QString &sessionId);
    void blocksChanged();
    void listsChanged();
    void triggersChanged();

private:
    QDBusInterface *m_iface;
};

} // namespace Ulysses

#endif // ULYSSES_DBUS_CLIENT_H
