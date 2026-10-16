#include "ShopPricesCard.h"
#include "I18n.h"
#include "../core/Process.h"
#include "../mods/Mods.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

namespace dk {

ShopPricesCard::ShopPricesCard(CurrencyMod* currency, std::function<Process*()> proc, QWidget* parent)
    : QFrame(parent), cur_(currency), proc_(std::move(proc)) {
    setObjectName("Card");
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    auto* t = new QLabel("Merchant prices");
    t->setObjectName("CardTitle");
    v->addWidget(t);
    auto* h = new QLabel("Shop prices cannot be changed directly. When you buy something, the part you do not want to pay is given back in emeralds and gold. You still need the full price to buy.");
    h->setObjectName("Hint");
    h->setWordWrap(true);
    v->addWidget(h);

    auto* row = new QHBoxLayout;
    row->setSpacing(6);
    struct Preset { const char* label; int pct; };
    static const Preset presets[] = {{"Free", 0}, {"25% of normal", 25}, {"50% of normal", 50}, {"75% of normal", 75}, {"Normal", 100}};
    pay_ = new QDoubleSpinBox;
    pay_->setRange(0, 100);
    pay_->setDecimals(0);
    pay_->setSingleStep(5);
    pay_->setSuffix(" %");
    pay_->setValue(0);
    for (const Preset& pr : presets) {
        auto* b = new QPushButton(pr.label);
        b->setCursor(Qt::PointingHandCursor);
        const int pct = pr.pct;
        connect(b, &QPushButton::clicked, this, [this, pct]() { pay_->setValue(pct); });
        row->addWidget(b);
    }
    row->addStretch(1);
    v->addLayout(row);

    auto* c = new QHBoxLayout;
    c->setSpacing(6);
    c->addWidget(new QLabel("I pay"));
    c->addWidget(pay_);
    active_ = new QCheckBox("On");
    connect(active_, &QCheckBox::toggled, this, [this](bool on) { setActive(on); });
    c->addWidget(active_);
    c->addStretch(1);
    v->addLayout(c);

    state_ = new QLabel("-");
    state_->setObjectName("Hint");
    state_->setWordWrap(true);
    v->addWidget(state_);
    lastText_ = new QLabel(QString());
    v->addWidget(lastText_);

    connect(&timer_, &QTimer::timeout, this, [this]() { tick(); });
    timer_.start(40);
}

void ShopPricesCard::setActive(bool on) {
    last_[0] = last_[1] = -1;
    if (!on) return;
    Process* p = proc_();
    if (!p) { emit message(I18n::tr("Waiting for the game...")); return; }
    if (!cur_->enabled()) {                      // the Currency Editor hook is what finds the emerald count
        std::string err;
        if (!cur_->enable(*p, err)) emit message(QString::fromStdString(err));
    }
}

void ShopPricesCard::tick() {
    Process* p = proc_();
    if (!p) {
        last_[0] = last_[1] = -1;
        if (active_->isChecked()) state_->setText(I18n::tr("Waiting for the game..."));
        else state_->setText("-");
        return;
    }
    if (!active_->isChecked()) { state_->setText(I18n::tr("Turn it on, then buy something.")); return; }
    int32_t now[2] = {0, 0};
    const CurrencyMod::Kind kinds[2] = {CurrencyMod::Emerald, CurrencyMod::Gold};
    if (!cur_->enabled() || !cur_->get(*p, kinds[0], now[0])) {
        last_[0] = last_[1] = -1;
        state_->setText(I18n::tr("Open your inventory or pick up an emerald to link."));
        return;
    }
    const bool haveGold = cur_->get(*p, kinds[1], now[1]);
    state_->setText(I18n::tr("Emeralds: ") + QString::number(now[0]) + (haveGold ? "    " + I18n::tr("Gold: ") + QString::number(now[1]) : QString()));
    for (int i = 0; i < 2; ++i) {
        if (i == 1 && !haveGold) { last_[1] = -1; continue; }
        if (last_[i] >= 0 && now[i] < last_[i]) {
            const int32_t spent = last_[i] - now[i];
            const int32_t refund = (int32_t)std::lround(spent * (1.0 - pay_->value() / 100.0));
            if (refund > 0 && cur_->set(*p, kinds[i], now[i] + refund)) {
                now[i] += refund;
                lastText_->setText(I18n::tr("Last purchase: ") + QString::number(spent) + (i == 0 ? I18n::tr(" emeralds, paid ") : I18n::tr(" gold, paid ")) +
                                   QString::number(spent - refund));
            }
        }
        last_[i] = now[i];
    }
}

void ShopPricesCard::restore() {
    active_->setChecked(false);
}

} // namespace dk
