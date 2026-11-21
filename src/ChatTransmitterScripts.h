#ifndef _MOD_CHAT_TRANSMITTER_SCRIPTS_H_
#define _MOD_CHAT_TRANSMITTER_SCRIPTS_H_

#include "Define.h"
#include <mutex>

class Player;

namespace ModChatTransmitter
{
    void AddWorldScripts();
    void AddPlayerScripts();
    void AddEventScripts();
    void AddLinkCommandScripts();

    // EventScripts.cpp: sends the deaths seen during the last world update
    // one lock for the event hooks' shared state (deaths, boss kills, the poller), safe with MapUpdate.Threads > 1
    std::recursive_mutex& EventsLock();
    void ProcessPendingEvents();
    // EventScripts.cpp: loads the bosses that were already killed (for realm first kills)
    void LoadKilledBosses();

    // EventPoller.cpp: expeditions and the chat_transmitter_event_queue table
    void StartEventPoller();
    void UpdateEventPoller(uint32 diff);
    void EventPollerLogin(Player* player);
    void EventPollerLogout(Player* player);
}

#endif // _MOD_CHAT_TRANSMITTER_SCRIPTS_H_
