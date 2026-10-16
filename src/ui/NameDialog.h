#pragma once
#include <QDialog>
#include <QString>
#include "Identity.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QShowEvent;

namespace dk {

// Shown every time DungeonKit starts. Pre-filled with the last name used.
class NameDialog : public QDialog {
    Q_OBJECT
public:
    explicit NameDialog(const QString& previous, QWidget* parent = nullptr);
    QString name() const;                       // trimmed (proper capitalisation for reserved names)
    Identity::Role role() const { return role_; }   // set when a reserved name was unlocked with its password

    static bool isValidName(const QString& trimmed);   // 2-24 characters

protected:
    void showEvent(QShowEvent* e) override;

private:
    void updateState();
    void tryAccept();
    Identity::Role role_ = Identity::Role::None;
    QLineEdit* edit_ = nullptr;
    QPushButton* ok_ = nullptr;
    QLabel* hint_ = nullptr;
};

} // namespace dk
