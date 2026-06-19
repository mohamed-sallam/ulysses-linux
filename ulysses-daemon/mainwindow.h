#ifndef ULYSSES_MAINWINDOW_H
#define ULYSSES_MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QMenu>
#include <QSystemTrayIcon>

#include "gui/dbus_client.h"
#include "gui/home_tab.h"
#include "gui/lists_tab.h"
#include "gui/blocks_tab.h"
#include "gui/triggers_tab.h"
#include "gui/network_tab.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void setupUI();
    void setupTrayIcon();

    Ulysses::DBusClient *m_client;

    QListWidget *m_sideBar;
    QStackedWidget *m_stack;

    Ulysses::HomeTab *m_homeTab;
    Ulysses::ListsTab *m_listsTab;
    Ulysses::BlocksTab *m_blocksTab;
    Ulysses::TriggersTab *m_triggersTab;
    Ulysses::NetworkTab *m_networkTab;

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
};

#endif // ULYSSES_MAINWINDOW_H
