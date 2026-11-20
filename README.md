## Introduction

mod-chat-transmitter links your AzerothCore worldserver to a Discord bot: [chat-transmitter-bot](https://github.com/Temucraft/chat-transmitter-bot).

This is the Temucraft fork of [azerothcore/mod-chat-transmitter](https://github.com/azerothcore/mod-chat-transmitter). It adds:

- **No Eluna needed.** The module builds and works without Eluna. When an Eluna module (mod-ale or mod-eluna) is in the build, the Eluna parts turn on by themselves.
- **Game events on Discord:** server online and shutting down (and "connection lost" when it crashes), player deaths (with the boss that killed them), level milestones, boss kills with realm first kills, and expedition completions from mod-temucraft-expeditions.
- **Chat relay both ways:** custom channels (like World) and the guild chat of guilds that want it, from the game to Discord and from Discord into the game.
- **Account linking:** `.discord link` in game shows a code; `/link code` on Discord confirms it.
- **Privacy:** account names, account ids and IP addresses are no longer sent to the bot (unless you turn `ChatTransmitter.SendAccountInfo` on), and playerbots are left out.

The worldserver connects to the bot over a WebSocket, like upstream. The bot reads the game databases with its own read-only MySQL user for its commands.

## Requirements

- AzerothCore 3.3.5a (wotlk) with this module in `modules/`.
- The bot: [chat-transmitter-bot](https://github.com/Temucraft/chat-transmitter-bot) (Node.js 22).
- Eluna is **optional**.

## Setup

1. Put the module in `modules/mod-chat-transmitter` (Temucraft: it is in `modulesList.txt`) and run CMake again, then build.
   The CMake output says `mod-chat-transmitter: no Eluna, building without the Eluna features` (or that Eluna was found).
2. Start the worldserver once. The database updater makes the module's tables:
   - auth: `chat_transmitter_discord_link` (account links)
   - characters: `chat_transmitter_boss_kills` (realm first kills) and `chat_transmitter_event_queue` (messages from other modules)
   If your updater does not apply module SQL, run the files in `data/sql/db-auth/base` and `data/sql/db-characters/base` yourself.
3. Copy `conf/ChatTransmitter.conf.dist` to `ChatTransmitter.conf` (next to `worldserver.conf`, in `configs/modules`) and set at least:

| Option | What it does |
| --- | --- |
| `ChatTransmitter.Enabled` | `1` to turn the module on. |
| `ChatTransmitter.BotWsHost` / `BotWsPort` | Where the bot listens (`127.0.0.1` and `22141` when it runs on the same machine). |
| `ChatTransmitter.BotWsKey` | The same long random string as `SECRET_KEY` in the bot's `.env`. |
| `ChatTransmitter.DiscordGuildId` | The id of your Discord server. |

   The other options (events, relay, link codes) are explained in the file. The defaults work.
4. Set up the bot (see its README), start it, then start the worldserver. The worldserver log says
   `[ModChatTransmitter] Connected to WebSocket server.` and Discord shows "The server is online".

## Game events

| Event | Option | Sent when |
| --- | --- | --- |
| `serverStatus` | `Events.ServerStatus` | The server is ready, and when it shuts down. If the connection drops without a shutdown, the bot posts "Lost the connection". A config reload does not post anything. |
| `death` | `Events.Deaths`, `Events.Deaths.MinLevel` | A player dies. `1` (default): only deaths to a boss. `2`: every death. Battlegrounds and arenas are left out. |
| `levelUp` | `Events.LevelUp.Milestones` | A player reaches one of the listed levels. |
| `bossKill` | `Events.BossKills` | A dungeon boss or world boss dies. `1`: only the first kill on the realm (per difficulty). `2`: every kill. |
| `expedition` | `Events.Expeditions` | A character's completion count in mod-temucraft-expeditions goes up. |

### Expeditions and other modules (extension point)

mod-temucraft-expeditions has no hook other modules can use, and modules do not include each other's code. So this
module reads `mod_temucraft_expedition_progress` every `Events.PollInterval` seconds, only for the players online on
this server, and sends an event when a completion count goes up. Without that module nothing happens.

Any module, script or tool can post on Discord without linking to this module:

- SQL: `INSERT INTO chat_transmitter_event_queue (source, message) VALUES ('events', 'The Darkmoon Faire is here!');`
  (characters database). The row is sent and deleted. The bot picks the channel with `channels.default.notifications.<source>` in `config.json`, or `default`.
- C++ (when you do build against this module): `sChatTransmitter->QueueNotification(source, message)` or
  `sChatTransmitter->QueueEvent("myEvent", nlohmann::json{...})`.

## Chat relay

- **Game to Discord:** chat in custom channels always goes to the bot (like upstream); the bot only posts the channels you map in `RELAY_CHANNELS`. Guild chat is only sent for the guild ids in `ChatTransmitter.Relay.Guilds`. Officer chat is never sent. Say, yell and emote go to zone channels as before.
- **Discord to game:** a message in a mapped Discord channel is shown in game as `[Discord] [World] Name: text` to everyone in that channel, or as a system line in the guild. The channel must be listed in `ChatTransmitter.Relay.Channels` and the guild in `ChatTransmitter.Relay.Guilds`. WoW codes (`|c`, `|H`) and line breaks are removed and the text is cut at 255 characters.

## Account linking

1. In game: `.discord link`. You get an 8 letter code (it works for `ChatTransmitter.Link.CodeMinutes` minutes, one new code every 30 seconds).
2. On Discord: `/link code CODE`. The bot sends the code to the worldserver, which writes the link in `chat_transmitter_discord_link` (auth database) and tells you in game.
3. `.discord status` shows whether your account is linked, `.discord unlink` (or `/link remove` on Discord) removes it. `/link status` on Discord shows only you which characters are linked.

The bot never writes to the game databases: linking goes through the worldserver.

## Messages between the worldserver and the bot

All messages are JSON `{ "message": name, "data": {...} }` over the WebSocket.

- To the bot (upstream): `localChat`, `channelChat`, `commandResult`, `queryResult`, `anticheatReport`, `elunaError` (only with Eluna), `notification`.
- To the bot (new): `gameEvent` (`type` is one of the events above), `guildChat`, `linkResult`.
- To the worldserver (upstream): `command`, `query`. New: `relayChat`, `linkAccount`, `unlinkAccount`.

## Eluna

Without Eluna the module skips the "eluna" query database and the Lua error forwarding; everything else works.
With mod-ale or mod-eluna in the build, set `ChatTransmitter.ElunaDatabaseInfo` if you want the bot's `/query` to reach the Eluna database
(`data/sql/eluna_create` makes an empty `acore_eluna` database). Lua errors are only forwarded with an Eluna build that has the `OnError` event.

## Credits

Made by [Roboto](https://ko-fi.com/roboto) and the AzerothCore community. Temucraft changes by FlyingArowana.
