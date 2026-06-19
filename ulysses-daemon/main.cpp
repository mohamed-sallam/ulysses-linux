#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));
    
    a.setStyleSheet(
        "QListWidget#SideBar { background-color: #2D323C; color: #E2E4E9; border: none; font-size: 16px; outline: none; }"
        "QListWidget#SideBar::item { padding: 15px; border-bottom: 1px solid #3A3F4B; }"
        "QListWidget#SideBar::item:selected { background-color: #3B82F6; color: white; border-left: 4px solid #93C5FD; }"
        "QListWidget#SideBar::item:hover:!selected { background-color: #3A3F4B; }"
    );
    
    MainWindow w;
    w.show();
    return a.exec();
}
