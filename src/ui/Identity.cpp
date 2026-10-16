#include "Identity.h"

#include <QCryptographicHash>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QSettings>

namespace dk {
namespace Identity {

namespace {
const char* kDevNames[] = {"Plushy", "Venatic"};
const char* kAssistantNames[] = {"Moonlitfoxi"};
const char* kCreatorNames[] = {"JustGaming101", "TDF"};
const char* kHelperNames[] = {"Dweem", "Calamari", "Hybrid", "5149", "Mist", "Santoryu", "Lucas", "Arno", "Inspirity", "Terrarings", "Tomcat"};
const char* kSupporterNames[] = {"Dweem JR", "Floatzyy", "Grim", "Jai", "Pink", "Star", "Shadow", "RAGE", "Maz"};
// SHA-256 of salt + password, so the password itself is not sitting in the program as readable text.
const char* kSalt = "DungeonKit::reserved::v1::";
const char* kHash = "eabc3ec96283fc443eec5f79ca4d5ef5d731ed8fa7a183ed51f01f9fe3f6e2b9";
const char* kCreatorSalt = "DungeonKit::creator::v1::";
const char* kCreatorHash = "d731ebe70637daa9fa47aa80ac9ab415194346d8265a34fe191e2b539534cf55";
const char* kHelperSalt = "DungeonKit::helper::v1::";
const char* kHelperHash = "548ad15b8ab86b88ca5a0b0ed146b778c0cd71b79a75975507423a40cd9d7113";
const char* kSupporterSalt = "DungeonKit::supporter::v1::";
const char* kSupporterHash = "a40298a10df392719ec35bcd8026cf996e34c122b5fdc080ea79e81c1b0330d3";

QString simplify(const QString& s) {
    QString out;
    for (QChar c : s) if (c.isLetterOrNumber()) out += c.toLower();
    return out;
}
}

Role roleOf(const QString& name) {
    const QString key = simplify(name);
    for (const char* n : kDevNames)
        if (key == simplify(QString::fromLatin1(n))) return Role::Dev;
    for (const char* n : kAssistantNames)
        if (key == simplify(QString::fromLatin1(n))) return Role::Assistant;
    for (const char* n : kHelperNames)
        if (key == simplify(QString::fromLatin1(n))) return Role::Helper;
    for (const char* n : kCreatorNames)
        if (key == simplify(QString::fromLatin1(n))) return Role::Creator;
    for (const char* n : kSupporterNames)
        if (key == simplify(QString::fromLatin1(n))) return Role::Supporter;
    return Role::None;
}

bool isReserved(const QString& name) { return roleOf(name) != Role::None; }

QString canonicalName(const QString& name) {
    const QString key = simplify(name);
    for (const char* n : kDevNames)
        if (key == simplify(QString::fromLatin1(n))) return QString::fromLatin1(n);
    for (const char* n : kAssistantNames)
        if (key == simplify(QString::fromLatin1(n))) return QString::fromLatin1(n);
    for (const char* n : kHelperNames)
        if (key == simplify(QString::fromLatin1(n))) return QString::fromLatin1(n);
    for (const char* n : kCreatorNames)
        if (key == simplify(QString::fromLatin1(n))) return QString::fromLatin1(n);
    for (const char* n : kSupporterNames)
        if (key == simplify(QString::fromLatin1(n))) return QString::fromLatin1(n);
    return QString();
}

bool checkPassword(Role role, const QString& password) {
    if (role == Role::None) return false;
    const bool dev = role == Role::Dev || role == Role::Assistant;      // Assistants use the Dev password
    const char* salt = dev ? kSalt : role == Role::Creator ? kCreatorSalt : role == Role::Supporter ? kSupporterSalt : kHelperSalt;
    const char* hash = dev ? kHash : role == Role::Creator ? kCreatorHash : role == Role::Supporter ? kSupporterHash : kHelperHash;
    QByteArray data = QByteArray(salt) + password.toUtf8();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex() == QByteArray(hash);
}

namespace {
QString rememberedKey(const QString& name) { return "identity/remembered/" + simplify(name); }
}

bool isRemembered(const QString& name) {
    return QSettings().value(rememberedKey(name), false).toBool();
}

bool askPassword(QWidget* parent, Role role, const QString& name) {
    if (isRemembered(name)) return true;             // typed correctly before on this computer - sign in right away

    bool ok = false;
    const QString pw = QInputDialog::getText(parent, "Reserved name",
                                             "That name is reserved. Enter the password to use it:",
                                             QLineEdit::Password, QString(), &ok);
    if (!ok) return false;
    if (!checkPassword(role, pw)) {
        QMessageBox::warning(parent, "Wrong password", "That password is not correct.");
        return false;
    }
    QSettings().setValue(rememberedKey(name), true);  // remember it so it signs in automatically next time
    return true;
}

} // namespace Identity
} // namespace dk
