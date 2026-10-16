#pragma once
#include <QAbstractSpinBox>
#include <QComboBox>
#include <QEvent>
#include <QObject>
#include <QWheelEvent>

namespace dk {

// Application-wide filter: the mouse wheel never changes a number box or dropdown.
// The event is ignored and so passes up to the parent, which lets lists and pages scroll normally.
class WheelGuard : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject* obj, QEvent* ev) override {
        if (ev->type() == QEvent::Wheel && (qobject_cast<QAbstractSpinBox*>(obj) || qobject_cast<QComboBox*>(obj))) {
            ev->ignore();
            return true;      // swallowed here; ignored events keep propagating to the parent widget
        }
        return QObject::eventFilter(obj, ev);
    }
};

} // namespace dk
