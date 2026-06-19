#include <QCoreApplication>
#include <QDBusConnection>
#include <QDebug>
#include "daemon.h"
#include "../common/constants.h"

#include <sys/prctl.h>
#include <linux/capability.h>

int main(int argc, char *argv[])
{
    // Raise ambient capabilities so child processes like iptables inherit them
    // This requires the binary to have these capabilities in its Inheritable set
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_NET_ADMIN, 0, 0);
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_SYS_PTRACE, 0, 0);
    prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, CAP_KILL, 0, 0);

    QCoreApplication app(argc, argv);
    app.setApplicationName(Ulysses::APP_NAME);
    app.setApplicationVersion(Ulysses::APP_VERSION);

    Ulysses::Daemon daemon(argv, argc);

    if (!daemon.registerOnBus()) {
        qCritical() << "Failed to register daemon on D-Bus. Exiting.";
        return 1;
    }

    qInfo() << "ulyssesd started successfully";
    return app.exec();
}
