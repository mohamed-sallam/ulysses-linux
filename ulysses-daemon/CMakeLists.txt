cmake_minimum_required(VERSION 3.5)

project(Ulysses VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets DBus Sql Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets DBus Sql Network)

find_package(PkgConfig REQUIRED)
pkg_check_modules(OPENDHT REQUIRED opendht)

# ---- Daemon (ulyssesd) ----
set(DAEMON_SOURCES
    daemon/main_daemon.cpp
    daemon/daemon.cpp
    daemon/daemon.h
    daemon/hardening.cpp
    daemon/hardening.h
    daemon/session_manager.cpp
    daemon/session_manager.h
    daemon/web_blocker.cpp
    daemon/web_blocker.h
    daemon/app_blocker.cpp
    daemon/app_blocker.h
    daemon/enforcement_manager.cpp
    daemon/enforcement_manager.h
    daemon/network_manager.cpp
    daemon/network_manager.h
    common/constants.h
    common/database.h
    common/crypto.h
)

add_executable(ulyssesd ${DAEMON_SOURCES})
target_link_libraries(ulyssesd PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Sql
    Qt${QT_VERSION_MAJOR}::Network
    ${OPENDHT_LIBRARIES}
)
target_include_directories(ulyssesd PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENDHT_INCLUDE_DIRS}
)

# ---- GUI (ulysses) ----
set(GUI_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    gui/dbus_client.h
    gui/home_tab.cpp
    gui/home_tab.h
    gui/lists_tab.cpp
    gui/lists_tab.h
    gui/blocks_tab.cpp
    gui/blocks_tab.h
    gui/triggers_tab.cpp
    gui/triggers_tab.h
    gui/network_tab.cpp
    gui/network_tab.h
    common/constants.h
    common/database.h
)

add_executable(ulysses ${GUI_SOURCES})
target_link_libraries(ulysses PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::DBus
    Qt${QT_VERSION_MAJOR}::Sql
)
target_include_directories(ulysses PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ---- Install ----
include(GNUInstallDirs)
install(TARGETS ulyssesd ulysses
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
