#include <QApplication>
#include <QSettings>
#include "ui/ClickSound.h"
#include "ui/I18n.h"
#include "ui/MainWindow.h"
#include "ui/NameDialog.h"
#include "ui/Splash.h"
#include "ui/WheelGuard.h"
#include "ui/Theme.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("DungeonKit");
    QCoreApplication::setApplicationName("DungeonKit");

    app.installEventFilter(new dk::WheelGuard(&app));       // wheel never edits values, still scrolls pages

    dk::ClickSound::install();                              // button click sounds (off unless chosen in Customize)
    dk::I18n::init();

    // Use the saved look for the welcome dialog too.
    dk::Theme theme;
    theme.load();
    app.setStyleSheet(theme.toStyleSheet());

    // Ask for a name on every start (pre-filled with the last one). Quit closes the app.
    dk::NameDialog dlg(QSettings().value("account/username").toString());
    if (dlg.exec() != QDialog::Accepted) return 0;
    QSettings().setValue("account/username", dlg.name());

    dk::MainWindow w;
    w.setRole(dlg.role());
    {
        dk::SplashDialog splash(theme);            // cosmetic loading screen, 5-10 seconds
        splash.exec();
    }
    w.show();
    return app.exec();
}
