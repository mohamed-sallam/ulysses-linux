#ifndef ULYSSES_SESSION_MANAGER_H
#define ULYSSES_SESSION_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QJsonObject>
#include <QMap>
#include <QDateTime>

namespace Ulysses {

struct ActiveSession {
    QString id;
    QString blockId;
    QString triggerUuid;
    QString lockType;    // "timer" or "password"
    QString lockParam;
    qint64 startedAt;
    qint64 expiresAt;    // 0 if password-locked
    QString passwordHash;
    QList<qint64> frozenPids;
};

class SessionManager : public QObject {
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);

    QString startSession(const QString &blockId, const QString &lockType,
                         const QString &lockParam, const QString &triggerUuid = "");
    bool endSession(const QString &sessionId, const QString &authToken);
    void forceEndSession(const QString &sessionId); // Bypass lock, used by network disable
    QList<ActiveSession> activeSessions() const;
    void resumePersistedSessions();

signals:
    void sessionStarted(const ActiveSession &session);
    void sessionEnded(const QString &sessionId, const QString &triggerUuid);

private slots:
    void checkTimers();

private:
    void persistSession(const ActiveSession &session);
    void removePersistedSession(const QString &sessionId);
    bool verifyPassword(const QString &input, const QString &hash);
    QString hashPassword(const QString &password);

    QMap<QString, ActiveSession> m_sessions;
    QTimer m_timerCheck;
};

} // namespace Ulysses

#endif // ULYSSES_SESSION_MANAGER_H
