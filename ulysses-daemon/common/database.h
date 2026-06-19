#ifndef ULYSSES_DATABASE_H
#define ULYSSES_DATABASE_H

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDir>
#include <QDebug>
#include "constants.h"

namespace Ulysses {

class Database {
public:
    static bool initialize() {
        QDir dir;
        dir.mkpath(configDir());

        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName(dbPath());

        if (!db.open()) {
            qCritical() << "Failed to open database:" << db.lastError().text();
            return false;
        }

        return createTables();
    }

    static QSqlDatabase get() {
        return QSqlDatabase::database();
    }

private:
    static bool createTables() {
        QSqlQuery q;

        // Lists table
        if (!q.exec("CREATE TABLE IF NOT EXISTS lists ("
                     "id TEXT PRIMARY KEY,"
                     "name TEXT NOT NULL,"
                     "created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
                     "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
                     ")")) {
            qCritical() << "Failed to create lists table:" << q.lastError().text();
            return false;
        }

        // List entries table
        if (!q.exec("CREATE TABLE IF NOT EXISTS list_entries ("
                     "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                     "list_id TEXT NOT NULL REFERENCES lists(id) ON DELETE CASCADE,"
                     "type TEXT NOT NULL CHECK(type IN ('allow','deny')),"
                     "value TEXT NOT NULL,"
                     "rule_type TEXT NOT NULL DEFAULT 'domain'"
                     ")")) {
            qCritical() << "Failed to create list_entries table:" << q.lastError().text();
            return false;
        }

        // Blocks table
        if (!q.exec("CREATE TABLE IF NOT EXISTS blocks ("
                     "id TEXT PRIMARY KEY,"
                     "name TEXT NOT NULL,"
                     "lock_type TEXT NOT NULL DEFAULT 'timer',"
                     "lock_param TEXT NOT NULL DEFAULT '3600',"
                     "created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
                     "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
                     ")")) {
            qCritical() << "Failed to create blocks table:" << q.lastError().text();
            return false;
        }

        // Block-list associations
        if (!q.exec("CREATE TABLE IF NOT EXISTS block_lists ("
                     "block_id TEXT NOT NULL REFERENCES blocks(id) ON DELETE CASCADE,"
                     "list_id TEXT NOT NULL REFERENCES lists(id) ON DELETE CASCADE,"
                     "PRIMARY KEY (block_id, list_id)"
                     ")")) {
            qCritical() << "Failed to create block_lists table:" << q.lastError().text();
            return false;
        }

        // Triggers table
        if (!q.exec("CREATE TABLE IF NOT EXISTS triggers ("
                     "uuid TEXT PRIMARY KEY,"
                     "name TEXT NOT NULL,"
                     "block_id TEXT NOT NULL REFERENCES blocks(id) ON DELETE CASCADE,"
                     "propagate_to_network INTEGER NOT NULL DEFAULT 0,"
                     "lamport_ts INTEGER NOT NULL DEFAULT 0,"
                     "deleted INTEGER NOT NULL DEFAULT 0,"
                     "created_at INTEGER NOT NULL DEFAULT (strftime('%s','now')),"
                     "updated_at INTEGER NOT NULL DEFAULT (strftime('%s','now'))"
                     ")")) {
            qCritical() << "Failed to create triggers table:" << q.lastError().text();
            return false;
        }

        // Active sessions table (for persistence across restarts)
        if (!q.exec("CREATE TABLE IF NOT EXISTS active_sessions ("
                     "id TEXT PRIMARY KEY,"
                     "block_id TEXT NOT NULL,"
                     "trigger_uuid TEXT,"
                     "lock_type TEXT NOT NULL,"
                     "lock_param TEXT NOT NULL,"
                     "started_at INTEGER NOT NULL,"
                     "expires_at INTEGER,"
                     "password_hash TEXT,"
                     "frozen_pids TEXT"
                     ")")) {
            qCritical() << "Failed to create active_sessions table:" << q.lastError().text();
            return false;
        }

        // Network group info
        if (!q.exec("CREATE TABLE IF NOT EXISTS network_group ("
                     "id INTEGER PRIMARY KEY CHECK(id = 1),"
                     "group_uuid TEXT,"
                     "group_secret TEXT,"
                     "device_id TEXT,"
                     "device_private_key TEXT"
                     ")")) {
            qCritical() << "Failed to create network_group table:" << q.lastError().text();
            return false;
        }

        return true;
    }
};

} // namespace Ulysses

#endif // ULYSSES_DATABASE_H
