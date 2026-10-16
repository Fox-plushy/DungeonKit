#pragma once
#include <QString>
#include <QStringList>

class QWidget;

namespace dk {
namespace I18n {

enum Lang { English = 0, Spanish, French, Japanese, Chinese, Russian, LangCount };

void init();                                   // reads the saved language
Lang language();
void setLanguage(Lang l);                      // saves it too
QString nativeName(Lang l);                    // "Espanol", "Francais", ...
QString code(Lang l);                          // "en", "es", ...

// Translates an English UI string. Strings with %1/%2 placeholders are matched as templates.
// Anything without a translation comes back unchanged.
QString tr(const QString& en);
inline QString tr(const char* en) { return tr(QString::fromUtf8(en)); }

// Walks a widget tree and swaps the text of labels, buttons, placeholders and tree headers.
// Safe to call repeatedly; text set later by code is picked up on the next call.
void retranslate(QWidget* root);

} // namespace I18n
} // namespace dk
