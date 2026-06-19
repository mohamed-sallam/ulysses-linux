#include "network_manager.h"
#include <QNetworkDatagram>
#include <QJsonDocument>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDebug>

namespace Ulysses {

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent), m_hasGroup(false)
{
    m_dht.run(4222, dht::crypto::generateIdentity(), true);
    m_dht.bootstrap("bootstrap.jami.net", "4222");

    connect(&m_presenceTimer, &QTimer::timeout, this, &NetworkManager::publishPresence);
    connect(&m_scanTimer, &QTimer::timeout, this, &NetworkManager::scanPeers);
    connect(&m_nonceCleanupTimer, &QTimer::timeout, this, &NetworkManager::cleanupStaleNonces);

    m_presenceTimer.start(60000);  // Announce presence every 60s
    m_scanTimer.start(30000);       // Scan for stale peers every 30s
    m_nonceCleanupTimer.start(120000); // Cleanup nonces every 2 min

    loadGroupFromDb();
}

NetworkManager::~NetworkManager() {
    cancelDhtListeners();
    m_dht.join();
}

// --- Group management ---

GroupInfo NetworkManager::createGroup() {
    m_group.groupUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_group.groupSecret = Crypto::generateKey();
    m_group.salt = Crypto::generateSalt();
    m_group.deviceId = Crypto::generateDeviceId();
    m_group.devicePrivKey = Crypto::generateKey(); // Placeholder

    m_hasGroup = true;
    saveGroupToDb();
    setupDhtListeners();
    publishPresence();
    emit groupChanged();

    qInfo() << "Created group:" << m_group.groupUuid << "device:" << m_group.deviceId;
    return m_group;
}

bool NetworkManager::joinGroup(const QString &groupUuid, const QByteArray &groupSecret, const QByteArray &salt) {
    m_group.groupUuid = groupUuid;
    m_group.groupSecret = groupSecret;
    m_group.salt = salt;
    m_group.deviceId = Crypto::generateDeviceId();
    m_group.devicePrivKey = Crypto::generateKey();

    m_hasGroup = true;
    saveGroupToDb();
    setupDhtListeners();
    publishPresence();
    emit groupChanged();

    qInfo() << "Joined group:" << groupUuid << "device:" << m_group.deviceId;
    return true;
}

bool NetworkManager::hasGroup() const { return m_hasGroup; }
GroupInfo NetworkManager::groupInfo() const { return m_group; }

void NetworkManager::leaveGroup() {
    cancelDhtListeners();
    m_group = {};
    m_hasGroup = false;
    m_peers.clear();

    QSqlQuery q;
    q.exec("DELETE FROM network_group");

    emit groupChanged();
    qInfo() << "Left group";
}

QList<PeerDevice> NetworkManager::peers() const {
    return m_peers.values();
}

// --- Key derivation ---

QByteArray NetworkManager::groupDiscoveryKey() const {
    return Crypto::deriveDhtKey("ulysses:group:", m_group.groupUuid, m_group.salt);
}

QByteArray NetworkManager::triggerSyncKey() const {
    return Crypto::deriveDhtKey("ulysses:triggers:", m_group.groupUuid, m_group.salt);
}

QByteArray NetworkManager::fireEventKey() const {
    return Crypto::deriveDhtKey("ulysses:fire:", m_group.groupUuid, m_group.salt);
}

// --- Message passing ---

void NetworkManager::sendEncryptedMessage(const QByteArray &dhtKey, const QJsonObject &message) {
    if (!m_hasGroup) return;

    QByteArray plaintext = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray nonce = Crypto::generateNonce();
    QByteArray encrypted = Crypto::encrypt(plaintext, m_group.groupSecret, nonce);

    // Build packet: [4-byte key prefix][encrypted data]
    QByteArray packet;
    packet.append(dhtKey.left(4)); // 4-byte channel identifier
    packet.append(encrypted);

    dht::InfoHash keyHash = dht::InfoHash::get(dhtKey.toStdString());
    m_dht.put(keyHash, dht::Value(std::vector<uint8_t>(packet.begin(), packet.end())));
}

void NetworkManager::setupDhtListeners() {
    if (!m_hasGroup) return;
    cancelDhtListeners();

    auto cb = [this](const std::vector<std::shared_ptr<dht::Value>>& values) {
        for (const auto& v : values) {
            QByteArray data(reinterpret_cast<const char*>(v->data.data()), v->data.size());
            QMetaObject::invokeMethod(this, "processMessage", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        }
        return true;
    };

    m_tokenGroup = m_dht.listen(dht::InfoHash::get(groupDiscoveryKey().toStdString()), cb).get();
    m_tokenSync = m_dht.listen(dht::InfoHash::get(triggerSyncKey().toStdString()), cb).get();
    m_tokenFire = m_dht.listen(dht::InfoHash::get(fireEventKey().toStdString()), cb).get();
}

void NetworkManager::cancelDhtListeners() {
    if (m_tokenGroup) m_dht.cancelListen(dht::InfoHash::get(groupDiscoveryKey().toStdString()), m_tokenGroup);
    if (m_tokenSync) m_dht.cancelListen(dht::InfoHash::get(triggerSyncKey().toStdString()), m_tokenSync);
    if (m_tokenFire) m_dht.cancelListen(dht::InfoHash::get(fireEventKey().toStdString()), m_tokenFire);
    m_tokenGroup = m_tokenSync = m_tokenFire = 0;
}

void NetworkManager::processMessage(const QByteArray &data) {
    if (!m_hasGroup || data.size() < 5) return;

    QByteArray channelPrefix = data.left(4);
    QByteArray encrypted = data.mid(4);

    QByteArray plaintext = Crypto::decrypt(encrypted, m_group.groupSecret);
    if (plaintext.isEmpty()) return; // Decryption failed (wrong group or tampered)

    QJsonObject msg = QJsonDocument::fromJson(plaintext).object();
    QString msgType = msg["type"].toString();

    // Nonce replay protection
    QByteArray nonce = QByteArray::fromHex(msg["nonce"].toString().toUtf8());
    if (m_seenNonces.contains(nonce)) return;
    m_seenNonces.insert(nonce);

    QString senderId = msg["device_id"].toString();
    if (senderId == m_group.deviceId) return; // Ignore own messages

    if (msgType == "presence") {
        PeerDevice peer;
        peer.deviceId = senderId;
        peer.lastSeen = QDateTime::currentSecsSinceEpoch();

        bool isNew = !m_peers.contains(senderId);
        m_peers[senderId] = peer;

        if (isNew) {
            qInfo() << "Peer discovered:" << senderId.left(16) << "...";
            emit peerDiscovered(senderId);
        }
    } else if (msgType == "trigger_sync") {
        QJsonObject triggerData = msg["trigger"].toObject();
        qInfo() << "Trigger sync received:" << triggerData["uuid"].toString();
        emit triggerReceived(triggerData);
    } else if (msgType == "fire") {
        QString triggerUuid = msg["trigger_uuid"].toString();
        qInfo() << "Fire event received for trigger:" << triggerUuid;
        emit fireEventReceived(triggerUuid);
    } else if (msgType == "disable") {
        QString triggerUuid = msg["trigger_uuid"].toString();
        qInfo() << "Disable event received for trigger:" << triggerUuid;
        emit disableEventReceived(triggerUuid);
    }
}

// --- Trigger sync (CRDT) ---

void NetworkManager::publishTrigger(const QJsonObject &triggerData) {
    QJsonObject msg;
    msg["type"] = "trigger_sync";
    msg["device_id"] = m_group.deviceId;
    msg["nonce"] = QString::fromUtf8(Crypto::generateNonce().toHex());
    msg["trigger"] = triggerData;
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    sendEncryptedMessage(triggerSyncKey(), msg);
}

void NetworkManager::publishTombstone(const QString &triggerUuid, quint64 lamportTs) {
    QJsonObject triggerData;
    triggerData["uuid"] = triggerUuid;
    triggerData["deleted"] = true;
    triggerData["lamport_ts"] = static_cast<qint64>(lamportTs);

    publishTrigger(triggerData);
}

// --- Fire/Disable events ---

void NetworkManager::publishFireEvent(const QString &triggerUuid, quint64 lamportTs) {
    QJsonObject msg;
    msg["type"] = "fire";
    msg["device_id"] = m_group.deviceId;
    msg["nonce"] = QString::fromUtf8(Crypto::generateNonce().toHex());
    msg["trigger_uuid"] = triggerUuid;
    msg["lamport_ts"] = static_cast<qint64>(lamportTs);
    msg["action"] = "fire";

    sendEncryptedMessage(fireEventKey(), msg);
    qInfo() << "Published fire event for trigger:" << triggerUuid;
}

void NetworkManager::publishDisableEvent(const QString &triggerUuid, quint64 lamportTs) {
    QJsonObject msg;
    msg["type"] = "disable";
    msg["device_id"] = m_group.deviceId;
    msg["nonce"] = QString::fromUtf8(Crypto::generateNonce().toHex());
    msg["trigger_uuid"] = triggerUuid;
    msg["lamport_ts"] = static_cast<qint64>(lamportTs);
    msg["action"] = "disable";

    sendEncryptedMessage(fireEventKey(), msg);
    qInfo() << "Published disable event for trigger:" << triggerUuid;
}

// --- Presence and peer management ---

void NetworkManager::publishPresence() {
    if (!m_hasGroup) return;

    QJsonObject msg;
    msg["type"] = "presence";
    msg["device_id"] = m_group.deviceId;
    msg["nonce"] = QString::fromUtf8(Crypto::generateNonce().toHex());
    msg["timestamp"] = QDateTime::currentSecsSinceEpoch();

    sendEncryptedMessage(groupDiscoveryKey(), msg);
}

void NetworkManager::scanPeers() {
    qint64 now = QDateTime::currentSecsSinceEpoch();
    QStringList stale;

    for (auto it = m_peers.constBegin(); it != m_peers.constEnd(); ++it) {
        if (now - it->lastSeen > 180) { // 3 minutes timeout
            stale.append(it.key());
        }
    }

    for (const auto &id : stale) {
        m_peers.remove(id);
        qInfo() << "Peer lost:" << id.left(16) << "...";
        emit peerLost(id);
    }
}

void NetworkManager::cleanupStaleNonces() {
    // Keep nonce set from growing unbounded
    if (m_seenNonces.size() > 10000)
        m_seenNonces.clear();
}

// --- Persistence ---

void NetworkManager::saveGroupToDb() {
    QSqlQuery q;
    q.exec("DELETE FROM network_group");

    q.prepare("INSERT INTO network_group (id, group_uuid, group_secret, device_id, device_private_key) "
              "VALUES (1, ?, ?, ?, ?)");
    q.addBindValue(m_group.groupUuid);
    q.addBindValue(QString::fromUtf8(m_group.groupSecret.toHex()));
    q.addBindValue(m_group.deviceId);
    q.addBindValue(QString::fromUtf8(m_group.devicePrivKey.toHex()));

    if (!q.exec())
        qWarning() << "Failed to save group info:" << q.lastError().text();
}

void NetworkManager::loadGroupFromDb() {
    QSqlQuery q("SELECT group_uuid, group_secret, device_id, device_private_key FROM network_group WHERE id=1");
    if (q.next()) {
        m_group.groupUuid = q.value(0).toString();
        m_group.groupSecret = QByteArray::fromHex(q.value(1).toString().toUtf8());
        m_group.deviceId = q.value(2).toString();
        m_group.devicePrivKey = QByteArray::fromHex(q.value(3).toString().toUtf8());
        m_group.salt = Crypto::deriveDhtKey("salt:", m_group.groupUuid, m_group.groupSecret).left(16);

        if (!m_group.groupUuid.isEmpty()) {
            m_hasGroup = true;
            setupDhtListeners();
            publishPresence();
            qInfo() << "Loaded group from DB:" << m_group.groupUuid;
        }
    }
}

} // namespace Ulysses
