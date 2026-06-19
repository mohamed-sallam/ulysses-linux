#include "app_blocker.h"
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <signal.h>
#include <dirent.h>
#include <fstream>
#include <unistd.h>

namespace Ulysses {

AppBlocker::AppBlocker(QObject *parent)
    : QObject(parent), m_active(false)
{
    m_cgroupPath = "/sys/fs/cgroup/freezer/ulysses";
    connect(&m_scanTimer, &QTimer::timeout, this, &AppBlocker::scanAndFreeze);
}

AppBlocker::~AppBlocker() {
    stopBlocking();
}

void AppBlocker::setBlockedApps(const QStringList &appNames) {
    m_blockedApps = appNames;
}

void AppBlocker::setBlockedWindowTitles(const QStringList &titlePatterns) {
    m_blockedTitles = titlePatterns;
}

void AppBlocker::startBlocking() {
    if (m_active) return;
    m_active = true;
    setupCgroupFreezer();
    m_scanTimer.start(2000); // Scan every 2 seconds as per spec
    scanAndFreeze(); // Immediate first scan
    qInfo() << "App blocking started. Apps:" << m_blockedApps << "Titles:" << m_blockedTitles;
}

void AppBlocker::stopBlocking() {
    if (!m_active) return;
    m_scanTimer.stop();
    m_active = false;

    // Unfreeze all frozen processes
    for (auto &proc : m_frozenProcesses) {
        if (proc.frozen)
            unfreezeProcess(proc.pid);
    }
    m_frozenProcesses.clear();

    qInfo() << "App blocking stopped, all processes unfrozen";
}

QList<pid_t> AppBlocker::frozenPids() const {
    QList<pid_t> pids;
    for (const auto &p : m_frozenProcesses) {
        if (p.frozen)
            pids.append(p.pid);
    }
    return pids;
}

void AppBlocker::scanAndFreeze() {
    // Check if previously frozen processes are still alive
    auto it = m_frozenProcesses.begin();
    while (it != m_frozenProcesses.end()) {
        if (kill(it->pid, 0) != 0) {
            // Process no longer exists
            it = m_frozenProcesses.erase(it);
        } else {
            ++it;
        }
    }

    QSet<pid_t> alreadyFrozen;
    for (const auto &p : m_frozenProcesses)
        alreadyFrozen.insert(p.pid);

    // Scan by app name
    for (const auto &appName : m_blockedApps) {
        QList<pid_t> pids = findProcessesByName(appName);
        for (pid_t pid : pids) {
            if (!alreadyFrozen.contains(pid)) {
                freezeProcess(pid, appName);
            }
        }
    }

    // Scan by window title
    for (const auto &pattern : m_blockedTitles) {
        QList<pid_t> pids = findProcessesByWindowTitle(pattern);
        for (pid_t pid : pids) {
            if (!alreadyFrozen.contains(pid)) {
                freezeProcess(pid, "window:" + pattern);
            }
        }
    }
}

QList<pid_t> AppBlocker::findProcessesByName(const QString &name) {
    QList<pid_t> result;
    DIR *procDir = opendir("/proc");
    if (!procDir) return result;

    struct dirent *entry;
    while ((entry = readdir(procDir)) != nullptr) {
        bool isNumeric = true;
        for (const char *p = entry->d_name; *p; ++p) {
            if (*p < '0' || *p > '9') { isNumeric = false; break; }
        }
        if (!isNumeric) continue;

        pid_t pid = static_cast<pid_t>(atoi(entry->d_name));
        if (pid == getpid()) continue;

        std::string cmdlinePath = std::string("/proc/") + entry->d_name + "/cmdline";
        std::ifstream f(cmdlinePath, std::ios::binary);
        if (!f.is_open()) continue;

        std::string cmdline((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());
        // Replace null bytes with spaces for matching
        for (auto &c : cmdline) {
            if (c == '\0') c = ' ';
        }

        if (QString::fromStdString(cmdline).contains(name, Qt::CaseInsensitive)) {
            result.append(pid);
        }
    }
    closedir(procDir);
    return result;
}

QList<pid_t> AppBlocker::findProcessesByWindowTitle(const QString &pattern) {
    QList<pid_t> result;

    // Use xdotool to find windows matching title pattern
    QProcess proc;
    proc.start("xdotool", {"search", "--name", pattern});
    proc.waitForFinished(5000);

    QString output = proc.readAllStandardOutput();
    QStringList windowIds = output.trimmed().split('\n', Qt::SkipEmptyParts);

    for (const auto &wid : windowIds) {
        // Get PID from window ID
        QProcess pidProc;
        pidProc.start("xdotool", {"getwindowpid", wid.trimmed()});
        pidProc.waitForFinished(2000);

        bool ok;
        pid_t pid = pidProc.readAllStandardOutput().trimmed().toInt(&ok);
        if (ok && pid > 0 && pid != getpid()) {
            result.append(pid);
        }
    }

    return result;
}

void AppBlocker::freezeProcess(pid_t pid, const QString &name) {
    // Send SIGSTOP
    if (kill(pid, SIGSTOP) == 0) {
        BlockedProcess bp;
        bp.pid = pid;
        bp.name = name;
        bp.frozen = true;
        m_frozenProcesses.append(bp);

        // Also add to cgroup freezer if available
        QString tasksPath = m_cgroupPath + "/tasks";
        QFile tasks(tasksPath);
        if (tasks.open(QIODevice::WriteOnly | QIODevice::Append)) {
            QTextStream(&tasks) << pid << "\n";
        }

        qDebug() << "Frozen process:" << pid << name;
    } else {
        qWarning() << "Failed to freeze process:" << pid << name;
    }
}

void AppBlocker::unfreezeProcess(pid_t pid) {
    kill(pid, SIGCONT);
    qDebug() << "Unfrozen process:" << pid;
}

void AppBlocker::setupCgroupFreezer() {
    QDir dir;
    if (!dir.exists(m_cgroupPath)) {
        dir.mkpath(m_cgroupPath);
    }

    // Set freezer state to FROZEN
    QFile state(m_cgroupPath + "/freezer.state");
    if (state.open(QIODevice::WriteOnly)) {
        QTextStream(&state) << "FROZEN\n";
        qInfo() << "Cgroup freezer set up at:" << m_cgroupPath;
    }
}

} // namespace Ulysses
