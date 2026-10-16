#include "IdPicker.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QCompleter>
#include <QLineEdit>

namespace dk {

IdPicker::IdPicker(QWidget* parent) : QComboBox(parent) {
    setEditable(true);
    setInsertPolicy(QComboBox::NoInsert);
    setMinimumContentsLength(18);
    setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    if (QCompleter* c = completer()) {
        c->setFilterMode(Qt::MatchContains);
        c->setCaseSensitivity(Qt::CaseInsensitive);
        c->setCompletionMode(QCompleter::PopupCompletion);
    }
    connect(this, QOverload<int>::of(&QComboBox::activated), this, [this](int) { emitIfValid(); });
    connect(lineEdit(), &QLineEdit::returnPressed, this, [this]() { emitIfValid(); });
}

void IdPicker::setList(const IdList& list) {
    int32_t cur = 0;
    bool had = value(cur);
    blockSignals(true);
    clear();
    for (size_t i = 0; i < list.count; ++i)
        addItem(QString("%1  [%2]").arg(QString::fromUtf8(list.items[i].name)).arg(list.items[i].id), list.items[i].id);
    blockSignals(false);
    if (had) setValueQuiet(cur); else setCurrentIndex(-1);
}

void IdPicker::setValueQuiet(int32_t id) {
    blockSignals(true);
    int idx = findData(id);
    if (idx >= 0) setCurrentIndex(idx);
    else { setCurrentIndex(-1); setEditText(QString::number(id)); }
    blockSignals(false);
}

bool IdPicker::value(int32_t& id) const {
    int idx = currentIndex();
    if (idx >= 0 && currentText() == itemText(idx)) { id = itemData(idx).toInt(); return true; }
    QString t = currentText().trimmed();
    int i = findText(t, Qt::MatchFixedString);            // exact name typed
    if (i >= 0) { id = itemData(i).toInt(); return true; }
    int open = t.lastIndexOf('['), close = t.lastIndexOf(']');   // "Name [123]" pasted
    if (open >= 0 && close > open) t = t.mid(open + 1, close - open - 1);
    bool ok = false;
    qlonglong v = t.startsWith("0x", Qt::CaseInsensitive) ? t.mid(2).toLongLong(&ok, 16) : t.toLongLong(&ok, 10);
    if (!ok) return false;
    id = (int32_t)v;
    return true;
}

bool IdPicker::busy() const {
    QWidget* f = QApplication::focusWidget();
    return (f && (f == this || isAncestorOf(f))) || (view() && view()->isVisible());
}

void IdPicker::emitIfValid() {
    int32_t v;
    if (value(v)) emit committed(v);
}

} // namespace dk
