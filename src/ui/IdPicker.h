#pragma once
#include <QComboBox>
#include "../data/IdTables.h"

namespace dk {

// Searchable dropdown of "Name [id]" entries that also accepts a typed number (decimal or 0x hex).
class IdPicker : public QComboBox {
    Q_OBJECT
public:
    explicit IdPicker(QWidget* parent = nullptr);

    void setList(const IdList& list);          // replaces entries, keeps the current value
    void setValueQuiet(int32_t id);            // show `id` without emitting committed()
    bool value(int32_t& id) const;             // current selection / typed number
    bool busy() const;                         // user is interacting (don't overwrite it)

signals:
    void committed(int value);                 // user picked or entered a value

private:
    void emitIfValid();
};

} // namespace dk
