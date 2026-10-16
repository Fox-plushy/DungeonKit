#pragma once
// Experiments > Probe DLL / Console DLL (test): loads one of our DLLs into the running game and shows the log it writes. It is a first step toward our own console DLL, inspired by the
// Universal UE4 Unlocker (UUU) by Otis_Inf.
#include <QFrame>
#include <functional>

class QLabel;
class QPlainTextEdit;
class QVBoxLayout;

namespace dk {

class Process;

class ProbeCard : public QFrame {
    Q_OBJECT
public:
    struct Spec { QString title, hint, dll, log, button; bool closeButton = false; };      // dll and log are file names inside the Probe folder; closeButton adds a "Close game" button next to Open folder
    ProbeCard(const Spec& spec, std::function<Process*()> proc, QWidget* parent = nullptr);
    void addExtra(QWidget* w);           // extra controls between the buttons and the log
signals:
    void message(const QString& text);

private:
    QString dllDir() const;
    QString logPath() const;             // next to the DLL, or the temp folder if that was not writable
    void inject();
    void showLog();
    void openFolder();
    void closeGame();

    Spec spec_;
    std::function<Process*()> proc_;
    QPlainTextEdit* log_ = nullptr;
    QVBoxLayout* extra_ = nullptr;
};

} // namespace dk
