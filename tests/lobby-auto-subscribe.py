"""Regression test for the lobby event feedback loop, using the production method."""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'gui/NEMainpage.cpp').read_text(encoding='utf-8')
start = source.index('void NEMainpage::autoJoinOfficialLobby()')
end = source.index('void NEMainpage::showOfficialLobby()', start)
method = source[start:end]
prefix = r"""
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <list>
#include <vector>
using ChatLobbyId = uint64_t;
const ChatLobbyId OFFICIAL_RETROCHESS_LOBBY_ID = 42;
struct RsGxsId { bool isNull() const { return false; } };
struct RsIdentityDetails { unsigned int mFlags = 1; };
const unsigned int RS_IDENTITY_FLAGS_PGP_LINKED = 1;
struct VisibleChatLobbyRecord { ChatLobbyId lobby_id = 42; };
struct Identity {
    bool getIdDetails(RsGxsId, RsIdentityDetails &) { return true; }
    void getOwnIds(std::list<RsGxsId> &ids) { ids.push_back({}); }
} identity;
auto rsIdentity = &identity;
struct Chats {
    bool subscribed = true, autoSubscribe = false;
    int pendingEvents = 0, writes = 0, joins = 0;
    void getChatLobbyList(std::list<ChatLobbyId> &ids) { if (subscribed) ids.push_back(42); }
    bool getLobbyAutoSubscribe(ChatLobbyId) { return autoSubscribe; }
    void setLobbyAutoSubscribe(ChatLobbyId, bool enabled) {
        autoSubscribe = enabled;
        ++writes;
        ++pendingEvents; // Mirrors DistributedChatService: even unchanged values emit an event.
    }
    void getListOfNearbyChatLobbies(std::vector<VisibleChatLobbyRecord> &lobbies) { lobbies.push_back({}); }
    void getDefaultIdentityForChatLobby(RsGxsId &) {}
    bool joinVisibleChatLobby(ChatLobbyId, RsGxsId) {
        subscribed = true;
        ++joins;
        setLobbyAutoSubscribe(42, true);
        ++pendingEvents;
        return true;
    }
} chats;
auto rsChats = &chats;
struct Label { void setText(const char *) {} };
struct Ui { Label label; Label *officialLobbyStatus = &label; };
struct NEMainpage {
    Ui widgets; Ui *ui = &widgets;
    const char *tr(const char *text) { return text; }
    void showOfficialLobby() {}
    void autoJoinOfficialLobby();
};
"""
suffix = r"""
int main() {
    NEMainpage page;
    auto drainEvents = [&]() {
        for (int i = 0; i < 20 && chats.pendingEvents; ++i) {
            --chats.pendingEvents;
            page.autoJoinOfficialLobby(); // CHAT_LOBBY_LIST_CHANGED handler.
        }
        return chats.pendingEvents == 0;
    };
    page.autoJoinOfficialLobby();
    if (!drainEvents() || chats.writes != 1) {
        std::cerr << "Lobby events keep generating more events\n";
        return 1;
    }
    for (int i = 0; i < 100; ++i) page.autoJoinOfficialLobby();
    if (chats.writes != 1 || chats.pendingEvents) return 2;
    chats = Chats(); chats.subscribed = false;
    page.autoJoinOfficialLobby();
    if (!drainEvents() || chats.joins != 1 || chats.writes != 1) return 3;
    std::cout << "Lobby event-loop regression checks passed\n";
}
"""
compiler = shutil.which('g++')
with tempfile.TemporaryDirectory(prefix='lobby-test-', dir=root / 'temp') as work:
    work = Path(work)
    environment = dict(os.environ, TMPDIR=str(work))
    # Verify this test detects the original unconditional setter before checking the fix.
    old = method.replace('if (!rsChats->getLobbyAutoSubscribe(OFFICIAL_RETROCHESS_LOBBY_ID))', '')
    for name, body, expected in [('before', old, 1), ('after', method, 0)]:
        cpp = work / (name + '.cpp')
        exe = work / (name + ('.exe' if os.name == 'nt' else ''))
        cpp.write_text(prefix + body + suffix, encoding='utf-8')
        subprocess.run([compiler, '-std=c++11', str(cpp), '-o', str(exe)], check=True, env=environment)
        result = subprocess.run([str(exe)], env=environment)
        if result.returncode != expected:
            raise RuntimeError(f'{name}: unexpected exit {result.returncode}, expected {expected}')
