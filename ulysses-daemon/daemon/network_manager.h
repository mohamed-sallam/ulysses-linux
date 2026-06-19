#ifndef ULYSSES_NETWORK_MANAGER_H
#define ULYSSES_NETWORK_MANAGER_H

#include <QObject>
#include <QTimer>
#include <opendht.h>
#include <future>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QSet>
#include "../common/crypto.h"

namespace Ulysses {

struct GroupInfo {
    QString groupUuid;
    QByteArray groupSecret;  // 32-byte key
    QByteArray salt;          // 16-byte salt
    QString deviceId;
    QByteArray devicePrivKey; // Not used without libsodium, placeholder
};

struct PeerDevice {
    QString deviceId;
    qint64 lastSeen;
};

class NetworkManager : public QObject {
    Q_OBJECT

public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager() override;

    // Group management
    GroupInfo createGroup();
    bool joinGroup(const QString &groupUuid, const QByteArray &groupSecret, const QByteArray &salt);
    bool hasGroup() const;
    GroupInfo groupInfo() const;
    void leaveGroup();

    // Device discovery
    QList<PeerDevice> peers() const;

    // Trigger sync (CRDT)
    void publishTrigger(const QJsonObject &triggerData);
    void publishTombstone(const QString &triggerUuid, quint64 lamportTs);

    // Fire/Disable events
    void publishFireEvent(const QString &triggerUuid, quint64 lamportTs);
    void publishDisableEvent(const QString &triggerUuid, quint64 lamportTs);

    // Persistence
    void saveGroupToDb();
    void loadGroupFromDb();

signals:
    void peerDiscovered(const QString &deviceId);
    void peerLost(const QString &deviceId);
    void triggerReceived(const QJsonObject &triggerData);
    void fireEventReceived(const QString &triggerUuid);
    void disableEventReceived(const QString &triggerUuid);
    void groupChanged();

private slots:
    void processMessage(const QByteArray &data);
    void publishPresence();
    void scanPeers();
    void cleanupStaleNonces();

private:
    void sendEncryptedMessage(const QByteArray &dhtKey, const QJsonObject &message);
    void setupDhtListeners();
    void cancelDhtListeners();
    QByteArray groupDiscoveryKey() const;
    QByteArray triggerSyncKey() const;
    QByteArray fireEventKey() const;

    GroupInfo m_group;
    bool m_hasGroup;

    dht::DhtRunner m_dht;
    size_t m_tokenGroup = 0;
    size_t m_tokenSync = 0;
    size_t m_tokenFire = 0;

    QMap<QString, PeerDevice> m_peers;
    QSet<QByteArray> m_seenNonces; // Replay protection
    QTimer m_presenceTimer;
    QTimer m_scanTimer;
    QTimer m_nonceCleanupTimer;
};

} // namespace Ulysses

#endif // ULYSSES_NETWORK_MANAGER_H
