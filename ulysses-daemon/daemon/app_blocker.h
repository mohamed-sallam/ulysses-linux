#ifndef ULYSSES_APP_BLOCKER_H
#define ULYSSES_APP_BLOCKER_H

#include <QObject>
#include <QTimer>
#include <QStringList>
#include <QList>
#include <QRegularExpression>
#include <sys/types.h>

namespace Ulysses {

struct BlockedProcess {
    pid_t pid;
    QString name;
    bool frozen;
};

class AppBlocker : public QObject {
    Q_OBJECT

public:
    explicit AppBlocker(QObject *parent = nullptr);
    ~AppBlocker() override;

    void setBlockedApps(const QStringList &appNames);
    void setBlockedWindowTitles(const QStringList &titlePatterns);
    void startBlocking();
    void stopBlocking();
    QList<pid_t> frozenPids() const;

private slots:
    void scanAndFreeze();

private:
    void freezeProcess(pid_t pid, const QString &name);
    void unfreezeProcess(pid_t pid);
    void setupCgroupFreezer();
    QList<pid_t> findProcessesByName(const QString &name);
    QList<pid_t> findProcessesByWindowTitle(const QString &pattern);

    QStringList m_blockedApps;
    QStringList m_blockedTitles;
    QList<BlockedProcess> m_frozenProcesses;
    QTimer m_scanTimer;
    bool m_active;
    QString m_cgroupPath;
};

} // namespace Ulysses

#endif // ULYSSES_APP_BLOCKER_H
