#ifndef ULYSSES_ENFORCEMENT_MANAGER_H
#define ULYSSES_ENFORCEMENT_MANAGER_H

#include <QObject>
#include <QMap>
#include "web_blocker.h"
#include "app_blocker.h"
#include "session_manager.h"

namespace Ulysses {

class EnforcementManager : public QObject {
    Q_OBJECT

public:
    explicit EnforcementManager(QObject *parent = nullptr);
    ~EnforcementManager() override;

    void enforceSession(const ActiveSession &session);
    void releaseSession(const QString &sessionId);

private:
    struct SessionEnforcement {
        WebBlocker *webBlocker;
        AppBlocker *appBlocker;
    };

    void collectBlockRules(const QString &blockId,
                           QStringList &denyDomains, QStringList &denyUrls,
                           QStringList &denyWildcards, QStringList &allowDomains,
                           bool &blockEntireInternet,
                           QStringList &blockedApps, QStringList &blockedTitles);

    QMap<QString, SessionEnforcement> m_enforcements;
};

} // namespace Ulysses

#endif // ULYSSES_ENFORCEMENT_MANAGER_H
