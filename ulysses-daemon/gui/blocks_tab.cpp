#include "blocks_tab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QSpinBox>
#include <QStackedWidget>
#include <QLineEdit>

namespace Ulysses {

BlocksTab::BlocksTab(DBusClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setupUI();
    connect(m_client, &DBusClient::blocksChanged, this, &BlocksTab::refreshBlocks);
    connect(m_client, &DBusClient::listsChanged, this, &BlocksTab::refreshBlocks);
    refreshBlocks();
}

void BlocksTab::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_blockList = new QListWidget();
    mainLayout->addWidget(m_blockList);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addBlockBtn = new QPushButton("Add Block");
    m_deleteBlockBtn = new QPushButton("Delete");
    m_startSessionBtn = new QPushButton("Start Session");
    m_startSessionBtn->setStyleSheet("QPushButton { background-color: #d32f2f; color: white; }");
    
    btnLayout->addWidget(m_addBlockBtn);
    btnLayout->addWidget(m_deleteBlockBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_startSessionBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_addBlockBtn, &QPushButton::clicked, this, &BlocksTab::addBlock);
    connect(m_deleteBlockBtn, &QPushButton::clicked, this, &BlocksTab::deleteBlock);
    connect(m_startSessionBtn, &QPushButton::clicked, this, &BlocksTab::startSession);
    
    connect(m_blockList, &QListWidget::itemDoubleClicked, this, &BlocksTab::editBlock);
}

void BlocksTab::refreshBlocks() {
    m_blockList->clear();
    QString json = m_client->getBlocks();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();

    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        QListWidgetItem *item = new QListWidgetItem(obj["name"].toString());
        item->setData(Qt::UserRole, QJsonDocument(obj).toJson(QJsonDocument::Compact));
        m_blockList->addItem(item);
    }
}

void BlocksTab::addBlock() {
    openEditorDialog("");
}

void BlocksTab::deleteBlock() {
    int row = m_blockList->currentRow();
    if (row < 0) return;

    QString data = m_blockList->item(row)->data(Qt::UserRole).toString();
    QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();

    if (QMessageBox::question(this, "Delete Block", "Delete this block?") == QMessageBox::Yes) {
        m_client->deleteBlock(obj["id"].toString());
    }
}

void BlocksTab::editBlock() {
    int row = m_blockList->currentRow();
    if (row < 0) return;

    QString data = m_blockList->item(row)->data(Qt::UserRole).toString();
    QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();
    openEditorDialog(obj["id"].toString());
}

void BlocksTab::openEditorDialog(const QString &id) {
    QDialog dlg(this);
    dlg.setWindowTitle(id.isEmpty() ? "Add Block" : "Edit Block");
    dlg.resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QFormLayout *formLayout = new QFormLayout();
    QLineEdit *nameEdit = new QLineEdit();
    formLayout->addRow("Name:", nameEdit);

    QComboBox *lockTypeCombo = new QComboBox();
    lockTypeCombo->addItems({"timer", "password"});
    formLayout->addRow("Lock Type:", lockTypeCombo);

    QStackedWidget *lockStack = new QStackedWidget();
    QWidget *timerWidget = new QWidget();
    QHBoxLayout *timerLayout = new QHBoxLayout(timerWidget);
    timerLayout->setContentsMargins(0,0,0,0);
    QSpinBox *timerSpinBox = new QSpinBox();
    timerSpinBox->setRange(1, 480);
    timerSpinBox->setValue(60);
    timerSpinBox->setSuffix(" min");
    timerLayout->addWidget(new QLabel("Duration:"));
    timerLayout->addWidget(timerSpinBox);
    timerLayout->addStretch();

    QWidget *passWidget = new QWidget();
    QHBoxLayout *passLayout = new QHBoxLayout(passWidget);
    passLayout->setContentsMargins(0,0,0,0);
    QLineEdit *passwordEdit = new QLineEdit();
    passwordEdit->setEchoMode(QLineEdit::Password);
    passwordEdit->setPlaceholderText("Enter password...");
    passLayout->addWidget(new QLabel("Password:"));
    passLayout->addWidget(passwordEdit);

    lockStack->addWidget(timerWidget);
    lockStack->addWidget(passWidget);
    formLayout->addRow("", lockStack);
    layout->addLayout(formLayout);

    connect(lockTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            lockStack, &QStackedWidget::setCurrentIndex);

    layout->addWidget(new QLabel("Assigned Lists:"));

    QHBoxLayout *listAssignLayout = new QHBoxLayout();
    QVBoxLayout *availLayout = new QVBoxLayout();
    availLayout->addWidget(new QLabel("Available:"));
    QListWidget *availableListsWidget = new QListWidget();
    availLayout->addWidget(availableListsWidget);

    QVBoxLayout *arrowLayout = new QVBoxLayout();
    arrowLayout->addStretch();
    QPushButton *assignListBtn = new QPushButton("→");
    assignListBtn->setObjectName("assignBtn");
    QPushButton *unassignListBtn = new QPushButton("←");
    unassignListBtn->setObjectName("unassignBtn");
    arrowLayout->addWidget(assignListBtn);
    arrowLayout->addWidget(unassignListBtn);
    arrowLayout->addStretch();

    QVBoxLayout *assignedLayout = new QVBoxLayout();
    assignedLayout->addWidget(new QLabel("Assigned:"));
    QListWidget *assignedListsWidget = new QListWidget();
    assignedLayout->addWidget(assignedListsWidget);

    listAssignLayout->addLayout(availLayout);
    listAssignLayout->addLayout(arrowLayout);
    listAssignLayout->addLayout(assignedLayout);
    layout->addLayout(listAssignLayout);

    connect(assignListBtn, &QPushButton::clicked, [&]() {
        auto item = availableListsWidget->currentItem();
        if (item) assignedListsWidget->addItem(availableListsWidget->takeItem(availableListsWidget->row(item)));
    });

    connect(unassignListBtn, &QPushButton::clicked, [&]() {
        auto item = assignedListsWidget->currentItem();
        if (item) availableListsWidget->addItem(assignedListsWidget->takeItem(assignedListsWidget->row(item)));
    });

    // Load data
    QSet<QString> assignedSet;
    if (!id.isEmpty()) {
        QString json = m_client->getBlocks();
        QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            if (obj["id"].toString() == id) {
                nameEdit->setText(obj["name"].toString());
                QString lockType = obj["lock_type"].toString();
                lockTypeCombo->setCurrentText(lockType);
                if (lockType == "timer") {
                    timerSpinBox->setValue(obj["lock_param"].toString().toInt() / 60);
                }
                QJsonArray assignedIds = obj["list_ids"].toArray();
                for (const auto &v : assignedIds) assignedSet.insert(v.toString());
                break;
            }
        }
    }

    QString listsJson = m_client->getLists();
    QJsonArray allLists = QJsonDocument::fromJson(listsJson.toUtf8()).array();
    for (const auto &v : allLists) {
        QJsonObject lo = v.toObject();
        QString lid = lo["id"].toString();
        QString label = lo["name"].toString();
        QListWidgetItem *item = new QListWidgetItem(label);
        item->setData(Qt::UserRole, lid);
        if (assignedSet.contains(lid))
            assignedListsWidget->addItem(item);
        else
            availableListsWidget->addItem(item);
    }

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (nameEdit->text().isEmpty()) return;

        QJsonObject obj;
        obj["name"] = nameEdit->text();
        obj["lock_type"] = lockTypeCombo->currentText();
        if (lockTypeCombo->currentText() == "timer")
            obj["lock_param"] = QString::number(timerSpinBox->value() * 60);
        else
            obj["lock_param"] = passwordEdit->text();

        QJsonArray listIds;
        for (int i = 0; i < assignedListsWidget->count(); ++i)
            listIds.append(assignedListsWidget->item(i)->data(Qt::UserRole).toString());
        obj["list_ids"] = listIds;

        if (id.isEmpty()) {
            m_client->createBlock(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        } else {
            obj["id"] = id;
            m_client->updateBlock(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        }
    }
}

void BlocksTab::startSession() {
    int row = m_blockList->currentRow();
    if (row < 0) return;
    QString data = m_blockList->item(row)->data(Qt::UserRole).toString();
    QJsonObject obj = QJsonDocument::fromJson(data.toUtf8()).object();
    QString blockId = obj["id"].toString();

    QDialog dlg(this);
    dlg.setWindowTitle("Start Block Session");
    QFormLayout *form = new QFormLayout(&dlg);

    QComboBox *lockType = new QComboBox();
    lockType->addItems({"timer", "password"});
    lockType->setCurrentText(obj["lock_type"].toString());
    form->addRow("Lock Type:", lockType);

    QSpinBox *timer = new QSpinBox();
    timer->setRange(1, 480);
    int defaultTime = (obj["lock_type"].toString() == "timer") ? obj["lock_param"].toString().toInt() / 60 : 60;
    timer->setValue(defaultTime);
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

    QString sessionId = m_client->startSession(blockId, lt, lp);
    if (!sessionId.isEmpty()) {
        QMessageBox::information(this, "Session Started",
                                 "Block session started.\nSession ID: " + sessionId);
    } else {
        QMessageBox::warning(this, "Error", "Failed to start session. Is the daemon running?");
    }
}

} // namespace Ulysses
