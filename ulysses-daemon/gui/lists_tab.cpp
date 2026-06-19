#include "lists_tab.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QComboBox>
#include <QListWidget>
#include <QFormLayout>

namespace Ulysses
{

ListsTab::ListsTab(DBusClient *client, QWidget *parent)
    : QWidget(parent), m_client(client)
{
    setupUI();
    connect(m_client, &DBusClient::listsChanged, this, &ListsTab::refreshLists);
    refreshLists();
}

void ListsTab::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_listTree = new QTreeWidget();
    m_listTree->setHeaderHidden(true);
    m_listTree->setIndentation(0);
    m_listTree->setColumnHidden(1, true);
    mainLayout->addWidget(m_listTree);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_addListBtn = new QPushButton("Add List");
    m_deleteListBtn = new QPushButton("Delete");
    btnLayout->addWidget(m_addListBtn);
    btnLayout->addWidget(m_deleteListBtn);
    mainLayout->addLayout(btnLayout);

    connect(m_addListBtn, &QPushButton::clicked, this, &ListsTab::addList);
    connect(m_deleteListBtn, &QPushButton::clicked, this, &ListsTab::deleteList);
    connect(m_listTree, &QTreeWidget::itemDoubleClicked, this, &ListsTab::editList);
}

void ListsTab::refreshLists()
{
    m_listTree->clear();
    QString json = m_client->getLists();
    QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();

    for (const auto &val : arr)
    {
        QJsonObject obj = val.toObject();
        QTreeWidgetItem *item = new QTreeWidgetItem(m_listTree);
        item->setText(1, obj["id"].toString());

        int entryCount = obj["entries"].toArray().size();
        item->setText(0, QString("%1 (%2 entries)").arg(obj["name"].toString().replace(QRegularExpression(" \\(\\d+ entries\\)"),
                      "")).arg(entryCount));
    }
}

void ListsTab::addList()
{
    openEditorDialog("", "");
}

void ListsTab::deleteList()
{
    QTreeWidgetItem *item = m_listTree->currentItem();
    if (!item) return;

    QString id = item->text(1);
    if (QMessageBox::question(this, "Delete List", "Delete this list?") == QMessageBox::Yes)
    {
        m_client->deleteList(id);
    }
}

void ListsTab::editList()
{
    QTreeWidgetItem *item = m_listTree->currentItem();
    if (!item) return;
    openEditorDialog(item->text(1), item->text(0).split(" (").first());
}

void ListsTab::openEditorDialog(const QString &id, const QString &name)
{
    QDialog dlg(this);
    dlg.setWindowTitle(id.isEmpty() ? "Add List" : "Edit List");
    dlg.resize(600, 500);

    QVBoxLayout *layout = new QVBoxLayout(&dlg);

    QFormLayout *formLayout = new QFormLayout();
    QLineEdit *nameEdit = new QLineEdit(name);
    nameEdit->setPlaceholderText("List Name");
    formLayout->addRow("List Name:", nameEdit);
    layout->addLayout(formLayout);

    QListWidget *entryList = new QListWidget();
    layout->addWidget(entryList);

    QHBoxLayout *entryInputLayout = new QHBoxLayout();
    QLineEdit *entryValueEdit = new QLineEdit();
    entryValueEdit->setPlaceholderText("Enter value...");
    QComboBox *entryTypeCombo = new QComboBox();
    entryTypeCombo->addItems({"deny", "allow"});
    QComboBox *ruleTypeCombo = new QComboBox();
    ruleTypeCombo->addItems({"domain", "url", "wildcard", "keyword", "app_name", "window_title", "entire_internet"});
    entryInputLayout->addWidget(entryTypeCombo);
    entryInputLayout->addWidget(ruleTypeCombo);
    entryInputLayout->addWidget(entryValueEdit, 1);
    layout->addLayout(entryInputLayout);

    QHBoxLayout *entryBtnLayout = new QHBoxLayout();
    QPushButton *addEntryBtn = new QPushButton("Add Entry");
    QPushButton *removeEntryBtn = new QPushButton("Remove Entry");
    entryBtnLayout->addWidget(addEntryBtn);
    entryBtnLayout->addWidget(removeEntryBtn);
    entryBtnLayout->addStretch();
    layout->addLayout(entryBtnLayout);

    // Load existing entries if editing
    if (!id.isEmpty()) {
        QString json = m_client->getLists();
        QJsonArray arr = QJsonDocument::fromJson(json.toUtf8()).array();
        for (const auto &val : arr) {
            QJsonObject obj = val.toObject();
            if (obj["id"].toString() == id) {
                QJsonArray entries = obj["entries"].toArray();
                for (const auto &e : entries) {
                    QJsonObject entry = e.toObject();
                    entryList->addItem(QString("[%1] %2 [%3]")
                                         .arg(entry["type"].toString(), entry["value"].toString(), entry["rule_type"].toString()));
                }
                break;
            }
        }
    }

    connect(addEntryBtn, &QPushButton::clicked, [&]() {
        QString value = entryValueEdit->text().trimmed();
        if (value.isEmpty()) return;
        QString type = entryTypeCombo->currentText();
        QString ruleType = ruleTypeCombo->currentText();
        entryList->addItem(QString("[%1] %2 [%3]").arg(type, value, ruleType));
        entryValueEdit->clear();
    });

    connect(removeEntryBtn, &QPushButton::clicked, [&]() {
        auto items = entryList->selectedItems();
        for (auto *item : items) delete item;
    });

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        if (nameEdit->text().isEmpty()) return; // Discard if empty

        QJsonObject obj;
        obj["name"] = nameEdit->text();
        QJsonArray entries;
        for (int i = 0; i < entryList->count(); ++i) {
            QJsonObject e;
            QString text = entryList->item(i)->text();
            int firstBracketEnd = text.indexOf("]");
            int lastBracketStart = text.lastIndexOf(" [");
            if (firstBracketEnd > 0 && lastBracketStart > firstBracketEnd) {
                e["type"] = text.mid(1, firstBracketEnd - 1);
                e["value"] = text.mid(firstBracketEnd + 2, lastBracketStart - firstBracketEnd - 2).trimmed();
                e["rule_type"] = text.mid(lastBracketStart + 2, text.length() - lastBracketStart - 3);
            } else {
                e["value"] = text;
                e["rule_type"] = "domain";
                e["type"] = "deny";
            }
            entries.append(e);
        }
        obj["entries"] = entries;

        if (id.isEmpty()) {
            m_client->createList(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        } else {
            obj["id"] = id;
            m_client->updateList(QJsonDocument(obj).toJson(QJsonDocument::Compact));
        }
    }
}

} // namespace Ulysses
