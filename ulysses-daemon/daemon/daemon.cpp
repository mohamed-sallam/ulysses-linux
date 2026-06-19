#include "daemon.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUuid>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>

namespace Ulysses {

Daemon::Daemon(char **argv, int argc, QObject *parent)
    : QObject(parent)
{
    if (!Database::initialize()) {
        qCritical() << "Database initialization failed";
    }

    m_sessionMgr = new SessionManager(this);
    m_hardening = new Hardening(argv, argc, this);
    m_enforcement = new EnforcementManager(this);
    m_network = new NetworkManager(this);

    connect(m_sessionMgr, &SessionManager::sessionStarted, this, [this](const ActiveSession &s) {
        m_enforcement->enforceSession(s);
        QJsonObject obj;
        obj["id"] = s.id;
        obj["block_id"] = s.blockId;
        obj["lock_type"] = s.lockType;
        obj["started_at"] = s.startedAt;
        obj["expires_at"] = s.expiresAt;
        emit SessionStarted(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    });

    connect(m_sessionMgr, &SessionManager::sessionEnded, this, [this](const QString &id, const QString &triggerUuid) {
        m_enforcement->releaseSession(id);
        // Propagate disable event if trigger has network flag
        if (!triggerUuid.isEmpty()) {
            QSqlQuery q;
            q.prepare("SELECT propagate_to_network, lamport_ts FROM triggers WHERE uuid=?");
            q.addBindValue(triggerUuid);
            if (q.exec() && q.next() && q.value(0).toBool()) {
                m_network->publishDisableEvent(triggerUuid, q.value(1).toULongLong());
            }
        }
        emit SessionEnded(id);
    });

    // Network: handle incoming fire events
    connect(m_network, &NetworkManager::fireEventReceived, this, [this](const QString &triggerUuid) {
        FireTrigger(triggerUuid);
    });

    // Network: handle incoming disable events
    connect(m_network, &NetworkManager::disableEventReceived, this, [this](const QString &triggerUuid) {
        // Find and force-end any session associated with this trigger
        for (const auto &s : m_sessionMgr->activeSessions()) {
            if (s.triggerUuid == triggerUuid) {
                m_enforcement->releaseSession(s.id);
                m_sessionMgr->forceEndSession(s.id);
                emit SessionEnded(s.id);
                break;
            }
        }
    });

    // Network: handle incoming trigger sync (CRDT merge)
    connect(m_network, &NetworkManager::triggerReceived, this, [this](const QJsonObject &triggerData) {
        QString uuid = triggerData["uuid"].toString();
        qint64 remoteLts = static_cast<qint64>(triggerData["lamport_ts"].toDouble());
        bool deleted = triggerData["deleted"].toBool();

        QSqlQuery q;
        q.prepare("SELECT lamport_ts FROM triggers WHERE uuid=?");
        q.addBindValue(uuid);

        if (q.exec() && q.next()) {
            qint64 localLts = q.value(0).toLongLong();
            if (remoteLts > localLts) {
                // Remote wins - update local
                if (deleted) {
                    QSqlQuery uq;
                    uq.prepare("UPDATE triggers SET deleted=1, lamport_ts=? WHERE uuid=?");
                    uq.addBindValue(remoteLts);
                    uq.addBindValue(uuid);
                    uq.exec();
                } else {
                    QSqlQuery uq;
                    uq.prepare("UPDATE triggers SET name=?, block_id=?, propagate_to_network=?, lamport_ts=? WHERE uuid=?");
                    uq.addBindValue(triggerData["name"].toString());
                    uq.addBindValue(triggerData["block_id"].toString());
                    uq.addBindValue(triggerData["propagate_to_network"].toBool() ? 1 : 0);
                    uq.addBindValue(remoteLts);
                    uq.addBindValue(uuid);
                    uq.exec();
                }
                emit TriggersChanged();
            }
        } else if (!deleted) {
            // New trigger from network
            QSqlQuery iq;
            iq.prepare("INSERT INTO triggers (uuid, name, block_id, propagate_to_network, lamport_ts) VALUES (?, ?, ?, ?, ?)");
            iq.addBindValue(uuid);
            iq.addBindValue(triggerData["name"].toString());
            iq.addBindValue(triggerData["block_id"].toString());
            iq.addBindValue(triggerData["propagate_to_network"].toBool() ? 1 : 0);
            iq.addBindValue(remoteLts);
            iq.exec();
            emit TriggersChanged();
        }
    });

    // Parse dev mode
    bool devMode = false;
    for (int i = 1; i < argc; ++i) {
        if (QString(argv[i]) == "--dev") {
            devMode = true;
            break;
        }
    }

    // Resume persisted sessions and start hardening
    m_sessionMgr->resumePersistedSessions();
    m_hardening->start(devMode);
}

Daemon::~Daemon() {}

bool Daemon::registerOnBus() {
    QDBusConnection bus = QDBusConnection::sessionBus();

    if (!bus.isConnected()) {
        qCritical() << "Cannot connect to D-Bus session bus";
        return false;
    }

    if (!bus.registerService(DBUS_SERVICE_NAME)) {
        qCritical() << "Cannot register D-Bus service:" << bus.lastError().message();
        return false;
    }

    if (!bus.registerObject(DBUS_OBJECT_PATH, this,
                            QDBusConnection::ExportAllSlots |
                            QDBusConnection::ExportAllSignals)) {
        qCritical() << "Cannot register D-Bus object:" << bus.lastError().message();
        return false;
    }

    qInfo() << "Daemon registered on D-Bus:" << DBUS_SERVICE_NAME;
    return true;
}

// --- List CRUD ---

QString Daemon::CreateList(const QString &list_json) {
    QJsonDocument doc = QJsonDocument::fromJson(list_json.toUtf8());
    QJsonObject obj = doc.object();

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = obj["name"].toString();
    QJsonArray entries = obj["entries"].toArray();

    QSqlQuery q;
    q.prepare("INSERT INTO lists (id, name) VALUES (?, ?)");
    q.addBindValue(id);
    q.addBindValue(name);
    if (!q.exec()) {
        qWarning() << "CreateList failed:" << q.lastError().text();
        return "";
    }

    for (const auto &entry : entries) {
        QJsonObject e = entry.toObject();
        QSqlQuery eq;
        eq.prepare("INSERT INTO list_entries (list_id, type, value, rule_type) VALUES (?, ?, ?, ?)");
        eq.addBindValue(id);
        eq.addBindValue(e["type"].toString("deny"));
        eq.addBindValue(e["value"].toString());
        eq.addBindValue(e["rule_type"].toString("domain"));
        eq.exec();
    }

    emit ListsChanged();
    return id;
}

bool Daemon::UpdateList(const QString &list_json) {
    QJsonDocument doc = QJsonDocument::fromJson(list_json.toUtf8());
    QJsonObject obj = doc.object();
    QString id = obj["id"].toString();

    QSqlQuery q;
    q.prepare("UPDATE lists SET name=?, updated_at=strftime('%s','now') WHERE id=?");
    q.addBindValue(obj["name"].toString());
    q.addBindValue(id);
    if (!q.exec()) return false;

    QSqlQuery del;
    del.prepare("DELETE FROM list_entries WHERE list_id=?");
    del.addBindValue(id);
    del.exec();

    QJsonArray entries = obj["entries"].toArray();
    for (const auto &entry : entries) {
        QJsonObject e = entry.toObject();
        QSqlQuery eq;
        eq.prepare("INSERT INTO list_entries (list_id, type, value, rule_type) VALUES (?, ?, ?, ?)");
        eq.addBindValue(id);
        eq.addBindValue(e["type"].toString("deny"));
        eq.addBindValue(e["value"].toString());
        eq.addBindValue(e["rule_type"].toString("domain"));
        eq.exec();
    }

    emit ListsChanged();
    return true;
}

bool Daemon::DeleteList(const QString &list_id) {
    QSqlQuery q;
    q.prepare("DELETE FROM lists WHERE id=?");
    q.addBindValue(list_id);
    bool ok = q.exec();
    if (ok) emit ListsChanged();
    return ok;
}

QString Daemon::GetLists() {
    QJsonArray result;
    QSqlQuery q("SELECT id, name FROM lists ORDER BY name");
    while (q.next()) {
        QJsonObject obj;
        obj["id"] = q.value(0).toString();
        obj["name"] = q.value(1).toString();

        QJsonArray entries;
        QSqlQuery eq;
        eq.prepare("SELECT type, value, rule_type FROM list_entries WHERE list_id=?");
        eq.addBindValue(q.value(0).toString());
        eq.exec();
        while (eq.next()) {
            QJsonObject e;
            e["type"] = eq.value(0).toString();
            e["value"] = eq.value(1).toString();
            e["rule_type"] = eq.value(2).toString();
            entries.append(e);
        }
        obj["entries"] = entries;
        result.append(obj);
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

// --- Block CRUD ---

QString Daemon::CreateBlock(const QString &block_json) {
    QJsonDocument doc = QJsonDocument::fromJson(block_json.toUtf8());
    QJsonObject obj = doc.object();

    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery q;
    q.prepare("INSERT INTO blocks (id, name, lock_type, lock_param) VALUES (?, ?, ?, ?)");
    q.addBindValue(id);
    q.addBindValue(obj["name"].toString());
    q.addBindValue(obj["lock_type"].toString("timer"));
    q.addBindValue(obj["lock_param"].toString("3600"));
    if (!q.exec()) return "";

    QJsonArray listIds = obj["list_ids"].toArray();
    for (const auto &lid : listIds) {
        QSqlQuery lq;
        lq.prepare("INSERT INTO block_lists (block_id, list_id) VALUES (?, ?)");
        lq.addBindValue(id);
        lq.addBindValue(lid.toString());
        lq.exec();
    }

    emit BlocksChanged();
    return id;
}

bool Daemon::UpdateBlock(const QString &block_json) {
    QJsonDocument doc = QJsonDocument::fromJson(block_json.toUtf8());
    QJsonObject obj = doc.object();
    QString id = obj["id"].toString();

    QSqlQuery q;
    q.prepare("UPDATE blocks SET name=?, lock_type=?, lock_param=?, updated_at=strftime('%s','now') WHERE id=?");
    q.addBindValue(obj["name"].toString());
    q.addBindValue(obj["lock_type"].toString("timer"));
    q.addBindValue(obj["lock_param"].toString("3600"));
    q.addBindValue(id);
    if (!q.exec()) return false;

    QSqlQuery del;
    del.prepare("DELETE FROM block_lists WHERE block_id=?");
    del.addBindValue(id);
    del.exec();

    QJsonArray listIds = obj["list_ids"].toArray();
    for (const auto &lid : listIds) {
        QSqlQuery lq;
        lq.prepare("INSERT INTO block_lists (block_id, list_id) VALUES (?, ?)");
        lq.addBindValue(id);
        lq.addBindValue(lid.toString());
        lq.exec();
    }

    emit BlocksChanged();
    return true;
}

bool Daemon::DeleteBlock(const QString &block_id) {
    QSqlQuery q;
    q.prepare("DELETE FROM blocks WHERE id=?");
    q.addBindValue(block_id);
    bool ok = q.exec();
    if (ok) emit BlocksChanged();
    return ok;
}

QString Daemon::GetBlocks() {
    QJsonArray result;
    QSqlQuery q("SELECT id, name, lock_type, lock_param FROM blocks ORDER BY name");
    while (q.next()) {
        QJsonObject obj;
        obj["id"] = q.value(0).toString();
        obj["name"] = q.value(1).toString();
        obj["lock_type"] = q.value(2).toString();
        obj["lock_param"] = q.value(3).toString();

        QJsonArray listIds;
        QSqlQuery lq;
        lq.prepare("SELECT list_id FROM block_lists WHERE block_id=?");
        lq.addBindValue(q.value(0).toString());
        lq.exec();
        while (lq.next())
            listIds.append(lq.value(0).toString());
        obj["list_ids"] = listIds;
        result.append(obj);
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

// --- Trigger CRUD ---

QString Daemon::CreateTrigger(const QString &trigger_json) {
    QJsonDocument doc = QJsonDocument::fromJson(trigger_json.toUtf8());
    QJsonObject obj = doc.object();

    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlQuery q;
    q.prepare("INSERT INTO triggers (uuid, name, block_id, propagate_to_network, lamport_ts) VALUES (?, ?, ?, ?, 1)");
    q.addBindValue(uuid);
    q.addBindValue(obj["name"].toString());
    q.addBindValue(obj["block_id"].toString());
    q.addBindValue(obj["propagate_to_network"].toBool() ? 1 : 0);
    if (!q.exec()) return "";

    emit TriggersChanged();
    return uuid;
}

bool Daemon::DeleteTrigger(const QString &uuid) {
    QSqlQuery q;
    q.prepare("UPDATE triggers SET deleted=1, lamport_ts=lamport_ts+1, updated_at=strftime('%s','now') WHERE uuid=?");
    q.addBindValue(uuid);
    bool ok = q.exec();
    if (ok) emit TriggersChanged();
    return ok;
}

QString Daemon::GetTriggers() {
    QJsonArray result;
    QSqlQuery q("SELECT uuid, name, block_id, propagate_to_network, lamport_ts FROM triggers WHERE deleted=0");
    while (q.next()) {
        QJsonObject obj;
        obj["uuid"] = q.value(0).toString();
        obj["name"] = q.value(1).toString();
        obj["block_id"] = q.value(2).toString();
        obj["propagate_to_network"] = q.value(3).toBool();
        obj["lamport_ts"] = q.value(4).toInt();
        result.append(obj);
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

// --- Sessions ---

QString Daemon::StartSession(const QString &block_id, const QString &lock_type, const QString &lock_param) {
    QString sessionId = m_sessionMgr->startSession(block_id, lock_type, lock_param);
    if (!sessionId.isEmpty()) {
        // Find the session and enforce it
        for (const auto &s : m_sessionMgr->activeSessions()) {
            if (s.id == sessionId) {
                m_enforcement->enforceSession(s);
                break;
            }
        }
    }
    return sessionId;
}

bool Daemon::EndSession(const QString &session_id, const QString &auth_token) {
    return m_sessionMgr->endSession(session_id, auth_token);
}

QString Daemon::FireTrigger(const QString &uuid) {
    // Look up trigger to get block_id and lock defaults
    QSqlQuery q;
    q.prepare("SELECT block_id, propagate_to_network FROM triggers WHERE uuid=? AND deleted=0");
    q.addBindValue(uuid);
    if (!q.exec() || !q.next()) return "";

    QString blockId = q.value(0).toString();
    bool propagate = q.value(1).toBool();

    // Get lamport_ts for network event
    QSqlQuery tsq;
    tsq.prepare("SELECT lamport_ts FROM triggers WHERE uuid=?");
    tsq.addBindValue(uuid);
    quint64 lamportTs = 0;
    if (tsq.exec() && tsq.next()) lamportTs = tsq.value(0).toULongLong();

    // Get block's default lock settings
    QSqlQuery bq;
    bq.prepare("SELECT lock_type, lock_param FROM blocks WHERE id=?");
    bq.addBindValue(blockId);
    if (!bq.exec() || !bq.next()) return "";

    QString lockType = bq.value(0).toString();
    QString lockParam = bq.value(1).toString();

    QString sessionId = m_sessionMgr->startSession(blockId, lockType, lockParam, uuid);
    if (!sessionId.isEmpty()) {
        for (const auto &s : m_sessionMgr->activeSessions()) {
            if (s.id == sessionId) {
                m_enforcement->enforceSession(s);
                break;
            }
        }
        emit TriggerFired(uuid, sessionId);
        // Propagate fire event to network if enabled
        if (propagate) {
            m_network->publishFireEvent(uuid, lamportTs);
        }
    }

    return sessionId;
}

QString Daemon::GetActiveSessions() {
    QJsonArray result;
    for (const auto &s : m_sessionMgr->activeSessions()) {
        QJsonObject obj;
        obj["id"] = s.id;
        obj["block_id"] = s.blockId;
        obj["trigger_uuid"] = s.triggerUuid;
        obj["lock_type"] = s.lockType;
        obj["started_at"] = s.startedAt;
        obj["expires_at"] = s.expiresAt;
        result.append(obj);
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

QString Daemon::GetStatus() {
    QJsonObject status;
    status["version"] = APP_VERSION;
    status["running"] = true;
    status["active_sessions"] = QJsonDocument::fromJson(GetActiveSessions().toUtf8()).array();
    status["has_group"] = m_network->hasGroup();
    if (m_network->hasGroup())
        status["group_uuid"] = m_network->groupInfo().groupUuid;
    status["peers"] = m_network->peers().size();
    return QJsonDocument(status).toJson(QJsonDocument::Compact);
}

// --- Network D-Bus methods ---

QString Daemon::CreateNetworkGroup() {
    GroupInfo info = m_network->createGroup();
    QJsonObject obj;
    obj["group_uuid"] = info.groupUuid;
    obj["group_secret"] = QString::fromUtf8(info.groupSecret.toHex());
    obj["salt"] = QString::fromUtf8(info.salt.toHex());
    obj["device_id"] = info.deviceId;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

bool Daemon::JoinNetworkGroup(const QString &groupUuid, const QString &groupSecretHex, const QString &saltHex) {
    QByteArray secret = QByteArray::fromHex(groupSecretHex.toUtf8());
    QByteArray salt = QByteArray::fromHex(saltHex.toUtf8());
    return m_network->joinGroup(groupUuid, secret, salt);
}

void Daemon::LeaveNetworkGroup() {
    m_network->leaveGroup();
}

QString Daemon::GetNetworkStatus() {
    QJsonObject obj;
    obj["has_group"] = m_network->hasGroup();
    if (m_network->hasGroup()) {
        obj["group_uuid"] = m_network->groupInfo().groupUuid;
        obj["device_id"] = m_network->groupInfo().deviceId;
    }
    QJsonArray peersArr;
    for (const auto &p : m_network->peers()) {
        QJsonObject po;
        po["device_id"] = p.deviceId;
        po["last_seen"] = p.lastSeen;
        peersArr.append(po);
    }
    obj["peers"] = peersArr;
    return QJsonDocument(obj).toJson(QJsonDocument::Compact);
}

QString Daemon::GetPeers() {
    QJsonArray result;
    for (const auto &p : m_network->peers()) {
        QJsonObject obj;
        obj["device_id"] = p.deviceId;
        obj["last_seen"] = p.lastSeen;
        result.append(obj);
    }
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

} // namespace Ulysses
