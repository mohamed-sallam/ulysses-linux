#include "network_tab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QInputDialog>
#include <QClipboard>
#include <QApplication>
#include <QDateTime>

namespace Ulysses {

NetworkTab::NetworkTab(DBusClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setupUI();
    connect(&m_refreshTimer, &QTimer::timeout, this, &NetworkTab::refreshStatus);
    m_refreshTimer.start(10000);
    refreshStatus();
}

void NetworkTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Status section
    QGroupBox *statusGroup = new QGroupBox("Network Status");
    QFormLayout *statusLayout = new QFormLayout(statusGroup);

    m_statusLabel = new QLabel("Not connected");
    m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; }");
    statusLayout->addRow("Status:", m_statusLabel);

    m_groupUuidLabel = new QLabel("—");
    statusLayout->addRow("Group UUID:", m_groupUuidLabel);

    m_deviceIdLabel = new QLabel("—");
    statusLayout->addRow("Device ID:", m_deviceIdLabel);

    mainLayout->addWidget(statusGroup);

    // Group management buttons
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_createGroupBtn = new QPushButton("Create New Group");
    m_createGroupBtn->setStyleSheet("QPushButton { padding: 8px 16px; }");
    m_joinGroupBtn = new QPushButton("Join Group");
    m_joinGroupBtn->setStyleSheet("QPushButton { padding: 8px 16px; }");
    m_leaveGroupBtn = new QPushButton("Leave Group");
    m_leaveGroupBtn->setStyleSheet("QPushButton { padding: 8px 16px; color: #d32f2f; }");
    m_leaveGroupBtn->setEnabled(false);

    btnLayout->addWidget(m_createGroupBtn);
    btnLayout->addWidget(m_joinGroupBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_leaveGroupBtn);
    mainLayout->addLayout(btnLayout);

    // QR data display (text-based pairing info)
    m_qrGroup = new QGroupBox("Pairing Data (share with other devices)");
    QVBoxLayout *qrLayout = new QVBoxLayout(m_qrGroup);
    m_qrDataDisplay = new QTextEdit();
    m_qrDataDisplay->setReadOnly(true);
    m_qrDataDisplay->setMaximumHeight(80);
    m_qrDataDisplay->setPlaceholderText("Create or join a group to see pairing data");
    qrLayout->addWidget(m_qrDataDisplay);

    QPushButton *copyBtn = new QPushButton("Copy Pairing Data to Clipboard");
    connect(copyBtn, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_qrDataDisplay->toPlainText());
        QMessageBox::information(this, "Copied", "Pairing data copied to clipboard.");
    });
    qrLayout->addWidget(copyBtn);
    m_qrGroup->setVisible(false);
    mainLayout->addWidget(m_qrGroup);

    // Peers section
    QGroupBox *peersGroup = new QGroupBox("Paired Devices");
    QVBoxLayout *peersLayout = new QVBoxLayout(peersGroup);
    m_peersList = new QListWidget();
    peersLayout->addWidget(m_peersList);
    mainLayout->addWidget(peersGroup);

    mainLayout->addStretch();

    // Connections
    connect(m_createGroupBtn, &QPushButton::clicked, this, &NetworkTab::createGroup);
    connect(m_joinGroupBtn, &QPushButton::clicked, this, &NetworkTab::joinGroup);
    connect(m_leaveGroupBtn, &QPushButton::clicked, this, &NetworkTab::leaveGroup);
}

void NetworkTab::refreshStatus() {
    QString json = m_client->getNetworkStatus();
    QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();

    bool hasGroup = obj["has_group"].toBool();

    if (hasGroup) {
        m_statusLabel->setText("Connected to group");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #2e7d32; }");
        m_groupUuidLabel->setText(obj["group_uuid"].toString());
        m_deviceIdLabel->setText(obj["device_id"].toString().left(32) + "...");
        m_createGroupBtn->setEnabled(false);
        m_joinGroupBtn->setEnabled(false);
        m_leaveGroupBtn->setEnabled(true);
        m_qrGroup->setVisible(true);
    } else {
        m_statusLabel->setText("No group — create or join one");
        m_statusLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; color: #757575; }");
        m_groupUuidLabel->setText("—");
        m_deviceIdLabel->setText("—");
        m_createGroupBtn->setEnabled(true);
        m_joinGroupBtn->setEnabled(true);
        m_leaveGroupBtn->setEnabled(false);
        m_qrGroup->setVisible(false);
    }

    // Update peers list
    m_peersList->clear();
    QJsonArray peers = obj["peers"].toArray();
    for (const auto &p : peers) {
        QJsonObject po = p.toObject();
        QString deviceId = po["device_id"].toString();
        qint64 lastSeen = static_cast<qint64>(po["last_seen"].toDouble());
        QString timeStr = QDateTime::fromSecsSinceEpoch(lastSeen).toString("hh:mm:ss");
        m_peersList->addItem(QString("Device: %1... (last seen: %2)")
                             .arg(deviceId.left(16), timeStr));
    }

    if (peers.isEmpty() && hasGroup) {
        m_peersList->addItem("No peers discovered yet. Waiting for other devices...");
    }
}

void NetworkTab::createGroup() {
    QString json = m_client->createNetworkGroup();
    if (json.isEmpty()) {
        QMessageBox::warning(this, "Error", "Failed to create group. Is the daemon running?");
        return;
    }

    QJsonObject obj = QJsonDocument::fromJson(json.toUtf8()).object();
    QString pairingData = QString("%1|%2|%3")
        .arg(obj["group_uuid"].toString(),
             obj["group_secret"].toString(),
             obj["salt"].toString());

    m_qrDataDisplay->setText(pairingData);
    refreshStatus();

    QMessageBox::information(this, "Group Created",
        "Group created successfully!\n\n"
        "Share the pairing data with other devices to join this group.\n"
        "In a future release, this will be shown as a QR code.");
}

void NetworkTab::joinGroup() {
    bool ok;
    QString pairingData = QInputDialog::getText(this, "Join Group",
        "Paste pairing data (format: uuid|secret|salt):",
        QLineEdit::Normal, "", &ok);

    if (!ok || pairingData.isEmpty()) return;

    QStringList parts = pairingData.split('|');
    if (parts.size() != 3) {
        QMessageBox::warning(this, "Invalid Data",
            "Pairing data must be in format: uuid|secret|salt");
        return;
    }

    if (m_client->joinNetworkGroup(parts[0], parts[1], parts[2])) {
        refreshStatus();
        QMessageBox::information(this, "Joined Group", "Successfully joined group!");
    } else {
        QMessageBox::warning(this, "Error", "Failed to join group.");
    }
}

void NetworkTab::leaveGroup() {
    if (QMessageBox::question(this, "Leave Group",
            "Are you sure? You will need pairing data to rejoin.") == QMessageBox::Yes) {
        m_client->leaveNetworkGroup();
        m_qrDataDisplay->clear();
        refreshStatus();
    }
}

} // namespace Ulysses
