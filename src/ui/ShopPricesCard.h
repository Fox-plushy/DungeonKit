#pragma once
// Item Editor (Minecraft Dungeons): merchant prices. The game computes shop prices in its own code and stores no
// discount value, so this works on the emeralds instead: when a purchase takes emeralds away, the share you choose
// is given back straight away.
#include <QFrame>
#include <QTimer>
#include <functional>

class QCheckBox;
class QDoubleSpinBox;
class QLabel;

namespace dk {

class Process;
class CurrencyMod;

class ShopPricesCard : public QFrame {
    Q_OBJECT
public:
    ShopPricesCard(CurrencyMod* currency, std::function<Process*()> proc, QWidget* parent = nullptr);
    void restore();                          // switch the refund off
signals:
    void message(const QString& text);

private:
    void tick();
    void setActive(bool on);
    CurrencyMod* cur_;
    std::function<Process*()> proc_;
    QLabel* state_ = nullptr;
    QLabel* lastText_ = nullptr;
    QCheckBox* active_ = nullptr;
    QDoubleSpinBox* pay_ = nullptr;
    QTimer timer_;
    int32_t last_[2] = {-1, -1};            // emeralds, gold as of the last check
};

} // namespace dk
