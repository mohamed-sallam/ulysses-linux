#include "triggers_tab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFormLayout>
#include <QDialog>
#include <QDialogButtonBox>
#include <QSpinBox>

namespace Ulysses {

TriggersTab::TriggersTab(DBusClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setupUI();
    connect(m_client, &DBusClient::triggersChanged, this, &TriggersTab::refreshTriggers);
    connect(m_client, &DBusClient::blocksChanged, this, &TriggersTab::refreshBlocks);
    refreshBlocks();
    refreshTriggers();
}

void TriggersTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // --- Manual trigger section ---
    QGroupBox *manualGroup = new QGroupBox("Quick Block (Manual Trigger)");
    QHBoxLayout *manualLayout = new QHBoxLayout(manualGroup);
    manualLayout->addWidget(new QLabel("Block:"));
    m_manualBlockCombo = new QComboBox();
    manualLayout->addWidget(m_manualBlockCombo, 1);
    m_manualFireBtn = new QPushButton("🔥 Start Block Now");
    m_manualFireBtn->setStyleSheet("QPushButton { background-color: #d32f2f; color: white; font-weight: bold; padding: 8px 16px; }");
    manualLayout->addWidget(m_manualFireBtn);
    mainLayout->addWidget(manualGroup);

    connect(m_manualFireBtn, &QPushButton::clicked, this, &TriggersTab::fireManualBlock);

    // --- Named triggers ---
    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    // Left: trigger list
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->addWidget(new QLabel("Named Triggers:"));

    m_triggerList = new QListWidget();
    leftLayout->addWidget(m_triggerList);

    QHBoxLayout *triggerBtnLayout = new QHBoxLayout();
    m_addTriggerBtn = new QPushButton("Add Trigger");
    m_deleteTriggerBtn = new QPushButton("Delete");
    m_fireTriggerBtn = new QPushButton("🔥 Fire");
    m_fireTriggerBtn->setStyleSheet("QPushButton { background-color: #e65100; color: white; font-weight: bold; }");
    triggerBtnLayout->addWidget(m_addTriggerBtn);
    triggerBtnLayout->addWidget(m_deleteTriggerBtn);
    triggerBtnLayout->addStretch();
    triggerBtnLayout->addWidget(m_fireTriggerBtn);
    leftLayout->addLayout(triggerBtnLayout);

    // Right: editor
    m_editorGroup = new QGroupBox("Trigger Details");
    QFormLayout *formLayout = new QFormLayout(m_editorGroup);

    m_triggerNameEdit = new QLineEdit();
    m_triggerNameEdit->setReadOnly(true);
    formLayout->addRow("Name:", m_triggerNameEdit);

    m_blockCombo = new QComboBox();
    m_blockCombo->setEnabled(false);
    formLayout->addRow("Associated Block:", m_blockCombo);

    m_propagateCheck = new QCheckBox("Propagate to network (cross-device)");
    m_propagateCheck->setEnabled(false);
    formLayout->addRow("", m_propagateCheck);

    m_statusLabel = new QLabel("Select a trigger to view details");
    formLayout->addRow("Status:", m_statusLabel);

    m_editorGroup->setEnabled(false);

    splitter->addWidget(leftPanel);
    splitter->addWidget(m_editorGroup);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter);

    // Connections
    connect(m_addTriggerBtn, &QPushButton::clicked, this, &TriggersTab::addTrigger);
    connect(m_deleteTriggerBtn, &QPushButton::clicked, this, &TriggersTab::deleteTrigger);
    connect(m_fireTriggerBtn, &QPushButton::clicked, this, &TriggersTab::fireTrigger);

    connect(m_triggerList, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row < 0) {
            m_editorGroup->setEnabled(false);
            return;
        }
        m_editorGroup->setEnabled(true);

        QString data = m_triggerList->item(row)->data(Qt::UserRole).toString();
        QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();

        m_triggerNameEdit->setText(obj["name"].toString());
        m_propagateCheck->setChecked(obj["propagate_to_network"].toBool());

        // Find block in combo
        QString blockId = obj["block_id"].toString();
        for (int i = 0; i < m_blockIds.size(); ++i) {
            if (m_blockIds[i] == blockId) {
                m_blockCombo->setCurrentIndex(i);
                break;
            }
        }

        m_statusLabel->setText(QString("Lamport TS: %1").arg(obj["lamport_ts"].toInt()));
    });
}

void TriggersTab::refreshBlocks() {
    m_manualBlockCombo->clear();
    m_blockCombo->clear();
    m_blockIds.clear();

    QString json = m_client->getBlocks();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        m_manualBlockCombo->addItem(obj["name"].toString());
        m_blockCombo->addItem(obj["name"].toString());
        m_blockIds.append(obj["id"].toString());
    }
}

void TriggersTab::refreshTriggers() {
    m_triggerList->clear();
    QString json = m_client->getTriggers();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();

    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        QString label = obj["name"].toString();
        if (obj["propagate_to_network"].toBool())
            label += " 🌐";
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        m_triggerList->addItem(item);
    }
}

void TriggersTab::addTrigger() {
    if (m_blockIds.isEmpty()) {
        QMessageBox::warning(this, "No Blocks", "Create a block first before adding triggers.");
        return;
    }

    bool ok;
    QString name = QInputDialog::getText(this, "New Trigger", "Trigger name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) return;

    QDialog dlg(this);
    dlg.setWindowTitle("Configure Trigger");
    QFormLayout *form = new QFormLayout(&dlg);

    QComboBox *blockCombo = new QComboBox();
    for (int i = 0; i < m_blockIds.size(); ++i)
        blockCombo->addItem(m_blockCombo->itemText(i));
    form->addRow("Block:", blockCombo);

    QCheckBox *propagateCheck = new QCheckBox("Propagate to network");
    form->addRow("", propagateCheck);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QJsonObject obj;
    obj["name"] = name;
    obj["block_id"] = m_blockIds[blockCombo->currentIndex()];
    obj["propagate_to_network"] = propagateCheck->isChecked();

    m_client->createTrigger(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void TriggersTab::deleteTrigger() {
    int row = m_triggerList->currentRow();
    if (row < 0) return;

    QString data = m_triggerList->item(row)->data(Qt::UserRole).toString();
    QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();

    if (QMessageBox::question(this, "Delete Trigger",
            "Delete trigger \"" + obj["name"].toString() + "\"?") == QMessageBox::Yes) {
        m_client->deleteTrigger(obj["uuid"].toString());
    }
}

void TriggersTab::fireTrigger() {
    int row = m_triggerList->currentRow();
    if (row < 0) return;

    QString data = m_triggerList->item(row)->data(Qt::UserRole).toString();
    QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();

    if (QMessageBox::question(this, "Fire Trigger",
            "Fire trigger \"" + obj["name"].toString() + "\"?\nThis will start the associated block session.") == QMessageBox::Yes) {
        QString sessionId = m_client->fireTrigger(obj["uuid"].toString());
        if (!sessionId.isEmpty()) {
            QMessageBox::information(this, "Trigger Fired", "Session started: " + sessionId);
        } else {
            QMessageBox::warning(this, "Error", "Failed to fire trigger. Is the daemon running?");
        }
    }
}

void TriggersTab::fireManualBlock() {
    int idx = m_manualBlockCombo->currentIndex();
    if (idx < 0 || idx >= m_blockIds.size()) return;

    // Ask for lock override
    QDialog dlg(this);
    dlg.setWindowTitle("Start Block Session");
    QFormLayout *form = new QFormLayout(&dlg);

    QComboBox *lockType = new QComboBox();
    lockType->addItems({"timer", "password"});
    form->addRow("Lock Type:", lockType);

    QSpinBox *timer = new QSpinBox();
    timer->setRange(1, 480);
    timer->setValue(60);
    timer->setSuffix(" min");
    form->addRow("Duration:", timer);

    QLineEdit *pass = new QLineEdit();
    pass->setEchoMode(QLineEdit::Password);
    form->addRow("Password:", pass);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) return;

    QString lt = lockType->currentText();
    QString lp = (lt == "timer") ? QString::number(timer->value() * 60) : pass->text();

    QString sessionId = m_client->startSession(m_blockIds[idx], lt, lp);
    if (!sessionId.isEmpty()) {
        QMessageBox::information(this, "Session Started", "Block session started.\nSession ID: " + sessionId);
    } else {
        QMessageBox::warning(this, "Error", "Failed to start session. Is the daemon running?");
    }
}

} // namespace Ulysses
