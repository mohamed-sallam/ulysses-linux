#include "session_manager.h"
#include <QUuid>
#include <QCryptographicHash>
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

namespace Ulysses {

SessionManager::SessionManager(QObject *parent)
    : QObject(parent)
{
    connect(&m_timerCheck, &QTimer::timeout, this, &SessionManager::checkTimers);
    m_timerCheck.start(1000); // Check every second
}

QString SessionManager::startSession(const QString &blockId, const QString &lockType,
                                      const QString &lockParam, const QString &triggerUuid) {
    ActiveSession session;
    session.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    session.blockId = blockId;
    session.triggerUuid = triggerUuid;
    session.lockType = lockType;
    session.lockParam = lockParam;
    session.startedAt = QDateTime::currentSecsSinceEpoch();

    if (lockType == "timer") {
        bool ok;
        int duration = lockParam.toInt(&ok);
        if (!ok || duration <= 0) duration = 3600;
        session.expiresAt = session.startedAt + duration;
    } else if (lockType == "password") {
        session.expiresAt = 0;
        session.passwordHash = hashPassword(lockParam);
    }

    m_sessions.insert(session.id, session);
    persistSession(session);

    qInfo() << "Session started:" << session.id << "block:" << blockId
            << "lock:" << lockType;

    emit sessionStarted(session);
    return session.id;
}

bool SessionManager::endSession(const QString &sessionId, const QString &authToken) {
    if (!m_sessions.contains(sessionId))
        return false;

    const ActiveSession &session = m_sessions[sessionId];

    if (session.lockType == "timer") {
        if (QDateTime::currentSecsSinceEpoch() < session.expiresAt)
            return false; // Timer hasn't expired yet
    } else if (session.lockType == "password") {
        if (!verifyPassword(authToken, session.passwordHash))
            return false;
    }

    QString triggerUuid = session.triggerUuid;
    removePersistedSession(sessionId);
    m_sessions.remove(sessionId);

    qInfo() << "Session ended:" << sessionId;
    emit sessionEnded(sessionId, triggerUuid);
    return true;
}

void SessionManager::forceEndSession(const QString &sessionId) {
    if (!m_sessions.contains(sessionId)) return;
    QString triggerUuid = m_sessions[sessionId].triggerUuid;
    removePersistedSession(sessionId);
    m_sessions.remove(sessionId);
    qInfo() << "Session force-ended (network disable):" << sessionId;
    emit sessionEnded(sessionId, triggerUuid);
}

QList<ActiveSession> SessionManager::activeSessions() const {
    return m_sessions.values();
}

void SessionManager::checkTimers() {
    qint64 now = QDateTime::currentSecsSinceEpoch();
    QStringList expired;

    for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd(); ++it) {
        if (it->lockType == "timer" && it->expiresAt > 0 && now >= it->expiresAt) {
            expired.append(it.key());
        }
    }

    for (const auto &id : expired) {
        QString triggerUuid = m_sessions[id].triggerUuid;
        removePersistedSession(id);
        m_sessions.remove(id);
        qInfo() << "Session timer expired:" << id;
        emit sessionEnded(id, triggerUuid);
    }
}

void SessionManager::persistSession(const ActiveSession &session) {
    QSqlQuery q;
    q.prepare("INSERT OR REPLACE INTO active_sessions "
              "(id, block_id, trigger_uuid, lock_type, lock_param, started_at, expires_at, password_hash) "
              "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    q.addBindValue(session.id);
    q.addBindValue(session.blockId);
    q.addBindValue(session.triggerUuid);
    q.addBindValue(session.lockType);
    q.addBindValue(session.lockParam);
    q.addBindValue(session.startedAt);
    q.addBindValue(session.expiresAt);
    q.addBindValue(session.passwordHash);
    if (!q.exec())
        qWarning() << "Failed to persist session:" << q.lastError().text();
}

void SessionManager::removePersistedSession(const QString &sessionId) {
    QSqlQuery q;
    q.prepare("DELETE FROM active_sessions WHERE id=?");
    q.addBindValue(sessionId);
    q.exec();
}

void SessionManager::resumePersistedSessions() {
    QSqlQuery q("SELECT id, block_id, trigger_uuid, lock_type, lock_param, started_at, expires_at, password_hash FROM active_sessions");
    qint64 now = QDateTime::currentSecsSinceEpoch();
    int count = 0;

    while (q.next()) {
        ActiveSession session;
        session.id = q.value(0).toString();
        session.blockId = q.value(1).toString();
        session.triggerUuid = q.value(2).toString();
        session.lockType = q.value(3).toString();
        session.lockParam = q.value(4).toString();
        session.startedAt = q.value(5).toLongLong();
        session.expiresAt = q.value(6).toLongLong();
        session.passwordHash = q.value(7).toString();

        // If timer-based session has already expired, remove it
        if (session.lockType == "timer" && session.expiresAt > 0 && now >= session.expiresAt) {
            removePersistedSession(session.id);
            continue;
        }

        m_sessions.insert(session.id, session);
        emit sessionStarted(session);
        ++count;
    }

    qInfo() << "Resumed" << count << "persisted sessions";
}

bool SessionManager::verifyPassword(const QString &input, const QString &hash) {
    return hashPassword(input) == hash;
}

QString SessionManager::hashPassword(const QString &password) {
    QByteArray salted = QByteArray("ulysses_salt_v1_") + password.toUtf8();
    return QCryptographicHash::hash(salted, QCryptographicHash::Sha256).toHex();
}

} // namespace Ulysses
