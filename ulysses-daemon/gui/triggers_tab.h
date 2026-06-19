#ifndef ULYSSES_TRIGGERS_TAB_H
#define ULYSSES_TRIGGERS_TAB_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include "../gui/dbus_client.h"

namespace Ulysses {

class TriggersTab : public QWidget {
    Q_OBJECT

public:
    explicit TriggersTab(DBusClient *client, QWidget *parent = nullptr);

private slots:
    void refreshTriggers();
    void refreshBlocks();
    void addTrigger();
    void deleteTrigger();
    void fireTrigger();
    void fireManualBlock();

private:
    void setupUI();

    DBusClient *m_client;

    // Manual trigger section
    QComboBox *m_manualBlockCombo;
    QPushButton *m_manualFireBtn;

    // Named triggers list
    QListWidget *m_triggerList;
    QPushButton *m_addTriggerBtn;
    QPushButton *m_deleteTriggerBtn;
    QPushButton *m_fireTriggerBtn;

    // Trigger editor
    QGroupBox *m_editorGroup;
    QLineEdit *m_triggerNameEdit;
    QComboBox *m_blockCombo;
    QCheckBox *m_propagateCheck;
    QLabel *m_statusLabel;

    // Cache block IDs
    QStringList m_blockIds;
};

} // namespace Ulysses

#endif // ULYSSES_TRIGGERS_TAB_H
