#include "NameDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>
#include "../Version.h"
#include "Identity.h"
#include "I18n.h"
#include "Logo.h"
#include "Theme.h"
#include "TitleBar.h"

namespace dk {

bool NameDialog::isValidName(const QString& t) { return t.size() >= 2 && t.size() <= 24; }

NameDialog::NameDialog(const QString& previous, QWidget* parent) : QDialog(parent) {
    setWindowTitle(QString::fromLatin1(DK_APP_NAME " " DK_APP_VERSION));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setModal(true);
    setMinimumWidth(420);

    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(28, 24, 28, 22);
    v->setSpacing(10);

    Theme theme;
    theme.load();
    const QIcon ic = Logo::icon(theme.accent, Logo::recolorEnabled());
    setWindowIcon(ic);

    auto* head = new QHBoxLayout;
    auto* logo = new QLabel;
    logo->setPixmap(Logo::pixmap(64, theme.accent, Logo::recolorEnabled()));
    logo->setFixedSize(64, 64);
    auto* tv = new QVBoxLayout;
    tv->setSpacing(2);
    auto* title = new QLabel(I18n::tr("Welcome to %1").arg(QString::fromLatin1(DK_APP_NAME)));
    title->setObjectName("PageTitle");
    auto* sub = new QLabel(I18n::tr("Choose a name to use this session."));
    sub->setObjectName("Muted");
    tv->addStretch(1);
    tv->addWidget(title);
    tv->addWidget(sub);
    tv->addStretch(1);
    head->addWidget(logo);
    head->addSpacing(10);
    head->addLayout(tv, 1);
    v->addLayout(head);
    v->addSpacing(6);

    edit_ = new QLineEdit(previous);
    edit_->setMaxLength(24);
    edit_->setPlaceholderText(I18n::tr("Your name"));
    v->addWidget(edit_);

    hint_ = new QLabel;
    hint_->setObjectName("Hint");
    v->addWidget(hint_);
    v->addSpacing(8);

    auto* row = new QHBoxLayout;
    auto* quit = new QPushButton(I18n::tr("Quit"));
    ok_ = new QPushButton(I18n::tr("Continue"));
    ok_->setObjectName("Primary");
    ok_->setDefault(true);
    row->addStretch(1);
    row->addWidget(quit);
    row->addWidget(ok_);
    v->addLayout(row);

    connect(edit_, &QLineEdit::textChanged, this, [this]() { updateState(); });
    connect(ok_, &QPushButton::clicked, this, [this]() { tryAccept(); });
    connect(quit, &QPushButton::clicked, this, &QDialog::reject);
    updateState();
    edit_->selectAll();
    edit_->setFocus();
}

void NameDialog::showEvent(QShowEvent* e) {
    QDialog::showEvent(e);
    Theme t;
    t.load();
    applyTitleBarColors(this, t.sidebar.rgb() & 0xFFFFFF, t.text.rgb() & 0xFFFFFF, t.border.rgb() & 0xFFFFFF);
}

QString NameDialog::name() const {
    const QString t = edit_->text().trimmed();
    const QString canon = Identity::canonicalName(t);
    return role_ != Identity::Role::None && !canon.isEmpty() ? canon : t;
}

void NameDialog::tryAccept() {
    const QString t = edit_->text().trimmed();
    if (Identity::isReserved(t)) {
        const Identity::Role r = Identity::roleOf(t);
        if (!Identity::askPassword(this, r, t)) return;    // stay on the dialog
        role_ = r;
    } else {
        role_ = Identity::Role::None;
    }
    accept();
}

void NameDialog::updateState() {
    const bool ok = isValidName(name());
    ok_->setEnabled(ok);
    const QString t = edit_->text().trimmed();
    const bool remembered = ok && Identity::isReserved(t) && Identity::isRemembered(t);
    hint_->setText(I18n::tr(remembered ? "Remembered on this computer - you'll be signed in automatically." :
                             ok ? "You can change it later on the Account page." : "Use 2 to 24 characters."));
}

} // namespace dk
