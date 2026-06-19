#ifndef ULYSSES_WEB_BLOCKER_H
#define ULYSSES_WEB_BLOCKER_H

#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QDebug>

namespace Ulysses {

class WebBlocker : public QObject {
    Q_OBJECT

public:
    explicit WebBlocker(QObject *parent = nullptr);
    ~WebBlocker() override;

    void applyRules(const QStringList &denyDomains, const QStringList &denyUrls,
                    const QStringList &denyWildcards, const QStringList &allowDomains,
                    bool blockEntireInternet);
    void clearRules();

private:
    void addIptablesRule(const QString &rule);
    void removeIptablesRule(const QString &rule);
    void startDnsmasq(const QStringList &denyDomains, const QStringList &allowDomains, bool blockAll);
    void stopDnsmasq();
    int runCmd(const QString &cmd);

    QProcess *m_dnsmasqProcess;
    QStringList m_activeRules;
    bool m_active;
    QString m_dnsmasqConfPath;
};

} // namespace Ulysses

#endif // ULYSSES_WEB_BLOCKER_H
