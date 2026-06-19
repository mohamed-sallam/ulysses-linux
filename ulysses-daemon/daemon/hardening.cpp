#include "hardening.h"
#include <sys/types.h>
#include <algorithm>

namespace Ulysses {

Hardening::Hardening(char **argv, int argc, QObject *parent)
    : QObject(parent), m_argv(argv), m_argc(argc), m_argvBufLen(0)
{
    // Calculate total argv buffer length for process name overwriting
    if (argc > 0) {
        char *end = argv[argc - 1] + strlen(argv[argc - 1]);
        m_argvBufLen = static_cast<size_t>(end - argv[0]);
    }
}

void Hardening::start(bool devMode) {
    if (devMode) {
        qInfo() << "Developer mode active: Hardening (process renaming, signal masking) disabled.";
        return;
    }
    maskSignals();

    // Rename process every 30 seconds
    connect(&m_renameTimer, &QTimer::timeout, this, &Hardening::renameProcess);
    m_renameTimer.start(30000);
    renameProcess(); // Do it once immediately
}

void Hardening::maskSignals() {
    // Block all catchable signals that would terminate us
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    qInfo() << "Signals masked (SIGTERM, SIGINT, SIGHUP, SIGUSR1/2, SIGQUIT, SIGPIPE)";
}

std::vector<std::string> Hardening::collectProcessNames() {
    std::vector<std::string> names;
    DIR *procDir = opendir("/proc");
    if (!procDir) return names;

    struct dirent *entry;
    while ((entry = readdir(procDir)) != nullptr) {
        // Only numeric directories (PIDs)
        bool isNumeric = true;
        for (const char *p = entry->d_name; *p; ++p) {
            if (*p < '0' || *p > '9') { isNumeric = false; break; }
        }
        if (!isNumeric) continue;

        pid_t pid = static_cast<pid_t>(atoi(entry->d_name));
        if (pid == getpid()) continue; // Skip ourselves

        std::string cmdlinePath = std::string("/proc/") + entry->d_name + "/cmdline";
        std::ifstream f(cmdlinePath, std::ios::binary);
        if (!f.is_open()) continue;

        std::string cmdline;
        std::getline(f, cmdline, '\0');
        if (cmdline.empty()) continue;

        // Extract just the binary name (basename)
        auto pos = cmdline.rfind('/');
        std::string basename = (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;

        if (!basename.empty() && basename.size() < m_argvBufLen && basename.size() > 3) {
            names.push_back(basename);
        }
    }
    closedir(procDir);
    return names;
}

void Hardening::renameProcess() {
    auto names = collectProcessNames();
    if (names.empty()) return;

    // Pick a random name
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, names.size() - 1);
    const std::string &newName = names[dist(gen)];

    // Overwrite argv[0]
    if (m_argv && m_argvBufLen > 0) {
        memset(m_argv[0], 0, m_argvBufLen);
        strncpy(m_argv[0], newName.c_str(), m_argvBufLen - 1);
        // Null out other argv entries
        for (int i = 1; i < m_argc; ++i)
            m_argv[i] = nullptr;
    }

    // Change /proc/self/comm via prctl
    prctl(PR_SET_NAME, newName.substr(0, 15).c_str(), 0, 0, 0);

    qDebug() << "Process renamed to:" << QString::fromStdString(newName);
}

} // namespace Ulysses
