#include "enforcement_manager.h"
#include <QSqlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

namespace Ulysses {

EnforcementManager::EnforcementManager(QObject *parent)
    : QObject(parent)
{
}

EnforcementManager::~EnforcementManager() {
    for (auto it = m_enforcements.begin(); it != m_enforcements.end(); ++it) {
        it->webBlocker->clearRules();
        it->appBlocker->stopBlocking();
        delete it->webBlocker;
        delete it->appBlocker;
    }
}

void EnforcementManager::collectBlockRules(const QString &blockId,
                                            QStringList &denyDomains, QStringList &denyUrls,
                                            QStringList &denyWildcards, QStringList &allowDomains,
                                            bool &blockEntireInternet,
                                            QStringList &blockedApps, QStringList &blockedTitles)
{
    blockEntireInternet = false;

    // Get all lists associated with this block
    QSqlQuery blq;
    blq.prepare("SELECT list_id FROM block_lists WHERE block_id=?");
    blq.addBindValue(blockId);
    if (!blq.exec()) return;

    while (blq.next()) {
        QString listId = blq.value(0).toString();

        // Get entries
        QSqlQuery eq;
        eq.prepare("SELECT type, value, rule_type FROM list_entries WHERE list_id=?");
        eq.addBindValue(listId);
        if (!eq.exec()) continue;

        while (eq.next()) {
            QString listType = eq.value(0).toString();
            QString value = eq.value(1).toString();
            QString ruleType = eq.value(2).toString();

            QString listCategory = (ruleType == "app_name" || ruleType == "window_title") ? "app" : "web";

            if (listCategory == "web") {
                if (listType == "deny") {
                    if (ruleType == "entire_internet" || value == ".") {
                        blockEntireInternet = true;
                    } else if (ruleType == "domain") {
                        denyDomains.append(value);
                    } else if (ruleType == "url") {
                        denyUrls.append(value);
                    } else if (ruleType == "wildcard" || ruleType == "keyword") {
                        denyWildcards.append(value);
                    }
                } else { // allow
                    allowDomains.append(value);
                }
            } else { // app
                if (listType == "deny") {
                    if (ruleType == "app_name") {
                        blockedApps.append(value);
                    } else if (ruleType == "window_title") {
                        blockedTitles.append(value);
                    }
                }
            }
        }
    }
}

void EnforcementManager::enforceSession(const ActiveSession &session) {
    if (m_enforcements.contains(session.id)) return; // Already enforcing

    QStringList denyDomains, denyUrls, denyWildcards, allowDomains;
    QStringList blockedApps, blockedTitles;
    bool blockEntireInternet = false;

    collectBlockRules(session.blockId, denyDomains, denyUrls, denyWildcards,
                      allowDomains, blockEntireInternet, blockedApps, blockedTitles);

    SessionEnforcement enf;
    enf.webBlocker = new WebBlocker(this);
    enf.appBlocker = new AppBlocker(this);

    // Apply web blocking if there are web rules
    if (!denyDomains.isEmpty() || !denyUrls.isEmpty() || !denyWildcards.isEmpty() || blockEntireInternet) {
        enf.webBlocker->applyRules(denyDomains, denyUrls, denyWildcards, allowDomains, blockEntireInternet);
    }

    // Apply app blocking if there are app rules
    if (!blockedApps.isEmpty() || !blockedTitles.isEmpty()) {
        enf.appBlocker->setBlockedApps(blockedApps);
        enf.appBlocker->setBlockedWindowTitles(blockedTitles);
        enf.appBlocker->startBlocking();
    }

    m_enforcements.insert(session.id, enf);
    qInfo() << "Enforcement started for session:" << session.id
            << "web_deny:" << denyDomains.size()
            << "apps:" << blockedApps.size()
            << "titles:" << blockedTitles.size();
}

void EnforcementManager::releaseSession(const QString &sessionId) {
    if (!m_enforcements.contains(sessionId)) return;

    auto &enf = m_enforcements[sessionId];
    enf.webBlocker->clearRules();
    enf.appBlocker->stopBlocking();
    delete enf.webBlocker;
    delete enf.appBlocker;
    m_enforcements.remove(sessionId);

    qInfo() << "Enforcement released for session:" << sessionId;
}

} // namespace Ulysses
