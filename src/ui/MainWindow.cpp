#include <QScrollBar>
#include "MainWindow.h"
#include <algorithm>

#include <QApplication>
#include <QGuiApplication>
#include <QShowEvent>
#include <QStandardPaths>
#include <QThread>
#include <QDesktopServices>
#include <QDir>
#include <memory>
#include <QUrl>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMessageBox>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPushButton>
#include <QToolButton>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSettings>
#include <QSet>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QRadioButton>
#include <QSlider>
#include "ClickSound.h"
#include "AppSound.h"
#include <QDoubleSpinBox>
#include <QClipboard>
#include <QTreeWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
#include <QVBoxLayout>
#include <climits>
#include "Icons.h"
#include "I18n.h"
#include "SaveBackup.h"
#include "Identity.h"
#include "GameFolders.h"
#include "ItemsPage.h"
#include "ItemViewerPage.h"
#include "Logo.h"
#include "TeleportsPage.h"
#include "TitleBar.h"
#include "../mods/PlayerTools.h"
#include "../core/UE4.h"
#include "../core/UE4Dump.h"
#include "ProbeCard.h"
#include "NameHideCard.h"
#include "WidgetHideCard.h"
#include "ConsoleCommands.h"
#include "ConsoleCommands2.h"
#include "Mcd1Pages.h"
#include "ShopPricesCard.h"
#include "ModMaker.h"
#include "DiscordPresence.h"
#include "Mcd2Pages.h"
#include "../Version.h"

namespace dk {

static const QString kAppName = QStringLiteral(DK_APP_NAME);
static const QString kAppVersion = QStringLiteral(DK_APP_VERSION);

// ====================================================================== construction
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QString("%1 v%2").arg(kAppName, kAppVersion));
    resize(1080, 820);
    setMinimumSize(920, 760);

    game_ = makeMinecraftDungeons();
    theme_.load();
    if (QSettings().value("theme/randomStartup", false).toBool()) theme_ = Theme::randomized(theme_);     // Random Startup
    username_ = QSettings().value("account/username", "").toString();
    target_ = Process::Target::None;                                  // always starts on Default (not connected)

    auto* root = new QWidget(this);
    auto* h = new QHBoxLayout(root);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);
    h->addWidget(buildSidebar());

    pages_ = new QStackedWidget(root);
    pages_->addWidget(buildHome());                                   // 0 Home
    itemsPage_ = new ItemsPage(proc_, game_->mods.items, this);
    pages_->addWidget(itemsPage_);                                    // 1 Items
    pages_->addWidget(buildMods());                                   // 2 Player
    teleportsPage_ = new TeleportsPage(proc_, game_->mods.position, this);
    pages_->addWidget(new QWidget);                                   // 3 (Teleports moved into the Minecraft Dungeons World page)
    pages_->addWidget(buildNoClip());                                 // 4 No Clip & Stats
    pages_->addWidget(buildTools());                                  // 5 Tools
    viewerPage_ = new ItemViewerPage(game_->mods.viewer, this);
    pages_->addWidget(viewerPage_);                                   // 6 Item Stat Viewer
    toggles_.push_back({game_->mods.viewer, viewerPage_->toggle()});
    {
        Mod* vm = game_->mods.viewer;
        QCheckBox* vb = viewerPage_->toggle();
        connect(vb, &QCheckBox::clicked, this, [this, vm, vb](bool on) { toggleMod(vm, vb, on); });
        connect(viewerPage_, &ItemViewerPage::disableAllRequested, this, &MainWindow::disableAll);
    }
    connect(teleportsPage_, &TeleportsPage::message, this, &MainWindow::setMessage);
    toggles_.push_back({game_->mods.position, teleportsPage_->toggle()});
    {
        Mod* pm = game_->mods.position;
        QCheckBox* pb = teleportsPage_->toggle();
        connect(pb, &QCheckBox::clicked, this, [this, pm, pb](bool on) { toggleMod(pm, pb, on); });
    }
    connect(itemsPage_, &ItemsPage::message, this, &MainWindow::setMessage);
    connect(itemsPage_, &ItemsPage::disableAllRequested, this, &MainWindow::disableAll);
    toggles_.push_back({game_->mods.items, itemsPage_->toggle()});
    {
        Mod* im = game_->mods.items;
        QCheckBox* ib = itemsPage_->toggle();
        connect(ib, &QCheckBox::clicked, this, [this, im, ib](bool on) { toggleMod(im, ib, on); });
    }
    pages_->addWidget(buildPakDirectory());                           // 7 Pak Directory
    pages_->addWidget(buildWebsites());                               // 8 Websites
    mcd2_ = std::make_unique<Mcd2>(game_->mods.ue4);
    mcd2Hub_ = new Mcd2Hub(*mcd2_, [this]() -> bool { return mcd2Ready(); }, this);
    connect(mcd2Hub_, &Mcd2Hub::message, this, &MainWindow::setMessage);
    connect(mcd2Hub_, &Mcd2Hub::disableAllRequested, this, &MainWindow::disableAll);
    pages_->addWidget(buildAccount());                                // 9
    pages_->addWidget(buildCustomize());
    pages_->addWidget(buildInformation());
    pages_->addWidget(buildBackup());                                 // 12 Backup Saves
    pages_->addWidget(buildChangelog());                              // 13 Changelog
    pages_->addWidget(buildTest());                                   // 14 Test
    pages_->addWidget(mcd2Hub_->playerPage());                        // 15 MCD2 Player
    pages_->addWidget(mcd2Hub_->inventoryPage());                     // 16 MCD2 Inventory
    pages_->addWidget(mcd2Hub_->worldPage());                         // 17 MCD2 World
    pages_->addWidget(mcd2Hub_->cameraPage());                        // 18 Camera Editor
    pages_->addWidget(mcd2Hub_->nearbyPage());                        // 19 Nearby Viewer
    pages_->addWidget(mcd2Hub_->editorPage());                        // 20 Inventory Editor
    pages_->addWidget(mcd2Hub_->motionPage());                        // 21 No Clip & Stats (Dungeons II)
    {
        auto mcd1Proc = [this]() -> Process* { return target_ == Process::Target::Dungeons && proc_.attached() && proc_.alive() ? &proc_ : nullptr; };
        mcd1Cam_ = new Mcd1CameraPage(game_->mods.ue4, mcd1Proc, this);
        mcd1World_ = new Mcd1WorldPage(game_->mods.ue4, mcd1Proc, this);
        shopCard_ = new ShopPricesCard(game_->mods.currency, mcd1Proc, this);
        connect(shopCard_, &ShopPricesCard::message, this, &MainWindow::setMessage);
        itemsPage_->addExtra(shopCard_);
        connect(mcd1Cam_, &Mcd1CameraPage::message, this, &MainWindow::setMessage);
        connect(mcd1Cam_, &Mcd1CameraPage::disableAllRequested, this, &MainWindow::disableAll);
        connect(mcd1World_, &Mcd1WorldPage::message, this, &MainWindow::setMessage);
        connect(mcd1World_, &Mcd1WorldPage::disableAllRequested, this, &MainWindow::disableAll);
        pages_->addWidget(mcd1Cam_);                                      // 22 Camera Editor (Minecraft Dungeons)
        mcd1World_->addTeleports(teleportsPage_);
        pages_->addWidget(mcd1World_);                                    // 23 World (Minecraft Dungeons)
        pages_->addWidget(buildExperiments());                            // 24 Experiments (hidden until BETA is on)
    }
    h->addWidget(pages_, 1);
    setCentralWidget(root);

    connect(navGroup_, &QButtonGroup::idClicked, pages_, &QStackedWidget::setCurrentIndex);
    openedAt_ = QDateTime::currentDateTime();
    logEvent(QString("DungeonKit %1 opened.").arg(DK_APP_VERSION));
    connect(navGroup_, &QButtonGroup::idClicked, this, [this](int id) {
        if (auto* b = navGroup_->button(id)) logEvent(QString("Opened page: %1").arg(b->text().trimmed()));
    });

    applyTheme();
    setStatus("off", "Offline", "Default: choose a game on the Home page.");
    refreshEnabledState();

    pollTimer_ = new QTimer(this);
    connect(pollTimer_, &QTimer::timeout, this, &MainWindow::pollGame);
    pollTimer_->start(1000);
    tickTimer_ = new QTimer(this);
    connect(tickTimer_, &QTimer::timeout, this, &MainWindow::tickValues);
    tickTimer_->start(150);
    i18nTimer_ = new QTimer(this);
    connect(i18nTimer_, &QTimer::timeout, this, [this]() { if (I18n::language() != I18n::English) I18n::retranslate(this); });
    i18nTimer_->start(300);
    discordStart_ = QDateTime::currentSecsSinceEpoch();
    discordTimer_ = new QTimer(this);
    connect(discordTimer_, &QTimer::timeout, this, [this]() { updatePresence(false); });
    discordTimer_->start(5000);
    pollGame();
    applyLanguage();
    QSettings().remove("theme/lcd");                 // the animated LCD theme was removed
}

MainWindow::~MainWindow() {
    if (QSettings().value("theme/randomStartup", false).toBool()) { Theme t = Theme::randomized(theme_); t.save(); }   // Random Startup also picks one on closing
    // Leave the game exactly as we found it.
    if (proc_.attached()) disableAllMods();
    if (discord_) discord_->clear();
}

// Discord Rich Presence: shows that DungeonKit is open, its version and which game it is connected to.
void MainWindow::updatePresence(bool force) {
    const bool on = QSettings().value("discord/richPresence", false).toBool();
    auto status = [this](const QString& t) { if (discordStatus_) discordStatus_->setText(I18n::tr(t)); };
    if (!on) {
        if (discord_) discord_->clear();
        discordLast_.clear();
        status("Rich Presence is off.");
        return;
    }
    if (!discord_) discord_ = std::make_shared<DiscordPresence>();
    if (!discordStart_) discordStart_ = QDateTime::currentSecsSinceEpoch();
    const bool online = proc_.attached() && proc_.alive();
    QString game;
    if (online) {
        const bool second = target_ == Process::Target::Dungeons2 || target_ == Process::Target::Dungeons2Launcher;
        game = second ? "Minecraft Dungeons II" : "Minecraft Dungeons";
        if (const auto v = proc_.version()) game += QString(" (v%1)").arg(QString::fromLatin1(v->name));
    }
    const QString details = QString("DungeonKit v%1").arg(kAppVersion);
    const QString state = online ? QString("Connected to %1").arg(game) : QString("Open, no game connected");
    const QString key = details + "|" + state;
    if (!force && key == discordLast_ && discord_->connected()) return;
    if (discord_->update(details, state, discordStart_)) {
        discordLast_ = key;
        status(username_.isEmpty() ? QString("Rich Presence is on.") : I18n::tr("Rich Presence is on, shown as ") + username_);   // the DungeonKit name you are logged in as
    } else {
        discordLast_.clear();
        status("Waiting for the Discord app. Open Discord on this computer and it will connect on its own.");
    }
}

QPixmap MainWindow::logoPixmap(int size) const {
    return Logo::pixmap(size, theme_.accent, Logo::recolorEnabled());
}

static const int kNavCount = 24;
static const char* const kNavNames[kNavCount + 1] = {"Home", "Item Editor", "Player", "Teleports", "No Clip && Stats", "Tools", "Item Stat Viewer",
                                          "Pak Directory", "Websites", "Account", "Customize", "Information", "Backup Saves", "Changelog", "Tools", "Player", "Inventory Viewer", "World", "Camera && Display", "Nearby Viewer", "Inventory Editor", "No Clip && Stats", "Camera Editor", "World", "Experiments"};

std::vector<MainWindow::NavCat> MainWindow::defaultNavLayout() {
    return {{"DUNGEON KIT", {0}},
            {"MINECRAFT DUNGEONS", {1, 2, 4, 22, 23, 5, 6}},
            {"QUICK ACCESS", {7, 8, 12, 13}},
            {"MINECRAFT DUNGEONS II", {20, 15, 21, 17, 19, 18, 14}},
            {"MISC", {9, 10, 11}}};
}

void MainWindow::loadNavLayout() {
    navCats_ = defaultNavLayout();
    // 1.18.0 moved Tools to the bottom of the Minecraft Dungeons II category; apply that once to saved layouts
    const bool moveTools = !QSettings().value("sidebar/toolsAtBottom").toBool();
    QSettings().setValue("sidebar/toolsAtBottom", true);
    const bool reorderD2 = !QSettings().value("sidebar/d2Order2").toBool();     // 1.19.0: viewer and editor first, then Player, World ...
    QSettings().setValue("sidebar/d2Order2", true);
    const QStringList saved = QSettings().value("sidebar/layout").toStringList();
    if (saved.isEmpty()) return;
    std::vector<NavCat> cats;
    std::vector<int> seen(kNavCount, 0);
    bool ok = true;
    for (const QString& s : saved) {
        const int eq = s.indexOf('=');
        if (eq <= 0) { ok = false; break; }
        NavCat c;
        c.name = s.left(eq);
        for (const QString& p : s.mid(eq + 1).split(',', Qt::SkipEmptyParts)) {
            bool good = false;
            const int id = p.toInt(&good);
            if (!good || id < 0 || id >= kNavCount || seen[id]++) { ok = false; break; }
            c.pages.push_back(id);
        }
        if (!ok) break;
        cats.push_back(c);
    }
    if (!ok || cats.empty()) return;
    // older builds called the Dungeons II category TEST and kept it last; rename it and keep MISC at the bottom
    for (auto& c : cats) if (c.name == "TEST") c.name = "MINECRAFT DUNGEONS II";
    for (auto& c : cats) if (c.name == "MODDING") c.name = "MINECRAFT DUNGEONS";
    {
        size_t d2 = cats.size(), misc = cats.size();
        for (size_t i = 0; i < cats.size(); ++i) { if (cats[i].name == "MINECRAFT DUNGEONS II") d2 = i; if (cats[i].name == "MISC") misc = i; }
        if (d2 < cats.size() && misc < cats.size() && d2 > misc) { NavCat moved = cats[d2]; cats.erase(cats.begin() + d2); cats.insert(cats.begin() + misc, moved); }
    }
    for (auto& c : cats) c.pages.erase(std::remove(c.pages.begin(), c.pages.end(), 16), c.pages.end());     // the Inventory Viewer is no longer in the sidebar
    seen[16] = 1;
    for (auto& c : cats) c.pages.erase(std::remove(c.pages.begin(), c.pages.end(), 3), c.pages.end());      // Teleports now lives inside World
    seen[3] = 1;
    // pages added by a newer version go into QUICK ACCESS (or the last category)
    size_t home = cats.size() - 1;
    for (size_t i = 0; i < cats.size(); ++i) if (cats[i].name == "QUICK ACCESS") home = i;
    for (int id = 0; id < kNavCount; ++id) {
        if (seen[id]) continue;
        if (id >= 22) {                                   // Minecraft Dungeons Camera Editor and World: after No Clip & Stats
            size_t t = home;
            for (size_t i = 0; i < cats.size(); ++i) if (cats[i].name == "MINECRAFT DUNGEONS") t = i;
            auto& pg = cats[t].pages;
            auto afterNoClip = std::find(pg.begin(), pg.end(), 4);
            if (afterNoClip != pg.end()) pg.insert(afterNoClip + 1, id); else pg.push_back(id);
            if (id == 23) {                                // keep Camera Editor before World
                auto w = std::find(pg.begin(), pg.end(), 23), c = std::find(pg.begin(), pg.end(), 22);
                if (c != pg.end() && w != pg.end() && c > w) std::iter_swap(c, w);
            }
        } else if (id >= 14) {                            // the Minecraft Dungeons II pages live in their own category
            size_t t = cats.size();
            for (size_t i = 0; i < cats.size(); ++i) if (cats[i].name == "MINECRAFT DUNGEONS II") t = i;
            if (t == cats.size()) {
                size_t misc = cats.size();
                for (size_t i = 0; i < cats.size(); ++i) if (cats[i].name == "MISC") misc = i;
                cats.insert(cats.begin() + misc, NavCat{"MINECRAFT DUNGEONS II", {}});
                t = misc;
            }
            auto& pg = cats[t].pages;
            auto after = std::find(pg.begin(), pg.end(), 16);            // the Inventory Editor goes right after the Inventory Viewer
            auto afterPlayer = std::find(pg.begin(), pg.end(), 15);          // No Clip & Stats goes right after Player
            if (id == 21 && afterPlayer != pg.end()) pg.insert(afterPlayer + 1, id);
            else if (id == 20 && after != pg.end()) pg.insert(after + 1, id);
            else pg.push_back(id);
        } else cats[home].pages.push_back(id);
    }
    if (reorderD2)
        for (auto& c : cats)
            if (std::find(c.pages.begin(), c.pages.end(), 15) != c.pages.end()) {
                static const int order[] = {20, 15, 21, 17, 19, 18, 14};
                std::vector<int> np;
                for (int id : order) if (std::find(c.pages.begin(), c.pages.end(), id) != c.pages.end()) np.push_back(id);
                for (int id : c.pages) if (std::find(np.begin(), np.end(), id) == np.end()) np.push_back(id);
                c.pages = np;
                break;
            }
    if (moveTools)
        for (auto& c : cats) {
            auto it = std::find(c.pages.begin(), c.pages.end(), 14);
            if (it != c.pages.end()) { c.pages.erase(it); c.pages.push_back(14); break; }
        }
    navCats_ = cats;
}

void MainWindow::saveNavLayout() {
    QStringList out;
    for (const NavCat& c : navCats_) {
        QStringList ids;
        for (int p : c.pages) ids << QString::number(p);
        out << c.name + "=" + ids.join(',');
    }
    QSettings().setValue("sidebar/layout", out);
}

void MainWindow::rebuildNav() {
    auto* nl = qobject_cast<QVBoxLayout*>(navHolder_->layout());
    for (QPushButton* b : navButtons_) if (b) navGroup_->removeButton(b);
    while (QLayoutItem* it = nl->takeAt(0)) {
        if (it->widget()) it->widget()->deleteLater();
        delete it;
    }
    navButtons_.assign(kNavCount + 1, nullptr);
    const int current = pages_ ? pages_->currentIndex() : 0;
    for (const NavCat& c : navCats_) {
        if (c.pages.empty()) continue;
        auto* sec = new QLabel(c.name);
        sec->setObjectName("Section");
        nl->addWidget(sec);
        for (int id : c.pages) {
            auto* b = new QPushButton(QString::fromLatin1(kNavNames[id]));
            b->setObjectName("Nav");
            b->setCheckable(true);
            b->setCursor(Qt::PointingHandCursor);
            b->setIconSize(QSize(18, 18));
            if (id == current) b->setChecked(true);
            navButtons_[id] = b;
            navGroup_->addButton(b, id);
            nl->addWidget(b);
            if (id == 0 && betaOn_) {                          // the BETA tab sits right under Home
                auto* eb = new QPushButton(QString::fromLatin1(kNavNames[24]));
                eb->setObjectName("Nav");
                eb->setCheckable(true);
                eb->setCursor(Qt::PointingHandCursor);
                eb->setIconSize(QSize(18, 18));
                if (current == 24) eb->setChecked(true);
                navButtons_[24] = eb;
                navGroup_->addButton(eb, 24);
                nl->addWidget(eb);
            }
        }
    }
    refreshNavIcons();
}

void MainWindow::fillNavTree(int selCat, int selPos) {
    navTree_->clear();
    for (size_t ci = 0; ci < navCats_.size(); ++ci) {
        auto* top = new QTreeWidgetItem(navTree_, QStringList(I18n::tr(navCats_[ci].name)));
        top->setData(0, Qt::UserRole, -1);
        QFont f = top->font(0);
        f.setBold(true);
        top->setFont(0, f);
        for (int id : navCats_[ci].pages) {
            QString n = QString::fromLatin1(kNavNames[id]);
            n.replace("&&", "&");
            n = I18n::tr(n);
            auto* it = new QTreeWidgetItem(top, QStringList(n));
            it->setData(0, Qt::UserRole, id);
        }
        top->setExpanded(true);
    }
    if (selCat >= 0 && selCat < navTree_->topLevelItemCount()) {
        QTreeWidgetItem* top = navTree_->topLevelItem(selCat);
        navTree_->setCurrentItem(selPos >= 0 && selPos < top->childCount() ? top->child(selPos) : top);
    }
}

void MainWindow::moveNav(int dir) {          // dir: -1 up, +1 down
    QTreeWidgetItem* cur = navTree_->currentItem();
    if (!cur) return;
    int cat, pos = -1;
    if (cur->parent()) { cat = navTree_->indexOfTopLevelItem(cur->parent()); pos = cur->parent()->indexOfChild(cur); }
    else cat = navTree_->indexOfTopLevelItem(cur);
    const int nc = int(navCats_.size());
    if (pos < 0) {                                           // moving a whole category
        const int to = cat + dir;
        if (to < 0 || to >= nc) return;
        std::swap(navCats_[cat], navCats_[to]);
        cat = to;
    } else {                                                 // moving one page
        auto& pg = navCats_[cat].pages;
        const int to = pos + dir;
        if (to >= 0 && to < int(pg.size())) {
            std::swap(pg[pos], pg[to]);
            pos = to;
        } else {                                             // step into the neighbouring category
            const int nc2 = cat + dir;
            if (nc2 < 0 || nc2 >= nc) return;
            const int id = pg[pos];
            pg.erase(pg.begin() + pos);
            auto& dst = navCats_[nc2].pages;
            if (dir < 0) { dst.push_back(id); pos = int(dst.size()) - 1; }
            else         { dst.insert(dst.begin(), id); pos = 0; }
            cat = nc2;
        }
    }
    saveNavLayout();
    rebuildNav();
    fillNavTree(cat, pos);
}

QWidget* MainWindow::buildSidebar() {
    auto* side = new QFrame;
    side->setObjectName("Sidebar");
    side->setFixedWidth(230);
    auto* v = new QVBoxLayout(side);
    v->setContentsMargins(16, 20, 16, 16);
    v->setSpacing(3);

    auto* head = new QHBoxLayout;
    sidebarLogo_ = new QLabel;
    sidebarLogo_->setFixedSize(44, 44);
    sidebarLogo_->setPixmap(logoPixmap(44));
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    auto* title = new QLabel(kAppName);
    title->setObjectName("Title");
    auto* ver = new QLabel(QString("v%1").arg(kAppVersion));
    ver->setObjectName("Muted");
    titles->addWidget(title);
    titles->addWidget(ver);
    head->addWidget(sidebarLogo_);
    head->addSpacing(8);
    head->addLayout(titles, 1);
    v->addLayout(head);
    v->addSpacing(18);

    navGroup_ = new QButtonGroup(this);
    navGroup_->setExclusive(true);
    navHolder_ = new QWidget;
    auto* nl = new QVBoxLayout(navHolder_);
    nl->setContentsMargins(0, 0, 0, 0);
    nl->setSpacing(3);
    auto* navScroll = new QScrollArea;
    navScroll->setObjectName("NavScroll");
    navScroll->setWidgetResizable(true);
    navScroll->setFrameShape(QFrame::NoFrame);
    navScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    navScroll->setStyleSheet("QScrollArea#NavScroll, QScrollArea#NavScroll > QWidget > QWidget { background: transparent; }");
    navScroll->viewport()->setAutoFillBackground(false);
    navHolder_->setAutoFillBackground(false);
    navScroll->setWidget(navHolder_);
    v->addWidget(navScroll, 1);
    navButtons_.assign(kNavCount + 1, nullptr);
    loadNavLayout();
    rebuildNav();

    sidebarUser_ = new QLabel;
    sidebarUser_->setObjectName("Muted");
    sidebarUser_->setAlignment(Qt::AlignCenter);
    v->addWidget(sidebarUser_);
    refreshNavIcons();

    auto* box = new QFrame;
    box->setObjectName("StatusBox");
    auto* bv = new QVBoxLayout(box);
    bv->setContentsMargins(12, 10, 12, 10);
    bv->setSpacing(2);
    auto* top = new QHBoxLayout;
    statusDot_ = new QLabel;
    statusDot_->setFixedSize(10, 10);
    statusTitle_ = new QLabel;
    statusTitle_->setObjectName("CardTitle");
    top->addWidget(statusDot_);
    top->addWidget(statusTitle_, 1);
    statusLine1_ = new QLabel;
    statusLine1_->setObjectName("Hint");
    statusLine1_->setWordWrap(true);
    statusLine2_ = new QLabel;
    statusLine2_->setObjectName("Hint");
    statusLine2_->setWordWrap(true);
    for (QLabel* sl : {statusTitle_, statusLine1_, statusLine2_}) sl->setProperty("dkSkip", true);     // the status card is always English
    bv->addLayout(top);
    bv->addWidget(statusLine1_);
    bv->addWidget(statusLine2_);
    v->addWidget(box);
    return side;
}

QFrame* MainWindow::cardStandalone(const QString& title) {
    auto* f = new QFrame;
    f->setObjectName("Card");
    auto* v = new QVBoxLayout(f);
    v->setContentsMargins(16, 14, 16, 14);
    v->setSpacing(8);
    auto* t = new QLabel(title);
    t->setObjectName("CardTitle");
    v->addWidget(t);
    return f;
}

QFrame* MainWindow::card(QVBoxLayout* into, const QString& title) {
    auto* f = cardStandalone(title);
    into->addWidget(f);
    return f;
}

QWidget* MainWindow::wrapCollapsible(QVBoxLayout* into, QWidget* content, const QString& key, bool defaultOpen) {
    auto* wrap = new QWidget;
    wrap->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(wrap);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(6);
    auto* head = new QToolButton;
    head->setObjectName("CollapsibleHeader");
    head->setCheckable(true);
    head->setAutoRaise(true);
    head->setCursor(Qt::PointingHandCursor);
    head->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    QString titleText = "Section";
    if (auto* t = content->findChild<QLabel*>("CardTitle")) { titleText = t->text(); t->hide(); }
    head->setText(titleText);
    v->addWidget(head);
    v->addWidget(content);
    into->addWidget(wrap);

    const QString sk = "experiments/open_" + key;
    const bool open = QSettings().value(sk, defaultOpen).toBool();
    content->setVisible(open);
    head->setChecked(open);
    head->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
    connect(head, &QToolButton::clicked, this, [content, head, sk](bool checked) {
        content->setVisible(checked);
        head->setArrowType(checked ? Qt::DownArrow : Qt::RightArrow);
        QSettings().setValue(sk, checked);
    });
    return wrap;
}

void MainWindow::addToggle(QVBoxLayout* into, Mod* mod) {
    auto* box = new QCheckBox(QString::fromStdString(mod->name()));
    box->setCursor(Qt::PointingHandCursor);
    auto* desc = new QLabel(QString::fromStdString(mod->description()));
    desc->setObjectName("Hint");
    desc->setWordWrap(true);
    desc->setContentsMargins(46, 0, 0, 4);
    into->addWidget(box);
    into->addWidget(desc);
    toggles_.push_back({mod, box});
    connect(box, &QCheckBox::clicked, this, [this, mod, box](bool on) { toggleMod(mod, box, on); });
}

void MainWindow::addIntRow(QVBoxLayout* into, const QString& label, std::function<bool(int32_t&)> get,
                           std::function<bool(int32_t)> set, Mod* owner, bool withFreeze) {
    auto* w = new QWidget;
    w->setStyleSheet("background: transparent;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* l = new QLabel(label);
    l->setMinimumWidth(110);
    auto* spin = new QSpinBox;
    spin->setRange(INT_MIN, INT_MAX);
    spin->setMinimumWidth(130);
    auto* apply = new QPushButton("Set");
    QCheckBox* freeze = withFreeze ? new QCheckBox("Freeze") : nullptr;
    h->addWidget(l);
    h->addWidget(spin, 1);
    h->addWidget(apply);
    if (freeze) h->addWidget(freeze);
    into->addWidget(w);

    Row r;
    r.owner = owner; r.ispin = spin; r.freeze = freeze; r.iget = get; r.iset = set; r.holder = w;
    rows_.push_back(r);
    connect(apply, &QPushButton::clicked, this, [this, spin, set]() {
        if (!set(spin->value())) setMessage("Not linked yet - the game has not run that code. Do something in-game (pick up an emerald, open inventory...).");
        spin->clearFocus();
    });
}

void MainWindow::addFloatRow(QVBoxLayout* into, const QString& label, std::function<bool(float&)> get,
                             std::function<bool(float)> set, Mod* owner) {
    auto* w = new QWidget;
    w->setStyleSheet("background: transparent;");
    auto* h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    auto* l = new QLabel(label);
    l->setMinimumWidth(110);
    auto* spin = new QDoubleSpinBox;
    spin->setRange(-1.0e9, 1.0e9);
    spin->setDecimals(3);
    spin->setMinimumWidth(130);
    auto* apply = new QPushButton("Set");
    auto* freeze = new QCheckBox("Freeze");
    h->addWidget(l);
    h->addWidget(spin, 1);
    h->addWidget(apply);
    h->addWidget(freeze);
    into->addWidget(w);

    Row r;
    r.owner = owner; r.fspin = spin; r.freeze = freeze; r.fget = get; r.fset = set; r.holder = w;
    rows_.push_back(r);
    connect(apply, &QPushButton::clicked, this, [this, spin, set]() {
        if (!set((float)spin->value())) setMessage("Not linked yet - the game has not run that code yet.");
        spin->clearFocus();
    });
}

// ====================================================================== Home
QWidget* MainWindow::buildHome() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);

    auto* title = new QLabel("Home");
    title->setObjectName("PageTitle");
    outer->addWidget(title);
    welcome_ = new QLabel;
    welcome_->setObjectName("Muted");
    outer->addWidget(welcome_);
    welcome_->setText(username_.isEmpty() ? "Welcome." : QString("Welcome back, %1.").arg(username_));

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);

    {   // status
        auto* c = card(v, "Status");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* row = new QHBoxLayout;
        homeDot_ = new QLabel;
        homeDot_->setFixedSize(14, 14);
        homeState_ = new QLabel;
        homeState_->setObjectName("PageTitle");
        row->addWidget(homeDot_);
        row->addSpacing(6);
        row->addWidget(homeState_);
        row->addStretch(1);
        cl->addLayout(row);
        homeStateText_ = new QLabel;
        homeStateText_->setWordWrap(true);
        cl->addWidget(homeStateText_);
        homeGame_ = new QLabel;
        homeGame_->setObjectName("Hint");
        homeGame_->setWordWrap(true);
        cl->addWidget(homeGame_);
        auto* off = new QPushButton("Disable all mods");
        off->setCursor(Qt::PointingHandCursor);
        auto* offRow = new QHBoxLayout;
        offRow->addWidget(off);
        offRow->addStretch(1);
        cl->addLayout(offRow);
        connect(off, &QPushButton::clicked, this, [this]() { disableAll(); });
        for (QLabel* sl : {homeState_, homeStateText_, homeGame_, c->findChild<QLabel*>("CardTitle")}) if (sl) sl->setProperty("dkSkip", true);   // the status section is always English
    }
    {   // target game
        auto* c = card(v, "Target game");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel("Default means DungeonKit is not connected to any game. When you are ready, choose Minecraft Dungeons or Minecraft Dungeons II. "
                             "DungeonKit never starts or changes a game by itself; it only connects to the one you pick, and switching back to Default disconnects it.");
        t->setObjectName("Hint");
        t->setWordWrap(true);
        cl->addWidget(t);
        auto* r0 = new QRadioButton("Default   (not connected to a game)");
        auto* r1 = new QRadioButton("Minecraft Dungeons");
        auto* r2 = new QRadioButton("Minecraft Dungeons II");
        for (auto* r : {r0, r1, r2}) { r->setCursor(Qt::PointingHandCursor); cl->addWidget(r); }
        r0->setChecked(true);
        connect(r0, &QRadioButton::clicked, this, [this]() { setTarget(Process::Target::None); });
        connect(r1, &QRadioButton::clicked, this, [this]() { setTarget(Process::Target::Dungeons); });
        connect(r2, &QRadioButton::clicked, this, [this]() { setTarget(Process::Target::Dungeons2); });
        auto* killHint = new QLabel("Sometimes a game keeps running in the background after you close it, and DungeonKit still treats it as open. "
                                    "Close game processes ends every Minecraft Dungeons and Minecraft Dungeons II process (Dungeons.exe, Dungeons-Win64-Shipping.exe and "
                                    "Dungeons-WinGDK-Shipping.exe). Unsaved game progress is lost.");
        killHint->setObjectName("Hint");
        killHint->setWordWrap(true);
        cl->addSpacing(6);
        cl->addWidget(killHint);
        auto* kill = new QPushButton("Close game processes");
        kill->setCursor(Qt::PointingHandCursor);
        auto* killRow = new QHBoxLayout;
        killRow->addWidget(kill);
        killRow->addStretch(1);
        cl->addLayout(killRow);
        connect(kill, &QPushButton::clicked, this, [this]() {
            const auto a = QMessageBox::question(this, kAppName,
                I18n::tr("End every Minecraft Dungeons and Minecraft Dungeons II process? Unsaved game progress will be lost."),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (a != QMessageBox::Yes) return;
            if (proc_.attached()) disableAllMods();                         // leave the game as we found it first
            if (mcd2Hub_) mcd2Hub_->stopFreezes();
            proc_.detach();
            procAlt_.detach();
            int failed = 0;
            const int ended = Process::terminateGameProcesses(failed);
            if (failed > 0) setMessage(I18n::tr(QString("Ended %1 game process(es), %2 could not be ended. Try running DungeonKit as administrator.").arg(ended).arg(failed)));
            else if (ended > 0) setMessage(I18n::tr(QString("Ended %1 game process(es).").arg(ended)));
            else setMessage("No game processes were running.");
            pollGame();
        });
    }
    // General information / Minecraft Dungeons FAQ / Minecraft Dungeons II FAQ share one area; three buttons choose which one shows
    auto* tabRow = new QHBoxLayout;
    auto* tabGroup = new QButtonGroup(this);
    tabGroup->setExclusive(true);
    QWidget* tabPages[3];
    QVBoxLayout* tabLayouts[3];
    const char* tabNames[3] = {"General Information", "Minecraft Dungeons FAQ", "Minecraft Dungeons II FAQ"};
    for (int i = 0; i < 3; ++i) {
        auto* b = new QPushButton(tabNames[i]);
        b->setObjectName("Tab");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        tabGroup->addButton(b, i);
        tabRow->addWidget(b);
        tabPages[i] = new QWidget;
        tabPages[i]->setStyleSheet("background: transparent;");
        tabLayouts[i] = new QVBoxLayout(tabPages[i]);
        tabLayouts[i]->setContentsMargins(0, 0, 0, 0);
        tabLayouts[i]->setSpacing(12);
    }
    tabRow->addStretch(1);
    tabGroup->button(0)->setChecked(true);
    v->addLayout(tabRow);
    for (int i = 0; i < 3; ++i) { v->addWidget(tabPages[i]); tabPages[i]->setVisible(i == 0); }
    connect(tabGroup, &QButtonGroup::idClicked, this, [tabPages](int id) { for (int i = 0; i < 3; ++i) tabPages[i]->setVisible(i == id); });
    {   // about
        auto* c = card(tabLayouts[0], "What DungeonKit does");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel(
            "DungeonKit is a free, public mod trainer and multipurpose toolkit for Minecraft Dungeons and Minecraft Dungeons II, and one of the first modding menus to support both games. While the game is running, it lets you change how it behaves: items and enchantments, emeralds and gold, XP, teleports, flying, character stats, item stats, world editing, god mode, streamer mode, and more.\n\n"
            "On its own, with none of the advanced tools switched on, DungeonKit works by reading and editing the game's memory, so you can use whatever you like. For more complex features, such as the console and its commands, it uses its own custom DLL, which is only loaded into the game when you press its button. Whether you want to mod the game, see information you can't find anywhere else, or just use quality-of-life features like streamer mode, DungeonKit has you covered.\n\n"
            "DungeonKit only reads files related to the game, such as the Pak folders, to find what it needs and to let you add your own mods. Apart from the optional Discord Rich Presence, which only talks to the Discord app on your computer, it makes no connections outside the game's files and process.");
        t->setWordWrap(true);
        cl->addWidget(t);
    }
    {   // start
        auto* c = card(tabLayouts[0], "Getting started");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel(
            "1.  Start Minecraft Dungeons or Minecraft Dungeons II (Minecraft Launcher, Xbox app or Steam).\n"
            "2.  On Home, under Target game, choose whichever one you started.\n"
            "3.  Wait until the box at the bottom left says Online.\n"
            "4.  Open a section under Minecraft Dungeons or Minecraft Dungeons II in the sidebar (whichever you chose) and flip the switches you want.\n"
            "5.  Some features only link once the game has used them, so open your inventory, load into a level, or move around.");
        t->setWordWrap(true);
        cl->addWidget(t);
        auto* more = new QLabel("Want more details? You can find them in the Information section.");
        more->setObjectName("Hint");
        more->setWordWrap(true);
        cl->addWidget(more);
        auto* goInfo = new QPushButton("Open Information");
        goInfo->setCursor(Qt::PointingHandCursor);
        auto* goRow = new QHBoxLayout;
        goRow->addWidget(goInfo);
        goRow->addStretch(1);
        cl->addLayout(goRow);
        connect(goInfo, &QPushButton::clicked, this, [this]() {
            if (auto* b = navGroup_->button(11)) b->click();          // 11 = Information; clicking keeps the sidebar highlight in step
            else pages_->setCurrentIndex(11);
        });
    }
    {   // FAQ
        auto* c = card(tabLayouts[1], "Minecraft Dungeons FAQ");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        struct Qa { const char* q; const char* a; };
        static const Qa faq[] = {
            {"How do I attach DungeonKit to my game?",
             "Start Minecraft Dungeons, then on Home choose Minecraft Dungeons under Target game. DungeonKit only connects to the game you pick, "
             "and choosing Default disconnects it."},
            {"It says Open, no game connected.",
             "The game is not chosen yet, or it is not running. Start the game, then pick Minecraft Dungeons under Target game on Home. "
             "If the game was just closed, Close game processes on the same page clears anything left running in the background."},
            {"Why isn't something working?",
             "It depends on what isn't working. Try restarting DungeonKit or the game first, make sure Minecraft Dungeons is chosen under Target game, "
             "you're on a supported version with a legitimate copy of the game, and you've loaded into a level or your camp (some features need that). "
             "If it still doesn't work, join the Discord server and ask for help."},
            {"Nothing happens when I turn a feature on.",
             "It's very unlikely that a feature breaks just from being used. If you didn't change anything and it still "
             "doesn't work after restarting the game and trying the steps above, let Plushy know so a fix can be looked into."},
            {"Why does my game crash when loading into levels?",
             "This is usually caused by too many things being active at once. Press Disable all mods when you're finished "
             "so everything is switched off before you load into a level."},
            {"What versions and platforms does DungeonKit support?",
             "DungeonKit has worked with many versions and platforms. It's now mainly made for Minecraft Dungeons v1.17.0.0, "
             "and works with the Minecraft Launcher, the Xbox app and Steam. The Camera Editor and World pages find the game's data by name, so they cope better with other versions."},
            {"Where can I get help?",
             "In the Discord server. If your problem isn't covered in this FAQ, you can ask there as well."},
            {"Can I gift modded items to a player?",
             "Yes, although some things are limited. Item IDs, armor traits, Gilded Levels, tagging and normal enchantments "
             "stay when you gift an item, but power level and enchantment levels have some limitations."},
            {"Can you edit other people's inventories?",
             "Not while you're in a lobby with them. You can only create items for yourself and then gift them to another player."},
            {"Can I get in trouble for using this?",
             "It depends on where and how you use it. You may get in trouble in places like Discord servers for using or "
             "sharing modded content, so it's best not to show or share it in communities that don't allow it."},
            {"Why are some things from older versions missing?",
             "Some older features caused problems, broke for no clear reason, were tedious to update, or just weren't needed. "
             "They may come back if people still want them, but there are no plans to add them back right now."},
            {"Is there a way to gift more than once?",
             "Yes and no. It's believed to be possible, but nobody on the team knows how yet. If you know how it can be done "
             "and want it added, mention it in the Discord server and you'll be credited."},
        };
        for (const Qa& e : faq) {
            auto* q = new QLabel(QString::fromUtf8(e.q));
            q->setObjectName("Name");
            q->setWordWrap(true);
            auto* a = new QLabel(QString::fromUtf8(e.a));
            a->setWordWrap(true);
            a->setContentsMargins(0, 0, 0, 8);
            cl->addWidget(q);
            cl->addWidget(a);
        }
    }
    {   // Minecraft Dungeons II FAQ
        auto* c = card(tabLayouts[2], "Minecraft Dungeons II FAQ");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        struct Qa { const char* q; const char* a; };
        static const Qa faq2[] = {
            {"How do I use DungeonKit with Minecraft Dungeons II?",
             "Choose Minecraft Dungeons II under Target game on this page, start the game and load into a level, then open the pages in the Minecraft Dungeons II section. "
             "Pages that scan the game (Inventory Editor, World, Nearby Viewer, Camera Editor) need Refresh, Scan or Find camera pressed once first and again after you change level."},
            {"It says Minecraft Dungeons II is not connected.",
             "Make sure the game is running, that Minecraft Dungeons II is chosen under Target game, and that you have loaded into a level and not just the menu. "
             "The Connection check on the Tools page shows what DungeonKit can see."},
            {"Does it work online?",
             "Mostly no. Stats, Threat level and difficulty, Movement speed, Teleports, Nearby Viewer and the item editing work offline. "
             "No Clip works offline and somewhat online (the game tries to push you back to the ground, but you can still use it to glitch onto higher spots). "
             "Online, the server owns your data and may undo changes."},
            {"Can I get in trouble or banned for using it?",
             "Changing gameplay values or items in an online session may be undone by the server. Use DungeonKit offline or in a solo session, and keep it away from places that don't allow modded content."},
            {"Which game version does it support?",
             "Version 1.1.2 is recognised. After a game update the layout can change, so a page may stop working until DungeonKit is updated. "
             "DungeonKit looks for its data by name, which helps, but it cannot promise a new version will work."},
            {"How do I edit an item?",
             "Open the Inventory Editor, press Refresh, then double-click an item in the list. It loads into Selected item under the list, where you can change the item, rarity, power, level and stack, and the effects below it. "
             "Press Apply to write the change. Power and Level accept values up to 99,999."},
            {"The game crashed while I was editing an effect.",
             "Changing which effect an entry is can crash the game if the game has not loaded that effect. Try changing Strength and Points invested instead, press Disable all mods before loading a level, and tell Plushy what you changed so it can be looked into."},
            {"I pressed Set but the value went back.",
             "The game recalculates many values, such as threat level and power. Tick Freeze to keep writing the value, and remember that the server can undo values online."},
            {"What is No Clip?",
             "No Clip lets you fly and pass through walls and floors in one switch. Hold Page Up to rise and Page Down to sink. "
             "Turn No Clip off before you walk back into the level. Press Disable all mods if you ever get stuck."},
            {"Can I see my stats on screen while I play?",
             "Minecraft Dungeons II > Camera & Display has a Display mode card: a small on-screen overlay drawn over the game window itself, with Health, Damage, "
             "Damage per second, Walk speed, Coordinates (X/Y/Z) and the active quest, each its own color and switched on individually. It finds the game window "
             "automatically, so it stays in its corner whether the game is windowed, borderless or you move it (use Nudge if it sits slightly off); it will not "
             "show over true exclusive fullscreen."},
            {"Can I unlock the whole map, bosses or doors?",
             "Not from DungeonKit. The map and doors are stored in structures that cannot be safely changed from outside the game. "
             "World has a Missions (test) section that can set locked missions to Available, but it is only a test."},
            {"Can I hide the game's interface for screenshots?",
             "No. The Camera Editor can change the camera and slow or freeze time for Photo mode, but the on-screen interface cannot be hidden by DungeonKit."},
            {"What is the Data dump on the Tools page?",
             "A developer tool that writes the game's internal tables to large text files on your Desktop. It does nothing for normal use, so leave it alone unless you know what you are doing."},
            {"Where can I get help?",
             "In the Discord server. Mention which page you used, whether you were online or offline, and what happened."},
        };
        for (const Qa& e : faq2) {
            auto* q = new QLabel(QString::fromUtf8(e.q));
            q->setObjectName("Name");
            q->setWordWrap(true);
            auto* a = new QLabel(QString::fromUtf8(e.a));
            a->setWordWrap(true);
            a->setContentsMargins(0, 0, 0, 8);
            cl->addWidget(q);
            cl->addWidget(a);
        }
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

void MainWindow::refreshFolders() {
    if (!paksLabel_) return;
    const auto r = GameFolders::find(QString::fromStdWString(proc_.exePath()));
    paksDir_ = r.paks;
    const bool running = proc_.attached() && proc_.alive();
    if (paksRunning_) paksRunning_->setVisible(running);
    if (paksDir_.isEmpty()) {
        paksLabel_->setText("Not found yet.\n"
                            "Start the game and press Search again, or paste the folder below. "
                            "Games installed through the Xbox app can keep their files in a protected folder.");
        if (paksCount_) paksCount_->setText("Mods installed: unknown until the folder is found.");
        refreshMods();
        return;
    }
    const QString mods = GameFolders::modsDir(paksDir_);
    const bool hasMods = QDir(mods).exists();
    const int inMods = hasMods ? GameFolders::countPaks(mods, true) : 0;
    const int inPaks = GameFolders::countPaks(paksDir_, false);
    paksLabel_->setText(QString("Found (%1):\n%2").arg(r.how, QDir::toNativeSeparators(paksDir_)));
    if (paksCount_)
        paksCount_->setText(QString("Mods installed: %1 pak file(s) in the ~mods folder%2.\nPak files in the Paks folder itself: %3 (these are the game's own files, leave them alone).")
                                .arg(inMods)
                                .arg(hasMods ? "" : " (the ~mods folder does not exist yet; Open mods folder creates it)")
                                .arg(inPaks));
    refreshMods();
}

QWidget* MainWindow::buildPakDirectory() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);
    auto* title = new QLabel("Pak Directory");
    title->setObjectName("PageTitle");
    outer->addWidget(title);

    auto* warn = new QLabel(
        "Do not add or remove pak mods while the game is open. Close Minecraft Dungeons first, change your mods, "
        "then start the game again. Changing them while it is running can break the game or corrupt your mods.");
    warn->setObjectName("Notice");
    warn->setWordWrap(true);
    outer->addWidget(warn);

    auto* safe = new QLabel(
        "Stay safe with pak mods. We recommend only downloading them from Nexus Mods, CurseForge, or our Discord server. "
        "If someone sends you a random mod, check it before you use it. A normal pak mod is just a .pak file, so be wary of "
        "anything that comes with an .exe, .bat or an installer. If you think it's fine, go ahead and use it.");
    safe->setObjectName("Notice");
    safe->setWordWrap(true);
    outer->addWidget(safe);

    paksRunning_ = new QLabel("The game is running right now. Close it before you add or remove any mods.");
    paksRunning_->setObjectName("Message");
    paksRunning_->setWordWrap(true);
    paksRunning_->setVisible(false);
    outer->addWidget(paksRunning_);
    paksMsg_ = new QLabel;
    paksMsg_->setObjectName("Message");
    paksMsg_->setWordWrap(true);
    outer->addWidget(paksMsg_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);

    {
        auto* c = card(v, "Find the Paks folder");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel(
            "Pak mods are .pak files that the game loads from its Paks folder. DungeonKit looks for that folder "
            "using the running game first, then the usual install spots for the Minecraft Launcher, Xbox app and Steam "
            "on all your drives. Put mods in the ~mods folder inside Paks, which keeps them apart from the game's own files.");
        t->setWordWrap(true);
        cl->addWidget(t);
        paksLabel_ = new QLabel;
        paksLabel_->setObjectName("Hint");
        paksLabel_->setWordWrap(true);
        paksLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(paksLabel_);
        paksCount_ = new QLabel;
        paksCount_->setWordWrap(true);
        cl->addWidget(paksCount_);
        auto* row = new QHBoxLayout;
        auto* search = new QPushButton("Search again");
        auto* openPaks = new QPushButton("Open Paks folder");
        auto* openMods = new QPushButton("Open mods folder");
        for (QPushButton* b : {search, openPaks, openMods}) { b->setCursor(Qt::PointingHandCursor); row->addWidget(b); }
        row->addStretch(1);
        cl->addLayout(row);
        connect(search, &QPushButton::clicked, this, [this]() { GameFolders::savePath(QString()); refreshFolders(); });
        connect(openPaks, &QPushButton::clicked, this, [this]() {
            if (paksDir_.isEmpty()) { setMessage("Paks folder not found yet. Paste it in below."); return; }
            QDesktopServices::openUrl(QUrl::fromLocalFile(paksDir_));
        });
        connect(openMods, &QPushButton::clicked, this, [this]() {
            if (paksDir_.isEmpty()) { setMessage("Paks folder not found yet. Paste it in below."); return; }
            const QString mods = GameFolders::modsDir(paksDir_);
            if (!QDir(mods).exists() && !QDir().mkpath(mods)) { setMessage("Could not create the ~mods folder here."); return; }
            QDesktopServices::openUrl(QUrl::fromLocalFile(mods));
            refreshFolders();
        });
    }
    {
        auto* c = card(v, "Can't find it? Paste the location");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel(
            "Paste the path to the Paks folder, the ~mods folder, or the game's main install folder, and DungeonKit works out the rest. "
            "In File Explorer you can hold Shift, right-click the folder and choose Copy as path. The saved location is used from then on, "
            "until you press Search again.");
        t->setWordWrap(true);
        cl->addWidget(t);
        auto* row = new QHBoxLayout;
        paksEdit_ = new QLineEdit;
        paksEdit_->setPlaceholderText("C:\\Program Files (x86)\\Steam\\steamapps\\common\\MinecraftDungeons\\Dungeons\\Content\\Paks");
        auto* use = new QPushButton("Use this location");
        auto* browse = new QPushButton("Browse...");
        use->setCursor(Qt::PointingHandCursor);
        browse->setCursor(Qt::PointingHandCursor);
        row->addWidget(paksEdit_, 1);
        row->addWidget(use);
        row->addWidget(browse);
        cl->addLayout(row);
        auto apply = [this](const QString& text) {
            const QString p = GameFolders::resolve(text);
            if (p.isEmpty()) {
                QMessageBox::warning(this, kAppName,
                                     I18n::tr("That doesn't look like the Minecraft Dungeons Paks folder. It should contain .pak files, "
                                              "and usually ends in Content\\Paks."));
                return;
            }
            GameFolders::savePath(p);
            paksEdit_->clear();
            refreshFolders();
            setMessage("Paks folder saved.");
        };
        connect(use, &QPushButton::clicked, this, [this, apply]() { apply(paksEdit_->text()); });
        connect(paksEdit_, &QLineEdit::returnPressed, this, [this, apply]() { apply(paksEdit_->text()); });
        connect(browse, &QPushButton::clicked, this, [this, apply]() {
            const QString dir = QFileDialog::getExistingDirectory(this, "Pick the game's Paks folder", paksDir_);
            if (!dir.isEmpty()) apply(dir);
        });
    }
    {
        auto* c = card(v, "Installed mods");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel(
            "Every pak mod in your ~mods folder, with its size and the date it was last changed. Disabling a mod renames it to "
            ".pak.disabled, so the game ignores it and you can switch it back on later without downloading it again. "
            "Only do this while the game is closed.");
        t->setWordWrap(true);
        cl->addWidget(t);
        modsSummary_ = new QLabel;
        modsSummary_->setObjectName("Hint");
        modsSummary_->setWordWrap(true);
        cl->addWidget(modsSummary_);
        modTree_ = new QTreeWidget;
        modTree_->setColumnCount(4);
        modTree_->setHeaderLabels(QStringList() << "Mod" << "Size" << "Date modified" << "Status");
        modTree_->setRootIsDecorated(false);
        modTree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        modTree_->setMinimumHeight(230);
        modTree_->header()->setStretchLastSection(false);
        modTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        for (int col = 1; col < 4; ++col) modTree_->header()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
        modTree_->setSortingEnabled(true);
        modTree_->sortByColumn(0, Qt::AscendingOrder);
        cl->addWidget(modTree_);
        auto* row = new QHBoxLayout;
        auto* refresh = new QPushButton("Refresh list");
        auto* dis = new QPushButton("Disable selected");
        auto* en = new QPushButton("Enable selected");
        for (QPushButton* b : {refresh, dis, en}) { b->setCursor(Qt::PointingHandCursor); row->addWidget(b); }
        row->addStretch(1);
        cl->addLayout(row);
        connect(refresh, &QPushButton::clicked, this, [this]() { refreshFolders(); });
        connect(dis, &QPushButton::clicked, this, [this]() { renameSelectedMods(true); });
        connect(en, &QPushButton::clicked, this, [this]() { renameSelectedMods(false); });
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

QWidget* MainWindow::buildWebsites() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);
    auto* title = new QLabel("Websites");
    title->setObjectName("PageTitle");
    outer->addWidget(title);
    auto* note = new QLabel("Community sites and servers for Minecraft Dungeons. They open in your normal browser or Discord. Apart from the DungeonKit server, none of them are run by DungeonKit. Only join or download from places you trust.");
    note->setObjectName("Hint");
    note->setWordWrap(true);
    outer->addWidget(note);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setObjectName("SitesInner");
    // Only the page and its labels are transparent. A blanket "background: transparent" here also wiped the gradient off the buttons, which made them look dark.
    inner->setStyleSheet("QWidget#SitesInner { background: transparent; } QLabel { background: transparent; }");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);

    struct Site { const char* name; const char* url; const char* text; const char* button; const char* icon; };
    auto addSection = [v](const char* name) {
        auto* sec = new QLabel(QString::fromLatin1(name));
        sec->setObjectName("Section");
        sec->setContentsMargins(0, 8, 0, 0);
        v->addWidget(sec);
    };
    // Each site is a card: a round logo (all the same size), the name with its address under it, a description and one button.
    auto addSites = [this, v](const Site* list, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            const Site& s = list[i];
            auto* f = new QFrame;
            f->setObjectName("Card");
            auto* fl = new QVBoxLayout(f);
            fl->setContentsMargins(16, 14, 16, 14);
            fl->setSpacing(10);

            auto* head = new QHBoxLayout;
            head->setSpacing(14);
            auto* logo = new QLabel;
            logo->setFixedSize(56, 56);
            logo->setAttribute(Qt::WA_TranslucentBackground);
            const qreal dpr = logo->devicePixelRatioF();
            QPixmap px(QString(":/sites/%1.png").arg(QString::fromLatin1(s.icon)));
            px = px.scaled(QSize(int(56 * dpr), int(56 * dpr)), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            px.setDevicePixelRatio(dpr);
            logo->setPixmap(px);
            head->addWidget(logo, 0, Qt::AlignTop);

            auto* names = new QVBoxLayout;
            names->setSpacing(2);
            auto* title = new QLabel(QString::fromUtf8(s.name));
            title->setObjectName("CardTitle");
            names->addWidget(title);
            const QUrl parsed(QString::fromLatin1(s.url));
            auto* host = new QLabel(parsed.host() + (parsed.path().size() > 1 ? parsed.path() : QString()));
            host->setObjectName("Hint");
            host->setTextInteractionFlags(Qt::TextSelectableByMouse);
            host->setProperty("dkSkip", true);                     // an address, not text to translate
            names->addWidget(host);
            head->addLayout(names, 1);

            auto* open = new QPushButton(QString::fromLatin1(s.button));
            open->setObjectName("Primary");
            open->setCursor(Qt::PointingHandCursor);
            head->addWidget(open, 0, Qt::AlignTop);
            fl->addLayout(head);

            auto* t = new QLabel(QString::fromUtf8(s.text));
            t->setWordWrap(true);
            fl->addWidget(t);
            v->addWidget(f);
            const QString url = QString::fromLatin1(s.url);
            connect(open, &QPushButton::clicked, this, [url]() { QDesktopServices::openUrl(QUrl(url)); });
        }
    };

    static const Site tools[] = {
        {"MCD Builder", "https://mcdbuilder.vercel.app/",
         "Build sharing. Plan a full build with your weapons, armor, artifacts and enchantments, then share the link so friends can see "
         "exactly what you are running. Handy for theorycrafting before you spend resources in game.", "Open website", "creeper"},
        {"Dungeon Collector", "https://www.dungeoncollector.co.uk/dungeons/",
         "An archive of older Minecraft Dungeons information. Useful for looking up items, mobs, missions and details from earlier "
         "versions of the game that are hard to find elsewhere.", "Open website", "gold"},
        {"Dungeon Collector II", "https://www.dungeoncollector.co.uk/dungeonsii/",
         "The same archive for Minecraft Dungeons II. Look up items, mobs and other details from the sequel.", "Open website", "gold"},
        {"Dungeons Tools", "https://www.dungeons.tools/",
         "A recent database covering both Minecraft Dungeons titles. Look up gear, stats and other game data in one place, "
         "and it is kept up to date.", "Open website", "hex"},
    };
    static const Site mods[] = {
        {"Nexus Mods", "https://www.nexusmods.com/games/minecraftdungeons/mods",
         "The biggest home for Minecraft Dungeons mods, with nearly every mod there is, mainly the pak mods used to change the game. "
         "If you are looking for a mod, start here.", "Open Nexus Mods", "nexus"},
        {"CurseForge", "https://www.curseforge.com/minecraft-dungeons",
         "A good place for the better known and more popular mods. It has fewer mods than Nexus Mods, but what is there tends to be "
         "well used and easy to find.", "Open CurseForge", "curseforge"},
    };
    static const Site discords[] = {
        {"DungeonKit", "https://discord.gg/PfCZu5rSU4",
         "The DungeonKit server. Get help, report bugs, share ideas and hear about updates.", "Join server", "dungeonkit"},
        {"Grim's Themed Builds", "https://discord.gg/BXkXzh3Prr",
         "A server built around Grim, one of the most knowledgeable players when it comes to base-game mechanics like enchantments, "
         "stats and items. It isn't focused on modded content, but it is a great place for help with builds, optimization, and "
         "learning the best items and enchants in the game.", "Join server", "discord"},
        {"The Dungeon Fishes", "https://discord.gg/mKstemjuRF",
         "The main and current server for Minecraft Dungeons topics. Come for some of the smartest players, builds and build sharing, "
         "and useful game resources. It is run by the popular YouTuber The Dungeon Fish.", "Join server", "discord"},
        {"Minecraft Dungeons FR", "https://discord.gg/6MSSbS9hDp",
         "A community server for both English and French players. Chat, find people to play with, and discuss items or modded content. "
         "It isn't mainly about item trading or requests, but it is a good general community space for Minecraft Dungeons players.",
         "Join server", "discord"},
    };
    addSection("WEBSITE TOOLS");
    addSites(tools, sizeof(tools) / sizeof(tools[0]));
    addSection("MODDING WEBSITES");
    addSites(mods, sizeof(mods) / sizeof(mods[0]));
    addSection("DISCORD SERVERS");
    addSites(discords, sizeof(discords) / sizeof(discords[0]));
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

namespace {
struct ModItem : QTreeWidgetItem {
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& o) const override {
        const int c = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (c == 1 || c == 2) return data(c, Qt::UserRole).toLongLong() < o.data(c, Qt::UserRole).toLongLong();
        return text(c).toLower() < o.text(c).toLower();
    }
};
} // namespace

void MainWindow::refreshMods() {
    if (!modTree_) return;
    modTree_->setSortingEnabled(false);
    modTree_->clear();
    if (paksDir_.isEmpty()) {
        if (modsSummary_) modsSummary_->setText(I18n::tr("Find the Paks folder first to see your mods here."));
        modTree_->setSortingEnabled(true);
        return;
    }
    const QString mods = GameFolders::modsDir(paksDir_);
    int on = 0, off = 0;
    qint64 total = 0;
    if (QDir(mods).exists()) {
        const QDir base(mods);
        QDirIterator it(mods, QStringList() << "*.pak" << "*.pak.disabled", QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo fi(path);
            const bool disabled = path.endsWith(".disabled", Qt::CaseInsensitive);
            QString name = base.relativeFilePath(path);
            if (disabled) name.chop(9);
            auto* item = new ModItem(modTree_);
            item->setText(0, name);
            item->setText(1, QLocale().formattedDataSize(fi.size()));
            item->setData(1, Qt::UserRole, fi.size());
            item->setText(2, QLocale().toString(fi.lastModified(), QLocale::ShortFormat));
            item->setData(2, Qt::UserRole, fi.lastModified().toMSecsSinceEpoch());
            item->setText(3, I18n::tr(disabled ? "Disabled" : "Enabled"));
            item->setData(0, Qt::UserRole + 10, path);
            (disabled ? off : on)++;
            total += fi.size();
        }
    }
    if (modsSummary_)
        modsSummary_->setText(I18n::tr(QString("%1 mod file(s) in ~mods: %2 enabled, %3 disabled. Total size: %4")
                                           .arg(on + off).arg(on).arg(off).arg(QLocale().formattedDataSize(total))));
    modTree_->setSortingEnabled(true);
}

void MainWindow::renameSelectedMods(bool disable) {
    if (!modTree_) return;
    if (proc_.attached() && proc_.alive()) { setMessage("Close the game before you change mods."); return; }
    const auto sel = modTree_->selectedItems();
    if (sel.isEmpty()) { setMessage("Select a mod in the list first."); return; }
    int done = 0, failed = 0;
    for (QTreeWidgetItem* it : sel) {
        const QString path = it->data(0, Qt::UserRole + 10).toString();
        const bool isDisabled = path.endsWith(".disabled", Qt::CaseInsensitive);
        if (disable == isDisabled) continue;
        const QString target = disable ? path + ".disabled" : path.left(path.size() - 9);
        if (QFile::exists(target) || !QFile::rename(path, target)) ++failed; else ++done;
    }
    refreshFolders();
    setMessage(QString("Changed %1 mod(s). %2 could not be renamed.").arg(done).arg(failed));
}

void MainWindow::refreshBackupDebug() {
    if (!backupDebug_) return;
    QString text = "SESSION EVENTS (oldest first):\n";
    text += QString("  Opened at %1\n").arg(openedAt_.toString("yyyy-MM-dd HH:mm:ss"));
    text += QString("  Target game now: %1\n").arg(target_ == Process::Target::Dungeons ? "Minecraft Dungeons"
                                                  : target_ == Process::Target::Dungeons2 ? "Minecraft Dungeons II" : "none");
    text += QString("  Game now: %1\n").arg(proc_.attached() && proc_.alive() ? QString("%1 (PID %2), attached from outside").arg(QString::fromStdString(proc_.exeName())).arg(proc_.pid()) : QString("not attached"));
    for (const QString& e : events_) text += "  " + e + "\n";
    auto* sb = backupDebug_->verticalScrollBar();
    const bool atEnd = sb->value() >= sb->maximum() - 4;
    const int keep = sb->value();
    backupDebug_->setPlainText(text);
    sb->setValue(atEnd ? sb->maximum() : keep);
}

void MainWindow::logEvent(const QString& text) {
    const QString line = QDateTime::currentDateTime().toString("HH:mm:ss") + "  " + text;
    if (!events_.isEmpty() && events_.last().mid(10) == line.mid(10)) return;   // skip exact repeats
    events_ << line;
    while (events_.size() > 400) events_.removeFirst();
    if (backupDebug_ && backupDebug_->isVisible()) refreshBackupDebug();
}

void MainWindow::watchToggles() {
    auto check = [this](QWidget* w, bool on, const QString& label) {
        if (label.isEmpty()) return;
        auto it = toggleState_.find(w);
        if (it == toggleState_.end()) { toggleState_.insert(w, on); return; }   // first sight: remember, do not log
        if (it.value() != on) { it.value() = on; logEvent(QString("Switch: %1 turned %2").arg(label, on ? "ON" : "OFF")); }
    };
    for (auto* b : findChildren<QCheckBox*>()) check(b, b->isChecked(), b->text().remove('&').trimmed());
    for (auto* b : findChildren<QPushButton*>()) if (b->isCheckable()) check(b, b->isChecked(), b->text().remove('&').trimmed());
}

void MainWindow::syncBackupMods() {
    const bool run1 = proc_.attached() && proc_.alive() && target_ == Process::Target::Dungeons;
    const bool run2 = proc_.attached() && proc_.alive() && (target_ == Process::Target::Dungeons2 || target_ == Process::Target::Dungeons2Launcher);
    const QString exe = QString::fromStdWString(proc_.exePath());
    const auto a = GameFolders::find(run1 ? exe : QString(), 0);
    const auto b = GameFolders::find(run2 ? exe : QString(), 1);
    SaveBackup::setModsFolders(a.paks.isEmpty() ? QString() : GameFolders::modsDir(a.paks),
                               b.paks.isEmpty() ? QString() : GameFolders::modsDir(b.paks));
}

void MainWindow::refreshBackupInfo() {
    if (!backupInfo_) return;
    syncBackupMods();
    const bool withMods = !backupMods_ || backupMods_->isChecked();
    const auto src = SaveBackup::sources(withMods);
    QString text;
    if (src.isEmpty()) {
        text = I18n::tr("No save folder found yet. If you know where it is, press one of the Choose folder buttons.");
    } else {
        for (int g : {(int)SaveBackup::Dungeons, (int)SaveBackup::Dungeons2}) {
            QString part;
            for (const auto& s : src) if (s.game == g) part += "\n  " + I18n::tr(s.label) + ":  " + QDir::toNativeSeparators(s.path);
            if (!part.isEmpty()) text += (text.isEmpty() ? QString() : QString("\n\n")) + I18n::tr(SaveBackup::gameName(g)) + ":" + part;
        }
    }
    backupInfo_->setText(text);
    if (backupResult_) {
        const int n = SaveBackup::existingBackups();
        QString r = I18n::tr(QString("Backups go to: %1\nBackups made so far: %2")
                                 .arg(QDir::toNativeSeparators(SaveBackup::backupRoot())).arg(n));
        if (!lastBackupText_.isEmpty()) r += "\n\n" + lastBackupText_;
        backupResult_->setText(r);
    }
}

QWidget* MainWindow::buildBackup() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);
    auto* title = new QLabel("Backup Saves");
    title->setObjectName("PageTitle");
    outer->addWidget(title);
    auto* note = new QLabel(
        "Backups are copied to a folder on your Desktop, one folder per game, named with the game, the date and the game version. "
        "Your real saves are only read, never changed. Make a backup before you try anything risky.");
    note->setObjectName("Notice");
    note->setWordWrap(true);
    outer->addWidget(note);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);
    {
        auto* c = card(v, "Your saves");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        backupInfo_ = new QLabel;
        backupInfo_->setWordWrap(true);
        backupInfo_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(backupInfo_);
        auto* row = new QHBoxLayout;
        auto* go = new QPushButton("Back up now");
        auto* openBk = new QPushButton("Open backup folder");
        auto* openSave = new QPushButton("Open save folder");
        auto* pick = new QPushButton("Choose Dungeons folder...");
        auto* pick2 = new QPushButton("Choose Dungeons II folder...");
        for (QPushButton* b : {go, openBk, openSave, pick, pick2}) { b->setCursor(Qt::PointingHandCursor); row->addWidget(b); }
        row->addStretch(1);
        cl->addLayout(row);
        backupMods_ = new QCheckBox("Also back up the ~mods folder");
        backupMods_->setChecked(QSettings().value("backup/mods", true).toBool());
        cl->addWidget(backupMods_);
        connect(backupMods_, &QCheckBox::toggled, this, [this](bool on) { QSettings().setValue("backup/mods", on); refreshBackupInfo(); refreshBackupDebug(); });
        connect(go, &QPushButton::clicked, this, [this]() {
            syncBackupMods();
            const auto src = SaveBackup::sources(backupMods_->isChecked());
            if (src.isEmpty()) { QMessageBox::warning(this, kAppName, I18n::tr("No save folder to back up. Press one of the Choose folder buttons and pick it.")); return; }
            SaveBackup::Info info;
            info.appVersion = kAppVersion;
            if (proc_.attached() && proc_.alive()) {
                if (const auto v = proc_.version()) {
                    const bool second = target_ == Process::Target::Dungeons2 || target_ == Process::Target::Dungeons2Launcher;
                    info.gameVersion[second ? SaveBackup::Dungeons2 : SaveBackup::Dungeons] = QString::fromLatin1(v->name);
                }
            }
            QStringList log;
            const auto r = SaveBackup::backup(src, info, &log);
            backupLog_ = log.join('\n');
            if (!r.ok) {
                lastBackupText_ = I18n::tr("Backup failed: ") + I18n::tr(r.error);
            } else {
                QStringList native;
                for (const QString& f : r.folders) native << QDir::toNativeSeparators(f);
                lastBackupText_ = I18n::tr(QString("Last backup saved to:\n%1\n%2 file(s), %3")
                                               .arg(native.join("\n")).arg(r.files).arg(QLocale().formattedDataSize(r.bytes)));
            }
            refreshBackupInfo();
            refreshBackupDebug();
        });
        connect(openBk, &QPushButton::clicked, this, [this]() {
            const QString root = SaveBackup::backupRoot();
            QDir().mkpath(root);
            QDesktopServices::openUrl(QUrl::fromLocalFile(root));
        });
        connect(openSave, &QPushButton::clicked, this, [this]() {
            const auto src = SaveBackup::sources(false);
            if (src.isEmpty()) { QMessageBox::warning(this, kAppName, I18n::tr("No save folder found yet. Press one of the Choose folder buttons and pick it.")); return; }
            QDesktopServices::openUrl(QUrl::fromLocalFile(src.first().path));
        });
        auto choose = [this](int game) {
            const QString dir = QFileDialog::getExistingDirectory(this, I18n::tr(game == SaveBackup::Dungeons2 ? "Pick your Minecraft Dungeons II save folder" : "Pick your Minecraft Dungeons save folder"), QDir::homePath());
            if (dir.isEmpty()) return;
            SaveBackup::savePath(dir, game);
            refreshBackupInfo();
            refreshBackupDebug();
        };
        connect(pick, &QPushButton::clicked, this, [choose]() { choose(SaveBackup::Dungeons); });
        connect(pick2, &QPushButton::clicked, this, [choose]() { choose(SaveBackup::Dungeons2); });
    }
    {
        auto* c = card(v, "Where backups go");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        backupResult_ = new QLabel;
        backupResult_->setWordWrap(true);
        backupResult_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(backupResult_);
    }
    {
        auto* c = card(v, "Debug: what DungeonKit accesses");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel("A live log of this session: what was opened, when the game was attached or closed (and whether it looks like a crash), and every switch turned on or off. "
                             "Below it: the folders for each game, the one place it writes, and the last backup. Copy puts the whole text on the clipboard.");
        t->setObjectName("Hint");
        t->setWordWrap(true);
        cl->addWidget(t);
        backupDebug_ = new QPlainTextEdit;
        backupDebug_->setReadOnly(true);
        backupDebug_->setLineWrapMode(QPlainTextEdit::NoWrap);
        backupDebug_->setMinimumHeight(260);
        backupDebug_->setFont(QFont("Consolas", 9));
        cl->addWidget(backupDebug_);
        auto* row = new QHBoxLayout;
        auto* rf = new QPushButton("Refresh");
        auto* cp = new QPushButton("Copy");
        auto* cl2 = new QPushButton("Clear log");
        for (QPushButton* b : {rf, cp, cl2}) { b->setCursor(Qt::PointingHandCursor); row->addWidget(b); }
        row->addStretch(1);
        cl->addLayout(row);
        connect(rf, &QPushButton::clicked, this, [this]() { refreshBackupDebug(); });
        connect(cp, &QPushButton::clicked, this, [this]() { QGuiApplication::clipboard()->setText(backupDebug_->toPlainText()); setMessage("Debug text copied."); });
        connect(cl2, &QPushButton::clicked, this, [this]() { backupLog_.clear(); refreshBackupDebug(); });
    }
    {
        auto* c = card(v, "Restoring a backup");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel(
            "To go back to a backup, close the game, then copy the files from the backup folder into the matching save folder and "
            "replace the ones there. DungeonKit does not restore on its own, so nothing gets overwritten by accident.");
        t->setWordWrap(true);
        cl->addWidget(t);
        auto* t2 = new QLabel(
            "The save locations are the usual ones for the Minecraft Launcher, Steam and the Xbox app, for both games. If yours is somewhere else, "
            "use the Choose folder button for that game.");
        t2->setObjectName("Hint");
        t2->setWordWrap(true);
        cl->addWidget(t2);
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    refreshBackupInfo();
    refreshBackupDebug();
    return page;
}

QWidget* MainWindow::buildChangelog() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);
    auto* title = new QLabel("Changelog");
    title->setObjectName("PageTitle");
    outer->addWidget(title);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);
    {
        auto* c = card(v, "Updates");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* cur = new QLabel(QString("You are using DungeonKit version %1.").arg(kAppVersion));
        cl->addWidget(cur);
        auto* t = new QLabel(
            "Update checking is not available yet. DungeonKit has not been published online, so there is nothing to check against. "
            "Once it is, this page will tell you when a new version is out.");
        t->setObjectName("Hint");
        t->setWordWrap(true);
        cl->addWidget(t);
        auto* row = new QHBoxLayout;
        auto* b = new QPushButton("Check for updates");
        b->setEnabled(false);
        row->addWidget(b);
        auto* soon = new QLabel("Coming in a later update");
        soon->setObjectName("Muted");
        row->addWidget(soon);
        row->addStretch(1);
        cl->addLayout(row);
    }
    struct Release { const char* fmt; const char* version; const char* items[8]; };
    static const Release releases[] = {
        {"Version %1", "1.121.0", {"Minecraft Dungeons II > Inventory Editor > Selected item: added an Enchants section that reads whichever enchant(s) are actually on the picked item and lets you change which enchant it is, its tier (I/II/III), and its enchantment-point cost. Editing only - it can't add an enchant to an item that doesn't have one.", nullptr}},
        {"Version %1", "1.120.0", {"Minecraft Dungeons II: removed Blur chat entirely - the capture-exclusion fix in 1.119.0 still didn't work well enough, so it's gone rather than left half-working. Camera & Display is back to just the Display mode stats overlay (Health, Damage, DPS, Walk speed, Coordinates, Quest, Deaths, Emeralds, Echo Shards, Enchantment points, Souls, Nearest blueprint) with no chat-related feature.", nullptr}},
        {"Version %1", "1.119.0", {"Minecraft Dungeons II > Blur chat: on Windows 10's May 2020 update or newer it no longer needs to hide itself to refresh - it marks its own window excluded from screen capture, so it stays live with no flicker (older Windows falls back to the previous brief hide-and-show). Raised Blur strength and See-through to their full 0-100 range, and added a Darken/Lighten slider to wash the patch toward black or white instead of staying neutral.", nullptr}},
        {"Version %1", "1.118.0", {"Minecraft Dungeons II: removed Hide chat - testing showed it didn't work, so it's gone. Added Blur chat instead, a different approach: a see-through, blurred patch you park over the chat log (or anywhere else) - its own corner, size, nudge, blur strength and see-through amount. It never touches the game; it only screenshots and blurs its own patch of the screen, so it can show a brief flicker when it refreshes a few times a second.", nullptr}},
        {"Version %1", "1.117.0", {"Minecraft Dungeons II > Camera & Display: added Emeralds, Echo Shards, Enchantment points and Souls as toggleable, color-coded stats, plus a Nearest blueprint line that names the closest enemy's class when one is close by. The panel now groups what's on into Combat / Position / Currency / Status with a blank line between sections. Added a Hide chat switch (removed in 1.118.0 - it didn't work).", nullptr}},
        {"Version %1", "1.116.0", {"Minecraft Dungeons II: removed Teleport to free camera - testing showed it didn't work, so it's gone rather than left broken. Kept Camera offset (confirmed working) with a shorter note. Console commands: replaced the two guessed free-camera-speed commands with a short note that scrollwheel speeds the free camera up or down while toggledebugcamera is on.", nullptr}},
        {"Version %1", "1.115.0", {"Minecraft Dungeons II > Camera & Display: added Camera offset X/Y/Z, shifting the camera from its normal spot while it still follows you around. Also added an experimental Teleport to free camera and two guessed console commands for free-camera speed (see 1.116.0).", nullptr}},
        {"Version %1", "1.114.0", {"Minecraft Dungeons II > Camera & Display: added a Deaths stat. Uses a real session counter when the game has one; otherwise falls back to counting your Health hitting zero.", nullptr}},
        {"Version %1", "1.113.0", {"Minecraft Dungeons II > Camera & Display: Damage per second is now self-contained, so the Raw damage numbers and Speedometer cards in Experiments were removed. Fixed the invisible \"&\" in the Camera & Display sidebar label.", nullptr}},
        {"Version %1", "1.112.0", {"Minecraft Dungeons II > Camera & Display: added an Enable Display mode master switch to turn every stat on/off together. Did not add an FPS reading - not something this app's safe reading technique can get at reliably.", nullptr}},
        {"Version %1", "1.111.0", {"Minecraft Dungeons II > Camera & Display (renamed from Camera Editor): fixed Damage per second getting stuck at 0. Added a Coordinates (X, Y, Z) line and Nudge controls to reposition the overlay. Simplified the Display mode note.", nullptr}},
        {"Version %1", "1.110.0", {"Minecraft Dungeons II: removed Soul Storm entirely (it never worked out). Added Camera Editor > Display mode: an on-screen overlay with Health, Damage, Damage per second, Walk speed and Active quest, each switched on independently.", nullptr}},
        {"Version %1", "1.109.0", {"Minecraft Dungeons II > World > Soul Storm: added an experimental, off-by-default Force Storminator reward card alongside the existing safe Active flag/Timer picker. High risk - only tested against one specific build's code layout.", nullptr}},
        {"Version %1", "1.108.0", {"Minecraft Dungeons II > Inventory Editor: simplified the notes around deleting items and editing an item.", nullptr}},
        {"Version %1", "1.107.0", {"Minecraft Dungeons II > Inventory Editor: cleaned up the layout - filters and the list are now one stack up top, with both delete tools moved into their own Delete items card below.", nullptr}},
        {"Version %1", "1.106.0", {"Minecraft Dungeons II > Inventory Editor: added a Category dropdown with Delete entire category (keep equipped).", nullptr}},
        {"Version %1", "1.105.0", {"Minecraft Dungeons II > No Clip & Stats > Soul Storm: rebuilt the old generic finder into two picks - an Active flag (Force start, Force stop, Keep storm active) and a Timer (Set, Freeze time) - instead of one plain select-a-field-and-set tool. The real switch and timer still aren't confirmed fields, so Scan storms and picking the right rows from the list is still needed first.", nullptr}},
        {"Version %1", "1.104.0", {"Minecraft Dungeons II > World > Item rarity drops: removed the Luck row from 1.103.0 - testing found it didn't help. Renamed Force this rarity only to Attempt force and reworded its note to be clear it's an experimental nudge, not a guarantee.", nullptr}},
        {"Version %1", "1.103.0", {"Minecraft Dungeons II > World > Item rarity drops: added a Luck row (the game's rarity bonus chance stat) with its own Set, Freeze, and a one-click Max luck button.", nullptr}},
        {"Version %1", "1.102.0", {"Minecraft Dungeons II > World > Item rarity drops: added Force [rarity] only - zeroes every other rarity's weight and maxes the one you pick, with a Keep forcing option that reapplies it automatically.", nullptr}},
        {"Version %1", "1.101.0", {"Minecraft Dungeons II > Inventory Editor: added a Rarity filter (Common, Rare, Special, Unique) to narrow the item list before picking what to edit.", nullptr}},
        {"Version %1", "1.100.0", {"Customize > Click sounds: added two more choices, Stone and Loud Click.", nullptr}},
        {"Version %1", "1.99.2", {"Minecraft Dungeons II > Nearby Viewer > Update automatically: now also re-scans the game every 6 seconds on its own, so newly loaded chests/enemies appear without a manual Refresh.", nullptr}},
        {"Version %1", "1.99.1", {"Minecraft Dungeons II > Nearby Viewer: fixed it sometimes showing/listing nothing, caused by an overly strict class-name match for enemies.", nullptr}},
        {"Version %1", "1.99.0", {"Minecraft Dungeons and Minecraft Dungeons II > Tools > Console: added a Macro button linking to Auto Dungeoneer on dungeoncollector.co.uk.",
                                   "Customize > Import / export style: fixed the Import style... button rendering all dark/black.", nullptr}},
        {"Version %1", "1.98.1", {"Minecraft Dungeons > Item Editor: moved Item presets below Enchantments and shortened its note, flagging it as a test feature that doesn't always work.", nullptr}},
        {"Version %1", "1.98.0", {"Minecraft Dungeons > Item Editor: added Legacy Mode and Modern Mode - Modern Mode lists your whole inventory and storage chest straight from the game's data instead of needing an in-game highlight.",
                                   "Modern Mode should work on every platform, Steam included, even where Legacy Mode's hook can't find anything.", nullptr}},
        {"Version %1", "1.97.0", {"Minecraft Dungeons > Tools > Console: Steam support is on hold for now after two attempts risked crashing the game; Minecraft Launcher and Xbox app are unaffected.",
                                   "Experiments: removed the mob-summon-fix Console section - that idea is being shelved.",
                                   "Home > Getting started: updated to cover both games.", nullptr}},
        {"Version %1", "1.96.2", {"Minecraft Dungeons > Tools > Console: reverted the Steam SpawnObject guess from 1.96.1 - it crashed the game on testing.", nullptr}},
        {"Version %1", "1.96.1", {"Minecraft Dungeons > Tools > Console: added a best-guess SpawnObject match for Steam 1.17.0.0 from the user's own log, picked out from 3 call-site candidates by which one looks like a real function (not a tiny field/flag check) and is only called once. Not yet confirmed by a successful test - please report back either way.", nullptr}},
        {"Version %1", "1.96.0", {"Minecraft Dungeons > Tools > Console: now tries to find the engine's object-creation function dynamically (the same technique Minecraft Dungeons II's console already uses) when the known build's hardcoded address doesn't match, instead of simply refusing. Should help on Steam - if it still doesn't connect, Refresh log and send it over so a confirmed build can be added.",
                                   "Mod toggles (Minecraft Dungeons): the on-screen message no longer includes the long \"how it's hooked in\" technical explanation, which used to resize the message area and shift the page layout every time a mod was switched on. That text still gets logged - find it in Backup > Debug.", nullptr}},
        {"Version %1", "1.95.0", {"Experiments: new Console: experimental mob-summon fix (Minecraft Dungeons II), with Safe Test and Blind Writes DLLs that try copying a live mob's combat attribute sets onto a console-summoned one. Untested and separate from the regular Console DLL so it can't be affected.",
                                   "Customize: new App sounds section under Click sounds - an optional looping background track (Futuristic - Pursuit), off by default, volume customizable (5% default). Avoid it while recording, in case of copyright.",
                                   "Minecraft Dungeons II: every page note about offline-only features now starts with \"Only works in OFFLINE - \".",
                                   "Changelog: shortened older multi-paragraph entries to a sentence or two.", nullptr}},
        {"Version %1", "1.94.1", {"Minecraft Dungeons II > No Cooldown: fixed Potions and Artifacts not being reusable instantly - their own cooldown-duration attributes are now zeroed too, not just charges/souls. Rolling was unaffected.", nullptr}},
        {"Version %1", "1.94.0", {"Minecraft Dungeons II > Inventory Editor: removed the Effects (enchantments) editing section - it kept risking item corruption. Item names and other fields (Rarity, Power, Level, Stack, XP, rerolls) still work as before.", nullptr}},
        {"Version %1", "1.93.2", {"Fixed a real MinGW/GCC build failure in the Inventory Editor's data catalog (Mcd2Catalog.cpp) caused by giant inline string/vector construction. Rewritten as small plain-data tables built via a loop - no behavior change.", nullptr}},
        {"Version %1", "1.93.1", {"Fixed a build error in the new Inventory Editor data catalog (Mcd2Catalog.cpp) - a few whole-number tier/level values were written as invalid C++ (e.g. 9f instead of 9.0f), which failed to compile. No behavior change once it compiles; this just makes 1.93.0's catalog overhaul actually build.", nullptr}},
        {"Version %1", "1.93.0", {"Minecraft Dungeons II > Inventory Editor: items and effects now show real in-game names (from a community data catalog) instead of raw tags. Strength and Points invested are now bounded to values the game can actually produce.", nullptr}},
        {"Version %1", "1.92.0",{"Minecraft Dungeons II > Inventory Editor > Effects (enchantments): the Effect slot list now labels each slot (Enchant) or (Base roll) - only the (Enchant) one is the item's actual enchantment slot; the others are its built-in signature ability and reroll effects, which the game itself never lets you retune. Added a short warning that editing effects can corrupt a character, since a built-in or reroll slot's Strength isn't meant to be changed and doing so risks corrupting the item.", nullptr}},
        {"Version %1", "1.91.1",{"Minecraft Dungeons II > Inventory Editor: Talisman Level is now capped at 3 - typing or scrolling higher just snaps back down to it, since 3 is the real max. Shortened the hint text under Selected item and Effects (enchantments).", nullptr}},
        {"Version %1", "1.91.0", {"Minecraft Dungeons II > Inventory Editor: item fields now apply instantly (no Apply button). Level/Item XP only show for Talismans. Effects (enchantments) got a Level field and auto-filled slots. The editor now refreshes itself automatically.", nullptr}},
        {"Version %1", "1.90.2", {"Information > Honorable mentions: added JustGaming101 to the list.", nullptr}},
        {"Version %1", "1.90.1", {"Information > Honorable mentions: replaced the whole list with Terrarings, Dweem, Calamari, Hybrid, Maz, Arno, Liaa, Moonlitfoxi, Lucas, Shadow, Inspirity and Jaded_Book.", nullptr}},
        {"Version %1", "1.90.0", {"Minecraft Dungeons > Tools: turning on a mod now shows how it actually reaches the game (patch, hook, or reflection) and the real address found. Backup Saves > Debug: removed the long folder/registry/log listing.", nullptr}},
        {"Version %1", "1.89.0", {"Minecraft Dungeons > Tools > Console: added a Close game button and Freecam & HUD commands, matching Minecraft Dungeons II's console. Removed the Dump functions tool added in 1.88.0 - it didn't lead anywhere useful.", nullptr}},
        {"Version %1", "1.88.0", {"Fixed 1.87's summoned-mob visibility fix only checking newly created objects - Unreal reuses freed object slots, so it rarely ran. It now checks every loaded object each pass, kept cheap by caching per class.", nullptr}},
        {"Version %1", "1.87.0", {"Minecraft Dungeons II > Tools > Console: summoned mobs used to appear invisible since stock summon skips the game's spawn-in logic. The DLL now makes them visible; it can't yet grant combat attribute sets, and logs how many each mob got.", nullptr}},
        {"Version %1", "1.86.0", {"Minecraft Dungeons II > Tools > Console: simplified the hint text, and added a Console key picker just like Minecraft Dungeons already has, so the console key can be changed from Tilde to F1-F12 or several other keys. The DLL now supports it too - it writes the chosen key into the game the same way the Minecraft Dungeons console does, and picks up changes within a couple of seconds.", nullptr}},
        {"Version %1", "1.85.0", {"Minecraft Dungeons II > Tools > Console: new Close game button next to Open folder. It ends every Minecraft Dungeons II app by process name (Microsoft Store/Xbox App, Steam, and the Minecraft Launcher), which is useful since closing the game normally doesn't work while the console is injected. Also cleaned up the wording on some older update notes that talked about things being untested.", nullptr}},
        {"Version %1", "1.84.0", {"Minecraft Dungeons II > Tools > Console: the freecam command group is now just toggledebugcamera (confirmed by the user to work fine on its own, so fly/ghost/walk were removed) plus a new showhud command to hide the on-screen HUD. Both are toggles - type the same command again to turn the camera or the HUD back to normal.", nullptr}},
        {"Version %1", "1.83.0", {"Minecraft Dungeons II > Tools > Console: added a Freecam & Noclip group to the command list - toggledebugcamera for a true detached flying camera, or fly/ghost/walk to make your own character fly through walls. These are built into Unreal Engine itself, so they should work the same on the Microsoft Store/Xbox build and Steam.", nullptr}},
        {"Version %1", "1.82.0", {"Minecraft Dungeons II > Streamer mode (BETA) moved out of Tools and into its own collapsible section under Account > BETA > Experiments, matching how Minecraft Dungeons II's other experimental features are organized. Same beta caveats still apply - it may not work yet.", nullptr}},
        {"Version %1", "1.81.1", {"Minecraft Dungeons II > Tools > Console: confirmed working on Steam (user-tested) - its SpawnObject code turned out to be byte-identical to the Microsoft Store/Xbox build, so no new signature was even needed. Updated the hint text to say so.", nullptr}},
        {"Version %1", "1.81.0", {"Minecraft Dungeons II > Tools > Console: it now at least attempts to start on the Steam build (previously it refused "
                                   "immediately, since it only recognised the Microsoft Store/Xbox exe name). Getting the console object itself made still "
                                   "needs the DLL to recognise the Steam build's exact compiled code, which it doesn't yet - if it refuses with \"not a build "
                                   "this DLL knows yet\", the log (DungeonKitConsole2.log) now records the exact bytes needed to add it properly.", nullptr}},
        {"Version %1", "1.80.1", {"Minecraft Dungeons II > Steam: added the real version-table entry (module size 216018944 -> 1.2.0), from a user-reported build. Home and the status bar now recognise it immediately on attach, on top of the live read added last version.", nullptr}},
        {"Version %1", "1.80.0", {"Minecraft Dungeons II: the shown version (Home and the status bar) is now read live from the game itself "
                                   "instead of only a fixed table, so it stays correct on Steam, Microsoft Store/Xbox, or any future patch without "
                                   "needing an update here. Checked every property name the Minecraft Dungeons II features use (teleports, quests, "
                                   "difficulty, ability system, etc.) against a fresh dump from the Steam build - all of them still match, so no mod "
                                   "code needed to change; the name-based lookup these already use is what makes that possible.", nullptr}},
        {"Version %1", "1.79.1", {"Fixed a build break in 1.79.0 (a stray edit had dropped the line declaring the changelog list itself, so the whole file failed to compile). No other changes.", nullptr}},
        {"Version %1", "1.79.0", {"Minecraft Dungeons II now also connects to the Steam release (same exe names as the original game). The version table only recognizes the Store build so far, so Steam may show as unrecognized - mods should still work by name/pattern.", nullptr}},
        {"Version %1", "1.78.3", {"Minecraft Dungeons > Tools > Hide my name: removed the BETA label added last version - that one isn't affected by the main-menu issue, only the Minecraft Dungeons II copy inside Streamer mode is.", nullptr}},
        {"Version %1", "1.78.2", {"Minecraft Dungeons II > Tools > Streamer mode, and Minecraft Dungeons > Tools > Hide my name: labeled BETA. Hide my name still doesn't reliably work (confirmed not working on the Minecraft Dungeons II main menu even after the 1.78.1 attempt at a fix) - still being worked on. Hide join ID and Hide version text are unaffected.", nullptr}},
        {"Version %1", "1.78.1", {"Minecraft Dungeons II > Streamer mode > Hide my name: fixed it not working on the main menu by also scanning for the player name directly, the same way Hide join ID does. Widened the list of name fields it checks.", nullptr}},
        {"Version %1", "1.78.0", {"Reserved names: once you type a reserved name's password correctly, this computer remembers it, so typing that same name again signs you in automatically with no password prompt. Works on the startup name screen and on the Account page.", nullptr}},
        {"Version %1", "1.77.1", {"Information > Credits: added JustGaming101 for providing the blueprint files used to expand the Console commands lists for both games.", nullptr}},
        {"Version %1", "1.77.0", {"Minecraft Dungeons > Tools > Console commands: added about 2,495 more commands (2,914 total) in 7 new alphabetized groups. Weapon/armor/artifact blueprints left out, as with Minecraft Dungeons II's pass.", nullptr}},
        {"Version %1", "1.76.0", {"Minecraft Dungeons II > Tools > Console commands: added about 710 more commands found in the game's own blueprint list, sorted alphabetically within each group (now 1,253 commands in the same 19 groups). Most of it is scenery, level gimmicks and mob variants; things like raw animation, weapon and armor blueprints were left out since they don't do anything useful on their own.", nullptr}},
        {"Version %1", "1.75.5", {"Languages: added translations for everything new this update (Experiments reorder, damage-per-second, Speedometer, the Supporters section and tag). Customize: the Experiments section titles you click to open or close can now be styled too (QToolButton#CollapsibleHeader), and the style-editor hint lists it.", nullptr}},
        {"Version %1", "1.75.4", {"Account > Notes: the tag note now also mentions supporters who've donated money and content creators, not just the honorable mentions.", nullptr}},
        {"Version %1", "1.75.3", {"The Content Creator reserved name is now JustGaming101 (was JustGaming), same password as before.", nullptr}},
        {"Version %1", "1.75.2", {"Information > Supporters: JustGaming101 and 51/49 are two different people, so they're back to being separate entries, filling out the whole list.", nullptr}},
        {"Version %1", "1.75.1", {"Information > Supporters: fixed a typo (51/49, not 51/47) and moved that entry to the empty spot at the end of the list.", nullptr}},
        {"Version %1", "1.75.0", {"Account: if a reserved name's role tag is too long to fit the sidebar, the text now shrinks a little instead of overflowing (short tags like Plushy - DEV are unaffected). Information: new Supporters section under Honorable mentions. New Supporter tag with its own reserved names and password, in the theme colours like the other tags.", nullptr}},
        {"Version %1", "1.74.0", {"Experiments: reordered and every section can now collapse. Raw damage numbers' note is shorter and now shows damage per second. New Speedometer card. Both now work in Minecraft Dungeons too.", nullptr}},
        {"Version %1", "1.73.0", {"Minecraft Dungeons > No Clip & Stats > Player stats: added 30 more attributes across Melee, Ranged, Health, Resistance and Movement. Some names aren't confirmed to exist on Minecraft Dungeons, but that's safe - an unavailable one just won't do anything.", nullptr}},
        {"Version %1", "1.72.0", {"Experiments > Item Editor (overhaul): rebuilt after it caused instant crashes from writing to the wrong kind of object. Editing now goes through the same verified object the main Item Editor uses. New: all enchant levels shown at once, named pickers instead of raw numbers, presets with names.", nullptr}},
        {"Version %1", "1.71.1", {"Fixed a build error in the new Item Editor (overhaul) card.", nullptr}},
        {"Version %1", "1.71.0", {"Experiments: new Item Editor (overhaul) card, a second item editor for Minecraft Dungeons that lists equipped items without highlighting and shares presets with the main editor. Edits ID, power, rarity and gilding; enchants only via presets.", nullptr}},
        {"Version %1", "1.70.7", {"Minecraft Dungeons > Items > Item presets: Copy preset code now reads the highlighted item directly, the same as Copy item data, so you don't need to save it as a preset first just to share it. Added a note that applying a preset onto a very different item can crash the game, same risk as Load item data.", nullptr}},
        {"Version %1", "1.70.6", {"Minecraft Dungeons > Items > Item presets: Add preset from code now opens a box to paste the code into, instead of silently reading the clipboard (which is likely why it looked broken - a wrong or stale clipboard failed with no way to see why).", nullptr}},
        {"Version %1", "1.70.5", {"Removed an incorrect line from the Backup saves debug info saying DungeonKit never injects anything into the game. The console feature does.", nullptr}},
        {"Version %1", "1.70.4", {"Minecraft Dungeons II > Tools > Console commands: Farm & Wild Animals merged into Pets, Villagers & Quest Givers merged with Merchants & Shops into Merchants & Villagers, and Weapons, Armor, Artifacts & Other Gear and Enchantments & Buffs removed. 542 commands in 18 groups. Console notes (both games) shortened.", nullptr}},
        {"Version %1", "1.70.3", {"Console (both games): the Inject console note now says Windows Defender may flag it, since it injects a DLL into the game, and to add the DungeonKit folder as an exclusion if you want to use it.", nullptr}},
        {"Version %1", "1.70.2", {"Minecraft Dungeons II > Tools > Console commands: 595 summon commands in 25 groups. All normal mobs are now in one Mobs group, with separate groups for Bosses, Pets, Farm & Wild Animals, Weapons, Armor, Artifacts & Other Gear, Fog, Lighting & Audio, Noteblocks & Music, and Death & Attack Effects.", nullptr}},
        {"Version %1", "1.70.1", {"Minecraft Dungeons II > Tools > Console commands: now 402 summon commands in 20 groups. New groups: Barrels & Containers, Pickups & Food, Zombies & Skeletons, Illagers & Spellcasters, Creepers, Endermen & Other Mobs, Mob Death Effects, Projectiles & Arrows, Gameplay Effects (GCN), Buildings & Landmarks and Lamps, Candles & Lights, plus the earlier groups with the new finds added.", nullptr}},
        {"Version %1", "1.70.0", {"Experiments: new Raw damage numbers card. Turn Watching on and it lists every hit as a number with decimals (enemy damage and, if you want, damage you take), by watching the Health value of the enemies near you. Show over the game puts the latest hits on top of the game window. Hits in the same instant are added together and a killing blow shows only the health that was left.", nullptr}},
        {"Version %1", "1.69.0", {"Minecraft Dungeons II > Tools: new Console commands card with 195 commands, search and Copy, matching Minecraft Dungeons' groups. Experiments: new Summonable things card that reads the running game for everything it can summon.", nullptr}},
        {"Version %1", "1.68.0", {"Minecraft Dungeons II > Tools: the Console now works the same way as in Minecraft Dungeons. It creates the game's console (press ~) and a cheat manager for each level, using the game's own SpawnObject function, which was found from your log. Only for Minecraft Dungeons II 1.1.2.0.", nullptr}},
        {"Version %1", "1.67.1", {"Console (test) for Minecraft Dungeons II now also looks for the game's own way of creating objects (SpawnObject) and writes the code it finds to the log. It still only reads the game.", nullptr}},
        {"Version %1", "1.67.0", {"Minecraft Dungeons II > Tools: Hide my name, Hide version text and Hide join ID are now one Streamer mode card, with an Update automatically switch (on by default) that keeps the changes applied if the game changes them back, and an Apply now button.", "New Console (test) card for Minecraft Dungeons II with its own DLL. This first version only looks at the game, writes a log and shows a popup in the game. It cannot create the console yet.", nullptr}},
        {"Version %1", "1.66.1", {"Home: the What DungeonKit does text is rewritten and now mentions the custom DLL, streamer mode and what the app connects to.", nullptr}},
        {"Version %1", "1.66.0", {"Minecraft Dungeons II > Tools: new Hide version text and Hide join ID cards under Hide my name, each with Show, Hide completely, Default and Custom. These are best effort and may not work, because the game may not redraw the text.", nullptr}},
        {"Version %1", "1.65.0", {"Hide my name is now also in Minecraft Dungeons II > Tools, under Pause Game, with the same choices (show, hide completely, Default (Player), Custom) and the lobby-list option. Hiding the version text and the join ID is not built yet.", nullptr}},
        {"Version %1", "1.64.2", {"Console commands: the Important category is always open. The others still start closed.", nullptr}},
        {"Version %1", "1.64.1", {"Console commands: every category now starts closed, and is closed again each time DungeonKit starts.", nullptr}},
        {"Version %1", "1.64.0", {"New Hide my name card in Minecraft Dungeons > Tools, under Pause Game. Hide your name completely, show Player, or use a custom name, and optionally hide everyone else's names (lobby list). It only changes what your own game shows.", nullptr}},
        {"Version %1", "1.63.0", {"Experiments: removed the UUU card and the bundled UUU files. DungeonKit's own Console (Minecraft Dungeons > Tools) replaces it.", nullptr}},
        {"Version %1", "1.62.2", {"Console: added a note that the game can't be closed the normal way while the console is injected. Close the game process from Home instead.", nullptr}},
        {"Version %1", "1.62.1", {"Console commands: added a note that they only work in your camp, and that other players can join your camp and see the same things.", nullptr}},
        {"Version %1", "1.62.0", {"Console commands: removed the Copy group buttons, added a search box (type any part of a command), and the note now says you only need to type enablecheats 1 once.", "Console commands: new Bosses, Mobs, Pets & Animals, Effects & Projectiles and Players & Other lists with about 390 more commands. The bigger lists start closed, click a title to open it.", nullptr}},
        {"Version %1", "1.61.0", {"Console: the Inject console button now looks like the other buttons. You can pick the key that opens the console (default ~). Command list is now in groups (Important, Chests, Merchants) with a Copy group button, and has all the merchants.", "Removed the Probe DLL and its card from Experiments. Moved Advanced (the dumps) to the bottom of Minecraft Dungeons > Tools.", nullptr}},
    {"Version %1", "1.60.0", {"The Console moved from Experiments to Minecraft Dungeons > Tools, under Advanced. Its note is shorter, and there is a new list of console commands you can copy.", nullptr}},
    {"Version %1", "1.59.0", {"Console DLL: it now shows a small popup in the game when it is connected and working, and shows a warning popup if it could not start.", "Console DLL: it also makes a cheat manager for each level, so commands like slomo and summon may work.", nullptr}},
        {"Version %1", "1.58.0", {"Experiments: new Console DLL (test) card for Minecraft Dungeons. The probe log showed the game has no console object, so the DLL creates one with the game's own Console class and puts it in the viewport; then the console key (~) works. It checks the game build first and only supports Minecraft Dungeons 1.17.0.0. Many console commands are removed from the shipping game. Inspired by UUU by Otis_Inf.", nullptr}},
        {"Version %1", "1.57.0", {"Experiments: removed the built-in UUU injection card (it crashed the game). The UUU app card stays. New Probe DLL (test) card: DungeonKit loads DungeonKitProbe.dll into the game. The DLL only reads the game and writes a log, which shows in the card. It is a first step toward our own console DLL, inspired by UUU by Otis_Inf.", nullptr}},
        {"Version %1", "1.56.1", {"Websites: added the Dungeon Collector page for Minecraft Dungeons II under the main Dungeon Collector, and the DungeonKit Discord server at the top of the Discord servers. Fixed the buttons on this page looking super dark.", nullptr}},
        {"Version %1", "1.56.0", {"New Content Creator tag with its own reserved names and password, in the theme colours like the other tags. The Websites page has a new look: every site has a round logo, its address under the name, and a theme coloured button. The Minecraft Dungeons II Inventory Editor note is shorter and simpler.", nullptr}},
        {"Version %1", "1.55.3", {"Information > General Information > Getting started: added a short line and an Open Information button that takes you to the Information section.", nullptr}},
        {"Version %1", "1.55.2", {"Minecraft Dungeons FAQ and Getting started fixed: the game is no longer attached automatically, so they now say to choose Minecraft Dungeons under Target game on Home. Added a question for \"Open, no game connected\", and updated the troubleshooting, platform and gifting answers.", nullptr}},
        {"Version %1", "1.55.1", {"Camera Editor (both games): the presets no longer change or freeze the camera distance (arm length), which could make the camera glitch. They only set the field of view and the default field of view. You can still set the distance yourself. The Minecraft Dungeons No Clip and Stats note no longer says to treat the features as experimental.", nullptr}},
        {"Version %1", "1.55.0", {"Experiments: two new cards above the Mod Maker for Minecraft Dungeons. Camera unlocker (UUU), built in: DungeonKit comes with UUU by Otis_Inf and loads it into the game for you. Camera unlocker (UUU), use the UUU app: open the UUU folder or start UuuClient.exe from DungeonKit, the bundled copy or your own.", nullptr}},
        {"Version %1", "1.54.0", {"Camera Editor (both games): removed the Minecraft, Close Up and Wide Angle presets.", "Minecraft Dungeons > Camera Editor: new Photo mode card with World speed, Freeze time, Slow motion and Normal speed, like Minecraft Dungeons II. It uses the same method as the rest of the Camera Editor, so it is meant to work on Steam, the Xbox App and the Minecraft Launcher.", nullptr}},
        {"Version %1", "1.53.2", {"Minecraft Dungeons > Item Editor: the Item presets instructions now include a short line on how to share a preset.", nullptr}},
        {"Version %1", "1.53.1", {"Minecraft Dungeons > Item Editor: shorter, simpler instructions for Item presets.", nullptr}},
        {"Version %1", "1.53.0", {"Minecraft Dungeons > Item Editor: Item presets are now a saved list, just like Copy / Load item data but kept for good. Save the highlighted item under a name, then apply any saved preset to any highlighted item later. Presets can be deleted, and shared with a short code. The old file import and export is gone.", nullptr}},
        {"Version %1", "1.52.0", {"Minecraft Dungeons > Item Editor: item presets and Copy / Load item data rebuilt on the same code. Both now include the enchant levels (tiers) and everything except stats. After loading, the item is read back and checked, and you are told if the game did not keep the values.", nullptr}},
        {"Version %1", "1.51.1", {"Fixed a build error in the Item presets code.", nullptr}},
        {"Version %1", "1.51.0", {"Minecraft Dungeons > Item Editor: new Item presets card. Save the highlighted item (item, power level, rarity, gilded, gilded enchant and level, enchantments and their tiers, but not stats) to a .dkitem file or a short code, and import a preset onto any highlighted item. Also new: a Cloned item row under Gilded level that shows whether the highlighted item is a clone.", nullptr}},
        {"Version %1", "1.50.0", {"Minecraft Dungeons II > No Clip and Stats now looks like the Minecraft Dungeons page: No Clip, God Mode and Overkill share one Movement card, followed by a Player stats card and the stat groups. Minecraft Dungeons II > Player: new Enemies Ignore You switch (experimental). It sets your aggro and every loaded enemy's sight and alert range to zero and puts them back when switched off.", nullptr}},
        {"Version %1", "1.49.0", {"Minecraft Dungeons II > No Clip and Stats: stats split into cards (General combat, Melee, Ranged, Artifacts, Elemental, Movement, Defense, Healing, Companions). About 100 new stats added.", nullptr}},
        {"Version %1", "1.48.1", {"Minecraft Dungeons II: the World page now shows the region you are in next to the map, for example Overworld (Sift). Missions can be set to Completed, Available, Active or Locked: pick one or several, or press Complete all active and available. Camera presets fixed: they now look for the camera again every time and also find cameras owned by the player controller.", nullptr}},
        {"Version %1", "1.48.0", {"Minecraft Dungeons II > World: new Item rarity drops card at the top. Rarity bonus chance, drop chance increase, drop duplication chance and looting multiplier can be set or frozen. Read chances lists every rarity with its weight and its drop chance in percent, and you can change a rarity's weight to get more commons, rares or uniques, or restore the original.", nullptr}},
        {"Version %1", "1.47.2", {"The Status section on Home no longer flickers when the language is not English. It always stays in English. The Experiments tab now has a clearer warning: features may change or stop working, and there are no guides or help planned for them.", nullptr}},
        {"Version %1", "1.47.1", {"The DEV, Assistant and Elite tags now use your theme's colours instead of fixed ones. Minecraft Dungeons II > Inventory Editor: Merchant prices moved to the bottom of the page. New Elite names: Maz, Terrarings and Tomcat.", nullptr}},
        {"Version %1", "1.47.0", {"Minecraft Dungeons II > Inventory Editor: new Merchant prices card, like the one in Minecraft Dungeons. When you buy something, the part you do not want to pay is given back in emeralds, spring stones and enchantment points. Presets Free, 25%, 50%, 75% and Normal, or any custom percentage.", nullptr}},
        {"Version %1", "1.46.2", {"Fixed a build error in the Mod Maker.", nullptr}},
        {"Version %1", "1.46.1", {"Experiments: the Mod Maker and the game data buttons now sit at the top of the page, above WASD Movement, so they are no longer hidden below the fold. Scroll down to see everything.", nullptr}},
        {"Version %1", "1.46.0", {"Experiments > Mod Maker: browse the live game's objects and properties and make your own mods. Pick a root (player, world ...) or find any object by class name, click a number or on/off property, then Set once or save it as a mod that holds its value while ticked. Mods can be shared as a short code. Also in Experiments: buttons to save the game's name, class and object dump files to your Desktop.", nullptr}},
        {"Version %1", "1.45.1", {"Fixed the status card at the bottom of the sidebar flickering between English and the chosen language. It now stays in English.", nullptr}},
        {"Version %1", "1.45.0", {"Account > BETA: new switch that adds an Experiments tab under Home. It is always off when the app starts. WASD Movement moved from Account to that tab. Turning BETA off also switches WASD off.", nullptr}},
        {"Version %1", "1.44.0", {"Minecraft Dungeons > Item Editor > Merchant prices: now covers gold as well as emeralds. Whatever you spend in either one, the part you chose not to pay is given back.", nullptr}},
        {"Version %1", "1.43.3", {"Minecraft Dungeons > Item Editor > Merchant prices: shorter, simpler text.", nullptr}},
        {"Version %1", "1.43.2", {"Minecraft Dungeons > Item Editor > Merchant prices: remade again, since the game stores no price/discount value - it now refunds the part of a purchase you chose not to pay, through the Currency Editor.", nullptr}},
        {"Version %1", "1.43.1", {"Minecraft Dungeons > World > Current mission: removed the Scaling list. Minecraft Dungeons > Item Editor > Merchant prices: remade. The game ships the shop price multiplier switched off (x1, from level 999), so the card now switches it on at every level with the price you pick, shows the original values, and has a Freeze checkbox that keeps the price in place.", nullptr}},
        {"Version %1", "1.43.0", {"Minecraft Dungeons > World > Current mission: multipliers now read from the live mission screen, plus a new Scaling list of the game's own numbers. Item Editor: new Merchant prices card with percentage presets.", nullptr}},
        {"Version %1", "1.42.2", {"Minecraft Dungeons > World > Teleports: shorter note at the top about mission teleports.", nullptr}},
        {"Version %1", "1.42.1", {"Minecraft Dungeons > World > Current mission: the mission screen multipliers now show up on their own, like the rest of the information. The button is gone. Open a mission on the map and the list appears within a few seconds.", nullptr}},
        {"Version %1", "1.42.0", {"Minecraft Dungeons > World > Current mission: new Read multipliers from the mission screen button. Open a mission on the map, press it, and the list the game shows there (recommended power, enemy damage, health, speed and so on) appears under it.", nullptr}},
        {"Version %1", "1.41.2", {"Minecraft Dungeons > World > Teleports: removed the Teleport button. Double-click a location to teleport there.", nullptr}},
        {"Version %1", "1.41.1", {"Minecraft Dungeons > World: removed the text at the top of the page. Fixed the Current mission section (and the Tower section) showing nothing: the data lives on the game's GameBP actor, not on the game mode, so the page now finds that actor. It also works when you are not the host.", nullptr}},
        {"Version %1", "1.41.0", {"Minecraft Dungeons: the Teleports page is now a section at the top of World, which frees a sidebar button. New My locations group: save your position under a name, then teleport back to it later or rename and delete it. Your locations are saved and load again the next time you open the app.", nullptr}},
        {"Version %1", "1.40.1", {"Minecraft Dungeons II > Tools: the note at the top now matches the Minecraft Dungeons one, with a sentence added that these tools were used to test whether DungeonKit could connect to Minecraft Dungeons II and work with its data.", nullptr}},
        {"Version %1", "1.40.0", {"Customize > Click sounds: pick No click sounds, Soft Click, Futuristic or Combined, and set the volume. Every button you press plays the sound. The choice and volume are remembered between launches, and it is off until you pick one.", nullptr}},
        {"Version %1", "1.39.2", {"Account > Experimental > WASD Movement: the directions were inverted, so W A S D now move you forward, back, left and right the right way round. In Minecraft Dungeons it also tells the game's controller which way you are moving, to help the camera follow.", nullptr}},
        {"Version %1", "1.39.1", {"Account > Experimental > WASD Movement now works a different way. Instead of moving your position, it gives the game its own walking input (the pawn's ControlInputVector), so walls, gravity and walking speed still apply. The Speed box is gone.", nullptr}},
        {"Version %1", "1.39.0", {"Account > Experimental: new card for features still in testing. First one is WASD Movement, for Minecraft Dungeons and for Minecraft Dungeons II. Hold W A S D to walk, with a direction offset. Offline only.", nullptr}},
        {"Version %1", "1.38.1", {"Account: the Rich Presence status now shows the DungeonKit username you are logged in as (and updates when you save a new one) instead of the Discord account name.", nullptr}},
        {"Version %1", "1.38.0", {"Minecraft Dungeons II > Inventory Viewer, World, Nearby Viewer and Inventory Editor: each page now has a Turn on switch in front of Refresh. While it is off the page reads nothing and its controls are disabled. Turning it on scans the game and fills the lists. Refresh then only reloads that page.", nullptr}},
        {"Version %1", "1.37.3", {"Minecraft Dungeons II > No Clip & Stats: the top card is now called God Mode & Overkill, and the God Mode description sits under its switch.", nullptr}},
        {"Version %1", "1.37.2", {"Minecraft Dungeons II > Overkill: the description now sits under the switch, like the Minecraft Dungeons switches. It works as a plain switch and does not need Refresh.", nullptr}},
        {"Version %1", "1.37.1", {"Minecraft Dungeons II > No Clip & Stats: new Overkill switch under God Mode. It raises your base, melee, ranged and artifact damage multipliers so almost everything dies in one hit, and puts the old values back when switched off. Offline only.", nullptr}},
        {"Version %1", "1.37.0", {"Minecraft Dungeons > No Clip & Stats: new Overkill switch under God Mode. It raises your melee and ranged damage so almost everything dies in one hit, and puts the old values back when switched off. It finds the values by name, so it is meant to work on Steam, the Xbox App and the Minecraft Launcher.", nullptr}},
        {"Version %1", "1.36.2", {"The honorable mention tag is now called Elite (it was Helper). There is a new blue Assistant tag, which uses the same password as the DEV tag.", nullptr}},
        {"Version %1", "1.36.1", {"Backup Saves: the Back up now button now looks like the buttons next to it. Helper tag renamed to Sage. The Account notes tell honorable mentions to contact Plushy on Discord to claim their tag.", nullptr}},
        {"Version %1", "1.36.0", {"Discord Rich Presence now works (Settings > Account > Discord). When switched on, your Discord profile shows that DungeonKit is open, its version and the game it is connected to. It is off by default and needs the Discord app running on the same computer.", nullptr}},
        {"Version %1", "1.35.0", {"Backup Saves: Minecraft Dungeons II saves are now included, and backups can include the ~mods folder. Each backup is a folder named with the game, the date and the game version, with a short info file inside. The Back up now button is readable on every theme, and the debug text is shorter.", nullptr}},
        {"Version %1", "1.34.0", {"Minecraft Dungeons II > Nearby Viewer: new radius presets (Normal, Far, Super far) and the radius can now go up to 5,000,000.", nullptr}},
        {"Version %1", "1.33.2", {"Minecraft Dungeons II: the notes under Selected item, Effects, Threat level and difficulty, Waypoints and Missions are shorter and simpler. The Missions card is no longer marked as a test.", nullptr}},
        {"Version %1", "1.33.1", {"Home: the selected game's dot now uses your theme colour instead of turning black. The What DungeonKit does text is updated.", nullptr}},
        {"Version %1", "1.33.0", {"Minecraft Dungeons II > Inventory Editor shows far more: every effect including the ones unlocked at each item level, plus Item XP, effect rerolls used and the item's properties. You can set a strength or points on all effects of an item at once, and list every effect the game has.", nullptr}},
        {"Version %1", "1.32.0", {"Minecraft Dungeons > Camera Editor and World pages, and Enemies Ignore You in Player. They read the game by name, so they are meant to work on Steam, the Xbox App and the Minecraft Launcher.", nullptr}},
        {"Version %1", "1.31.0", {"Minecraft Dungeons > Apocalypse Level: when no saved pointer path matches (Steam), it now finds the level on the open mission map by name. On Steam, open the mission map first, then switch it on.", nullptr}},
        {"Version %1", "1.30.1", {"Minecraft Dungeons > Tools > Advanced: new Dump classes and fields button, like the one in Minecraft Dungeons II.", nullptr}},
        {"Version %1", "1.30.0", {"Minecraft Dungeons > No Cooldown: when the game code isn't found (Steam), it now falls back to a slot cooldown mode that raises each equipment slot's cooldown reduction. On Steam, the message after switching it on shows what it changed.", nullptr}},
        {"Version %1", "1.29.1", {"Minecraft Dungeons II > No Clip & Stats: shorter, simpler text on every card.", nullptr}},
        {"Version %1", "1.29.0", {"Camera Editor: seven different presets - FOV Far, FOV Super, Quake Pro, Minecraft, Close Up, Tactical and Wide Angle. FOV Super, Quake Pro and Wide Angle only change the field of view and no longer touch the camera distance. FOV First Person is removed.",
                    "Removed the remaining notes about the account being at risk.", nullptr}},
        {"Version %1", "1.28.0", {"Minecraft Dungeons II > Player: the note at the top is shorter, and a small note says Infinite Arrows and the rolling part of No Cooldown also work online.",
                    "Customize: a third row with Randomize Theme (new random colours now) and Random Startup (a random theme every time DungeonKit opens and closes).", nullptr}},
        {"Version %1", "1.27.1", {"Minecraft Dungeons II > Player now reads in this order: Status, Player, Stats, Threat level and difficulty, Location.",
                    "Minecraft Dungeons II > Player card now looks like the Minecraft Dungeons one: Infinite Arrows, Infinite Enchant Points and No Cooldown, each with a one-line description.", nullptr}},
        {"Version %1", "1.27.0", {"Minecraft Dungeons II > Player: new Never run out card with Infinite arrows, Infinite enchantment points and No cooldown (potion, rolls, artifacts).",
                    "Customize: a second row of presets (Sandstone, Slime, Oxidized, Sunset, Neon, Royal, Ash).", nullptr}},
        {"Version %1", "1.26.0", {"Backup Saves: new Debug section that lists every folder the page reads, the one folder it writes to, the settings it stores, and a line for each step of the last backup (with Refresh, Copy and Clear log).", nullptr}},
        {"Version %1", "1.25.0", {"Inventory Editor (Minecraft Dungeons II): new Delete selected and Select all shown buttons. Pick one or many items (Ctrl / Shift) and delete them; equipped items are skipped. A test, so keep a backup of your save.", nullptr}},
        {"Version %1", "1.24.0", {"Home > Target game: new Close game processes button that ends every Minecraft Dungeons and Minecraft Dungeons II process, for games that keep running in the background after being closed.", nullptr}},
        {"Version %1", "1.23.1", {"Customize > Import / export style rewritten in plain words, with messages after each action and a list of the parts that can be styled (including the new tabs, sidebar scroll area and Disable all button).", nullptr}},
        {"Version %1", "1.23.0", {"Minecraft Dungeons II: No Clip and Fly are now one No Clip switch on No Clip & Stats, like in Minecraft Dungeons.",
                    "Minecraft Dungeons II: new Soul Storm (finder) on No Clip & Stats. It lists the game's storm values so the storm timer can be found, set or held (test).", nullptr}},
        {"Version %1", "1.22.1", {"The loading screen now looks like it is searching for Minecraft Dungeons: Steam, Xbox app, Minecraft Launcher and save folders, with the folder being checked shown underneath.", nullptr}},
        {"Version %1", "1.22.0", {"The sidebar now scrolls when the window is too short for every page button.",
                    "The animated LCD theme preset is removed (it made the interface lag while the colours changed).",
                    "Every new text since 1.17.0 is translated, including the changelog and the Inventory Editor and mission notes.", nullptr}},
        {"Version %1", "1.21.1", {"Minecraft Dungeons II: health, emeralds, XP, level, souls and arrows are back on Player; movement, interaction range and damage stay on No Clip & Stats.",
                    "Minecraft Dungeons II: new God Mode on No Clip & Stats.", nullptr}},
        {"Version %1", "1.21.0", {"Home: General Information, Minecraft Dungeons FAQ and Minecraft Dungeons II FAQ buttons, with new Minecraft Dungeons II questions and answers.",
                    "Items is now called Item Editor in the Minecraft Dungeons category.",
                    "Minecraft Dungeons II: the Inventory Viewer page is removed from the sidebar (the Inventory Editor lists everything).",
                    "Minecraft Dungeons II: new No Clip & Stats page with a new No Clip mod; Fly, Movement and Stats moved into it.", nullptr}},
        {"Version %1", "1.20.0", {"World: new Missions (test) section that lists the missions the game has loaded with their state and has an Unlock locked missions button.", nullptr}},
        {"Version %1", "1.19.0", {"Minecraft Dungeons II category reordered: Inventory Viewer, Inventory Editor, Player, World, Nearby Viewer, Camera Editor, Tools.",
                    "Player Viewer is now Player and World Viewer is now World. The Minecraft Dungeons II pages have their own icons.",
                    "Inventory Editor: double-click an item to load it into Selected item. Effects are safer to edit (see the note).", nullptr}},
        {"Version %1", "1.18.2", {"Inventory Editor: new Power field and a Power column; Power and Level accept values up to 99,999.",
                    "Inventory Editor: cosmetics, audio, consumables, quest items and materials are hidden (Show everything brings them back).",
                    "Inventory Editor: item stats (effects) are listed in the table, and names are shown cleaned up (Club instead of SW.Item.Club).",
                    "Inventory Editor: Equipped only filter and an Equipped slots picker.", nullptr}},
        {"Version %1", "1.18.1", {"Every Minecraft Dungeons II page now has a Disable all mods button (it also switches off Fly and every Freeze).",
                    "The Minecraft Dungeons II pages are translated into Spanish, French, Japanese, Chinese and Russian.", nullptr}},
        {"Version %1", "1.18.0", {"New Inventory Editor page (Minecraft Dungeons II): change an item's level, stack, rarity and item, and its enchantments.",
                    "Tools is now at the bottom of the Minecraft Dungeons II category.",
                    "World Viewer note: teleporting only works offline.", nullptr}},
        {"Version %1", "1.17.1", {"The MODDING category is now called Minecraft Dungeons.",
                    "Nearby Viewer: double-click a row to teleport. The note now says it does not work online.",
                    "Player Viewer: online/offline notes for Fly, Movement, Stats, Threat level and Difficulty.",
                    "Player Viewer > Stats: souls, arrows (ammo) and damage values added.", nullptr}},
        {"Version %1", "1.17.0", {"Nearby Viewer: chests and enemies around you with distance, state and teleport.",
                    "World Viewer: named waypoints saved between sessions.",
                    "Camera Editor: Photo mode (freeze time, slow motion, normal speed).",
                    "Player Viewer: Fly, threat level and difficulty.", nullptr}},
        {"Version %1", "1.16.0", {"Home: new Default target that is not connected to any game. Choose Minecraft Dungeons or Minecraft Dungeons II.",
                    "New Minecraft Dungeons II section with Tools, Player Viewer, Inventory Viewer, World Viewer and Camera Editor.",
                    "Camera Editor has presets: FOV Far, FOV Super and FOV First Person.",
                    "Minecraft Dungeons II version 1.1.2 is recognised.",
                    "Data dump now carries a developer-only warning. Warning-sign icons mark the Minecraft Dungeons II pages.", nullptr}},
        {"Version %1", "1.15.0-test", {"Minecraft Dungeons II test pages: MCD2 Player (location, stats, movement), MCD2 Inventory, MCD2 World and MCD2 Camera.",
                    "Inventory and World are read-only. Player and Camera can write values, with Freeze.", nullptr}},
        {"Version %1", "1.14.0-test", {"Test page: Data dump (names, classes and fields, objects) that works for Minecraft Dungeons II.",
                    "The object table is now found without game-specific patterns when the usual ones don't match.", nullptr}},
        {"Version %1", "1.13.3-test", {"Minecraft Dungeons II test: the automatic layout search accepts larger property offsets.", nullptr}},
        {"Version %1", "1.13.2-test", {"Minecraft Dungeons II test: the engine lookup can now work out an unknown engine's property layout automatically.",
                    "Run check shows each step of the path to the game's time settings.", nullptr}},
        {"Version %1", "1.13.1-test", {"Minecraft Dungeons II test: attaches to Dungeons-WinGDK-Shipping.exe and Dungeons.exe at the same time.",
                    "Timer and Pause Game use whichever of the two processes has readable engine data.",
                    "Run check reports both processes.", nullptr}},
        {"Version %1", "1.13.0-test", {"Home has a Target game choice: Minecraft Dungeons or Minecraft Dungeons II.",
                    "New Test category with Timer and Pause Game for Minecraft Dungeons II (Dungeons-WinGDK-Shipping.exe).",
                    "Test page has a Run check button that produces a copyable connection report.", nullptr}},
        {"Version %1", "1.12.1", {"Copy Item Data and Load Item Data also copy the item ID, gilded enchant and enchantment IDs, but not enchantment levels or stats.", nullptr}},
        {"Version %1", "1.12.0", {"Apocalypse Level can be frozen, and no longer shows a confirmation popup.",
                    "Enchantment tiers accept custom values.",
                    "Copy Item Data and Load Item Data now copy only rarity, power level, gilded and normal enchantments.",
                    "Item stat detection supports items with a very large number of stats.",
                    "Added a note that Player Model does not always work.",
                    "Changelog wording made neutral.", nullptr}},
        {"Version %1", "1.11.0", {"Backup Saves page: copies Minecraft Dungeons saves into a dated folder on the Desktop.",
                    "Pak Directory lists every mod with its size and date, and can switch mods off and on.",
                    "Changelog page, with a placeholder for update checking.",
                    "Animated colour theme preset (removed in 1.22.0).",
                    "Languages: Spanish, French, Japanese, Chinese and Russian.", nullptr}},
        {"Version %1", "1.10.0", {"Safety notice about downloading pak mods.",
                    "Websites page split into website tools, modding websites and Discord servers.",
                    "Sidebar layout editor in Customize (locked until unlocked).", nullptr}},
        {"Version %1", "1.9.0", {"New Quick Access category with the Pak Directory and Websites pages.",
                    "Pak Directory finds the Paks folder, accepts a pasted path, and counts mods.",
                    "Item Stat Viewer credit now reads Hybrid.", nullptr}},
        {"Version %1", "1.8.0", {"First Paks folder finder, on the Home page.", nullptr}},
        {"Versions %1", "1.3.0 - 1.7.0",
                   {"New logo that recolors with the theme, and a title bar that matches the theme.",
                    "Window title shows the app version, game version and user name.",
                    "Loading screen, sidebar categories, and a Home page with a status dot.",
                    "No Clip & Stats page, plus a Tools page with Timer and Pause Game.",
                    "Apocalypse Level, and the read-only Item Stat Viewer.",
                    "Disable all mods on every Modding page, FAQ, Account notes, Dev and Helper tags.",
                    "Items page shows the first three enchantment levels, with more on request.", nullptr}},
        {"Before version %1", "1.3.0", {"The core features ported from the Cheat Engine table: items, currency, XP, teleports and more.", nullptr}},
    };
    for (const Release& r : releases) {
        auto* c = card(v, QString::fromLatin1(r.fmt).arg(QString::fromLatin1(r.version)));
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        for (const char* const* it = r.items; *it; ++it) {
            auto* l = new QLabel(QString::fromUtf8("•  ") + QString::fromUtf8(*it));
            l->setWordWrap(true);
            cl->addWidget(l);
        }
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

void MainWindow::updateHomeStatus() {
    if (!homeDot_) return;
    enum S { Working, Updating, Testing, Down, Unknown } st = Unknown;
    const bool online = proc_.attached() && proc_.alive();
    std::optional<GameVersion> ver;
    if (online) ver = proc_.version();
    if (online) st = ver ? Working : Testing;

    // Optional override: a status.txt next to the exe whose first word is working / updating / testing / down / unknown.
    QFile f(QApplication::applicationDirPath() + "/status.txt");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString w = QString::fromUtf8(f.readLine()).trimmed().toLower();
        if (w.startsWith("working")) st = Working;
        else if (w.startsWith("updating")) st = Updating;
        else if (w.startsWith("testing")) st = Testing;
        else if (w.startsWith("down")) st = Down;
        else if (w.startsWith("unknown")) st = Unknown;
    }

    static const char* kName[] = {"Working", "Updating", "Testing", "Down", "Unknown"};
    static const char* kColor[] = {"#3ddc84", "#ff9f43", "#ffd93d", "#ff5c6c", "#f4f4f4"};
    static const char* kText[] = {
        "Everything is running as expected for this game version.",
        "A game update is being looked at. Some features may not work yet.",
        "This version is still being tested. Features may behave oddly.",
        "DungeonKit is not working right now. Check the Discord for news.",
        "DungeonKit can't tell yet. Start Minecraft Dungeons so it can check."};
    homeDot_->setStyleSheet(QString("background: %1; border-radius: 7px;").arg(kColor[st]));
    homeState_->setText(kName[st]);
    homeStateText_->setText(target_ == Process::Target::None ? QString("Default: DungeonKit is not connected to any game. Choose Minecraft Dungeons or Minecraft Dungeons II below.") : QString(kText[st]));
    const bool g2 = target_ == Process::Target::Dungeons2;
    if (target_ == Process::Target::None)
        homeGame_->setText("Game: none selected (Default)");
    else if (online && g2) {
        const QString plat = mcd2PlatformLabel(proc_.exeName());
        const QString live = mcd2LiveVersionFor(proc_);
        if (!live.isEmpty()) homeGame_->setText(QString("Game: Minecraft Dungeons II %1 (%2)").arg(live, plat));
        else if (ver) homeGame_->setText(QString("Game: Minecraft Dungeons II %1 (%2)").arg(QString::fromUtf8(ver->name), plat));
        else homeGame_->setText(QString("Game: Minecraft Dungeons II (%1), version not detected yet").arg(plat));
    }
    else if (online)
        homeGame_->setText(ver ? QString("Game: Minecraft Dungeons %1 (%2)")
                                     .arg(QString::fromUtf8(ver->name), QString::fromUtf8(platformName(ver->platform)))
                               : QString("Game: Minecraft Dungeons, build %1 (not in the version table)").arg(proc_.moduleSize()));
    else
        homeGame_->setText(g2 ? "Game: Minecraft Dungeons II not running" : "Game: not running");
}

// ====================================================================== Player (the mods that used to be on Home)
QWidget* MainWindow::buildMods() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);

    auto* top = new QHBoxLayout;
    auto* title = new QLabel("Player");
    title->setObjectName("PageTitle");
    top->addWidget(title);
    top->addStretch(1);
    auto* offAll = new QPushButton("Disable all mods");
    offAll->setCursor(Qt::PointingHandCursor);
    top->addWidget(offAll);
    connect(offAll, &QPushButton::clicked, this, [this]() { disableAll(); });
    outer->addLayout(top);

    message_ = new QLabel;
    message_->setObjectName("Message");
    message_->setWordWrap(true);
    outer->addWidget(message_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);
    auto& m = game_->mods;

    {   // Player
        auto* c = card(v, "Player");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.arrows);
        addToggle(cl, m.lives);
        addToggle(cl, m.enchant);
        addToggle(cl, m.cooldown);
        addToggle(cl, m.notarget);
    }
    {   // Currency
        auto* c = card(v, "Currency");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.currency);
        auto* cur = m.currency;
        auto mk = [&](const char* name, CurrencyMod::Kind k) {
            addIntRow(cl, name, [this, cur, k](int32_t& v) { return cur->get(proc_, k, v); },
                      [this, cur, k](int32_t v) { return cur->set(proc_, k, v); }, cur);
        };
        mk("Emeralds", CurrencyMod::Emerald);
        mk("Gold", CurrencyMod::Gold);
        mk("Ender Eyes", CurrencyMod::EnderEyes);
    }
    {   // XP + threat
        auto* c = card(v, "Progression");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.xp);
        auto* xp = m.xp;
        addIntRow(cl, "XP multiplier", [xp](int32_t& v) { v = (int32_t)xp->multiplier(); return xp->enabled(); },
                  [this, xp](int32_t v) { return v >= 1 && xp->setMultiplier(proc_, v); }, xp);
        addIntRow(cl, "Total XP", [this, xp](int32_t& v) { return xp->total(proc_, v); },
                  [this, xp](int32_t v) { return xp->setTotal(proc_, v); }, xp);
        addToggle(cl, m.threat);
        auto* th = m.threat;
        addIntRow(cl, "Threat level", [this, th](int32_t& v) { return th->get(proc_, v); },
                  [this, th](int32_t v) { return th->set(proc_, v); }, th);
    }
    {   // Position
        auto* c = card(v, "Position");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.position);
        auto* pos = m.position;
        const char* axes[] = {"X", "Y", "Z"};
        for (int i = 0; i < 3; ++i) {
            addFloatRow(cl, axes[i],
                [this, pos, i](float& out) { float a[3]; if (!pos->get(proc_, a)) return false; out = a[i]; return true; },
                [this, pos, i](float val) { float a[3]; if (!pos->get(proc_, a)) return false; a[i] = val; return pos->set(proc_, a); },
                pos);
        }
        auto* btns = new QHBoxLayout;
        auto* save = new QPushButton("Save position  (F5)");
        auto* tp = new QPushButton("Teleport to saved  (F6)");
        btns->addWidget(save);
        btns->addWidget(tp);
        btns->addStretch(1);
        cl->addLayout(btns);
        connect(save, &QPushButton::clicked, this, [this, pos]() {
            setMessage(pos->save(proc_) ? "Position saved." : "Could not read the position - enable Position / Teleport and move in-game.");
        });
        connect(tp, &QPushButton::clicked, this, [this, pos]() {
            setMessage(pos->teleportSaved(proc_) ? "Teleported." : "No saved position yet (or position not linked).");
        });
    }
    {   // Apocalypse level
        auto* c = card(v, "Apocalypse Level");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* warn = new QLabel(
            "Warning: if you use this while playing with other people, it can mess up their characters, and there is "
            "no way to reset it afterwards. Only use it on your own, and only if you're sure.");
        warn->setObjectName("Notice");
        warn->setWordWrap(true);
        cl->addWidget(warn);
        addToggle(cl, m.apocalypse);
        auto* ap = m.apocalypse;
        addIntRow(cl, "Level",
                  [this, ap](int32_t& val) { return ap->get(proc_, val); },
                  [this, ap](int32_t val) { return ap->set(proc_, val); }, ap, true);
    }
    {   // Model
        auto* c = card(v, "Player Model");
        {
            auto* pmWarn = new QLabel("Player Model does not always work. Sometimes the change applies and sometimes it does not. "
                                      "If nothing happens, switch it off and on again, or reload the level.");
            pmWarn->setObjectName("Notice");
            pmWarn->setWordWrap(true);
            qobject_cast<QVBoxLayout*>(c->layout())->addWidget(pmWarn);
        }
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.scale);
        auto* sc = m.scale;
        addFloatRow(cl, "Width",
            [this, sc](float& o) { float w, s; if (!sc->get(proc_, w, s)) return false; o = w; return true; },
            [this, sc](float val) { float w, s; if (!sc->get(proc_, w, s)) return false; return sc->set(proc_, val, s); }, sc);
        addFloatRow(cl, "Stretch",
            [this, sc](float& o) { float w, s; if (!sc->get(proc_, w, s)) return false; o = s; return true; },
            [this, sc](float val) { float w, s; if (!sc->get(proc_, w, s)) return false; return sc->set(proc_, w, val); }, sc);
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);

    return page;
}

// ====================================================================== Account
QWidget* MainWindow::buildAccount() {
    auto* page = new QWidget;
    auto* v = new QVBoxLayout(page);
    v->setContentsMargins(28, 24, 28, 20);
    v->setSpacing(12);
    auto* title = new QLabel("Account");
    title->setObjectName("PageTitle");
    v->addWidget(title);

    auto* c = card(v, "Username");
    auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
    auto* hint = new QLabel("Stored on this computer only. DungeonKit has no online or cloud accounts. It is recommended to keep it a gamertag and not use "
                          "real names. Be safe, and be smart.");
    hint->setObjectName("Hint");
    cl->addWidget(hint);
    auto* row = new QHBoxLayout;
    userEdit_ = new QLineEdit(username_);
    userEdit_->setPlaceholderText("Choose a username");
    userEdit_->setMaxLength(24);
    auto* save = new QPushButton("Save");
    save->setObjectName("Primary");
    row->addWidget(userEdit_, 1);
    row->addWidget(save);
    cl->addLayout(row);

    auto apply = [this]() {
        QString n = userEdit_->text().trimmed();
        if (n.size() < 2 || n.size() > 24) { setMessage("Use 2 to 24 characters for your username."); return; }
        Identity::Role role = Identity::Role::None;
        if (Identity::isReserved(n)) {
            const QString canon = Identity::canonicalName(n);
            const Identity::Role want = Identity::roleOf(n);
            if (role_ == want && canon == username_) {
                role = want;                                   // already unlocked this session
            } else if (Identity::askPassword(this, want, n)) {
                role = want;
            } else {
                userEdit_->setText(username_);                 // wrong password or cancelled: keep the old name
                return;
            }
            n = canon;
            userEdit_->setText(n);
        }
        username_ = n;
        role_ = role;
        QSettings().setValue("account/username", username_);
        updateUserLabel();
        updateTitle();
        if (QSettings().value("discord/richPresence", false).toBool()) updatePresence(true);   // refresh the name shown in the Account card
        welcome_->setText(QString("Welcome back, %1.").arg(username_));
        setMessage(role == Identity::Role::Dev ? "Developer name unlocked."
                       : role == Identity::Role::Assistant ? "Assistant name unlocked."
                       : role == Identity::Role::Helper ? "Elite name unlocked."
                       : role == Identity::Role::Creator ? "Content Creator name unlocked."
                       : role == Identity::Role::Supporter ? "Supporter name unlocked." : "Username saved.");
    };
    connect(save, &QPushButton::clicked, this, apply);
    connect(userEdit_, &QLineEdit::returnPressed, this, apply);
    updateUserLabel();

    {   // notes
        auto* nc = card(v, "Notes");
        auto* nl = qobject_cast<QVBoxLayout*>(nc->layout());
        auto* n1 = new QLabel(
            "If you're in the honorable mentions, a supporter who's donated money, or a content creator, you get your own "
            "special tag in the app, protected by a password. It doesn't give you any extra or hidden features. It's just "
            "a way to show appreciation for your support and give something back. To claim your tag, contact Plushy on Discord.");
        auto* n2 = new QLabel(
            "There's a plan for DungeonKit to connect to a Discord bot and use public channel webhooks to show information "
            "about the app, the game, and more. That may include ways to edit or share builds, mod IDs, and similar things. "
            "If people are against this feature, it may be scrapped.");
        for (QLabel* l : {n1, n2}) { l->setWordWrap(true); nl->addWidget(l); }
    }
    {   // Discord Rich Presence
        auto* dc = card(v, "Discord");
        auto* dl = qobject_cast<QVBoxLayout*>(dc->layout());
        auto* rp = new QCheckBox("Show DungeonKit as my Discord activity (Rich Presence)");
        rp->setChecked(QSettings().value("discord/richPresence", false).toBool());
        connect(rp, &QCheckBox::toggled, this, [this](bool on) { QSettings().setValue("discord/richPresence", on); updatePresence(true); });
        dl->addWidget(rp);
        discordStatus_ = new QLabel;
        discordStatus_->setObjectName("Hint");
        discordStatus_->setWordWrap(true);
        dl->addWidget(discordStatus_);
        auto* note = new QLabel("Rich Presence shows that DungeonKit is open, its version and the game it is connected to. It only talks to the Discord app on this computer. It never shows your items, stats or mods.");
        note->setObjectName("Hint");
        note->setWordWrap(true);
        dl->addWidget(note);
        updatePresence(true);
    }
    {   // BETA
        auto* bc = card(v, "BETA");
        auto* bl = qobject_cast<QVBoxLayout*>(bc->layout());
        auto* beta = new QCheckBox("Enable BETA features");
        beta->setCursor(Qt::PointingHandCursor);
        bl->addWidget(beta);
        auto* bh = new QLabel("Adds an Experiments tab under Home with features that are still being worked on. They may change, stop working or be removed. "
                              "This is always off when DungeonKit starts.");
        bh->setObjectName("Hint");
        bh->setWordWrap(true);
        bl->addWidget(bh);
        connect(beta, &QCheckBox::toggled, this, [this](bool on) { setBeta(on); });
    }
    v->addStretch(1);
    return page;
}

QWidget* MainWindow::buildExperiments() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(28, 24, 28, 20);
    v->setSpacing(12);
    auto* title = new QLabel("Experiments");
    title->setObjectName("PageTitle");
    v->addWidget(title);
    auto* note = new QLabel("Warning: these features are still being worked on. They may change, stop working, or be removed. There are no guides or help planned for them, so you will need to figure them out on your own. Turn them off in Account > BETA. Click a section's title to open or close it.");
    note->setObjectName("Hint");
    note->setWordWrap(true);
    v->addWidget(note);

    auto liveProc = [this]() -> Process* {
        if (target_ == Process::Target::None) return nullptr;
        if (target_ == Process::Target::Dungeons) return proc_.attached() && proc_.alive() ? &proc_ : nullptr;
        if (!proc_.attached() && !procAlt_.attached()) return nullptr;
        return engineProc();
    };

    // ---- 1. WASD Movement
    {
        auto* ec = cardStandalone("WASD Movement");
        auto* el = qobject_cast<QVBoxLayout*>(ec->layout());
        auto* intro = new QLabel("Walk with W A S D instead of the mouse. Still being worked on.");
        intro->setObjectName("Hint");
        intro->setWordWrap(true);
        el->addWidget(intro);
        auto hintUnder = [el](const QString& t) {
            auto* d = new QLabel(t);
            d->setObjectName("Hint");
            d->setWordWrap(true);
            d->setContentsMargins(46, 0, 0, 4);
            el->addWidget(d);
        };
        // WASD movement, Minecraft Dungeons
        auto* w1 = new QCheckBox("WASD Movement (Minecraft Dungeons)");
        w1->setCursor(Qt::PointingHandCursor);
        el->addWidget(w1);
        hintUnder("Hold W A S D to walk. It gives the game its own movement input, so walls, gravity and walking speed still apply. Offline only. Keys only count while the game is the active window.");
        toggles_.push_back({game_->mods.wasd, w1});
        connect(w1, &QCheckBox::clicked, this, [this, w1](bool on) { toggleMod(game_->mods.wasd, w1, on); });
        // WASD movement, Minecraft Dungeons II
        auto* w2 = new QCheckBox("WASD Movement (Minecraft Dungeons II)");
        w2->setCursor(Qt::PointingHandCursor);
        el->addWidget(w2);
        hintUnder("The same for Minecraft Dungeons II. Offline only. Switched off by Disable all mods and when the game closes.");
        connect(w2, &QCheckBox::clicked, this, [this, w2](bool on) {
            if (on && !mcd2Ready()) {
                w2->setChecked(false);
                setMessage(target_ == Process::Target::Dungeons2 ? "Minecraft Dungeons II is not ready yet. Load into a level first."
                                                                 : "Choose Minecraft Dungeons II on the Home page and start the game first.");
                return;
            }
            mcd2Hub_->setWasd(on);
            setMessage(on ? "WASD Movement (Minecraft Dungeons II) enabled." : "WASD Movement (Minecraft Dungeons II) disabled.");
        });
        connect(mcd2Hub_, &Mcd2Hub::wasdChanged, this, [w2](bool on) { w2->blockSignals(true); w2->setChecked(on); w2->blockSignals(false); });
        // shared settings
        auto* row = new QHBoxLayout;
        auto* rl = new QLabel("Direction offset (degrees)");
        auto* rot = new QDoubleSpinBox;
        rot->setRange(-180, 180);
        rot->setDecimals(0);
        rot->setSingleStep(15);
        rot->setValue(QSettings().value("experimental/wasdRotation", 0.0).toDouble());
        row->addWidget(rl);
        row->addWidget(rot);
        row->addStretch(1);
        el->addLayout(row);
        auto* sh = new QLabel("If W does not move you the way the screen points, change the direction offset (try 45 or -45).");
        sh->setObjectName("Hint");
        sh->setWordWrap(true);
        el->addWidget(sh);
        auto applyWasd = [this, rot]() {
            game_->mods.wasd->setRotation((float)rot->value());
            mcd2Hub_->setWasdRotation(rot->value());
            QSettings().setValue("experimental/wasdRotation", rot->value());
        };
        connect(rot, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, applyWasd);
        applyWasd();
        wrapCollapsible(v, ec, "wasd", false);
    }

    // ---- 2. Summonable things
    wrapCollapsible(v, addSummonCard(), "summon", false);

    // ---- 3. Mod Maker
    modMaker_ = new ModMakerCard(game_->mods.ue4, liveProc, this);
    connect(modMaker_, &ModMakerCard::message, this, &MainWindow::setMessage);
    wrapCollapsible(v, modMaker_, "modmaker", false);

    // ---- 4. Streamer mode (Minecraft Dungeons II)
    {
        auto liveProc2 = [this]() -> Process* {
            if (target_ != Process::Target::Dungeons2) return nullptr;
            Process* p = engineProc();
            return p && p->attached() && p->alive() ? p : nullptr;
        };
        auto* sc = cardStandalone("Streamer mode (Minecraft Dungeons II)");
        auto* sl = qobject_cast<QVBoxLayout*>(sc->layout());
        auto* sintro = new QLabel("Hide or change what your own game shows, for streaming. Other players still see your real name. "
                                 "Turn on Update automatically to keep it applied, because the game sometimes changes things back. "
                                 "Hide join ID and Hide version text are generally reliable, but Hide my name currently does not reliably "
                                 "work (it's known to fail on the main menu, and may not work in-level either). Still being worked on.");
        sintro->setObjectName("Hint");
        sintro->setWordWrap(true);
        sl->addWidget(sintro);
        auto* sautoRow = new QHBoxLayout;
        auto* sautoBox = new QCheckBox("Update automatically");
        sautoBox->setChecked(QSettings().value("streamer2/auto", true).toBool());
        auto* sapplyBtn = new QPushButton("Apply now");
        sapplyBtn->setCursor(Qt::PointingHandCursor);
        sautoRow->addWidget(sautoBox);
        sautoRow->addStretch(1);
        sautoRow->addWidget(sapplyBtn);
        sl->addLayout(sautoRow);

        auto* nameCard = new NameHideCard(liveProc2, "namehide2", this, true);
        WidgetHideCard::Spec ver;
        ver.title = "Hide version text";
        ver.hint = "Hides or changes the game's version text on screen (the Tab screen). Best effort: the game may not redraw it until the screen is opened again, and it may not work at all.";
        ver.settingsKey = "versionhide2";
        ver.widgetClass = "W_BuildWatermark_C";
        ver.defaultText = "Game";
        ver.textProps = {"WatermarkText"};
        auto* verCard = new WidgetHideCard(ver, liveProc2, this, true);
        WidgetHideCard::Spec jid;
        jid.title = "Hide join ID";
        jid.hint = "Hides or changes the party join code on screen. Best effort: the game may not redraw it until the screen is opened again, and it may not work at all. "
                   "Other people can still join with the real code if they have it.";
        jid.settingsKey = "joinidhide2";
        jid.widgetClass = "W_PartyCodeText_C";
        jid.defaultText = "------";
        jid.textProps = {"PartyCodeTextBlock"};
        auto* jidCard = new WidgetHideCard(jid, liveProc2, this, true);
        sl->addWidget(nameCard);
        sl->addWidget(verCard);
        sl->addWidget(jidCard);
        auto setAuto = [nameCard, verCard, jidCard](bool on) { nameCard->setAutoUpdate(on); verCard->setAutoUpdate(on); jidCard->setAutoUpdate(on); };
        setAuto(sautoBox->isChecked());
        connect(sautoBox, &QCheckBox::toggled, this, [setAuto](bool on) { QSettings().setValue("streamer2/auto", on); setAuto(on); });
        connect(sapplyBtn, &QPushButton::clicked, this, [nameCard, verCard, jidCard]() { nameCard->applyNow(); verCard->applyNow(); jidCard->applyNow(); });
        wrapCollapsible(v, sc, "streamer2mode", false);
    }

    // ---- 5. Game data (dump files)
    {
        auto* dc = cardStandalone("Game data (dump files)");
        auto* dl = qobject_cast<QVBoxLayout*>(dc->layout());
        auto* dh = new QLabel("These save text files to your Desktop that list everything in the game: every class with its properties and offsets (Classes and fields), "
                              "every name (Names) and every object (Objects). They are what you search to find "
                              "a setting - or a function - worth looking into. Load into a level first. Reading only, nothing is changed.");
        dh->setObjectName("Hint");
        dh->setWordWrap(true);
        dl->addWidget(dh);
        auto* drow = new QHBoxLayout;
        const char* labels[3] = {"Dump names", "Dump classes and fields", "Dump objects"};
        const int modes[3] = {0, 2, 1};
        for (int i = 0; i < 3; ++i) {
            expDumpBtns_[i] = new QPushButton(labels[i]);
            expDumpBtns_[i]->setCursor(Qt::PointingHandCursor);
            drow->addWidget(expDumpBtns_[i]);
            const int md = modes[i];
            connect(expDumpBtns_[i], &QPushButton::clicked, this, [this, md]() { startDump(md); });
        }
        auto* openDesk = new QPushButton("Open Desktop folder");
        openDesk->setCursor(Qt::PointingHandCursor);
        connect(openDesk, &QPushButton::clicked, this, []() { QDesktopServices::openUrl(QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))); });
        drow->addWidget(openDesk);
        drow->addStretch(1);
        dl->addLayout(drow);
        expDumpStatus_ = new QLabel("Nothing dumped yet.");
        expDumpStatus_->setObjectName("Hint");
        expDumpStatus_->setWordWrap(true);
        expDumpStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        dl->addWidget(expDumpStatus_);
        wrapCollapsible(v, dc, "dumps", false);
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll);
    return page;
}

void MainWindow::setBeta(bool on) {
    betaOn_ = on;
    if (!on) {
        if (game_->mods.wasd->enabled()) game_->mods.wasd->disable(proc_);       // no experiment keeps running while BETA is off
        mcd2Hub_->setWasd(false);
        if (modMaker_) modMaker_->stopAll();
        refreshEnabledState();
        if (pages_->currentIndex() == 24) {
            pages_->setCurrentIndex(0);
        }
    }
    rebuildNav();
    if (!on && navButtons_.size() > 0 && navButtons_[0]) navButtons_[0]->setChecked(true);
    setMessage(on ? "BETA features on. The Experiments tab is under Home." : "BETA features off.");
}

// ====================================================================== Customize
QWidget* MainWindow::buildCustomize() {
    auto* page = new QWidget;
    auto* outerC = new QVBoxLayout(page);
    outerC->setContentsMargins(28, 24, 28, 20);
    outerC->setSpacing(10);
    auto* title = new QLabel("Customize");
    title->setObjectName("PageTitle");
    outerC->addWidget(title);
    auto* scrollC = new QScrollArea;
    scrollC->setWidgetResizable(true);
    auto* innerC = new QWidget;
    innerC->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(innerC);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);
    scrollC->setWidget(innerC);
    outerC->addWidget(scrollC, 1);

    {   // language
        auto* cg = card(v, "Language");
        auto* cgl = qobject_cast<QVBoxLayout*>(cg->layout());
        auto* gh = new QLabel("Changes the language of the app. Item names, enchantments and map names stay as they are in the game, "
                              "and translations are a work in progress.");
        gh->setObjectName("Hint");
        gh->setWordWrap(true);
        cgl->addWidget(gh);
        auto* lrow = new QHBoxLayout;
        auto* combo = new QComboBox;
        for (int i = 0; i < I18n::LangCount; ++i) combo->addItem(I18n::nativeName(I18n::Lang(i)));
        combo->setCurrentIndex(int(I18n::language()));
        combo->setMinimumWidth(200);
        lrow->addWidget(combo);
        lrow->addStretch(1);
        cgl->addLayout(lrow);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
            I18n::setLanguage(I18n::Lang(i));
            applyLanguage();
        });
    }

    {   // click sounds
        auto* sc = card(v, "Click sounds");
        auto* scl = qobject_cast<QVBoxLayout*>(sc->layout());
        auto* sh = new QLabel("Plays a short sound whenever you press a button. Your choice and volume are remembered.");
        sh->setObjectName("Hint");
        sh->setWordWrap(true);
        scl->addWidget(sh);
        auto& cs = ClickSound::instance();
        const char* labels[] = {"No click sounds", "Soft Click", "Futuristic", "Combined", "Stone", "Loud Click"};
        for (int i = 0; i < 6; ++i) {
            auto* r = new QRadioButton(labels[i]);
            r->setCursor(Qt::PointingHandCursor);
            r->setChecked(cs.mode() == i);
            scl->addWidget(r);
            connect(r, &QRadioButton::clicked, this, [i]() {
                ClickSound::instance().setMode(i);
                ClickSound::instance().play();                  // preview
            });
        }
        auto* vrow = new QHBoxLayout;
        auto* vl = new QLabel("Volume");
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue(cs.volume());
        slider->setMinimumWidth(200);
        auto* vv = new QLabel(QString("%1%").arg(cs.volume()));
        vv->setMinimumWidth(40);
        vrow->addWidget(vl);
        vrow->addWidget(slider, 1);
        vrow->addWidget(vv);
        scl->addLayout(vrow);
        connect(slider, &QSlider::valueChanged, this, [vv](int val) {
            ClickSound::instance().setVolume(val);
            vv->setText(QString("%1%").arg(val));
        });
        connect(slider, &QSlider::sliderReleased, this, []() { ClickSound::instance().play(); });
    }

    {   // app sounds - separate from Click sounds: an optional looping background sound for the app itself, off by default
        auto* sc = card(v, "App sounds");
        auto* scl = qobject_cast<QVBoxLayout*>(sc->layout());
        auto* sh = new QLabel("An optional background sound while you use the app. Off until you turn it on; your choice and volume are remembered. Avoid having this on while recording a video, in case of copyright.");
        sh->setObjectName("Hint");
        sh->setWordWrap(true);
        scl->addWidget(sh);
        auto& as = AppSound::instance();
        auto* toggle = new QCheckBox("Futuristic - Pursuit");
        toggle->setCursor(Qt::PointingHandCursor);
        toggle->setChecked(as.enabled());
        scl->addWidget(toggle);
        auto* vrow = new QHBoxLayout;
        auto* vl = new QLabel("Volume");
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, 100);
        slider->setValue(as.volume());
        slider->setMinimumWidth(200);
        auto* vv = new QLabel(QString("%1%").arg(as.volume()));
        vv->setMinimumWidth(40);
        vrow->addWidget(vl);
        vrow->addWidget(slider, 1);
        vrow->addWidget(vv);
        scl->addLayout(vrow);
        connect(toggle, &QCheckBox::toggled, this, [](bool on) { AppSound::instance().setEnabled(on); });
        connect(slider, &QSlider::valueChanged, this, [vv](int val) {
            AppSound::instance().setVolume(val);
            vv->setText(QString("%1%").arg(val));
        });
    }

    auto* cp = card(v, "Presets");
    auto* cpl = qobject_cast<QVBoxLayout*>(cp->layout());
    auto* ph = new QLabel("One click colour themes. Fine tune them below afterwards if you like.");
    ph->setObjectName("Hint");
    cpl->addWidget(ph);
    for (const QStringList& names : {Theme::presetNames(), Theme::presetNames2()}) {
    auto* prow = new QHBoxLayout;
    for (const QString& pname : names) {
        auto* b = new QPushButton(pname);
        b->setCursor(Qt::PointingHandCursor);
        b->setMinimumWidth(96);
        Theme pt = Theme::preset(pname);
        b->setStyleSheet(QString("QPushButton { border-left: 5px solid %1; }").arg(pt.accent.name()));
        prow->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, pname]() {
            Theme t = Theme::preset(pname);
            t.radius = theme_.radius; t.fontSize = theme_.fontSize; t.customQss = theme_.customQss;
            theme_ = t;
            applyTheme();
        });
    }
    prow->addStretch(1);
    cpl->addLayout(prow);
    }

    {   // third row: random themes
        auto* rrow = new QHBoxLayout;
        auto* rnd = new QPushButton("Randomize Theme");
        rnd->setCursor(Qt::PointingHandCursor);
        rnd->setMinimumWidth(96);
        auto* rs0 = new QPushButton("Random Startup");
        rs0->setObjectName("Tab");
        rs0->setCheckable(true);
        rs0->setCursor(Qt::PointingHandCursor);
        rs0->setChecked(QSettings().value("theme/randomStartup", false).toBool());
        rrow->addWidget(rnd);
        rrow->addWidget(rs0);
        rrow->addStretch(1);
        cpl->addLayout(rrow);
        auto* rh = new QLabel("Randomize Theme picks new random colours right now. Random Startup picks a random theme every time you open and close DungeonKit (button lit = on).");
        rh->setObjectName("Hint");
        rh->setWordWrap(true);
        cpl->addWidget(rh);
        connect(rnd, &QPushButton::clicked, this, [this]() { theme_ = Theme::randomized(theme_); applyTheme(); });
        connect(rs0, &QPushButton::toggled, this, [](bool on) { QSettings().setValue("theme/randomStartup", on); });
    }

    auto* c = card(v, "Colors");
    auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
    struct Entry { const char* label; QColor Theme::*member; };
    static const Entry entries[] = {
        {"Accent", &Theme::accent}, {"Accent 2", &Theme::accent2}, {"Background", &Theme::bg}, {"Panels", &Theme::panel},
        {"Sidebar", &Theme::sidebar}, {"Text", &Theme::text}, {"Muted text", &Theme::muted}, {"Borders", &Theme::border}};
    for (const auto& e : entries) {
        auto* row = new QHBoxLayout;
        auto* l = new QLabel(e.label);
        l->setMinimumWidth(110);
        auto* b = new QPushButton("Change...");
        row->addWidget(l);
        row->addWidget(b);
        row->addStretch(1);
        cl->addLayout(row);
        auto member = e.member;
        connect(b, &QPushButton::clicked, this, [this, member]() {
            QColor c2 = QColorDialog::getColor(theme_.*member, this, "Pick a color");
            if (!c2.isValid()) return;
            theme_.*member = c2;
            applyTheme();
        });
    }

    auto* c2 = card(v, "Shape and size");
    auto* c2l = qobject_cast<QVBoxLayout*>(c2->layout());
    auto* rrow = new QHBoxLayout;
    auto* rl = new QLabel("Corner radius");
    rl->setMinimumWidth(110);
    auto* rs = new QSpinBox;
    rs->setRange(0, 20);
    rs->setValue(theme_.radius);
    auto* fl = new QLabel("Font size");
    auto* fs = new QSpinBox;
    fs->setRange(10, 20);
    fs->setValue(theme_.fontSize);
    rrow->addWidget(rl);
    rrow->addWidget(rs);
    rrow->addSpacing(20);
    rrow->addWidget(fl);
    rrow->addWidget(fs);
    rrow->addStretch(1);
    c2l->addLayout(rrow);
    auto* logoBox = new QCheckBox("Recolor the logo to match the theme");
    logoBox->setChecked(Logo::recolorEnabled());
    c2l->addWidget(logoBox);
    connect(logoBox, &QCheckBox::toggled, this, [this](bool on) { Logo::setRecolorEnabled(on); applyTheme(); });
    connect(rs, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) { theme_.radius = val; applyTheme(); });
    connect(fs, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int val) { theme_.fontSize = val; applyTheme(); });

    {   // sidebar layout
        auto* c4 = card(v, "Sidebar layout");
        auto* c4l = qobject_cast<QVBoxLayout*>(c4->layout());
        auto* sh = new QLabel("Change the order of the sidebar categories and of the pages inside them. It is locked by default so nothing "
                              "gets moved by accident. Tick the box below to edit, select an item, then use Move up or Move down. "
                              "A page at the top or bottom of a category moves into the next category.");
        sh->setObjectName("Hint");
        sh->setWordWrap(true);
        c4l->addWidget(sh);
        auto* unlock = new QCheckBox("Unlock sidebar editing");
        c4l->addWidget(unlock);
        navTree_ = new QTreeWidget;
        navTree_->setHeaderHidden(true);
        navTree_->setDragEnabled(false);
        navTree_->setMinimumHeight(260);
        navTree_->setEnabled(false);
        c4l->addWidget(navTree_);
        auto* nrow = new QHBoxLayout;
        auto* up = new QPushButton("Move up");
        auto* down = new QPushButton("Move down");
        auto* rst = new QPushButton("Reset sidebar");
        for (QPushButton* b : {up, down, rst}) { b->setCursor(Qt::PointingHandCursor); b->setEnabled(false); nrow->addWidget(b); }
        nrow->addStretch(1);
        c4l->addLayout(nrow);
        fillNavTree(-1, -1);
        connect(unlock, &QCheckBox::toggled, this, [this, up, down, rst](bool on) {
            navTree_->setEnabled(on);
            for (QPushButton* b : {up, down, rst}) b->setEnabled(on);
        });
        connect(up, &QPushButton::clicked, this, [this]() { moveNav(-1); });
        connect(down, &QPushButton::clicked, this, [this]() { moveNav(+1); });
        connect(rst, &QPushButton::clicked, this, [this]() {
            if (QMessageBox::question(this, kAppName, I18n::tr("Put the sidebar back to the default order?")) != QMessageBox::Yes) return;
            navCats_ = defaultNavLayout();
            saveNavLayout();
            rebuildNav();
            fillNavTree(-1, -1);
        });
    }

    auto* c3 = card(v, "Import / export style");
    auto* c3l = qobject_cast<QVBoxLayout*>(c3->layout());
    auto* hint = new QLabel("Share your look with friends or try theirs. Export saves your current colours and shape to a .qss file you can send to someone. "
                            "Import loads a .qss file someone made and applies it on top of your theme.");
    hint->setObjectName("Hint");
    hint->setWordWrap(true);
    c3l->addWidget(hint);
    auto* brow = new QHBoxLayout;
    auto* imp = new QPushButton("Import style...");
    auto* exp = new QPushButton("Export style...");
    auto* clr = new QPushButton("Remove imported style");
    auto* reset = new QPushButton("Reset everything to default");
    brow->addWidget(imp);
    brow->addWidget(exp);
    brow->addWidget(clr);
    brow->addWidget(reset);
    brow->addStretch(1);
    c3l->addLayout(brow);
    auto* styleMsg = new QLabel;
    styleMsg->setObjectName("Message");
    styleMsg->setWordWrap(true);
    c3l->addWidget(styleMsg);
    auto* hint2 = new QLabel("Remove imported style takes away only the imported file and keeps your own colours. "
                             "Reset everything to default puts every colour, the corner size and the font size back to the original look.");
    hint2->setObjectName("Hint");
    hint2->setWordWrap(true);
    c3l->addWidget(hint2);
    auto* hint3 = new QLabel("For advanced users: a style file uses Qt style sheet rules (like CSS). Parts you can style: QFrame#Sidebar, QPushButton#Nav, QScrollArea#NavScroll, "
                             "QPushButton#Tab (the Home tabs), QPushButton#Primary, QPushButton#DisableAll, QFrame#Card, QLabel#CardTitle, QLabel#PageTitle, QLabel#Hint, QLabel#Notice and "
                             "QToolButton#CollapsibleHeader (the Experiments section titles you click to open or close). "
                             "An exported file starts with this list. Anything in an imported file wins over the colours above.");
    hint3->setObjectName("Hint");
    hint3->setWordWrap(true);
    c3l->addWidget(hint3);
    v->addStretch(1);

    connect(imp, &QPushButton::clicked, this, [this, styleMsg]() {
        QString path = QFileDialog::getOpenFileName(this, I18n::tr("Import style"), QString(), I18n::tr("Style files (*.qss *.css);;All files (*)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { QMessageBox::warning(this, kAppName, I18n::tr("Could not open that file.")); return; }
        const QString text = QString::fromUtf8(f.readAll());
        if (text.trimmed().isEmpty()) { styleMsg->setText(I18n::tr("That file is empty, so nothing was imported.")); return; }
        theme_.customQss = text;
        applyTheme();
        styleMsg->setText(I18n::tr("Style imported. Use Remove imported style to undo it."));
    });
    connect(exp, &QPushButton::clicked, this, [this, styleMsg]() {
        QString path = QFileDialog::getSaveFileName(this, I18n::tr("Export style"), "dungeonkit.qss", I18n::tr("Style files (*.qss)"));
        if (path.isEmpty()) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { QMessageBox::warning(this, kAppName, I18n::tr("Could not save that file.")); return; }
        const QString header = QString(
            "/* DungeonKit %1 style. Import it from Customize > Import style.\n"
            "   Parts you can style: QFrame#Sidebar, QPushButton#Nav, QScrollArea#NavScroll, QPushButton#Tab, QPushButton#Primary,\n"
            "   QPushButton#DisableAll, QFrame#Card, QLabel#CardTitle, QLabel#PageTitle, QLabel#Hint, QLabel#Notice,\n"
            "   QToolButton#CollapsibleHeader. */\n\n").arg(kAppVersion);
        f.write((header + theme_.toStyleSheet()).toUtf8());
        styleMsg->setText(I18n::tr("Style exported."));
    });
    connect(clr, &QPushButton::clicked, this, [this, styleMsg]() {
        const bool had = !theme_.customQss.isEmpty();
        theme_.customQss.clear();
        applyTheme();
        styleMsg->setText(I18n::tr(had ? "Imported style removed." : "There was no imported style."));
    });
    connect(reset, &QPushButton::clicked, this, [this, rs, fs, styleMsg]() {
        theme_ = Theme();
        rs->setValue(theme_.radius);
        fs->setValue(theme_.fontSize);
        applyTheme();
        styleMsg->setText(I18n::tr("Everything is back to the default look."));
    });
    return page;
}

// ====================================================================== No Clip & Stats
QWidget* MainWindow::buildNoClip() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);

    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel("No Clip & Stats");
    title->setObjectName("PageTitle");
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    auto* offAll = new QPushButton("Disable all mods");
    offAll->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(offAll);
    connect(offAll, &QPushButton::clicked, this, [this]() { disableAll(); });
    outer->addLayout(titleRow);

    auto* note = new QLabel(
        "These features read the game's own object tables instead of patching code, so they depend on the game build and "
        "can't be tested against every version.");
    note->setObjectName("Notice");
    note->setWordWrap(true);
    outer->addWidget(note);

    playerMsg_ = new QLabel;
    playerMsg_->setObjectName("Message");
    playerMsg_->setWordWrap(true);
    outer->addWidget(playerMsg_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);
    auto& m = game_->mods;

    {   // movement
        auto* c = card(v, "Movement");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.noclip);
        auto* row = new QHBoxLayout;
        auto* l = new QLabel("Fly speed");
        l->setMinimumWidth(110);
        auto* sp = new QDoubleSpinBox;
        sp->setRange(200, 20000);
        sp->setDecimals(0);
        sp->setSingleStep(250);
        sp->setValue(m.noclip->speed());
        sp->setSuffix(" units/s");
        row->addWidget(l);
        row->addWidget(sp, 1);
        row->addStretch(2);
        cl->addLayout(row);
        NoClipMod* nc = m.noclip;
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [nc](double val) { nc->setSpeed((float)val); });
        addToggle(cl, m.god);
        addToggle(cl, m.overkill);
    }
    {   // stats
        auto* c = card(v, "Player stats");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.stats);
        playerInfo_ = new QLabel;
        playerInfo_->setObjectName("Hint");
        playerInfo_->setWordWrap(true);
        cl->addWidget(playerInfo_);
        auto* hint = new QLabel("Turn it on, then change values below. Tick Freeze on a row to hold it there. "
                                "Most of these are multipliers (1 = normal). The game may reset them on a level change; switch the toggle off and on again to re-link.");
        hint->setObjectName("Hint");
        hint->setWordWrap(true);
        cl->addWidget(hint);
    }
    StatsMod* st = m.stats;
    const auto& defs = StatsMod::defs();
    for (int g = 0; g < StatsMod::groupCount(); ++g) {
        static const char* kTitles[] = {"Melee", "Ranged", "Health", "Resistance", "Movement"};
        auto* c = card(v, kTitles[g]);
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        for (int i = 0; i < (int)defs.size(); ++i) {
            if (std::string(defs[i].group) != StatsMod::groupName(g)) continue;
            addFloatRow(cl, QString::fromUtf8(defs[i].label),
                        [st, i](float& out) { return st->get(i, out); },
                        [st, i](float val) { return st->set(i, val); }, st);
        }
    }
    {   // diagnostics
        auto* c = card(v, "Details");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        playerDiag_ = new QLabel("Nothing scanned yet. Turn on a feature above.");
        playerDiag_->setObjectName("Hint");
        playerDiag_->setWordWrap(true);
        playerDiag_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(playerDiag_);
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

// ====================================================================== Tools
QWidget* MainWindow::buildTools() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);

    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel("Tools");
    title->setObjectName("PageTitle");
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    auto* offAll = new QPushButton("Disable all mods");
    offAll->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(offAll);
    connect(offAll, &QPushButton::clicked, this, [this]() { disableAll(); });
    outer->addLayout(titleRow);

    auto* note = new QLabel(
        "Timer and Pause Game change the game's own clock inside a level, not your computer's clock. "
        "The game can reset its speed when a new level loads, so switch the feature off and on again if that happens.");
    note->setObjectName("Notice");
    note->setWordWrap(true);
    outer->addWidget(note);

    toolsMsg_ = new QLabel;
    toolsMsg_->setObjectName("Message");
    toolsMsg_->setWordWrap(true);
    outer->addWidget(toolsMsg_);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);
    auto& m = game_->mods;
    {
        auto* c = card(v, "Timer");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.timer);
        auto* row = new QHBoxLayout;
        auto* l = new QLabel("Game speed");
        l->setMinimumWidth(110);
        auto* sp = new QDoubleSpinBox;
        sp->setRange(0.1, 20.0);
        sp->setDecimals(2);
        sp->setSingleStep(0.25);
        sp->setValue(m.timer->speed());
        sp->setSuffix("x");
        row->addWidget(l);
        row->addWidget(sp, 1);
        row->addStretch(2);
        cl->addLayout(row);
        TimerMod* tm = m.timer;
        connect(sp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [tm](double val) { tm->setSpeed((float)val); });
    }
    {
        auto* c = card(v, "Pause Game");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.pause);
    }
    {
        auto liveProc = [this]() -> Process* {
            if (target_ != Process::Target::Dungeons) return nullptr;
            return proc_.attached() && proc_.alive() ? &proc_ : nullptr;
        };
        auto* hide = new NameHideCard(liveProc, "namehide", this);
        connect(hide, &NameHideCard::message, this, &MainWindow::setMessage);
        v->addWidget(hide);
    }
    {   // Console (our own DLL)
        auto liveProc = [this]() -> Process* {
            if (target_ != Process::Target::Dungeons) return nullptr;
            return proc_.attached() && proc_.alive() ? &proc_ : nullptr;
        };
        ProbeCard::Spec cs;
        cs.title = "Console";
        cs.hint = "Adds the game's console. Load in, press Inject console, then press the console key and type a command. "
                  "Works on 1.17.0.0 only, once per game start. Only works on the Minecraft Launcher and Xbox app - Steam uses a "
                  "different build of the game that this hasn't been able to connect to. Use Close game instead of closing "
                  "normally while injected. Windows Defender may flag this. Add the DungeonKit folder as an exclusion if you want to use it.";
        cs.dll = "DungeonKitConsole.dll";
        cs.log = "DungeonKitConsole.log";
        cs.button = "Inject console";
        cs.closeButton = true;   // closing normally doesn't work while the console is injected; ends the Minecraft Dungeons process by name
        auto* console = new ProbeCard(cs, liveProc, this);
        connect(console, &ProbeCard::message, this, &MainWindow::setMessage);

        // console key: saved in a small file next to the DLL, which the DLL checks every second or so
        const QStringList keys = {"Tilde", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "Insert", "Delete", "Home", "End",
                                  "PageUp", "PageDown", "Backslash", "Slash", "Semicolon", "Comma", "Period"};
        auto* keyBox = new QWidget;
        auto* kl = new QHBoxLayout(keyBox);
        kl->setContentsMargins(0, 0, 0, 0);
        auto* kLabel = new QLabel("Console key");
        auto* kCombo = new QComboBox;
        for (const QString& k : keys) kCombo->addItem(k == "Tilde" ? QString("Tilde (~) - default") : k, k);
        const QString savedKey = QSettings().value("console/key", "Tilde").toString();
        const int ki = keys.indexOf(savedKey);
        kCombo->setCurrentIndex(ki < 0 ? 0 : ki);
        auto* kHint = new QLabel("Changes apply in the game within a couple of seconds.");
        kHint->setObjectName("Hint");
        kl->addWidget(kLabel);
        kl->addWidget(kCombo);
        kl->addWidget(kHint, 1);
        console->addExtra(keyBox);
        auto writeKey = [kCombo]() {
            const QString dir = QCoreApplication::applicationDirPath() + "/Probe";
            QDir().mkpath(dir);
            QFile f(dir + "/DungeonKitConsole.ini");
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(QString("[console]\r\nkey=%1\r\n").arg(kCombo->currentData().toString()).toUtf8());
        };
        writeKey();
        connect(kCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [kCombo, writeKey]() {
            QSettings().setValue("console/key", kCombo->currentData().toString());
            writeKey();
        });

        // External macro tool (Auto Dungeoneer) that can spawn summons quickly through the console - just a link out.
        auto* macroRow = new QWidget;
        auto* ml = new QHBoxLayout(macroRow);
        ml->setContentsMargins(0, 0, 0, 0);
        auto* macroBtn = new QPushButton("Macro");
        macroBtn->setCursor(Qt::PointingHandCursor);
        connect(macroBtn, &QPushButton::clicked, this, []() {
            QDesktopServices::openUrl(QUrl("https://www.dungeoncollector.co.uk/dungeons/?file=Auto_Dungeoneer_2_v1.1.0.1"));
        });
        ml->addWidget(macroBtn);
        auto* macroHint = new QLabel("Opens a macro tool on another site that can spawn summons quickly through the console.");
        macroHint->setObjectName("Hint");
        ml->addWidget(macroHint, 1);
        console->addExtra(macroRow);

        v->addWidget(console);

        addCommandsCard(v, kConsoleCmdGroups, sizeof(kConsoleCmdGroups) / sizeof(kConsoleCmdGroups[0]), true);
    }
    {
        auto* c = card(v, "Advanced");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* warn = new QLabel(
            "Please don't touch anything in this section unless you know exactly what you're doing. These are developer "
            "tools that dump the game's internal tables to text files on your Desktop. They are only useful for finding "
            "new offsets or building new features, and they do nothing for normal use.");
        warn->setObjectName("Notice");
        warn->setWordWrap(true);
        cl->addWidget(warn);
        auto* row = new QHBoxLayout;
        dumpNamesBtn_ = new QPushButton("Dump names");
        dumpObjectsBtn_ = new QPushButton("Dump objects");
        dumpClassesBtn_ = new QPushButton("Dump classes and fields");
        row->addWidget(dumpNamesBtn_);
        row->addWidget(dumpClassesBtn_);
        row->addWidget(dumpObjectsBtn_);
        row->addStretch(1);
        cl->addLayout(row);
        dumpStatus_ = new QLabel("Load into a level first. Names is quick. Classes and fields lists every class with its properties and their offsets. Objects is the biggest and can take a minute or two.");
        dumpStatus_->setObjectName("Hint");
        dumpStatus_->setWordWrap(true);
        cl->addWidget(dumpStatus_);
        connect(dumpNamesBtn_, &QPushButton::clicked, this, [this]() { startDump(0); });
        connect(dumpClassesBtn_, &QPushButton::clicked, this, [this]() { startDump(2); });
        connect(dumpObjectsBtn_, &QPushButton::clicked, this, [this]() { startDump(1); });
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

// ====================================================================== Test (Minecraft Dungeons II)
void MainWindow::setTarget(Process::Target t) {
    if (t == target_) return;
    logEvent(QString("Target game changed to: %1").arg(t == Process::Target::Dungeons ? "Minecraft Dungeons" : t == Process::Target::Dungeons2 ? "Minecraft Dungeons II" : "none"));
    if (proc_.attached()) { disableAllMods(); proc_.detach(); }
    procAlt_.detach();
    if (mcd2Hub_) mcd2Hub_->stopFreezes();
    if (mcd2_) mcd2_->reset();
    if (game_->mods.ue4) game_->mods.ue4->reset();
    target_ = t;
    foldersPid_ = 0;
    if (t == Process::Target::None)
        setStatus("off", "Offline", "Default: choose a game on the Home page.");
    else
        setStatus("off", "Offline", t == Process::Target::Dungeons2 ? "Waiting for Minecraft Dungeons II..." : "Waiting for Minecraft Dungeons...");
    refreshEnabledState();
    updateTitle();
    updateHomeStatus();
    setMessage(t == Process::Target::None ? "Default: not connected to a game."
              : t == Process::Target::Dungeons2 ? "Target: Minecraft Dungeons II." : "Target: Minecraft Dungeons.");
}

QWidget* MainWindow::buildTest() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);
    auto* titleRow = new QHBoxLayout;
    auto* title = new QLabel("Tools");
    title->setObjectName("PageTitle");
    titleRow->addWidget(title);
    titleRow->addStretch(1);
    auto* offAll = new QPushButton("Disable all mods");
    offAll->setCursor(Qt::PointingHandCursor);
    titleRow->addWidget(offAll);
    connect(offAll, &QPushButton::clicked, this, [this]() { disableAll(); });
    outer->addLayout(titleRow);
    auto* note = new QLabel(
        "Timer and Pause Game change the game's own clock inside a level, not your computer's clock. "
        "The game can reset its speed when a new level loads, so switch the feature off and on again if that happens. "
        "These tools were also used to test whether DungeonKit could connect to Minecraft Dungeons II and work with its data, "
        "which is what let the rest of the app work with that game.");
    note->setObjectName("Notice");
    note->setWordWrap(true);
    outer->addWidget(note);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);
    auto& m = game_->mods;
    {
        auto* c = card(v, "Timer");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.timer);
        auto* row = new QHBoxLayout;
        auto* l = new QLabel("Game speed");
        l->setMinimumWidth(110);
        testSpeed_ = new QDoubleSpinBox;
        testSpeed_->setRange(0.1, 20.0);
        testSpeed_->setDecimals(2);
        testSpeed_->setSingleStep(0.25);
        testSpeed_->setValue(m.timer->speed());
        testSpeed_->setSuffix("x");
        row->addWidget(l);
        row->addWidget(testSpeed_, 1);
        row->addStretch(2);
        cl->addLayout(row);
        TimerMod* tm = m.timer;
        connect(testSpeed_, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [tm](double val) { tm->setSpeed((float)val); });
    }
    {
        auto* c = card(v, "Pause Game");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        addToggle(cl, m.pause);
    }
    {   // Console (Streamer mode moved to Experiments)
        auto liveProc = [this]() -> Process* {
            if (target_ != Process::Target::Dungeons2) return nullptr;
            Process* p = engineProc();
            return p && p->attached() && p->alive() ? p : nullptr;
        };
        ProbeCard::Spec cs;
        cs.title = "Console";
        cs.hint = "Adds the game's console. Load in, press Inject console, then press the console key and type a command. "
                  "Works on Microsoft Store/Xbox 1.1.2.0 and Steam 1.2.0, once per game start, offline or solo. "
                  "Summoned mobs are made visible automatically (check the log for details on each one). "
                  "Windows Defender may flag this. Add the DungeonKit folder as an exclusion if you want to use it.";
        cs.dll = "DungeonKitConsole2.dll";
        cs.log = "DungeonKitConsole2.log";
        cs.button = "Inject console";
        cs.closeButton = true;   // closing normally doesn't work while the console is injected; this ends every MCD2 app (Store/Xbox, Steam, Launcher) by process name
        auto* console2 = new ProbeCard(cs, liveProc, this);
        connect(console2, &ProbeCard::message, this, &MainWindow::setMessage);

        // console key: saved in a small file next to the DLL, which the DLL checks every second or so
        const QStringList keys2 = {"Tilde", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "Insert", "Delete", "Home", "End",
                                   "PageUp", "PageDown", "Backslash", "Slash", "Semicolon", "Comma", "Period"};
        auto* keyBox2 = new QWidget;
        auto* kl2 = new QHBoxLayout(keyBox2);
        kl2->setContentsMargins(0, 0, 0, 0);
        auto* kLabel2 = new QLabel("Console key");
        auto* kCombo2 = new QComboBox;
        for (const QString& k : keys2) kCombo2->addItem(k == "Tilde" ? QString("Tilde (~) - default") : k, k);
        const QString savedKey2 = QSettings().value("console2/key", "Tilde").toString();
        const int ki2 = keys2.indexOf(savedKey2);
        kCombo2->setCurrentIndex(ki2 < 0 ? 0 : ki2);
        auto* kHint2 = new QLabel("Changes apply in the game within a couple of seconds.");
        kHint2->setObjectName("Hint");
        kl2->addWidget(kLabel2);
        kl2->addWidget(kCombo2);
        kl2->addWidget(kHint2, 1);
        console2->addExtra(keyBox2);
        auto writeKey2 = [kCombo2]() {
            const QString dir = QCoreApplication::applicationDirPath() + "/Probe";
            QDir().mkpath(dir);
            QFile f(dir + "/DungeonKitConsole2.ini");
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(QString("[console]\r\nkey=%1\r\n").arg(kCombo2->currentData().toString()).toUtf8());
        };
        writeKey2();
        connect(kCombo2, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [kCombo2, writeKey2]() {
            QSettings().setValue("console2/key", kCombo2->currentData().toString());
            writeKey2();
        });

        // External macro tool (Auto Dungeoneer) that can spawn summons quickly through the console - just a link out.
        auto* macroRow2 = new QWidget;
        auto* ml2 = new QHBoxLayout(macroRow2);
        ml2->setContentsMargins(0, 0, 0, 0);
        auto* macroBtn2 = new QPushButton("Macro");
        macroBtn2->setCursor(Qt::PointingHandCursor);
        connect(macroBtn2, &QPushButton::clicked, this, []() {
            QDesktopServices::openUrl(QUrl("https://www.dungeoncollector.co.uk/dungeons/?file=Auto_Dungeoneer_2_v1.1.0.1"));
        });
        ml2->addWidget(macroBtn2);
        auto* macroHint2 = new QLabel("Opens a macro tool on another site that can spawn summons quickly through the console.");
        macroHint2->setObjectName("Hint");
        ml2->addWidget(macroHint2, 1);
        console2->addExtra(macroRow2);

        v->addWidget(console2);
        addCommandsCard(v, kConsole2CmdGroups, sizeof(kConsole2CmdGroups) / sizeof(kConsole2CmdGroups[0]), false);
    }
    {
        auto* c = card(v, "Data dump");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* warn = new QLabel(
            "Only developers should use the Data dump. These tools write the game's internal tables to large text files on your Desktop. "
            "They are only useful for finding new offsets or building new features, and they do nothing for normal use. "
            "Please don't touch anything in this section unless you know exactly what you're doing.");
        warn->setObjectName("Notice");
        warn->setWordWrap(true);
        cl->addWidget(warn);
        auto* t = new QLabel(
            "Reads the game's internal tables and writes them to text files on your Desktop. This only reads memory; it changes nothing in the game. "
            "Load into a level first. Names is quick. Classes and fields lists every class with its properties and their offsets, which is the most useful one "
            "for finding settings. Objects is the "
            "biggest and can take a minute or two.");
        t->setObjectName("Hint");
        t->setWordWrap(true);
        cl->addWidget(t);
        auto* row = new QHBoxLayout;
        const char* labels[3] = {"Dump names", "Dump classes and fields", "Dump objects"};
        const int modes[3] = {0, 2, 1};
        for (int i = 0; i < 3; ++i) {
            testDumpBtns_[i] = new QPushButton(labels[i]);
            testDumpBtns_[i]->setCursor(Qt::PointingHandCursor);
            row->addWidget(testDumpBtns_[i]);
            const int md = modes[i];
            connect(testDumpBtns_[i], &QPushButton::clicked, this, [this, md]() { startDump(md); });
        }
        row->addStretch(1);
        cl->addLayout(row);
        testDumpStatus_ = new QLabel("Nothing dumped yet.");
        testDumpStatus_->setObjectName("Hint");
        testDumpStatus_->setWordWrap(true);
        testDumpStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(testDumpStatus_);
    }
    {
        auto* c = card(v, "Connection check");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        testInfo_ = new QLabel("Not checked yet.");
        testInfo_->setObjectName("Hint");
        testInfo_->setWordWrap(true);
        cl->addWidget(testInfo_);
        auto* row = new QHBoxLayout;
        auto* run = new QPushButton("Run check");
        auto* copy = new QPushButton("Copy report");
        run->setCursor(Qt::PointingHandCursor);
        copy->setCursor(Qt::PointingHandCursor);
        row->addWidget(run);
        row->addWidget(copy);
        row->addStretch(1);
        cl->addLayout(row);
        testReport_ = new QLabel;
        testReport_->setWordWrap(true);
        testReport_->setTextInteractionFlags(Qt::TextSelectableByMouse);
        cl->addWidget(testReport_);
        connect(run, &QPushButton::clicked, this, [this]() { runTestCheck(); });
        connect(copy, &QPushButton::clicked, this, [this]() {
            QGuiApplication::clipboard()->setText(testReport_->text());
            setMessage("Report copied.");
        });
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

void MainWindow::runTestCheck() {
    QString r;
    r += QString("DungeonKit %1 (test build)\nTarget: %2\n").arg(kAppVersion,
         QString(target_ == Process::Target::Dungeons2 ? "Minecraft Dungeons II" : "Minecraft Dungeons"));
    if (!proc_.attached() || !proc_.alive()) {
        r += "Game: not running (looking for " + QString(target_ == Process::Target::Dungeons2 ? "Dungeons-WinGDK-Shipping.exe, Dungeons-Win64-Shipping.exe or Dungeons.exe" : "Dungeons-Win64-Shipping.exe") + ")\n";
        testReport_->setText(r);
        setMessage("The game is not running.");
        return;
    }
    auto ue = game_->mods.ue4;
    bool ok = false;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    Process* list[2] = {&proc_, &procAlt_};
    for (Process* p : list) {
        if (!p->attached() || !p->alive()) continue;
        r += QString("\nProcess: %1, PID %2\nModule base: 0x%3, size %4 bytes\n")
                 .arg(QString::fromStdString(p->exeName())).arg(p->pid())
                 .arg(QString::number((qulonglong)p->base(), 16).toUpper()).arg(p->moduleSize());
        std::string err;
        const bool here = ue->init(*p, err);
        r += here ? "Engine data: found\n" : "Engine data: NOT found - " + QString::fromStdString(err) + "\n";
        const std::string d = ue->diagnostics();
        if (!d.empty()) r += QString::fromStdString(d).trimmed() + "\n";
        if (here) { ok = true; break; }              // the engine is readable here; mods will use this process
    }
    QGuiApplication::restoreOverrideCursor();
    if (!procAlt_.attached() && (proc_.exeName() == "Dungeons-WinGDK-Shipping.exe" || proc_.exeName() == "Dungeons-Win64-Shipping.exe"))
        r += "\nDungeons.exe: not running (only the game exe is attached)\n";
    if (proc_.exeName() == "Dungeons.exe" && target_ == Process::Target::Dungeons2) r += "\nThe game's shipping exe: not running (only the Dungeons.exe launcher is attached)\n";
    if (ok) {
        // walk the same chain Timer and Pause use, and say where it stops
        uint64_t eng = 0, vp = 0, world = 0, level = 0, ws = 0;
        UE4::Prop pr;
        auto step = [&](const char* what, bool good) { r += QString("  %1: %2\n").arg(QString(what), QString(good ? "ok" : "NOT FOUND")); return good; };
        r += "Walking to the time settings:\n";
        bool g = step("GEngine", ue->engine(eng));
        if (g) g = step("GameViewport", ue->readPtr(eng, "GameViewport", vp));
        if (g) g = step("World", ue->readPtr(vp, "World", world));
        if (g) g = step("PersistentLevel", ue->readPtr(world, "PersistentLevel", level));
        if (g) g = step("WorldSettings", ue->readPtr(level, "WorldSettings", ws));
        if (g) g = step("TimeDilation", ue->find(ws, "TimeDilation", pr));
        TimeControl probe(ue);
        std::string e2;
        r += probe.locate(e2) ? "Time settings: reachable - Timer and Pause should work\n"
                              : "Time settings: " + QString::fromStdString(e2) + "\n";
    } else {
        r += "Result: the engine lookup that Timer and Pause rely on does not match this game build yet.\n";
    }
    testReport_->setText(r.trimmed());
    testInfo_->setText(ok ? "Check finished. See the report below." : "Check finished with problems. See the report below.");
    setMessage(ok ? "Check finished." : "Check finished - the game build was not recognised.");
}

void MainWindow::addCommandsCard(QVBoxLayout* v, const ConsoleCmdGroup* groups, size_t groupCount, bool mcd1) {
    auto* c = card(v, "Console commands");
    auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
    auto* info = new QLabel(mcd1 ? "You only need to type enablecheats 1 once. After that, the other commands work. Things appear near you."
                                 : "Type enablecheats 1 once, then the other commands. Things appear near you. These names came from the game, so some may do nothing, depending on the level.");
    info->setObjectName("Hint");
    info->setWordWrap(true);
    cl->addWidget(info);
    if (mcd1) {
        auto* campNote = new QLabel("These only work in your camp, not in other levels. Other players can join your camp and see the same things.");
        campNote->setObjectName("Hint");
        campNote->setWordWrap(true);
        cl->addWidget(campNote);
    }
    auto* freecamNote = new QLabel("With toggledebugcamera on, scroll the mouse wheel to speed up or slow down the free camera.");
    freecamNote->setObjectName("Hint");
    freecamNote->setWordWrap(true);
    cl->addWidget(freecamNote);
    auto* search = new QLineEdit;
    search->setPlaceholderText("Search commands, for example chest or wolf");
    search->setClearButtonEnabled(true);
    cl->addWidget(search);

    struct Row { QWidget* w; QString lower; };
    struct GroupUi { QPushButton* head; QString title; bool open; std::vector<Row> rows; };
    auto groupsUi = std::make_shared<std::vector<GroupUi>>();
    for (size_t gix = 0; gix < groupCount; ++gix) {
        const ConsoleCmdGroup& g = groups[gix];
        GroupUi gu;
        gu.title = QString::fromUtf8(g.title);
        gu.open = g.open;
        gu.head = new QPushButton;
        gu.head->setCursor(Qt::PointingHandCursor);
        gu.head->setProperty("dkSkip", true);
        cl->addWidget(gu.head);
        for (int i = 0; i < g.count; ++i) {
            const QString cmd = QString::fromUtf8(g.cmds[i]);
            auto* rw = new QWidget;
            auto* r = new QHBoxLayout(rw);
            r->setContentsMargins(0, 0, 0, 0);
            auto* l = new QLabel(cmd);
            l->setProperty("dkSkip", true);
            l->setTextInteractionFlags(Qt::TextSelectableByMouse);
            auto* b = new QPushButton("Copy");
            b->setCursor(Qt::PointingHandCursor);
            r->addWidget(l, 1);
            r->addWidget(b);
            cl->addWidget(rw);
            connect(b, &QPushButton::clicked, this, [this, cmd]() { QApplication::clipboard()->setText(cmd); setMessage("Copied."); });
            gu.rows.push_back({rw, cmd.toLower()});
        }
        groupsUi->push_back(std::move(gu));
    }
    // every word typed in the search box has to be somewhere in the command; with an empty box the groups open and close by their header
    auto refresh = [groupsUi, search]() {
        const QStringList words = search->text().toLower().split(' ', Qt::SkipEmptyParts);
        for (GroupUi& g : *groupsUi) {
            int shown = 0;
            for (Row& r : g.rows) {
                bool match = true;
                for (const QString& w : words) if (!r.lower.contains(w)) { match = false; break; }
                const bool vis = words.isEmpty() ? g.open : match;
                r.w->setVisible(vis);
                if (vis) ++shown;
            }
            g.head->setVisible(words.isEmpty() || shown > 0);
            g.head->setText(QString("%1 %2 (%3)").arg(QString(words.isEmpty() ? (g.open ? "v" : ">") : "v"), g.title).arg(words.isEmpty() ? (int)g.rows.size() : shown));
        }
    };
    for (size_t gi = 0; gi < groupsUi->size(); ++gi)
        connect((*groupsUi)[gi].head, &QPushButton::clicked, this, [groupsUi, gi, refresh, search]() {
            if (!search->text().trimmed().isEmpty()) return;
            if ((*groupsUi)[gi].title == "Important") return;          // always open
            (*groupsUi)[gi].open = !(*groupsUi)[gi].open;
            refresh();
        });
    connect(search, &QLineEdit::textChanged, this, [refresh]() { refresh(); });
    refresh();
}

// Lists every summonable thing the running game has loaded (Blueprint Actor classes).
QFrame* MainWindow::addSummonCard() {
    auto* c = cardStandalone("Summonable things");
    auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
    auto* info = new QLabel("Reads the game for every thing it can summon, like BP_TargetDummy_C. Load into a level first. "
                            "Only things the game has loaded show up, so scan again after visiting other places. Found things are remembered.");
    info->setObjectName("Hint");
    info->setWordWrap(true);
    cl->addWidget(info);

    auto* btnRow = new QWidget;
    auto* br = new QHBoxLayout(btnRow);
    br->setContentsMargins(0, 0, 0, 0);
    auto* scanBtn = new QPushButton("Find more in the game");
    auto* copyAll = new QPushButton("Copy all");
    auto* clearBtn = new QPushButton("Clear list");
    for (auto* b : {scanBtn, copyAll, clearBtn}) { b->setCursor(Qt::PointingHandCursor); br->addWidget(b); }
    br->addStretch(1);
    cl->addWidget(btnRow);
    auto* status = new QLabel;
    status->setObjectName("Hint");
    status->setWordWrap(true);
    status->setProperty("dkSkip", true);
    cl->addWidget(status);
    auto* search = new QLineEdit;
    search->setPlaceholderText("Search found things, for example target or zombie");
    search->setClearButtonEnabled(true);
    cl->addWidget(search);
    auto* listHost = new QVBoxLayout;
    cl->addLayout(listHost);

    struct Row { QWidget* w; QString lower; };
    auto rows = std::make_shared<std::vector<Row>>();
    auto names = std::make_shared<QStringList>(QSettings().value("summon2/found").toStringList());
    auto refresh = [rows, search]() {
        const QStringList words = search->text().toLower().split(' ', Qt::SkipEmptyParts);
        for (Row& r : *rows) {
            bool match = true;
            for (const QString& w : words) if (!r.lower.contains(w)) { match = false; break; }
            r.w->setVisible(match);
        }
    };
    auto rebuild = [this, rows, names, listHost, refresh, status, copyAll, clearBtn]() {
        while (QLayoutItem* it = listHost->takeAt(0)) {
            if (it->widget()) it->widget()->deleteLater();
            delete it;
        }
        rows->clear();
        for (const QString& n : *names) {
            const QString cmd = "summon " + n;
            auto* rw = new QWidget;
            auto* r = new QHBoxLayout(rw);
            r->setContentsMargins(0, 0, 0, 0);
            auto* l = new QLabel(cmd);
            l->setProperty("dkSkip", true);
            l->setTextInteractionFlags(Qt::TextSelectableByMouse);
            auto* b = new QPushButton("Copy");
            b->setCursor(Qt::PointingHandCursor);
            r->addWidget(l, 1);
            r->addWidget(b);
            listHost->addWidget(rw);
            connect(b, &QPushButton::clicked, this, [this, cmd]() { QApplication::clipboard()->setText(cmd); setMessage("Copied."); });
            rows->push_back({rw, cmd.toLower()});
        }
        copyAll->setEnabled(!names->isEmpty());
        clearBtn->setEnabled(!names->isEmpty());
        if (names->isEmpty()) status->setText("Nothing found yet. Load into a level and press Find more in the game.");
        refresh();
    };
    rebuild();
    if (!names->isEmpty()) status->setText(QString("%1 things remembered from earlier scans.").arg(names->size()));
    connect(search, &QLineEdit::textChanged, this, [refresh]() { refresh(); });

    connect(copyAll, &QPushButton::clicked, this, [this, names]() {
        QStringList lines;
        for (const QString& n : *names) lines << "summon " + n;
        QApplication::clipboard()->setText(lines.join("\n"));
        setMessage(QString("Copied %1 commands.").arg(lines.size()));
    });
    connect(clearBtn, &QPushButton::clicked, this, [names, rebuild]() {
        names->clear();
        QSettings().remove("summon2/found");
        rebuild();
    });

    auto busy = std::make_shared<bool>(false);
    connect(scanBtn, &QPushButton::clicked, this, [this, scanBtn, status, names, rebuild, busy]() {
        if (*busy) return;
        if (!(proc_.attached() && proc_.alive()) && !(procAlt_.attached() && procAlt_.alive())) { status->setText("The game is not running."); return; }
        auto ue = game_->mods.ue4;
        std::string err;
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);
        Process* usep = engineProc();
        const bool ok = ue->init(*usep, err);
        QGuiApplication::restoreOverrideCursor();
        if (!ok) { status->setText(QString::fromStdString(err)); return; }
        *busy = true;
        scanBtn->setEnabled(false);
        status->setText("Scanning... 0%");
        auto res = std::make_shared<std::vector<std::string>>();
        auto er = std::make_shared<std::string>();
        auto okf = std::make_shared<std::atomic<bool>>(false);
        auto prog = std::make_shared<std::atomic<int>>(0);
        QThread* th = QThread::create([ue, res, er, okf, prog]() { *okf = scanSummonable(*ue, *res, *prog, *er); });
        auto* tick = new QTimer(status);
        connect(tick, &QTimer::timeout, status, [status, prog]() { status->setText(QString("Scanning... %1%").arg(prog->load())); });
        tick->start(300);
        connect(th, &QThread::finished, this, [this, th, tick, scanBtn, status, names, rebuild, busy, res, er, okf]() {
            tick->stop();
            tick->deleteLater();
            *busy = false;
            scanBtn->setEnabled(true);
            if (!*okf) {
                status->setText("Scan failed: " + QString::fromStdString(*er));
            } else {
                const int before = names->size();
                QSet<QString> have;
                for (const QString& n : *names) have.insert(n.toLower());
                for (const std::string& n : *res) {
                    const QString q = QString::fromStdString(n);
                    if (!have.contains(q.toLower())) { have.insert(q.toLower()); *names << q; }
                }
                names->sort(Qt::CaseInsensitive);
                QSettings().setValue("summon2/found", *names);
                rebuild();
                status->setText(QString("Found %1 summonable things in the game, %2 new. %3 in the list. Scan again in other places to find more.")
                                    .arg((int)res->size()).arg(names->size() - before).arg(names->size()));
                setMessage("Scan finished.");
            }
            th->deleteLater();
        });
        th->start();
    });
    return c;
}

void MainWindow::startDump(int mode) {
    if (dumpRunning_) return;
    if (!proc_.attached() || !proc_.alive()) { setMessage("The game is not running."); return; }
    auto ue = game_->mods.ue4;
    std::string err;
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    Process* usep = target_ == Process::Target::Dungeons ? &proc_ : engineProc();
    const bool ok = ue->init(*usep, err);
    QGuiApplication::restoreOverrideCursor();
    if (!ok) { setMessage(QString::fromStdString(err)); return; }

    static const char* kFile[] = {"NamesDump", "ObjectsDump", "ClassesDump"};
    const QString tag = target_ == Process::Target::Dungeons ? QString() : QString("MCD2 ");
    const QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) +
                         "/DungeonKit " + tag + kFile[mode < 0 || mode > 2 ? 0 : mode] + ".txt";
    auto setButtons = [this](bool on) {
        if (dumpNamesBtn_) dumpNamesBtn_->setEnabled(on);
        if (dumpObjectsBtn_) dumpObjectsBtn_->setEnabled(on);
        if (dumpClassesBtn_) dumpClassesBtn_->setEnabled(on);
        for (auto* b : testDumpBtns_) if (b) b->setEnabled(on);
        for (auto* b : expDumpBtns_) if (b) b->setEnabled(on);
    };
    auto setStatusText = [this](const QString& t) {
        if (dumpStatus_) dumpStatus_->setText(t);
        if (testDumpStatus_) testDumpStatus_->setText(t);
        if (expDumpStatus_) expDumpStatus_->setText(t);
    };
    dumpRunning_ = true;
    dumpCount_ = 0;
    setButtons(false);
    setStatusText("Working...");
    auto okFlag = std::make_shared<std::atomic<bool>>(false);
    auto errText = std::make_shared<std::string>();
    QThread* th = QThread::create([ue, path, mode, okFlag, errText, this]() {
        std::string e;
        const bool r = mode == 1 ? dumpObjects(*ue, path.toStdWString(), dumpCount_, e)
                     : mode == 2 ? dumpClasses(*ue, path.toStdWString(), dumpCount_, e)
                                 : dumpNames(*ue, path.toStdWString(), dumpCount_, e);
        *errText = e;
        *okFlag = r;
    });
    connect(th, &QThread::finished, this, [this, th, okFlag, errText, path, setButtons, setStatusText]() {
        dumpRunning_ = false;
        setButtons(true);
        const QString text = *okFlag ? QString("Saved %1 entries to %2").arg(dumpCount_.load()).arg(path)
                                     : QString("Dump failed: %1").arg(QString::fromStdString(*errText));
        setStatusText(text);
        setMessage(text);
        th->deleteLater();
    });
    th->start();
}

// ====================================================================== Information
QWidget* MainWindow::buildInformation() {
    auto* page = new QWidget;
    auto* outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 20);
    outer->setSpacing(10);
    auto* title = new QLabel("Information");
    title->setObjectName("PageTitle");
    outer->addWidget(title);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* inner = new QWidget;
    inner->setStyleSheet("background: transparent;");
    auto* v = new QVBoxLayout(inner);
    v->setContentsMargins(0, 0, 8, 0);
    v->setSpacing(12);

    auto* tag = new QLabel("Complicated modding, made simple.");
    tag->setObjectName("Tagline");
    auto* by = new QLabel("By yours truly, and inspired by many others.");
    by->setObjectName("Muted");
    auto* slogan = new QLabel(QString::fromUtf8("EMBRACE THE VOID  \u2022  REAP YOUR REWARD  \u2022  UNLEASH YOUR POWER  \u2022  MASTER THE DUNGEON"));
    slogan->setObjectName("Slogan");
    slogan->setWordWrap(true);
    v->addWidget(tag);
    v->addWidget(by);
    v->addWidget(slogan);

    {   // about
        auto* c = card(v, "About");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* t = new QLabel(QString::fromUtf8(
            "DungeonKit started out as a Cheat Engine table, and it held its own against Birdi's Master Table. "
            "Now that Minecraft Dungeons II is out, it's getting a complete overhaul as a standalone app.\n\n"
            "The goal is simple: do the things Cheat Engine can't, make modding easy, and put the most advanced "
            "features in everyone's hands without needing Cheat Engine at all. There's more planned, but for now, enjoy.\n\n"
            "\u2014 Plushy | Auzziaruo & Moon | Moonlitfoxi"));
        t->setWordWrap(true);
        cl->addWidget(t);
    }
    {   // credits
        auto* c = card(v, "Credits");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* g = new QGridLayout;
        g->setColumnStretch(1, 1);
        const char* who[][2] = {
            {"Plushy", "App creator and lead developer. Built most of the features."},
            {"Claude AI", "Patching, importing and fixing mistakes in the code, and making testing and app building easier."},
            {"Moonlitfoxi", "Beta app tester. Came up with the idea of making an app instead of using Cheat Engine."},
            {"Hybrid", "Item Stat Viewer, and the inspiration for the World viewer."},
            {"Otis_Inf", "Creator of the Universal UE4 Unlocker (UUU), the inspiration behind our console command DLL."},
            {"JustGaming101", "Provided the blueprint files used to expand the Console commands lists for both games."},
        };
        int r = 0;
        for (auto& w : who) {
            auto* n = new QLabel(w[0]);
            n->setObjectName("Name");
            n->setMinimumWidth(120);
            g->addWidget(n, r, 0);
            auto* d = new QLabel(QString::fromUtf8(w[1]));
            d->setWordWrap(true);
            g->addWidget(d, r, 1);
            ++r;
        }
        cl->addLayout(g);
    }
    {   // honorable mentions
        auto* c = card(v, "Honorable mentions");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        const char* names[3][5] = {
            {"Terrarings", "Dweem", "Calamari", "Hybrid", "Maz"},
            {"Arno", "Liaa", "Moonlitfoxi", "Lucas", "Shadow"},
            {"Inspirity", "Jaded_Book", "JustGaming101", "", ""},
        };
        auto* g = new QGridLayout;
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 5; ++col) {
                auto* l = new QLabel(names[row][col]);
                g->addWidget(l, row, col);
                g->setColumnStretch(col, 1);
            }
        cl->addLayout(g);
    }
    {   // supporters
        auto* c = card(v, "Supporters");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        const char* names[] = {
            "Dweem", "Dweem JR", "Floatzyy", "Grim", "Inspirity",
            "Jai", "Lucas", "Mist", "Moonlitfoxi", "Pink",
            "Santoryu", "Star", "Shadow", "RAGE", "Calamari",
            "Hybrid", "Maz", "Arno", "JustGaming101", "51/49",
        };
        const int cols = 5;
        auto* g = new QGridLayout;
        for (int col = 0; col < cols; ++col) g->setColumnStretch(col, 1);
        int i = 0;
        for (const char* n : names) {
            auto* l = new QLabel(n);
            g->addWidget(l, i / cols, i % cols);
            ++i;
        }
        cl->addLayout(g);
    }
    {   // links
        auto* c = card(v, "Links");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* discord = new QLabel("Discord: <a href=\"https://discord.gg/PfCZu5rSU4\">DungeonKit Discord</a>");
        auto* donate = new QLabel("Donations: <a href=\"https://cash.app/$Mooningstarnight\">cash.app/$Mooningstarnight</a>");
        for (QLabel* l : {discord, donate}) {
            l->setTextFormat(Qt::RichText);
            l->setOpenExternalLinks(true);
            cl->addWidget(l);
        }
    }
    {   // admin note
        auto* c = card(v, "Notes");
        auto* cl = qobject_cast<QVBoxLayout*>(c->layout());
        auto* n = new QLabel("DungeonKit must run as administrator to edit the game's memory, just like Cheat Engine. If Windows Defender or "
            "another antivirus ever causes problems with the app (it shouldn't), try turning it off temporarily or adding "
            "DungeonKit as an exclusion. That should sort it out.");
        n->setWordWrap(true);
        cl->addWidget(n);
    }
    v->addStretch(1);
    scroll->setWidget(inner);
    outer->addWidget(scroll, 1);
    return page;
}

// ====================================================================== behaviour
void MainWindow::applyTheme(bool persist) {
    QPalette pal = qApp->palette();
    pal.setColor(QPalette::Link, theme_.accent2);          // clickable links on the Information page
    qApp->setPalette(pal);
    qApp->setStyleSheet(theme_.toStyleSheet());
    if (persist) theme_.save();
    {
        if (sidebarLogo_) sidebarLogo_->setPixmap(logoPixmap(44));
        const QIcon ic = Logo::icon(theme_.accent, Logo::recolorEnabled());
        setWindowIcon(ic);
        qApp->setWindowIcon(ic);
        refreshNavIcons();
        updateUserLabel();
    }
    applyTitleBar();
}

void MainWindow::applyLanguage() {
    I18n::retranslate(this);
    lastTitle_.clear();
    updateTitle();
    refreshFolders();
    refreshMods();
    refreshBackupInfo();
    if (navTree_) fillNavTree(-1, -1);
    updateHomeStatus();
}

void MainWindow::applyTitleBar() {
    if (!isVisible()) return;                    // the native window does not exist yet; showEvent calls this again
    applyTitleBarColors(this, theme_.sidebar.rgb() & 0xFFFFFF, theme_.text.rgb() & 0xFFFFFF, theme_.border.rgb() & 0xFFFFFF);
}

void MainWindow::showEvent(QShowEvent* e) {
    QMainWindow::showEvent(e);
    applyTitleBar();
}

void MainWindow::updateTitle() {
    const QString sep = QString::fromUtf8("   \u2022   ");
    QString t = QString("%1 v%2").arg(kAppName, kAppVersion);
    const bool online = proc_.attached() && proc_.alive();
    if (online) {
        auto ver = proc_.version();
        if (target_ == Process::Target::Dungeons2)
            t += sep + (ver ? QString("Minecraft Dungeons II %1").arg(QString::fromUtf8(ver->name)) : I18n::tr("Minecraft Dungeons II (unknown build)"));
        else t += sep + (ver ? I18n::tr("Minecraft Dungeons %1 (%2)")
                              .arg(QString::fromUtf8(ver->name), QString::fromUtf8(platformName(ver->platform)))
                        : I18n::tr("Minecraft Dungeons (unknown build)"));
    } else {
        t += sep + (target_ == Process::Target::None ? I18n::tr("Default (no game selected)")
                   : target_ == Process::Target::Dungeons2 ? I18n::tr("Waiting for Minecraft Dungeons II") : I18n::tr("Waiting for Minecraft Dungeons"));
    }
    if (!username_.isEmpty())
        t += sep + (role_ == Identity::Role::Dev ? I18n::tr("%1 (DEV)").arg(username_)
                  : role_ == Identity::Role::Assistant ? I18n::tr("%1 (Assistant)").arg(username_)
                  : role_ == Identity::Role::Helper ? I18n::tr("%1 (Elite)").arg(username_)
                  : role_ == Identity::Role::Creator ? I18n::tr("%1 (Content Creator)").arg(username_)
                  : role_ == Identity::Role::Supporter ? I18n::tr("%1 (Supporter)").arg(username_)
                  : I18n::tr("Signed in as %1").arg(username_));
    if (t != lastTitle_) { lastTitle_ = t; setWindowTitle(t); }
}

void MainWindow::refreshNavIcons() {
    static const Icons::Kind kinds[] = {Icons::Home, Icons::Items, Icons::Heart, Icons::Teleports, Icons::Player, Icons::Tools, Icons::Eye,
                                        Icons::Folder, Icons::Globe, Icons::Account, Icons::Customize, Icons::Information,
                                        Icons::Save, Icons::Book,
                                        Icons::Tools, Icons::Heart, Icons::Chest, Icons::Globe, Icons::Camera, Icons::Pin, Icons::Pencil, Icons::Player,
                                        Icons::Camera, Icons::Globe, Icons::Eye};
    for (size_t i = 0; i < navButtons_.size() && i < sizeof(kinds) / sizeof(kinds[0]); ++i)
        if (navButtons_[i]) navButtons_[i]->setIcon(Icons::navIcon(kinds[i], theme_.muted, theme_.text));
}

void MainWindow::setRole(Identity::Role role) {
    role_ = role;
    if (role_ != Identity::Role::None) username_ = Identity::canonicalName(username_).isEmpty() ? username_ : Identity::canonicalName(username_);
    updateUserLabel();
    updateTitle();
    if (welcome_ && !username_.isEmpty()) welcome_->setText(QString("Welcome back, %1.").arg(username_));
}

void MainWindow::updateUserLabel() {
    if (!sidebarUser_) return;
    if (username_.isEmpty()) {
        sidebarUser_->clear();
        sidebarUser_->setStyleSheet(QString());
    } else if (role_ != Identity::Role::None) {
        // Role tags use the theme's colours: a gradient from the accent to the second accent.
        const QString tagText = username_ + (role_ == Identity::Role::Helper ? " - Elite" : role_ == Identity::Role::Creator ? " - Content Creator" : role_ == Identity::Role::Assistant ? " - Assistant" : role_ == Identity::Role::Supporter ? " - Supporter" : " - DEV");   // role tags stay in English
        sidebarUser_->setText(tagText);
        const QColor mid((theme_.accent.red() + theme_.accent2.red()) / 2, (theme_.accent.green() + theme_.accent2.green()) / 2, (theme_.accent.blue() + theme_.accent2.blue()) / 2);
        const QString fg = mid.lightness() > 140 ? "#14101c" : "#ffffff";
        // If a long name makes the tag too wide for the sidebar, shrink the text a little (never more than a
        // few pixels) so it still fits on one line. Short tags like "Plushy - DEV" are left at the normal size.
        const int baseFs = theme_.fontSize > 0 ? theme_.fontSize : 13;
        const int maxTextWidth = 230 - 16 - 16 - 20;       // sidebar width, minus its margins, minus the tag's own padding
        int fs = baseFs;
        QFont tagFont = sidebarUser_->font();
        tagFont.setPixelSize(fs);
        tagFont.setBold(true);
        while (fs > baseFs - 3 && fs > 9 && QFontMetrics(tagFont).horizontalAdvance(tagText) > maxTextWidth) {
            tagFont.setPixelSize(--fs);
        }
        sidebarUser_->setStyleSheet(QString("QLabel { background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 %1, stop:1 %2);"
                                            " color: %3; font-weight: 700; font-size: %4px; padding: 6px 10px; border-radius: 6px; }")
                                        .arg(theme_.accent.name(), theme_.accent2.name(), fg).arg(fs));
    } else {
        sidebarUser_->setText(I18n::tr("Signed in as %1").arg(username_));
        sidebarUser_->setStyleSheet(QString());
    }
}

void MainWindow::setMessage(const QString& textIn) {
    logEvent("Message: " + textIn);
    const QString text = I18n::tr(textIn);
    if (message_) message_->setText(text);
    if (playerMsg_) playerMsg_->setText(text);
    if (toolsMsg_) toolsMsg_->setText(text);
    if (paksMsg_) paksMsg_->setText(text);
}

void MainWindow::setStatus(const QString& state, const QString& line1, const QString& line2) {
    QString color = state == "on" ? "#3ddc84" : state == "warn" ? "#ffb347" : "#ff5c6c";
    statusDot_->setStyleSheet(QString("background: %1; border-radius: 5px;").arg(color));
    statusTitle_->setText(state == "on" ? "Online" : state == "warn" ? "Online (unknown version)" : "Offline");
    statusLine1_->setText(line1);
    statusLine2_->setText(line2);
}

void MainWindow::refreshEnabledState() {
    const bool online = proc_.attached() && proc_.alive();
    for (auto& t : toggles_) {
        t.box->setEnabled(online);
        t.box->setChecked(online && t.mod->enabled());
    }
    for (auto& r : rows_) r.holder->setEnabled(online && r.owner->enabled());
}

void MainWindow::disableAllMods() {
    if (mcd1Cam_) mcd1Cam_->restore();
    if (shopCard_) shopCard_->restore();
    if (modMaker_) modMaker_->stopAll();
    for (Mod* m : game_->mods.all) m->disable(proc_);
}

void MainWindow::disableAll() {
    if (proc_.attached()) disableAllMods();
    for (auto& r : rows_) if (r.freeze) r.freeze->setChecked(false);
    if (mcd2Hub_) mcd2Hub_->stopFreezes();
    refreshEnabledState();
    setMessage("Every mod is switched off.");
}

bool MainWindow::mcd2Ready() {
    if (target_ != Process::Target::Dungeons2 || !proc_.attached() || !proc_.alive()) return false;
    auto ue = game_->mods.ue4;
    if (mcd2Pid_ != proc_.pid()) {                       // a different game process: forget everything about the old one
        mcd2Pid_ = proc_.pid();
        ue->reset();
        mcd2_->reset();
        mcd2Hub_->stopFreezes();
        mcd2Tried_.invalidate();
    }
    if (!ue->ready()) {
        if (mcd2Tried_.isValid() && mcd2Tried_.elapsed() < 4000) return false;      // the engine search is slow; do not repeat it every tick
        mcd2Tried_.restart();
        engineProc();
    }
    return ue->ready();
}

Process* MainWindow::engineProc() {
    // Dungeons 2 test: try each attached process and keep the first that has readable engine data.
    Process* list[2] = {&proc_, &procAlt_};
    for (Process* p : list) {
        if (!p->attached() || !p->alive()) continue;
        std::string e;
        if (game_->mods.ue4 && game_->mods.ue4->init(*p, e)) return p;
    }
    return proc_.attached() ? &proc_ : (procAlt_.attached() ? &procAlt_ : &proc_);
}

// Which storefront Minecraft Dungeons II is running from, straight from the exe name (not the version table), so
// it's right even for a build the table doesn't know about yet.
QString MainWindow::mcd2PlatformLabel(const std::string& exeName) {
    if (exeName == "Dungeons-WinGDK-Shipping.exe") return "Microsoft Store/Xbox App";
    return "Steam";   // Dungeons-Win64-Shipping.exe or its Dungeons.exe launcher
}

// The game's own GeneralProjectSettings.ProjectVersion, read live by name like everything else dk::UE4 does - no
// module-size table entry needed, so this keeps working on any platform (Microsoft Store/Xbox, Steam, ...) and any
// future patch. Scans the live object table once per game process and caches the result; "" while not found yet
// (the object table may not be ready in the first second or two after attaching).
QString MainWindow::mcd2LiveVersionFor(Process& p) {
    if (mcd2LiveVersionPid_ != p.pid()) { mcd2LiveVersion_.clear(); mcd2LiveVersionPid_ = 0; mcd2LiveVersionNextTry_ = 0; }
    if (!mcd2LiveVersion_.isEmpty()) return mcd2LiveVersion_;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < mcd2LiveVersionNextTry_) return QString();
    mcd2LiveVersionNextTry_ = now + 2000;
    auto ue = game_->mods.ue4;
    if (!ue) return QString();
    std::string err;
    if (!ue->ready() && !ue->init(p, err)) return QString();
    UE4::ObjArray arr;
    std::string aerr;
    if (!ue->objectArray(arr, aerr)) return QString();
    const int lim = arr.num < 2000000 ? arr.num : 2000000;
    for (int i = 0; i < lim; ++i) {
        const uint64_t o = ue->objectAt(arr, i);
        if (!UE4::plausible(o) || ue->className(o) != "GeneralProjectSettings") continue;
        UE4::Prop pr;
        if (!ue->find(o, "ProjectVersion", pr)) continue;
        const uint64_t addr = o + pr.offset;
        const uint64_t data = ue->rd64(addr);
        int32_t num = 0, max = 0;
        ue->rd(addr + 8, num);
        ue->rd(addr + 12, max);
        if (!UE4::plausible(data) || num < 1 || num > 64 || max < num) continue;
        std::vector<uint16_t> u((size_t)num, 0);
        if (!p.read(data, u.data(), (size_t)num * 2)) continue;
        std::wstring text;
        for (int k = 0; k < num && u[(size_t)k]; ++k) text.push_back((wchar_t)u[(size_t)k]);
        if (text.empty()) continue;
        mcd2LiveVersion_ = QString::fromStdWString(text);
        mcd2LiveVersionPid_ = p.pid();
        return mcd2LiveVersion_;
    }
    return QString();
}

void MainWindow::toggleMod(Mod* mod, QCheckBox* box, bool on) {
    if (on && target_ == Process::Target::None) {
        box->setChecked(false);
        setMessage("Choose Minecraft Dungeons or Minecraft Dungeons II on the Home page first.");
        return;
    }
    if (!proc_.attached() || !proc_.alive()) {
        box->setChecked(false);
        setMessage("The game is not running.");
        return;
    }
    if (on && target_ == Process::Target::Dungeons2 && mod != game_->mods.timer && mod != game_->mods.pause) {
        box->setChecked(false);
        setMessage("This feature is for Minecraft Dungeons. Minecraft Dungeons II has its own pages in the Minecraft Dungeons II section.");
        return;
    }
    if (on) {
        std::string err;
        QGuiApplication::setOverrideCursor(Qt::WaitCursor);      // some mods scan the whole game module
        Process* usep = target_ == Process::Target::Dungeons ? &proc_ : engineProc();
        const bool ok = mod->enable(*usep, err);
        QGuiApplication::restoreOverrideCursor();
        if (ok) {
            QString msg = QString("%1 enabled.").arg(QString::fromStdString(mod->name()));
            if (mod == game_->mods.apocalypse && game_->mods.apocalypse->mapMode())
                msg += " Apocalypse Level is using the open mission map. Keep the map open while you set the level.";
            if (mod == game_->mods.cooldown) {
                const std::string n = game_->mods.cooldown->note();
                if (!n.empty()) { msg += " " + QString::fromStdString(n); logEvent("No Cooldown is using slot mode (the code pattern was not found in this build)."); }
            }
            // The technique description used to be appended to the on-screen message, but it can run long and that
            // made the message bar resize and shove the surrounding layout around every time a mod was toggled. It's
            // only useful for troubleshooting, so it goes to the Backup > Debug text box instead (same text, same
            // "disabled."-chronological log, just out of the way of normal use) and the on-screen message stays short.
            const std::string tech = mod->technique();
            if (!tech.empty()) logEvent(QString::fromStdString(mod->name()) + " - how it's hooked in: " + QString::fromStdString(tech));
            setMessage(msg);
        } else {
            box->setChecked(false);
            setMessage(QString::fromStdString(mod->name()) + ": " + QString::fromStdString(err));
        }
    } else {
        mod->disable(proc_);
        setMessage(QString("%1 disabled.").arg(QString::fromStdString(mod->name())));
    }
    refreshEnabledState();
}

void MainWindow::pollGame() {
    watchToggles();
    if (proc_.attached() && !proc_.alive()) {
        {
            const unsigned long code = proc_.exitCode();
            const bool crash = (code & 0xC0000000UL) == 0xC0000000UL;
            logEvent(QString("Game closed: %1 (PID %2), exit code 0x%3%4").arg(QString::fromStdString(proc_.exeName())).arg(proc_.pid())
                         .arg(code, 0, 16).arg(code == 0 ? " - normal exit" : crash ? " - looks like a CRASH" : " - closed with a non-zero code (killed or crashed)"));
            logEvent("All mods were switched off because the game is gone.");
            lastPid_ = 0;
        }
        disableAllMods();        // drops state; memory is gone with the process
        if (mcd2Hub_) mcd2Hub_->stopFreezes();
        proc_.detach();
        setMessage("The game closed - all mods were switched off.");
    }
    if (target_ == Process::Target::None) {
        if (proc_.attached()) { disableAllMods(); proc_.detach(); }
        procAlt_.detach();
        setStatus("off", "Offline", "Default: not connected. Choose a game on the Home page.");
        refreshEnabledState();
        updateTitle();
        updateHomeStatus();
        return;
    }
    const bool g2 = target_ == Process::Target::Dungeons2;
    if (g2 && procAlt_.attached() && !procAlt_.alive()) procAlt_.detach();
    if (!proc_.attached()) {
        std::string err;
        bool got = proc_.attach(err, target_);
        if (!got && g2) got = proc_.attach(err, Process::Target::Dungeons2Launcher);   // game exe not up yet: fall back to Dungeons.exe
        if (!got) {
            setStatus("off", "Offline", QString::fromStdString(err));
            refreshEnabledState();
            updateTitle();
            updateHomeStatus();
            return;
        }
    }
    if (g2 && !procAlt_.attached() && (proc_.exeName() == "Dungeons-WinGDK-Shipping.exe" || proc_.exeName() == "Dungeons-Win64-Shipping.exe")) {
        std::string e2;
        procAlt_.attach(e2, Process::Target::Dungeons2Launcher);                  // second process (Dungeons.exe launcher), if it's running too
    }
    auto ver = proc_.version();
    if (proc_.pid() != lastPid_) {
        lastPid_ = proc_.pid();
        logEvent(QString("Attached to %1 (PID %2), module size %3. Nothing was injected: DungeonKit works from outside the game.")
                     .arg(QString::fromStdString(proc_.exeName())).arg(proc_.pid()).arg(proc_.moduleSize()));
        logEvent(ver ? QString("Game version recognised: %1 (%2)").arg(QString::fromUtf8(ver->name), QString::fromUtf8(platformName(ver->platform)))
                     : QString("Game version not in the table; features will search by pattern/name."));
    }
    if (g2) {
        const QString l1d2 = QString("%1  (PID %2)").arg(QString::fromStdString(proc_.exeName())).arg(proc_.pid());
        const QString plat = mcd2PlatformLabel(proc_.exeName());
        const QString live = mcd2LiveVersionFor(proc_);
        if (!live.isEmpty()) setStatus("on", l1d2, QString("Minecraft Dungeons II %1  -  %2").arg(live, plat));
        else if (ver) setStatus("on", l1d2, QString("Minecraft Dungeons II %1  -  %2").arg(QString::fromUtf8(ver->name), plat));
        else setStatus("warn", l1d2, QString("Version not detected yet (%1, module size %2). Features still try to find their data by name.").arg(plat).arg(proc_.moduleSize()));
        refreshEnabledState();
        updateTitle();
        updateHomeStatus();
        return;
    }
    {
        int num = ver ? ver->number : 29;     // unknown builds use the newest name lists
        if (num != listVersion_) { listVersion_ = num; itemsPage_->setVersionNumber(num); }
    }
    QString l1 = QString("%1  (PID %2)").arg(QString::fromStdString(proc_.exeName())).arg(proc_.pid());
    if (ver) {
        setStatus("on", l1, QString("Version %1  -  %2").arg(QString::fromUtf8(ver->name), QString::fromUtf8(platformName(ver->platform))));
    } else {
        setStatus("warn", l1, QString("Module size %1 is not in the version table. Mods still try to find their code by pattern.").arg(proc_.moduleSize()));
    }
    refreshEnabledState();
    updateTitle();
    updateHomeStatus();
    if (proc_.pid() != foldersPid_) { foldersPid_ = proc_.pid(); refreshFolders(); }   // game just started: look next to its exe
}

void MainWindow::tickValues() {
    if (mcd2Hub_) mcd2Hub_->tick();
    if (itemsPage_) itemsPage_->refresh();
    if (teleportsPage_) teleportsPage_->refresh();
    if (viewerPage_) viewerPage_->tick();
    if (dumpRunning_) {
        const QString w = QString("Working... %1 entries so far").arg(dumpCount_.load());
        if (dumpStatus_) dumpStatus_->setText(w);
        if (testDumpStatus_) testDumpStatus_->setText(w);
        if (expDumpStatus_) expDumpStatus_->setText(w);
    }
    if (playerDiag_ && playerDiag_->isVisible()) {
        std::string d = game_->mods.ue4 ? game_->mods.ue4->diagnostics() : std::string();
        playerDiag_->setText(d.empty() ? QString("Nothing scanned yet. Turn on a feature above.") : QString::fromStdString(d).trimmed());
        playerInfo_->setText(game_->mods.stats->enabled() ? QString::fromStdString(game_->mods.stats->summary()) : QString());
    }
    if (!proc_.attached() || !proc_.alive()) return;

    // live values + freezing
    for (auto& r : rows_) {
        if (!r.owner->enabled()) continue;
        if (r.ispin) {
            if (r.freeze && r.freeze->isChecked()) { r.iset(r.ispin->value()); continue; }
            int32_t v;
            if (!r.ispin->hasFocus() && r.iget(v)) { r.ispin->blockSignals(true); r.ispin->setValue(v); r.ispin->blockSignals(false); }
        } else if (r.fspin) {
            if (r.freeze && r.freeze->isChecked()) { r.fset((float)r.fspin->value()); continue; }
            float v;
            if (!r.fspin->hasFocus() && r.fget(v)) { r.fspin->blockSignals(true); r.fspin->setValue(v); r.fspin->blockSignals(false); }
        }
    }

    // global hotkeys (work while the game window has focus)
    auto down = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
    bool f5 = down(VK_F5), f6 = down(VK_F6);
    auto& m = game_->mods;
    if (f5 && !keyF5_ && m.position->enabled())
        setMessage(m.position->save(proc_) ? "Position saved." : "Could not read the position yet.");
    if (f6 && !keyF6_ && m.position->enabled())
        setMessage(m.position->teleportSaved(proc_) ? "Teleported." : "No saved position yet.");
    keyF5_ = f5; keyF6_ = f6;

    // Timer / Pause hotkeys (from the original table): Ctrl + Numpad + / -, and Pause / Break
    const bool ctrl = down(VK_CONTROL);
    const bool plus = ctrl && down(VK_ADD), minus = ctrl && down(VK_SUBTRACT), pauseKey = down(VK_PAUSE);
    auto boxFor = [this](Mod* mod) -> QCheckBox* {
        for (auto& t : toggles_) if (t.mod == mod) return t.box;
        return nullptr;
    };
    if (plus && !keyPlus_ && !m.timer->enabled()) { if (auto* b = boxFor(m.timer)) toggleMod(m.timer, b, true); }
    if (minus && !keyMinus_ && m.timer->enabled()) { if (auto* b = boxFor(m.timer)) toggleMod(m.timer, b, false); }
    if (pauseKey && !keyPause_) { if (auto* b = boxFor(m.pause)) toggleMod(m.pause, b, !m.pause->enabled()); }
    keyPlus_ = plus; keyMinus_ = minus; keyPause_ = pauseKey;
}

} // namespace dk
