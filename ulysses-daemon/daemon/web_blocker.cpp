#include "web_blocker.h"
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

namespace Ulysses {

WebBlocker::WebBlocker(QObject *parent)
    : QObject(parent), m_dnsmasqProcess(nullptr), m_active(false)
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                        + "/ulysses";
    QDir().mkpath(configDir);
    m_dnsmasqConfPath = configDir + "/dnsmasq.conf";
}

WebBlocker::~WebBlocker() {
    clearRules();
}

int WebBlocker::runCmd(const QString &cmd) {
    // iptables requires euid 0 even if we have CAP_NET_ADMIN
    QString fullCmd = cmd;
    if (cmd.startsWith("iptables ")) {
        fullCmd = "sudo " + cmd;
    }
    return QProcess::execute("/bin/sh", {"-c", fullCmd});
}

void WebBlocker::addIptablesRule(const QString &rule) {
    if (runCmd("iptables " + rule) == 0) {
        m_activeRules.append(rule);
    } else {
        qWarning() << "Failed to add iptables rule:" << rule;
    }
}

void WebBlocker::removeIptablesRule(const QString &rule) {
    QString deleteRule = rule;
    deleteRule.replace("-A ", "-D ");
    deleteRule.replace("-I ", "-D ");
    runCmd("iptables " + deleteRule);
}

void WebBlocker::startDnsmasq(const QStringList &denyDomains,
                               const QStringList &allowDomains,
                               bool blockAll) {
    stopDnsmasq();

    // Generate dnsmasq config
    QFile conf(m_dnsmasqConfPath);
    if (!conf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "Cannot write dnsmasq config";
        return;
    }

    QTextStream out(&conf);
    out << "# Ulysses auto-generated dnsmasq config\n";
    out << "port=5354\n";
    out << "no-resolv\n";
    out << "server=8.8.8.8\n";
    out << "server=1.1.1.1\n";

    if (blockAll) {
        // Block everything, then allow exceptions
        out << "address=/#/127.0.0.1\n";
        for (const auto &domain : allowDomains) {
            out << "server=/" << domain << "/8.8.8.8\n";
        }
    } else {
        // Block specific domains
        for (const auto &domain : denyDomains) {
            out << "address=/" << domain << "/127.0.0.1\n";
        }
    }

    conf.close();

    // Start dnsmasq
    m_dnsmasqProcess = new QProcess(this);
    m_dnsmasqProcess->start("dnsmasq", {"--no-daemon", "-C", m_dnsmasqConfPath});

    if (!m_dnsmasqProcess->waitForStarted(3000)) {
        qWarning() << "Failed to start dnsmasq";
        delete m_dnsmasqProcess;
        m_dnsmasqProcess = nullptr;
        return;
    }

    qInfo() << "dnsmasq started with config:" << m_dnsmasqConfPath;
}

void WebBlocker::stopDnsmasq() {
    if (m_dnsmasqProcess) {
        m_dnsmasqProcess->terminate();
        m_dnsmasqProcess->waitForFinished(3000);
        if (m_dnsmasqProcess->state() != QProcess::NotRunning)
            m_dnsmasqProcess->kill();
        delete m_dnsmasqProcess;
        m_dnsmasqProcess = nullptr;
    }
}

void WebBlocker::applyRules(const QStringList &denyDomains, const QStringList &denyUrls,
                             const QStringList &denyWildcards, const QStringList &allowDomains,
                             bool blockEntireInternet) {
    clearRules();
    m_active = true;

    // Start dnsmasq for DNS-level blocking
    startDnsmasq(denyDomains, allowDomains, blockEntireInternet);

    // Redirect DNS queries to our local dnsmasq
    addIptablesRule("-t nat -I OUTPUT -p udp --dport 53 -j REDIRECT --to-port 5354");
    addIptablesRule("-t nat -I OUTPUT -p tcp --dport 53 -j REDIRECT --to-port 5354");

    if (blockEntireInternet) {
        // Block all HTTP/HTTPS except allowed domains
        addIptablesRule("-I OUTPUT -p tcp --dport 80 -j DROP");
        addIptablesRule("-I OUTPUT -p tcp --dport 443 -j DROP");

        // Allow exceptions
        for (const auto &domain : allowDomains) {
            addIptablesRule(QString("-I OUTPUT -p tcp --dport 80 -m string --algo bm --string \"%1\" -j ACCEPT").arg(domain));
            addIptablesRule(QString("-I OUTPUT -p tcp --dport 443 -m string --algo bm --string \"%1\" -j ACCEPT").arg(domain));
        }
    }

    // URL-based blocking using string matching
    for (const auto &url : denyUrls) {
        addIptablesRule(QString("-I OUTPUT -p tcp --dport 80 -m string --algo bm --string \"%1\" -j DROP").arg(url));
        addIptablesRule(QString("-I OUTPUT -p tcp --dport 443 -m string --algo bm --string \"%1\" -j DROP").arg(url));
    }

    // Wildcard patterns
    for (const auto &pattern : denyWildcards) {
        QString regexPattern = pattern;
        regexPattern.replace("*", ".*");
        addIptablesRule(QString("-I OUTPUT -p tcp --dport 80 -m string --algo bm --string \"%1\" -j DROP").arg(pattern.split("*").first()));
        addIptablesRule(QString("-I OUTPUT -p tcp --dport 443 -m string --algo bm --string \"%1\" -j DROP").arg(pattern.split("*").first()));
    }

    qInfo() << "Web blocking active:" << m_activeRules.size() << "rules";
}

void WebBlocker::clearRules() {
    if (!m_active) return;

    for (const auto &rule : m_activeRules) {
        removeIptablesRule(rule);
    }
    m_activeRules.clear();

    stopDnsmasq();
    m_active = false;

    qInfo() << "Web blocking rules cleared";
}

} // namespace Ulysses
