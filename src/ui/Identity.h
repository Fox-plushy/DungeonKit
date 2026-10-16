#pragma once
#include <QString>

class QWidget;

namespace dk {

// A few names belong to the people who make DungeonKit. Using one needs a password.
// This is a courtesy check against impersonation inside the app, not real security:
// anyone who edits the program can bypass it. (Real enforcement has to happen on a server.)
namespace Identity {

enum class Role { None, Dev, Helper, Assistant, Creator, Supporter };

Role roleOf(const QString& name);                // Dev for the makers, Assistant for the assistants (same password as Dev), Helper (shown as Elite) for the honorable mentions, Creator (shown as Content Creator), Supporter for supporters, None otherwise

bool isReserved(const QString& name);            // any reserved name (ignores case, spaces and symbols)
QString canonicalName(const QString& name);      // proper capitalisation of a reserved name
bool checkPassword(Role role, const QString& password);

// True once this name's password has been entered correctly on this computer before (see askPassword).
bool isRemembered(const QString& name);

// Asks for the password in a dialog, unless this exact name was already typed correctly before on this
// computer, in which case it signs in right away with no prompt. The first correct password for a name
// is what gets remembered. Returns true only if the name is (or becomes) signed in.
bool askPassword(QWidget* parent, Role role, const QString& name);

}

} // namespace dk
