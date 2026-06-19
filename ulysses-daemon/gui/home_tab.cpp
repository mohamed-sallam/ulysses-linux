#include "home_tab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QInputDialog>
#include <QTimer>

namespace Ulysses {

HomeTab::HomeTab(DBusClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setupUI();
    connect(m_client, &DBusClient::blocksChanged, this, &HomeTab::refreshBlocks);
    refreshBlocks();

    QTimer *t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &HomeTab::refreshSessions);
    t->start(2000);
    refreshSessions();
}

void HomeTab::setupUI() {
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(20);

    // Left side: Blocks
    QVBoxLayout *blocksLayout = new QVBoxLayout();
    QLabel *blocksLabel = new QLabel("Available Blocks");
    blocksLabel->setStyleSheet("font-size: 20px; font-weight: bold; margin-bottom: 10px;");
    blocksLayout->addWidget(blocksLabel);

    m_blocksList = new QListWidget();
    m_blocksList->setStyleSheet("QListWidget { font-size: 16px; border: 1px solid #ccc; border-radius: 8px; } QListWidget::item { padding: 10px; border-bottom: 1px solid #eee; } QListWidget::item:selected { background-color: #3b82f6; color: white; }");
    blocksLayout->addWidget(m_blocksList);

    QPushButton *startBtn = new QPushButton("▶ Start Selected Block");
    startBtn->setStyleSheet("QPushButton { background-color: #10b981; color: white; font-weight: bold; padding: 10px; border-radius: 6px; font-size: 14px; } QPushButton:hover { background-color: #059669; }");
    connect(startBtn, &QPushButton::clicked, this, &HomeTab::startBlock);
    blocksLayout->addWidget(startBtn);

    mainLayout->addLayout(blocksLayout, 1);

    // Right side: Active Sessions
    QVBoxLayout *sessionsLayout = new QVBoxLayout();
    QLabel *sessionsLabel = new QLabel("Active Sessions");
    sessionsLabel->setStyleSheet("font-size: 20px; font-weight: bold; margin-bottom: 10px;");
    sessionsLayout->addWidget(sessionsLabel);

    m_sessionsList = new QListWidget();
    m_sessionsList->setStyleSheet("QListWidget { font-size: 16px; border: 1px solid #ccc; border-radius: 8px; } QListWidget::item { padding: 10px; border-bottom: 1px solid #eee; } QListWidget::item:selected { background-color: #ef4444; color: white; }");
    sessionsLayout->addWidget(m_sessionsList);

    QPushButton *stopBtn = new QPushButton("⏹ Stop Selected Session");
    stopBtn->setStyleSheet("QPushButton { background-color: #ef4444; color: white; font-weight: bold; padding: 10px; border-radius: 6px; font-size: 14px; } QPushButton:hover { background-color: #dc2626; }");
    connect(stopBtn, &QPushButton::clicked, this, &HomeTab::stopSession);
    sessionsLayout->addWidget(stopBtn);

    mainLayout->addLayout(sessionsLayout, 1);
}

void HomeTab::refreshBlocks() {
    m_blocksList->clear();
    QString json = m_client->getBlocks();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();

    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        QListWidgetItem *item = new QListWidgetItem(obj["name"].toString());
        item->setData(Qt::UserRole, obj["id"].toString());
        m_blocksList->addItem(item);
    }
}

void HomeTab::refreshSessions() {
    m_sessionsList->clear();
    QString json = m_client->getActiveSessions();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();

    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        QString label = QString("Session %1\n(Block: %2)").arg(obj["id"].toString(), obj["block_id"].toString());
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, obj["id"].toString());
        m_sessionsList->addItem(item);
    }
}

void HomeTab::startBlock() {
    int row = m_blocksList->currentRow();
    if (row < 0) return;
    QString blockId = m_blocksList->item(row)->data(Qt::UserRole).toString();

    QString json = m_client->getBlocks();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
    QJsonObject blockObj;
    for (const auto &val : arr) {
        if (val.toObject()["id"].toString() == blockId) {
            blockObj = val.toObject();
            break;
        }
    }
    
    QString lt = blockObj["lock_type"].toString();
    QString lp = blockObj["lock_param"].toString();

    if (lt == "password") {
        bool ok;
        QString text = QInputDialog::getText(this, "Start Block", "Enter password for block:", QLineEdit::Password, "", &ok);
        if (ok && !text.isEmpty()) {
            lp = text;
        } else return;
    }

    QString sessionId = m_client->startSession(blockId, lt, lp);
    if (!sessionId.isEmpty()) {
        QMessageBox::information(this, "Session Started", "Block session started.\nSession ID: " + sessionId);
        refreshSessions();
    } else {
        QMessageBox::warning(this, "Error", "Failed to start session.");
    }
}

void HomeTab::stopSession() {
    int row = m_sessionsList->currentRow();
    if (row < 0) return;
    QString sessionId = m_sessionsList->item(row)->data(Qt::UserRole).toString();

    bool ok;
    QString pass = QInputDialog::getText(this, "Stop Session", "Enter password to stop session:", QLineEdit::Password, "", &ok);
    if (!ok) return;

    if (m_client->endSession(sessionId, pass)) {
        refreshSessions();
    } else {
        QMessageBox::warning(this, "Failed", "Could not stop session. Incorrect password?");
    }
}

} // namespace Ulysses
