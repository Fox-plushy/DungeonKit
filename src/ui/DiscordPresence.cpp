#include "DiscordPresence.h"

#include <QCoreApplication>
#include <QDateTime>
#include <cstring>

#include <windows.h>

namespace dk {

// The application ID from the Discord developer portal. It is public (it is shown in every Rich Presence).
static const char* kClientId = "1558918250685931582";

static QByteArray esc(const QString& s) {
    QByteArray o;
    for (QChar c : s) {
        const ushort u = c.unicode();
        if (u == '"') o += "\\\"";
        else if (u == '\\') o += "\\\\";
        else if (u < 0x20) o += QByteArray("\\u%1").replace("%1", QByteArray::number(u, 16).rightJustified(4, '0'));
        else o += QString(c).toUtf8();
    }
    return o;
}

DiscordPresence::DiscordPresence() {}
DiscordPresence::~DiscordPresence() { closePipe(); }

bool DiscordPresence::connected() const { return pipe_ != nullptr; }
QString DiscordPresence::user() const { return user_; }

void DiscordPresence::closePipe() {
    if (pipe_) { CloseHandle((HANDLE)pipe_); pipe_ = nullptr; }
}

bool DiscordPresence::sendFrame(unsigned op, const QByteArray& json) {
    if (!pipe_) return false;
    QByteArray buf;
    buf.resize(8);
    const unsigned len = (unsigned)json.size();
    memcpy(buf.data(), &op, 4);
    memcpy(buf.data() + 4, &len, 4);
    buf += json;
    DWORD wrote = 0;
    if (!WriteFile((HANDLE)pipe_, buf.constData(), (DWORD)buf.size(), &wrote, nullptr) || wrote != (DWORD)buf.size()) { closePipe(); return false; }
    return true;
}

bool DiscordPresence::readFrame(unsigned& op, QByteArray& json, int waitMs) {
    if (!pipe_) return false;
    const qint64 end = QDateTime::currentMSecsSinceEpoch() + waitMs;
    for (;;) {
        DWORD avail = 0;
        if (!PeekNamedPipe((HANDLE)pipe_, nullptr, 0, nullptr, &avail, nullptr)) { closePipe(); return false; }
        if (avail >= 8) break;
        if (QDateTime::currentMSecsSinceEpoch() >= end) return false;
        Sleep(15);
    }
    unsigned head[2] = {0, 0};
    DWORD got = 0;
    if (!ReadFile((HANDLE)pipe_, head, 8, &got, nullptr) || got != 8) { closePipe(); return false; }
    op = head[0];
    json.clear();
    if (head[1] > 65536) { closePipe(); return false; }
    json.resize((int)head[1]);
    DWORD done = 0;
    while (done < head[1]) {
        DWORD n = 0;
        if (!ReadFile((HANDLE)pipe_, json.data() + done, head[1] - done, &n, nullptr) || n == 0) { closePipe(); return false; }
        done += n;
    }
    return true;
}

bool DiscordPresence::connectPipe() {
    if (pipe_) return true;
    for (int i = 0; i < 10; ++i) {
        const QString name = QString("\\\\.\\pipe\\discord-ipc-%1").arg(i);
        HANDLE h = CreateFileW((const wchar_t*)name.utf16(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        pipe_ = h;
        if (!sendFrame(0, QByteArray("{\"v\":1,\"client_id\":\"") + kClientId + "\"}")) continue;
        unsigned op = 0;
        QByteArray reply;
        if (readFrame(op, reply, 1500) && op == 1 && reply.contains("READY")) {
            user_.clear();
            const int k = reply.indexOf("\"username\":\"");
            if (k >= 0) {
                const int s = k + 12, e = reply.indexOf('"', s);
                if (e > s) user_ = QString::fromUtf8(reply.mid(s, e - s));
            }
            return true;
        }
        closePipe();
    }
    return false;
}

bool DiscordPresence::update(const QString& details, const QString& state, qint64 startUnixSeconds) {
    if (!connectPipe()) return false;
    QByteArray j = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + QByteArray::number((qlonglong)GetCurrentProcessId()) +
                   ",\"activity\":{\"details\":\"" + esc(details) + "\",\"state\":\"" + esc(state) + "\"," +
                   "\"timestamps\":{\"start\":" + QByteArray::number((qlonglong)startUnixSeconds) + "}," +
                   "\"assets\":{\"large_image\":\"logo\",\"large_text\":\"DungeonKit\"}}}," +
                   "\"nonce\":\"" + QByteArray::number((qlonglong)QDateTime::currentMSecsSinceEpoch()) + "\"}";
    if (!sendFrame(1, j)) return false;
    unsigned op = 0;
    QByteArray reply;
    readFrame(op, reply, 300);                      // Discord answers with the activity (or an error); either way we are done
    return pipe_ != nullptr;
}

void DiscordPresence::clear() {
    if (pipe_) {
        const QByteArray j = "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" + QByteArray::number((qlonglong)GetCurrentProcessId()) +
                             "},\"nonce\":\"clear\"}";
        sendFrame(1, j);
        unsigned op = 0;
        QByteArray reply;
        readFrame(op, reply, 200);
    }
    closePipe();
}

} // namespace dk
