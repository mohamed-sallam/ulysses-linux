#ifndef ULYSSES_HARDENING_H
#define ULYSSES_HARDENING_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include <signal.h>
#include <sys/prctl.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <random>
#include <dirent.h>
#include <unistd.h>

namespace Ulysses {

class Hardening : public QObject {
    Q_OBJECT

public:
    explicit Hardening(char **argv, int argc, QObject *parent = nullptr);

    void start(bool devMode = false);

private:
    void maskSignals();
    void renameProcess();
    std::vector<std::string> collectProcessNames();

    char **m_argv;
    int m_argc;
    size_t m_argvBufLen;
    QTimer m_renameTimer;
};

} // namespace Ulysses

#endif // ULYSSES_HARDENING_H
