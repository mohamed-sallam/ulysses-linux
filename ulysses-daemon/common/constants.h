#ifndef ULYSSES_CONSTANTS_H
#define ULYSSES_CONSTANTS_H

#include <QString>
#include <QStandardPaths>

namespace Ulysses {

constexpr const char* DBUS_SERVICE_NAME = "net.ulysses.Daemon";
constexpr const char* DBUS_OBJECT_PATH  = "/net/ulysses/Daemon";
constexpr const char* DBUS_INTERFACE    = "net.ulysses.Daemon";

constexpr const char* APP_NAME    = "Ulysses";
constexpr const char* APP_VERSION = "0.1.0";

inline QString configDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + "/ulysses";
}

inline QString dbPath() {
    return configDir() + "/ulysses.db";
}

} // namespace Ulysses

#endif // ULYSSES_CONSTANTS_H
