---
name: uselesspope-broker
description: Working knowledge of the UselessPope-Broker — the multi-threaded C++ daemon that drains the MariaDB command queues, plays audio via SDL2_mixer, launches the Raspberry Pi LED-animation Python processes, listens for remote-button UDP datagrams, and writes SERMON/STATS back to the database. Use when reading, modifying, or extending src/, config/, db/, common/, or when tracing how a command flows from the web front-end (UselessPope-www) or remote button through the broker to the Raspberry Pi (UselessPope-raspi).
---

# UselessPope-Broker

A single long-running C++ daemon (`broker`) that sits in the middle of the
Useless Pope installation: it **consumes** command rows that the web front-end and
the physical remote buttons leave in MariaDB, **executes** them (play a sound,
launch an LED animation), and **writes back** the results (`SERMON` log lines,
`STATS` counters) that the web UI renders.

The broker does **not** talk to the LED matrix, GPIO pins, fog machine or motor
directly — that is the job of the Python scripts in the sibling
`UselessPope-raspi` repo, which the broker spawns as child processes. The broker
*does* own audio playback (SDL2_mixer), the UDP listener for the remote buttons,
and the queue-draining logic.

## When to use this skill

- Reading or explaining any file in `src/`, `config/`, `db/`, `common/`.
- Adding or changing a queue-consuming worker, a config section, or a DB table.
- Tracing how a button/sound/animation command reaches the hardware.
- Debugging the daemon (threads, DB connection, config, build).

## Quick orientation

| Area | File(s) | Purpose |
|------|---------|---------|
| Entry point | `src/main.cc` | CLI parsing, DB connect + retry, seeds `CONFIG`/`STATS`, spawns all 9 worker threads. |
| App state | `src/App.h` / `App.cc` | Global `APP` — `quit_request` flag and the `ThreadedDatabase db` handle shared by all threads. |
| DB row bindings | `src/bindtypes.h` / `bindtypes.cc` | One C++ struct per table (`CONFIG`, `STATS`, `SERMON`, the `*_QUEUE` + `P_*` tables, `USERS_ACTION`) + `create_sql()`. |
| Threads | `src/Fetch*`, `src/Play*`, `src/ButtonListener.*`, `src/AutoStopFog.*` | Each is a `BasicThread` with a `run()` loop (see table below). |
| Config | `config/Config*.{h,cc}` | Typed INI sections via `Configfile2` (`database`, `network`, `animations`, `pope_reacts`, `global`). |
| SQL driver | `db/` | Abstraction over mysql/oracle/odbc; `Tools::ThreadedDatabase`, `StdSqlSelect/Insert/Update`. |
| Util submodule | `cpputils/` | Third-party git submodule (arg parsing, formatting, utf8, xml, …). |
| Schema source | `create.sql` | Authoritative `CREATE TABLE` script (also printed by `./broker -create-sql`). |
| Answers file | `papst_answers.txt` | Song-title → pope reaction mapping (consumed by `FetchAnswers`). |

## The nine worker threads

Started from `main.cc`; each is a `BasicThread` that loops on `APP.quit_request`:

| Thread (`run()`) | Class | What it does |
|------------------|-------|--------------|
| Answers | `FetchAnswers` | Loads `papst_answers.txt`; every 1 min matches recently played chunks → writes `SERMON` reaction rows, links via `P_PLAY_QUEUE_CHUNKS.sermon_reaction_idx`. |
| UDP listener | `ButtonListener` | Boost.Asio UDP server on `[network].UDPListenPort` (default 22000); parses remote-button datagrams into `BUTTON_QUEUE`. |
| Button worker | `FetchButton` | Polls `BUTTON_QUEUE`, maps MAC/username → `USERS_ACTION`, resolves audio file, enqueues `PLAY_QUEUE_CHUNKS`, archives to `P_BUTTON_QUEUE`. Physical-button clicks are counted (1–5) and select the N-th case-insensitively sorted `audio_main` file. |
| Sound player | `PlaySound` | Owns SDL2_mixer; plays music + chunks from in-memory lists. |
| Sound fetcher | `FetchSound` | Polls `PLAY_QUEUE_MUSIC` + `PLAY_QUEUE_CHUNKS` → `PlaySound`, archives to `P_*`. |
| Animation player | `PlayAnimation` | Spawns/stops animation processes (`fork`+`execve` of `<python> <file>`). |
| Animation fetcher | `FetchAnimation` | Polls `PLAY_QUEUE_ANIMATION` → `PlayAnimation`, archives to `P_PLAY_QUEUE_ANIMATION`. |
| Stats | `FetchStats` | Every 1 min computes `totalactions`, `user1..3`, `mostplayedsound` into `STATS`. |
| Fog watchdog | `AutoStopFog` | Turns `CONFIG.fog` back to `0` if it has been on for > 60 s. |

## Command flow through the broker (the big picture)

```
                 ┌──────────────────────────────────────────────────────────┐
 UselessPope-www │  PHP writes:  BUTTON_QUEUE (action=PLAYSOUND)            │
   (web panel)   │               PLAY_QUEUE_ANIMATION, CONFIG.{fog, ...}    │
                 └───────────────┬──────────────────────────────────────────┘
                                 │  shared MariaDB `papst`
 ┌───────────────┐               │
 │ RemoteButton  │  UDP :22000   │
 │  (Arduino)    │──────────────►│ ButtonListener → BUTTON_QUEUE
 └───────────────┘               │ (ButtonPressed/ButtonReleased/Ping)
                                 │
                                 ▼
                          FetchButton ──► PLAY_QUEUE_CHUNKS ──► FetchSound/PlaySound ──► speakers
                                 │                                    │
                                 ▼                                    ▼
                          P_BUTTON_QUEUE                      P_PLAY_QUEUE_CHUNKS ──► FetchAnswers ──► SERMON
                                 │
                          FetchAnimation ──► PlayAnimation ──► fork/execve ──► UselessPope-raspi python/*.py ──► LED matrix
                                 │
                                 ▼
                          P_PLAY_QUEUE_ANIMATION
```

`SERMON` is rendered by `UselessPope-www/log.php`; `STATS` by
`UselessPope-www/lib/get_uptime.php`. See
[references/interrepo.md](references/interrepo.md) for the full cross-repo wiring.

## Config (`~/.UselessPope-Broker.ini`)

Read at startup via `Configfile2::createDefaultInstaceWithAllModules()->read(true)`.
Defaults are defined in `config/*.cc` and overridden by the INI file:

| Section | Key | Default | Meaning |
|---------|-----|---------|---------|
| `database` | `host` | `localhost` | MariaDB host |
| `database` | `username` | `broker` | DB user (live: `papst`) |
| `database` | `password` | `broker` | DB password (live: `johannespaul`) |
| `database` | `instance` | `pope` | database name (live: `papst`) |
| `database` | `retry_db_timeout` | `60` | seconds to keep retrying DB logon |
| `network` | `UDPListenPort` | `22000` | remote-button UDP port |
| `animations` | `python` | `/home/papst/ypy/bin/python` | interpreter used to launch animations |
| `pope_reacts` | `answers` | `/home/papst/UselessPope-Broker/papst_answers.txt` | pope reaction mapping |

> The **live** credentials (`papst` / `johannespaul`, db `papst`) live in
> `~/.UselessPope-Broker.ini` and override the compiled defaults. The same INI is
> read by the Python side (`UselessPope-raspi/python/config.py`).

## Broker CLI

```
./broker -help                 show usage
./broker -version              show version
./broker -debug                print CPPDEBUG messages
./broker -log-file FILE       append debug output to a timestamped log file
./broker -enqueue-music FILE   insert FILE into PLAY_QUEUE_MUSIC (then exit)
./broker -enqueue-chunk FILE   insert FILE into PLAY_QUEUE_CHUNKS (then exit)
./broker -enqueue-animation F  insert FILE into PLAY_QUEUE_ANIMATION (then exit)
./broker -resolve-usernames    backfill missing usernames in archived chunks and SERMON (then exit)
./broker -create-sql           print the CREATE TABLE schema
./broker -with-drop-table      (with -create-sql) prepend DROP TABLE
```

The `-enqueue-*` flags are used by `UselessPope-raspi/python/ring.py` and
`ring_motor.py`, which shell out to this binary.

## Remote-button UDP protocol

The Arduino remote button sends plain-text UDP datagrams to
`[network].UDPListenPort`. Each line is `;`-separated `KEY=VALUE` pairs:

```
TIME=650922;SEQ=161;MAC=D8:BC:38:FA:EF:20;IP=192.168.1.137;ACTION=ButtonReleased
```

Recognised keys: `TIME`, `SEQ`, `IP`, `MAC`, `ACTION`. `ACTION` values consumed
by the broker are `ButtonPressed`, `ButtonReleased`, and `Ping` (heartbeat).
`ButtonListener::message_to_table()` maps a datagram to a `BUTTON_QUEUE` row and
rejects lines containing unknown keys.

Every datagram is delivered **twice** (the ESP32 sends it to the broadcast and
the unicast server address), so the queue contains byte-identical duplicate rows
(same `MAC`/`SEQ`/`TIME`). `FetchButton` drops the second copy.

### Multi-click detection (physical buttons)

`FetchButton` treats a `ButtonReleased` as a click and groups consecutive
releases into a burst (gap ≤ 600 ms, measured with the button's `TIME`). Once no
new click arrives for 800 ms the burst is flushed:

- 1 click → 1st file in `<home_directory>/audio_main/`
- 2 clicks → 2nd file
- 3 clicks → 3rd file
- 4 clicks → 4th file
- 5 clicks → 5th file (clicks beyond 5 use the 5th; fewer files clamp to the last)

Files are ordered **lexically, case-insensitively** (e.g. `Alpha.wav`,
`bravo.wav`, `Charlie.wav`). `ButtonPressed` is only archived; it does not
select a sound.

## Building

Autotools project (gnu++23):

```bash
./reconfigure.sh   # or: autoreconf -i && ./configure
make               # produces ./broker
```

Dependencies (from `configure.ac`): Boost ≥ 1.70 (Asio), `SDL2_mixer` ≥ 2.0.0
(pkg-config), `mysqlclient` (pkg-config). The `cpputils/` git submodule provides
the `arg`, `format`, `utf8`, `xml` and `ColoredOutput` helpers and is built as
static libraries linked into `broker`.

## Conventions & gotchas

- **Everything is thread + DB.** New functionality is usually a new
  `BasicThread` subclass started in `main.cc`. It gets its DB access through the
  global `APP.db` (`Tools::ThreadedDatabase`); grab a dispose token with
  `APP.db.get_dispose_token()` before a long-lived loop.
- **Queue → archive pattern.** Workers never physically delete history; they
  copy the row into the `P_*` table, `setHist(HIST_LO, ...)`, reset `idx` to 0,
  then `DELETE FROM <active> WHERE idx = ...`. Keep this when adding a new queue.
- **Audit columns.** Every row struct derives from `BASE` (6 `hist_*` columns);
  stamp them with `setHist(BASE::HIST_TYPE::HIST_AN|HIST_AE|HIST_LO, "broker")`.
- **DB error handling** is inconsistent: some threads throw, some only
  `CPPDEBUG`. Prefer `throw STDERR_EXCEPTION(...)` for fatal DB errors in new code.
- **`fetch_animations()`** only launches a new animation when the queue has
  ≥ 2 entries (or 0 currently running) — it always keeps one "current" process
  and swaps in the next pending one, killing the previous via `SIGTERM`.
- **Chunk drop logic** in `PlaySound::run()` drops queued sound effects in the
  first 5 s of the current chunk, after 30 s, or when interrupted — edit carefully.
- **Async logging.** `CPPDEBUG` goes through an async fan-out (`common/AsyncOutDebug.*`):
  `Tools::x_debug` is an `AsyncOut::Debug` frontend that distributes to an
  `AsyncOut::Logger` console backend and/or an `AsyncOut::FileLogger` file
  backend (timestamped `[YYYY-MM-DD HH:MM:SS] file:line message`). Wired once in
  `main.cc` via `logInit(logFile, console)` + a `LogSession` guard (which drains
  and joins the logger thread on every exit path). `-debug` mirrors to stdout;
  `-log-file` enables the file backend. `common/diag.h` adds per-thread
  `Heartbeat`/`threadCpuMs` diagnostics behind `LOG_TIMING`.
- **Schema is authoritative in `create.sql`** and must stay in sync with
  `bindtypes.h` (`create_sql()` in `bindtypes.cc`) and the `uselesspope-database`
  skill in the `UselessPope-www` repo.

## Common tasks

- **Add a new `CONFIG`/`STATS` default:** edit `insert_default_values()` in
  `src/main.cc` (`insert_config(...)` / `insert_stats(...)`).
- **Add a new queue table:** add a struct in `bindtypes.h` (+ `create_sql()` in
  `bindtypes.cc` and `create.sql`), then a `Fetch*`/`Play*` worker pair.
- **Add a reaction keyword:** edit `papst_answers.txt` — song title on a line,
  indented reaction lines below it (see `FetchAnswers::fetch_from_file`).
- **Enqueue a sound without the web UI:** `./broker -enqueue-chunk /path/file.wav`.
  The missing audit user is taken from the file path (`/home/<user>/…`) or the
  file owner, so CLI/cron enqueues (e.g. the hourly clock) show up as `papst`
  instead of an empty actor in the protocol.
- **Backfill protocol actors:** run `./broker -resolve-usernames`. It resolves
  `P_PLAY_QUEUE_CHUNKS.hist_an_user` from the stored file path/owner and rewrites
  `SERMON.action` entries that still start with `" hat …"`. `FetchAnswers` applies
  the same resolution to new chunks, so the task is only needed for old rows.

## Cross-repo relationship (summary)

- **`UselessPope-www`** (PHP) and the **broker** share the `papst` database: www
  *writes* the queues/`CONFIG`, the broker *drains* them and *writes back*
  `SERMON`/`STATS`/archives that www reads.
- **`UselessPope-raspi`** (Python) reads the same `~/.UselessPope-Broker.ini`;
  the broker *spawns* its animation scripts (paths in `CONFIG.animationN`), and
  some raspi GPIO scripts shell out to the `broker` binary.
- **`UselessPope-RemoteButton`** (Arduino, 4th repo) sends UDP datagrams to the
  broker's `ButtonListener`.
- The authoritative schema for all three is this repo's `create.sql`.

See [references/interrepo.md](references/interrepo.md) for the detailed wiring
and [references/architecture.md](references/architecture.md) for a file-by-file
walkthrough of the broker.
