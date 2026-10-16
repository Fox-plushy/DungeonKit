#include "ModMaker.h"
#include "I18n.h"
#include "../core/Process.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace dk {

namespace {

bool underTransient(UE4& ue, uint64_t o) {
    for (int depth = 0; depth < 24 && UE4::plausible(o); ++depth) {
        if (ue.objectName(o).find("Transient") != std::string::npos) return true;
        o = ue.rd64(o + 0x20);                                  // UObject::Outer
    }
    return false;
}

std::string toLowerStr(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

QString rootLabel(const QString& root) {
    if (root == "character") return "Player character";
    if (root == "controller") return "Controller";
    if (root == "gameinstance") return "Game instance";
    if (root == "world") return "World";
    if (root == "worldsettings") return "World settings";
    if (root == "gamestate") return "Game state";
    if (root == "engine") return "Engine";
    if (root.startsWith("class:")) return root.mid(6);
    return root;
}

} // namespace

bool ModMakerCard::numericType(const QString& t) {
    return t == "FloatProperty" || t == "DoubleProperty" || t == "IntProperty" || t == "UInt32Property" || t == "Int64Property" ||
           t == "UInt64Property" || t == "Int16Property" || t == "UInt16Property" || t == "Int8Property" || t == "ByteProperty" ||
           t == "EnumProperty" || t == "BoolProperty";
}

ModMakerCard::ModMakerCard(std::shared_ptr<UE4> ue, std::function<Process*()> proc, QWidget* parent)
    : QFrame(parent), ue_(std::move(ue)), proc_(std::move(proc)) {
    setObjectName("Card");
    auto* v = new QVBoxLayout(this);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    auto* t = new QLabel("Mod Maker");
    t->setObjectName("CardTitle");
    v->addWidget(t);
    auto* h = new QLabel("Make your own mods. 1) Pick where to look (Player character, World ...), or find any object by its class name. "
                         "2) Browse its properties. Double-click an object property to look inside it. 3) Click a number or on/off property, type a value and press Set once, "
                         "or save it as a mod you can switch on to hold that value. Use it offline only. A wrong value can crash the game.");
    h->setObjectName("Hint");
    h->setWordWrap(true);
    v->addWidget(h);

    auto* r1 = new QHBoxLayout;
    r1->setSpacing(6);
    root_ = new QComboBox;
    const char* const names[][2] = {{"Player character", "character"}, {"Controller", "controller"}, {"Game instance", "gameinstance"},
                                    {"World", "world"}, {"World settings", "worldsettings"}, {"Game state", "gamestate"}, {"Engine", "engine"}};
    for (auto& n : names) root_->addItem(I18n::tr(n[0]), QString(n[1]));
    r1->addWidget(root_, 1);
    classEdit_ = new QLineEdit;
    classEdit_->setPlaceholderText("Find objects by class name");
    r1->addWidget(classEdit_, 1);
    auto* find = new QPushButton("Find");
    find->setCursor(Qt::PointingHandCursor);
    r1->addWidget(find);
    v->addLayout(r1);

    auto* r2 = new QHBoxLayout;
    r2->setSpacing(6);
    crumb_ = new QLabel("-");
    auto* up = new QPushButton("Back");
    up->setCursor(Qt::PointingHandCursor);
    auto* reload = new QPushButton("Reload");
    reload->setCursor(Qt::PointingHandCursor);
    r2->addWidget(crumb_, 1);
    r2->addWidget(up);
    r2->addWidget(reload);
    v->addLayout(r2);

    filter_ = new QLineEdit;
    filter_->setPlaceholderText("Filter properties");
    v->addWidget(filter_);
    tree_ = new QTreeWidget;
    tree_->setColumnCount(3);
    tree_->setHeaderLabels({"Property", "Type", "Value"});
    tree_->setRootIsDecorated(false);
    tree_->setMinimumHeight(260);
    tree_->header()->setStretchLastSection(true);
    tree_->setColumnWidth(0, 230);
    tree_->setColumnWidth(1, 110);
    v->addWidget(tree_);

    selLabel_ = new QLabel("Nothing selected.");
    selLabel_->setObjectName("Hint");
    selLabel_->setWordWrap(true);
    v->addWidget(selLabel_);
    auto* r3 = new QHBoxLayout;
    r3->setSpacing(6);
    value_ = new QDoubleSpinBox;
    value_->setRange(-1.0e12, 1.0e12);
    value_->setDecimals(4);
    value_->setMinimumWidth(130);
    auto* once = new QPushButton("Set once");
    once->setCursor(Qt::PointingHandCursor);
    nameEdit_ = new QLineEdit;
    nameEdit_->setPlaceholderText("Mod name");
    auto* save = new QPushButton("Save as mod");
    save->setObjectName("Primary");
    save->setCursor(Qt::PointingHandCursor);
    r3->addWidget(value_);
    r3->addWidget(once);
    r3->addWidget(nameEdit_, 1);
    r3->addWidget(save);
    v->addLayout(r3);

    auto* lt = new QLabel("My mods (tick a mod to keep its value applied)");
    lt->setObjectName("CardTitle");
    v->addWidget(lt);
    list_ = new QListWidget;
    list_->setMinimumHeight(120);
    v->addWidget(list_);
    auto* r4 = new QHBoxLayout;
    r4->setSpacing(6);
    auto* del = new QPushButton("Delete");
    auto* copy = new QPushButton("Copy code");
    auto* paste = new QPushButton("Paste code");
    auto* off = new QPushButton("Switch all off");
    for (QPushButton* b : {del, copy, paste, off}) { b->setCursor(Qt::PointingHandCursor); r4->addWidget(b); }
    r4->addStretch(1);
    v->addLayout(r4);
    status_ = new QLabel(QString());
    status_->setObjectName("Hint");
    status_->setWordWrap(true);
    v->addWidget(status_);

    connect(root_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { path_.clear(); haveSel_ = false; refreshTree(); });
    connect(reload, &QPushButton::clicked, this, [this]() { refreshTree(); });
    connect(up, &QPushButton::clicked, this, [this]() { if (!path_.isEmpty()) { path_.removeLast(); haveSel_ = false; refreshTree(); } });
    connect(find, &QPushButton::clicked, this, [this]() {
        const QString n = classEdit_->text().trimmed();
        if (n.size() < 2) { status_->setText(I18n::tr("Type at least 2 letters of a class name.")); return; }
        startScan(n, false, true);
    });
    connect(classEdit_, &QLineEdit::returnPressed, find, &QPushButton::click);
    connect(filter_, &QLineEdit::textChanged, this, [this](const QString& f) {
        for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
            auto* it = tree_->topLevelItem(i);
            it->setHidden(!f.isEmpty() && !it->text(0).contains(f, Qt::CaseInsensitive));
        }
    });
    connect(tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* it, int) { selectField(it); });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* it, int) { descend(it); });
    connect(once, &QPushButton::clicked, this, [this]() {
        Process* p = proc_();
        Target tg;
        double cur = 0;
        if (!haveSel_) { status_->setText(I18n::tr("Click a number or on/off property first.")); return; }
        if (!ready(p) || !resolve(p, selected_, tg) || !writeValue(p, tg, selected_.type, value_->value())) {
            status_->setText(I18n::tr("Could not write that value. Load into a level first."));
            return;
        }
        readValue(p, tg, selected_.type, cur);
        status_->setText(I18n::tr("Written. Now: ") + QString::number(cur, 'g', 9));
    });
    connect(save, &QPushButton::clicked, this, [this]() {
        if (!haveSel_) { status_->setText(I18n::tr("Click a number or on/off property first.")); return; }
        Custom c;
        c.spec = selected_;
        c.value = value_->value();
        c.name = nameEdit_->text().trimmed().isEmpty() ? selected_.path.last() : nameEdit_->text().trimmed();
        mods_.push_back(c);
        refreshList();
        saveMods();
        status_->setText(I18n::tr("Saved. Tick it in the list to switch it on."));
    });
    connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem* it) {
        if (building_) return;
        const int i = list_->row(it);
        if (i >= 0 && i < (int)mods_.size()) { mods_[(size_t)i].on = it->checkState() == Qt::Checked; saveMods(); }
    });
    connect(del, &QPushButton::clicked, this, [this]() {
        const int i = list_->currentRow();
        if (i < 0 || i >= (int)mods_.size()) return;
        mods_.erase(mods_.begin() + i);
        refreshList();
        saveMods();
    });
    connect(copy, &QPushButton::clicked, this, [this]() {
        const int i = list_->currentRow();
        QString out;
        if (i >= 0 && i < (int)mods_.size()) out = toCode(mods_[(size_t)i]);
        else {
            QStringList all;
            for (const Custom& c : mods_) all << toCode(c);
            out = "[" + all.join(",") + "]";
        }
        if (mods_.empty()) { status_->setText(I18n::tr("No mods to copy.")); return; }
        QGuiApplication::clipboard()->setText(out);
        status_->setText(I18n::tr("Code copied. Send it to a friend; they use Paste code."));
    });
    connect(paste, &QPushButton::clicked, this, [this]() {
        int added = 0;
        if (!fromCode(QGuiApplication::clipboard()->text(), added) || added == 0) { status_->setText(I18n::tr("The clipboard does not hold a mod code.")); return; }
        refreshList();
        saveMods();
        status_->setText(I18n::tr("Added from the clipboard: ") + QString::number(added));
    });
    connect(off, &QPushButton::clicked, this, [this]() { stopAll(); });

    connect(&valueTimer_, &QTimer::timeout, this, [this]() { updateValues(); });
    connect(&applyTimer_, &QTimer::timeout, this, [this]() { applyAll(); });
    connect(&scanTimer_, &QTimer::timeout, this, [this]() { pollScan(); });
    valueTimer_.start(500);
    applyTimer_.start(100);
    scanTimer_.start(200);
    loadMods();
    refreshList();
    refreshTree();
}

ModMakerCard::~ModMakerCard() {
    valueTimer_.stop();
    applyTimer_.stop();
    scanTimer_.stop();
    if (scanThread_) { scanThread_->wait(); delete scanThread_; scanThread_ = nullptr; }
}

void ModMakerCard::stopAll() {
    for (Custom& c : mods_) c.on = false;
    refreshList();
    saveMods();
}

bool ModMakerCard::ready(Process* p) {
    std::string err;
    return p && ue_->init(*p, err);
}

uint64_t ModMakerCard::rootObject(Process* p, const QString& root) {
    uint64_t eng = 0, o = 0, vp = 0, world = 0, ch = 0;
    if (!ready(p)) return 0;
    if (root.startsWith("class:")) {
        auto it = classCache_.find(root);
        if (it != classCache_.end() && UE4::plausible(it->second) && QString::fromStdString(ue_->className(it->second)) == root.mid(6)) return it->second;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (!scanBusy_ && now - scanAsked_[root] > 3000) { scanAsked_[root] = now; startScan(root.mid(6), true, false); }
        return 0;
    }
    if (root == "character") return ue_->character(o) && UE4::plausible(o) ? o : 0;
    if (!ue_->engine(eng)) return 0;
    if (root == "engine") return eng;
    if (root == "gameinstance") return ue_->readPtr(eng, "GameInstance", o) && UE4::plausible(o) ? o : 0;
    if (root == "controller") return ue_->character(ch) && ue_->readPtr(ch, "Controller", o) && UE4::plausible(o) ? o : 0;
    if (!ue_->readPtr(eng, "GameViewport", vp) || !ue_->readPtr(vp, "World", world)) return 0;
    if (root == "world") return UE4::plausible(world) ? world : 0;
    if (root == "gamestate") return ue_->readPtr(world, "GameState", o) && UE4::plausible(o) ? o : 0;
    if (root == "worldsettings") {
        uint64_t lvl = 0;
        return ue_->readPtr(world, "PersistentLevel", lvl) && ue_->readPtr(lvl, "WorldSettings", o) && UE4::plausible(o) ? o : 0;
    }
    return 0;
}

uint64_t ModMakerCard::followPath(Process* p, const Spec& s, int steps) {
    uint64_t o = rootObject(p, s.root);
    for (int i = 0; i < steps && o; ++i) {
        uint64_t n = 0;
        if (i >= s.path.size() || !ue_->readPtr(o, s.path[i].toStdString().c_str(), n) || !UE4::plausible(n)) return 0;
        o = n;
    }
    return o;
}

bool ModMakerCard::resolve(Process* p, const Spec& s, Target& t) {
    if (s.path.isEmpty()) return false;
    const uint64_t o = followPath(p, s, (int)s.path.size() - 1);
    if (!o) return false;
    UE4::Prop pr;
    if (!ue_->find(o, s.path.last().toStdString().c_str(), pr)) return false;
    t.addr = o + pr.offset;
    t.prop = pr;
    return true;
}

bool ModMakerCard::readValue(Process* p, const Target& t, const QString& type, double& v) {
    if (!p) return false;
    const uint64_t a = t.addr;
    if (type == "FloatProperty") { float x; if (!p->read(a, &x, 4)) return false; v = x; return true; }
    if (type == "DoubleProperty") { double x; if (!p->read(a, &x, 8)) return false; v = x; return true; }
    if (type == "IntProperty") { int32_t x; if (!p->read(a, &x, 4)) return false; v = x; return true; }
    if (type == "UInt32Property") { uint32_t x; if (!p->read(a, &x, 4)) return false; v = x; return true; }
    if (type == "Int64Property") { int64_t x; if (!p->read(a, &x, 8)) return false; v = (double)x; return true; }
    if (type == "UInt64Property") { uint64_t x; if (!p->read(a, &x, 8)) return false; v = (double)x; return true; }
    if (type == "Int16Property") { int16_t x; if (!p->read(a, &x, 2)) return false; v = x; return true; }
    if (type == "UInt16Property") { uint16_t x; if (!p->read(a, &x, 2)) return false; v = x; return true; }
    if (type == "Int8Property") { int8_t x; if (!p->read(a, &x, 1)) return false; v = x; return true; }
    if (type == "ByteProperty" || type == "EnumProperty") { uint8_t x; if (!p->read(a, &x, 1)) return false; v = x; return true; }
    if (type == "BoolProperty") {
        uint8_t b;
        if (!t.prop.isBool || !p->read(a + t.prop.byteOffset, &b, 1)) return false;
        v = (b & t.prop.mask) ? 1 : 0;
        return true;
    }
    return false;
}

bool ModMakerCard::writeValue(Process* p, const Target& t, const QString& type, double v) {
    if (!p) return false;
    const uint64_t a = t.addr;
    if (type == "FloatProperty") { float x = (float)v; return p->write(a, &x, 4); }
    if (type == "DoubleProperty") return p->write(a, &v, 8);
    if (type == "IntProperty") { int32_t x = (int32_t)v; return p->write(a, &x, 4); }
    if (type == "UInt32Property") { uint32_t x = (uint32_t)v; return p->write(a, &x, 4); }
    if (type == "Int64Property") { int64_t x = (int64_t)v; return p->write(a, &x, 8); }
    if (type == "UInt64Property") { uint64_t x = (uint64_t)v; return p->write(a, &x, 8); }
    if (type == "Int16Property") { int16_t x = (int16_t)v; return p->write(a, &x, 2); }
    if (type == "UInt16Property") { uint16_t x = (uint16_t)v; return p->write(a, &x, 2); }
    if (type == "Int8Property") { int8_t x = (int8_t)v; return p->write(a, &x, 1); }
    if (type == "ByteProperty" || type == "EnumProperty") { uint8_t x = (uint8_t)v; return p->write(a, &x, 1); }
    if (type == "BoolProperty") {
        uint8_t b;
        if (!t.prop.isBool || !p->read(a + t.prop.byteOffset, &b, 1)) return false;
        b = v > 0.5 ? (uint8_t)(b | t.prop.mask) : (uint8_t)(b & ~t.prop.mask);
        return p->write(a + t.prop.byteOffset, &b, 1);
    }
    return false;
}

ModMakerCard::Spec ModMakerCard::currentSpec(const QString& field, const QString& type) const {
    Spec s;
    s.root = root_->currentData().toString();
    s.path = path_;
    s.path << field;
    s.type = type;
    return s;
}

QString ModMakerCard::describe(const Spec& s) const {
    return rootLabel(s.root) + " > " + s.path.join(" > ");
}

void ModMakerCard::refreshTree() {
    tree_->clear();
    const Spec base{root_->currentData().toString(), path_, QString()};
    crumb_->setText(rootLabel(base.root) + (path_.isEmpty() ? QString() : " > " + path_.join(" > ")));
    Process* p = proc_();
    if (!p) { status_->setText(I18n::tr("Waiting for the game...")); return; }
    const uint64_t obj = followPath(p, base, (int)path_.size());
    if (!obj) { status_->setText(I18n::tr("That object is not available right now. Load into a level, or press Reload.")); return; }
    std::vector<UE4::Field> fields;
    if (!ue_->listFields(ue_->rd64(obj + 0x10), fields) || fields.empty()) { status_->setText(I18n::tr("Could not read the property list for this object.")); return; }
    status_->setText(QString::fromStdString(ue_->className(obj)) + "  (" + QString::number((int)fields.size()) + ")");
    for (const UE4::Field& f : fields) {
        auto* it = new QTreeWidgetItem(tree_);
        QString ty = QString::fromStdString(f.type);
        it->setText(0, QString::fromStdString(f.name));
        QString shown = ty;
        shown.chop(8);
        it->setText(1, shown);
        it->setData(0, Qt::UserRole, ty);
        it->setData(0, Qt::UserRole + 1, (qulonglong)f.offset);
        if (ty == "BoolProperty") {
            UE4::Prop pr;
            if (ue_->find(obj, f.name.c_str(), pr) && pr.isBool) {
                it->setData(0, Qt::UserRole + 2, (uint)pr.byteOffset);
                it->setData(0, Qt::UserRole + 3, (uint)pr.mask);
            }
        }
        if (!numericType(ty) && ty != "ObjectProperty") it->setForeground(0, QBrush(QColor(130, 130, 130)));
    }
    const QString f = filter_->text();
    if (!f.isEmpty())
        for (int i = 0; i < tree_->topLevelItemCount(); ++i) tree_->topLevelItem(i)->setHidden(!tree_->topLevelItem(i)->text(0).contains(f, Qt::CaseInsensitive));
    updateValues();
}

void ModMakerCard::updateValues() {
    if (!isVisible() || tree_->topLevelItemCount() == 0) return;
    Process* p = proc_();
    if (!p) return;
    const Spec base{root_->currentData().toString(), path_, QString()};
    const uint64_t obj = followPath(p, base, (int)path_.size());
    if (!obj) return;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i) {
        auto* it = tree_->topLevelItem(i);
        if (it->isHidden()) continue;
        const QString ty = it->data(0, Qt::UserRole).toString();
        const uint64_t off = it->data(0, Qt::UserRole + 1).toULongLong();
        if (ty == "ObjectProperty") {
            const uint64_t ptr = ue_->rd64(obj + off);
            it->setText(2, UE4::plausible(ptr) ? QString::fromStdString(ue_->className(ptr)) : QString("none"));
        } else if (numericType(ty)) {
            Target t;
            t.addr = obj + off;
            if (ty == "BoolProperty") {
                t.prop.isBool = true;
                t.prop.byteOffset = (uint8_t)it->data(0, Qt::UserRole + 2).toUInt();
                t.prop.mask = (uint8_t)it->data(0, Qt::UserRole + 3).toUInt();
            }
            double v = 0;
            if (readValue(p, t, ty, v)) it->setText(2, ty == "BoolProperty" ? QString(v > 0.5 ? "On" : "Off") : QString::number(v, 'g', 9));
            else it->setText(2, "?");
        }
    }
}

void ModMakerCard::selectField(QTreeWidgetItem* it) {
    if (!it) return;
    const QString ty = it->data(0, Qt::UserRole).toString();
    if (!numericType(ty)) return;
    selected_ = currentSpec(it->text(0), ty);
    haveSel_ = true;
    selLabel_->setText(I18n::tr("Selected: ") + describe(selected_) + "  (" + it->text(1) + ")   " + I18n::tr("now: ") + it->text(2));
    bool ok = false;
    const double cur = it->text(2) == "On" ? 1.0 : it->text(2) == "Off" ? 0.0 : it->text(2).toDouble(&ok);
    if (ok || it->text(2) == "On" || it->text(2) == "Off") value_->setValue(cur);
}

void ModMakerCard::descend(QTreeWidgetItem* it) {
    if (!it || it->data(0, Qt::UserRole).toString() != "ObjectProperty") return;
    path_ << it->text(0);
    haveSel_ = false;
    refreshTree();
    if (tree_->topLevelItemCount() == 0) { path_.removeLast(); refreshTree(); }      // nothing there (null pointer): stay where we were
}

// ---- finding objects by class name (walks the object table on a background thread)
void ModMakerCard::scanWorker(Process* p, const std::string& needle, bool exact, std::vector<Hit>& out) {
    std::string err;
    UE4::ObjArray oa;
    if (!p || !ue_->objectArray(oa, err)) return;
    const std::string nl = toLowerStr(needle);
    std::map<uint64_t, std::pair<bool, std::string>> clsCache;
    std::vector<uint8_t> items((size_t)oa.perChunk * 0x18);
    int32_t index = 0;
    for (int32_t ch = 0; ch < oa.numChunks && index < oa.num; ++ch) {
        const uint64_t chunk = ue_->rd64(oa.chunks + (uint64_t)ch * 8);
        if (!UE4::plausible(chunk)) { index += oa.perChunk; continue; }
        const int32_t inChunk = std::min(oa.perChunk, oa.num - index);
        if (!p->read(chunk, items.data(), (size_t)inChunk * 0x18)) { index += inChunk; continue; }
        for (int32_t i = 0; i < inChunk; ++i, ++index) {
            uint64_t o;
            std::memcpy(&o, &items[(size_t)i * 0x18], 8);
            if (!UE4::plausible(o)) continue;
            uint64_t cls = 0;
            if (!p->read(o + 0x10, &cls, 8) || !UE4::plausible(cls)) continue;
            auto it = clsCache.find(cls);
            if (it == clsCache.end()) {
                const std::string cn = ue_->objectName(cls);
                const std::string cl = toLowerStr(cn);
                it = clsCache.emplace(cls, std::make_pair(exact ? cn == needle : cl.find(nl) != std::string::npos, cn)).first;
            }
            if (!it->second.first) continue;
            const std::string nm = ue_->objectName(o);
            if (nm.compare(0, 9, "Default__") == 0 || nm.compare(0, 5, "SKEL_") == 0 || nm.compare(0, 6, "REINST") == 0) continue;
            Hit h;
            h.addr = o;
            h.cls = it->second.second;
            h.name = nm;
            h.transient = underTransient(*ue_, o);
            out.push_back(h);
            if (out.size() >= 600) return;
        }
    }
}

void ModMakerCard::startScan(const QString& needle, bool exact, bool forUi) {
    Process* p = proc_();
    if (!ready(p)) { status_->setText(I18n::tr("Waiting for the game...")); return; }
    if (scanBusy_.exchange(true)) { status_->setText(I18n::tr("A search is already running.")); return; }
    if (scanThread_) { scanThread_->wait(); delete scanThread_; scanThread_ = nullptr; }
    scanNeedle_ = needle.toStdString();
    scanExact_ = exact;
    scanForUi_ = forUi;
    if (forUi) status_->setText(I18n::tr("Searching..."));
    const std::string nd = scanNeedle_;
    scanThread_ = QThread::create([this, p, nd, exact]() {
        std::vector<Hit> hits;
        scanWorker(p, nd, exact, hits);
        { CsGuard g(scanMu_); scanHits_ = hits; }
        scanDone_ = true;
        scanBusy_ = false;
    });
    scanThread_->start();
}

void ModMakerCard::pollScan() {
    if (!scanDone_.exchange(false)) return;
    std::vector<Hit> hits;
    { CsGuard g(scanMu_); hits = scanHits_; }
    std::map<std::string, std::vector<Hit>> byClass;
    for (const Hit& h : hits) byClass[h.cls].push_back(h);
    // the live instance of each class (the one under Transient) is the one worth looking at
    for (auto& kv : byClass) {
        Hit best = kv.second.front();
        for (const Hit& h : kv.second) if (h.transient) { best = h; break; }
        classCache_[QString("class:") + QString::fromStdString(kv.first)] = best.addr;
    }
    if (!scanForUi_) return;
    if (byClass.empty()) { status_->setText(I18n::tr("Nothing found. Check the spelling, and load into a level.")); return; }
    int added = 0;
    for (auto& kv : byClass) {
        const QString key = QString("class:") + QString::fromStdString(kv.first);
        if (root_->findData(key) < 0) {
            root_->addItem(QString::fromStdString(kv.first) + "  (" + QString::number((int)kv.second.size()) + ")", key);
            ++added;
        }
        if (added >= 60) break;
    }
    status_->setText(I18n::tr("Found classes: ") + QString::number((int)byClass.size()) + I18n::tr(". They are at the end of the first list."));
    if (added > 0) root_->setCurrentIndex(root_->count() - added);
}

// ---- saved mods
void ModMakerCard::applyAll() {
    bool any = false;
    for (const Custom& c : mods_) if (c.on) { any = true; break; }
    if (!any) return;
    Process* p = proc_();
    if (!p || !ready(p)) return;
    for (const Custom& c : mods_) {
        if (!c.on) continue;
        Target t;
        if (resolve(p, c.spec, t)) writeValue(p, t, c.spec.type, c.value);
    }
}

QString ModMakerCard::toCode(const Custom& c) const {
    QJsonObject o;
    o["name"] = c.name;
    o["root"] = c.spec.root;
    o["path"] = QJsonArray::fromStringList(c.spec.path);
    o["type"] = c.spec.type;
    o["value"] = c.value;
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool ModMakerCard::fromCode(const QString& json, int& added) {
    added = 0;
    if (json.size() > 200000) return false;
    const QJsonDocument d = QJsonDocument::fromJson(json.toUtf8());
    QJsonArray arr;
    if (d.isArray()) arr = d.array();
    else if (d.isObject()) arr.append(d.object());
    else return false;
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        Custom c;
        c.name = o["name"].toString().left(60);
        c.spec.root = o["root"].toString();
        for (const QJsonValue& s : o["path"].toArray()) c.spec.path << s.toString();
        c.spec.type = o["type"].toString();
        c.value = o["value"].toDouble();
        if (c.name.isEmpty() || c.spec.root.isEmpty() || c.spec.path.isEmpty() || c.spec.path.size() > 12 || !numericType(c.spec.type)) continue;
        c.on = false;                                           // imported mods always start switched off
        mods_.push_back(c);
        ++added;
    }
    return true;
}

void ModMakerCard::saveMods() {
    QJsonArray arr;
    for (const Custom& c : mods_) arr.append(QJsonDocument::fromJson(toCode(c).toUtf8()).object());
    QSettings().setValue("modmaker/mods", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void ModMakerCard::loadMods() {
    int added = 0;
    fromCode(QSettings().value("modmaker/mods").toString(), added);      // always starts switched off after a restart
}

void ModMakerCard::refreshList() {
    building_ = true;
    list_->clear();
    for (const Custom& c : mods_) {
        auto* it = new QListWidgetItem(c.name + "   [" + describe(c.spec) + " = " + QString::number(c.value, 'g', 9) + "]", list_);
        it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
        it->setCheckState(c.on ? Qt::Checked : Qt::Unchecked);
    }
    building_ = false;
}

} // namespace dk
