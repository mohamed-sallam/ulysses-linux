#include "sessions_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QMessageBox>

namespace Ulysses {

SessionsWidget::SessionsWidget(DBusClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setupUI();
    connect(m_client, &DBusClient::sessionStarted, this, [this](const QString &) { refreshSessions(); });
    connect(m_client, &DBusClient::sessionEnded, this, [this](const QString &) { refreshSessions(); });
    connect(&m_refreshTimer, &QTimer::timeout, this, &SessionsWidget::refreshSessions);
    m_refreshTimer.start(5000);
    refreshSessions();
}

void SessionsWidget::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_countLabel = new QLabel("Active Sessions: 0");
    m_countLabel->setStyleSheet("QLabel { font-weight: bold; font-size: 14px; }");
    layout->addWidget(m_countLabel);

    m_sessionList = new QListWidget();
    layout->addWidget(m_sessionList);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    m_authTokenEdit = new QLineEdit();
    m_authTokenEdit->setPlaceholderText("Password (for password-locked sessions)");
    m_authTokenEdit->setEchoMode(QLineEdit::Password);
    actionLayout->addWidget(m_authTokenEdit);

    m_endSessionBtn = new QPushButton("End Session");
    m_endSessionBtn->setStyleSheet("QPushButton { background-color: #2e7d32; color: white; font-weight: bold; padding: 6px 16px; }");
    actionLayout->addWidget(m_endSessionBtn);
    layout->addLayout(actionLayout);

    connect(m_endSessionBtn, &QPushButton::clicked, this, &SessionsWidget::endSession);
}

void SessionsWidget::refreshSessions() {
    m_sessionList->clear();
    QString json = m_client->getActiveSessions();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();

    m_countLabel->setText(QString("Active Sessions: %1").arg(arr.size()));

    qint64 now = QDateTime::currentSecsSinceEpoch();

    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        QString lockType = obj["lock_type"].toString();
        QString label = QString("Block: %1 | Lock: %2").arg(obj["block_id"].toString().left(8), lockType);

        if (lockType == "timer") {
            qint64 expiresAt = static_cast<qint64>(obj["expires_at"].toDouble());
            qint64 remaining = expiresAt - now;
            if (remaining > 0) {
                int hours = remaining / 3600;
                int mins = (remaining % 3600) / 60;
                int secs = remaining % 60;
                label += QString(" | Remaining: %1:%2:%3")
                    .arg(hours, 2, 10, QChar('0'))
                    .arg(mins, 2, 10, QChar('0'))
                    .arg(secs, 2, 10, QChar('0'));
            } else {
                label += " | Expired (will end soon)";
            }
        } else {
            label += " | Password required to end";
        }

        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, obj["id"].toString());
        m_sessionList->addItem(item);
    }
}

void SessionsWidget::endSession() {
    int row = m_sessionList->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Select a session to end.");
        return;
    }

    QString sessionId = m_sessionList->item(row)->data(Qt::UserRole).toString();
    QString authToken = m_authTokenEdit->text();

    if (m_client->endSession(sessionId, authToken)) {
        QMessageBox::information(this, "Session Ended", "The block session has been ended.");
        m_authTokenEdit->clear();
    } else {
        QMessageBox::warning(this, "Cannot End Session",
            "The session could not be ended.\n"
            "For timer-locked sessions, wait for the timer to expire.\n"
            "For password-locked sessions, enter the correct password.");
    }
}

} // namespace Ulysses
