#pragma once
#include <QString>

namespace dk {

// Discord Rich Presence over Discord's local named pipe. Nothing leaves the computer except through the Discord app
// the user is already running; no token or webhook is involved, only the public application ID.
class DiscordPresence {
public:
    DiscordPresence();
    ~DiscordPresence();

    // Sends (or refreshes) the activity. Connects first if needed. Returns false when Discord is not running.
    bool update(const QString& details, const QString& state, qint64 startUnixSeconds);
    void clear();                       // removes the activity and closes the connection
    bool connected() const;
    QString user() const;               // Discord user name from the handshake ("" if unknown)

private:
    bool connectPipe();
    bool sendFrame(unsigned op, const QByteArray& json);
    bool readFrame(unsigned& op, QByteArray& json, int waitMs);
    void closePipe();
    void* pipe_ = nullptr;              // HANDLE
    QString user_;
};

} // namespace dk
