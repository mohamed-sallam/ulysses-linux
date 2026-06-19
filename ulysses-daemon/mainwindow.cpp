#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QStatusBar>
#include <QJsonDocument>
#include <QJsonArray>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_client = new Ulysses::DBusClient(this);
    setupUI();
    setupTrayIcon();

    if (!m_client->isConnected()) {
        statusBar()->showMessage("⚠ Daemon not running. Start ulyssesd first.", 0);
    } else {
        statusBar()->showMessage("Connected to daemon", 3000);
    }
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI() {
    setWindowTitle("Ulysses – Distraction Blocker");
    resize(1000, 700);

    QWidget *central = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(central);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Sidebar
    m_sideBar = new QListWidget(central);
    m_sideBar->setObjectName("SideBar");
    m_sideBar->setFixedWidth(200);
    m_sideBar->addItem("🏠 Home");
    m_sideBar->addItem("📋 Lists");
    m_sideBar->addItem("🧱 Blocks");
    m_sideBar->addItem("⚡ Triggers");
    m_sideBar->addItem("🌐 Network");
    mainLayout->addWidget(m_sideBar);

    // Stacked Widget for Tabs
    m_stack = new QStackedWidget(central);
    mainLayout->addWidget(m_stack);

    m_homeTab = new Ulysses::HomeTab(m_client);
    m_stack->addWidget(m_homeTab);

    m_listsTab = new Ulysses::ListsTab(m_client);
    m_stack->addWidget(m_listsTab);

    m_blocksTab = new Ulysses::BlocksTab(m_client);
    m_stack->addWidget(m_blocksTab);

    m_triggersTab = new Ulysses::TriggersTab(m_client);
    m_stack->addWidget(m_triggersTab);

    m_networkTab = new Ulysses::NetworkTab(m_client);
    m_stack->addWidget(m_networkTab);

    connect(m_sideBar, &QListWidget::currentRowChanged, m_stack, &QStackedWidget::setCurrentIndex);
    m_sideBar->setCurrentRow(0);

    setCentralWidget(central);
}

void MainWindow::setupTrayIcon() {
    m_trayMenu = new QMenu(this);
    m_trayMenu->addAction("Show Window", this, &QWidget::show);
    m_trayMenu->addSeparator();

    QAction *statusAction = m_trayMenu->addAction("Status: Checking...");
    statusAction->setEnabled(false);

    auto updateStatus = [this, statusAction]() {
        QString status = m_client->getActiveSessions();
        QJsonArray arr = QJsonDocument::fromJson(status.toUtf8()).array();
        if (arr.isEmpty())
            statusAction->setText("Status: No active blocks");
        else
            statusAction->setText(QString("Status: %1 active block(s)").arg(arr.size()));
    };

    QTimer *statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, updateStatus);
    statusTimer->start(5000);
    updateStatus();

    m_trayMenu->addSeparator();
    m_trayMenu->addAction("Quit", qApp, &QApplication::quit);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->setToolTip("Ulysses – Distraction Blocker");
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (isVisible()) hide();
            else show();
        }
    });
}
