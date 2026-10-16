#include "I18n.h"

#include <QAbstractButton>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSettings>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <vector>

namespace dk {
namespace I18n {

struct Entry { const char* en; const char* tr[5]; };     // es fr ja zh ru
extern const Entry kEntries[];
extern const int kEntryCount;

namespace {

Lang g_lang = English;
bool g_touched = false;          // true once any widget has been given a translation

struct Tmpl { QRegularExpression re; QString out; };
QHash<QString, QString> g_exact;
std::vector<Tmpl> g_tmpl;
QHash<QString, QString> g_cache;
bool g_built = false;

void build() {
    g_exact.clear();
    g_tmpl.clear();
    g_cache.clear();
    g_built = true;
    if (g_lang == English) return;
    const int idx = int(g_lang) - 1;
    for (int i = 0; i < kEntryCount; ++i) {
        const Entry& e = kEntries[i];
        if (!e.tr[idx] || !*e.tr[idx]) continue;
        const QString en = QString::fromUtf8(e.en);
        const QString out = QString::fromUtf8(e.tr[idx]);
        if (!en.contains(QRegularExpression("%\\d"))) { g_exact.insert(en, out); continue; }
        // template: escape the literal parts, turn each %N into a capture group
        static const QRegularExpression ph("%(\\d)");
        QString pat;
        int last = 0;
        auto mi = ph.globalMatch(en);
        while (mi.hasNext()) {
            const QRegularExpressionMatch m = mi.next();
            pat += QRegularExpression::escape(en.mid(last, m.capturedStart() - last));
            pat += QString("(?<p%1>.+?)").arg(m.captured(1));
            last = m.capturedEnd();
        }
        pat += QRegularExpression::escape(en.mid(last));
        Tmpl t;
        t.re = QRegularExpression("^" + pat + "$", QRegularExpression::DotMatchesEverythingOption);
        t.out = out;
        if (t.re.isValid()) g_tmpl.push_back(t);
    }
}

} // namespace

QString nativeName(Lang l) {
    switch (l) {
    case Spanish:  return QString::fromUtf8("Español");
    case French:   return QString::fromUtf8("Français");
    case Japanese: return QString::fromUtf8("日本語");
    case Chinese:  return QString::fromUtf8("中文（简体）");
    case Russian:  return QString::fromUtf8("Русский");
    default:       return "English";
    }
}

QString code(Lang l) {
    static const char* c[] = {"en", "es", "fr", "ja", "zh", "ru"};
    return QString::fromLatin1(c[int(l)]);
}

void init() {
    const QString c = QSettings().value("app/language", "en").toString();
    g_lang = English;
    for (int i = 0; i < LangCount; ++i) if (code(Lang(i)) == c) g_lang = Lang(i);
    g_built = false;
}

Lang language() { return g_lang; }

void setLanguage(Lang l) {
    g_lang = l;
    QSettings().setValue("app/language", code(l));
    g_built = false;
}

QString tr(const QString& en) {
    if (g_lang == English || en.isEmpty()) return en;
    if (!g_built) build();
    auto it = g_exact.constFind(en);
    if (it != g_exact.constEnd()) return *it;
    auto c = g_cache.constFind(en);
    if (c != g_cache.constEnd()) return *c;
    QString result = en;
    for (const Tmpl& t : g_tmpl) {
        const QRegularExpressionMatch m = t.re.match(en);
        if (!m.hasMatch()) continue;
        QString out = t.out;
        for (int n = 1; n <= 9; ++n) {
            const QString name = QString("p%1").arg(n);
            if (t.re.namedCaptureGroups().contains(name))
                out.replace(QString("%%1").arg(n), tr(m.captured(name)));   // captured text may itself be a known string
        }
        result = out;
        break;
    }
    if (g_cache.size() > 3000) g_cache.clear();
    g_cache.insert(en, result);
    return result;
}

namespace {

// Remembers the English text a widget was given and the text we put there.
bool swap(const QString& cur, QString& src, QString& out) {
    if (cur.isEmpty()) return false;
    if (src.isEmpty() || cur != out) src = cur;               // text set by code since the last pass
    const QString want = tr(src);
    out = want;
    return want != cur;
}

void setProps(QObject* o, const QString& src, const QString& out) {
    if (o->property("dkSrc").toString() != src) o->setProperty("dkSrc", src);
    if (o->property("dkOut").toString() != out) o->setProperty("dkOut", out);
}

} // namespace

void retranslate(QWidget* root) {
    if (!root) return;
    if (g_lang == English && !g_touched) return;              // nothing to restore
    if (!g_built) build();
    if (g_lang != English) g_touched = true;
    for (QLabel* l : root->findChildren<QLabel*>()) {
        if (l->text().isEmpty() || l->property("dkSkip").toBool()) continue;       // dkSkip: stays English (the status card)
        QString src = l->property("dkSrc").toString(), out = l->property("dkOut").toString();
        if (swap(l->text(), src, out)) l->setText(out);
        setProps(l, src, out);
    }
    for (QAbstractButton* b : root->findChildren<QAbstractButton*>()) {
        if (b->text().isEmpty()) continue;
        QString src = b->property("dkSrc").toString(), out = b->property("dkOut").toString();
        if (swap(b->text(), src, out)) b->setText(out);
        setProps(b, src, out);
    }
    for (QLineEdit* e : root->findChildren<QLineEdit*>()) {
        if (e->placeholderText().isEmpty()) continue;
        QString src = e->property("dkSrc").toString(), out = e->property("dkOut").toString();
        if (swap(e->placeholderText(), src, out)) e->setPlaceholderText(out);
        setProps(e, src, out);
    }
    for (QTreeWidget* t : root->findChildren<QTreeWidget*>()) {
        QTreeWidgetItem* h = t->headerItem();
        if (!h) continue;
        for (int c = 0; c < t->columnCount(); ++c) {
            QString src = h->data(c, Qt::UserRole + 1).toString(), out = h->data(c, Qt::UserRole + 2).toString();
            if (swap(h->text(c), src, out)) h->setText(c, out);
            h->setData(c, Qt::UserRole + 1, src);
            h->setData(c, Qt::UserRole + 2, out);
        }
    }
}

} // namespace I18n
} // namespace dk
