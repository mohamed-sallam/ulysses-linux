#ifndef ULYSSES_LISTS_TAB_H
#define ULYSSES_LISTS_TAB_H

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include "../gui/dbus_client.h"

namespace Ulysses {

class ListsTab : public QWidget {
    Q_OBJECT

public:
    explicit ListsTab(DBusClient *client, QWidget *parent = nullptr);

private slots:
    void refreshLists();
    void addList();
    void deleteList();
    void editList();

private:
    void setupUI();
    void openEditorDialog(const QString &id, const QString &name);

    DBusClient *m_client;

    QTreeWidget *m_listTree;
    QPushButton *m_addListBtn;
    QPushButton *m_deleteListBtn;
};

} // namespace Ulysses

#endif // ULYSSES_LISTS_TAB_H
