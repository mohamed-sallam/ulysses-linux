#ifndef ULYSSES_BLOCKS_TAB_H
#define ULYSSES_BLOCKS_TAB_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include "../gui/dbus_client.h"

namespace Ulysses {

class BlocksTab : public QWidget {
    Q_OBJECT

public:
    explicit BlocksTab(DBusClient *client, QWidget *parent = nullptr);

private slots:
    void refreshBlocks();
    void addBlock();
    void deleteBlock();
    void editBlock();
    void startSession();

private:
    void setupUI();
    void openEditorDialog(const QString &id);

    DBusClient *m_client;
    QListWidget *m_blockList;
    QPushButton *m_addBlockBtn;
    QPushButton *m_deleteBlockBtn;
    QPushButton *m_startSessionBtn;
};

} // namespace Ulysses

#endif // ULYSSES_BLOCKS_TAB_H
