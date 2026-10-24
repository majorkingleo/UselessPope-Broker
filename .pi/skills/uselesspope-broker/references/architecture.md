# UselessPope-Broker — Architecture Reference

File-by-file walkthrough of the daemon, generated from source analysis.

## Build layout

Autotools, `gnu++23`. `Makefile.am` compiles one binary (`bin_PROGRAMS = broker`)
from the files in `src/` plus five static libraries:

| Library | Sources | Purpose |
|---------|---------|---------|
| `config/libconfig.a` | `config/Config*.{h,cc}` | Typed INI sections (Configfile2). |
| `db/libdb.a` | `db/*.{h,cc}` | SQL abstraction + `ThreadedDatabase`. |
| `common/libcommon.a` | `common/ColBuilder.{h,cc}` | Column builder helper. |
| `cpputils/io/libcpputilsio.a` | `cpputils/io/*` | `ColoredOutput`, `OutDebug`, `DetectLocale`, `read_file`. |
| `cpputils/cpputilsshared/*.a` | `cpputils/cpputilsshared/*` | arg parser, format/utf8/xml/string utils. |

Link libraries: `SDL2_mixer` + `mysqlclient` (via `pkg-config`), Boost.Asio.

`cpputils/` is a **git submodule** (`github.com/majorkingleo/cpputils`).

## Startup sequence (`src/main.cc`)

1. Parse CLI (`Arg::Arg` from cpputils).
2. If `-create-sql`, print `create_sql()` and stop.
3. Read config from `~/.UselessPope-Broker.ini` (via `Configfile2`).
4. Connect to MariaDB, retrying up to `retry_db_timeout` seconds (500 ms sleep
   between attempts) — `APP.reconnect_db` uses `Database::DB_MYSQL`.
5. If any `-enqueue-*` flag is set, insert the row and **exit** (one-shot mode).
   The audit user is resolved by `UsernameResolver` from the file path
   (`/home/<user>/…`) or the file/directory owner, so cron enqueues are not
   stored with an empty `hist_*_user`.
6. If `-resolve-usernames` is set, run the one-shot `ResolveUsernames` task and
   exit. It fills `P_PLAY_QUEUE_CHUNKS.hist_an_user` for old rows and rewrites
   `SERMON.action` entries whose actor is still empty.
7. `insert_default_values()` — seeds missing `CONFIG`/`STATS` keys (idempotent:
   existing keys are left alone, except `fog` which is force-updated).
8. Validate `pope_reacts.answers` file exists.
9. Start the 9 worker threads (see SKILL.md table).
10. Busy-wait on `SDL_QuitRequested()` every 250 ms; then set
   `APP.quit_request = true` and join all threads.

## Global state (`src/App.h`)

```cpp
struct App {
    std::atomic<bool>              quit_request {false};
    Tools::ThreadedDatabase        db {};
    std::function<void()>          reconnect_db {};
};
extern App APP;   // defined in App.cc
```

`ThreadedDatabase` is the broker's single DB connection shared across threads;
workers hold a "dispose token" (`APP.db.get_dispose_token()`) so the connection
is not torn down while in use.

## DB row bindings (`src/bindtypes.h` / `.cc`)

`BASE` is the common ancestor of every row struct and declares the six audit
columns plus `idx`:

```cpp
DBTypeInt idx;
DBTypeDateTime hist_an_zeit, hist_ae_zeit, hist_lo_zeit;
DBTypeVarChar  hist_an_user, hist_ae_user, hist_lo_user;
```

`BASE::setHist(HIST_TYPE, user)` stamps the current local time and user into the
chosen triple. `HIST_TYPE` is `HIST_AN` (created), `HIST_AE` (edited), `HIST_LO`
(archived/deleted).

Row structs (each maps 1:1 to a table):

- `PLAY_QUEUE_CHUNKS` — `file`.
- `P_PLAY_QUEUE_CHUNKS : PLAY_QUEUE_CHUNKS` — adds `sermon_reaction_idx`.
- `PLAY_QUEUE_MUSIC : PLAY_QUEUE_CHUNKS`, `P_PLAY_QUEUE_MUSIC : PLAY_QUEUE_MUSIC`.
- `PLAY_QUEUE_ANIMATION : PLAY_QUEUE_CHUNKS`, `P_PLAY_QUEUE_ANIMATION : PLAY_QUEUE_ANIMATION`.
- `BUTTON_QUEUE` — `time_stamp`, `seq`, `mac_address`, `ip_address`, `action`, `file`.
- `P_BUTTON_QUEUE : BUTTON_QUEUE`.
- `USERS_ACTION` — `username`, `button_mac_address`, `home_directory`,
  `button_press_event0..4`, `play_chunk0..4`.
- `CONFIG`, `STATS` — `key`, `value`.
- `SERMON` — `action`, `reaction`.

`create_sql(bool add_drop_table)` in `bindtypes.cc` generates the schema string
that is also stored in `create.sql`.

## Worker details

### FetchAnswers (`src/FetchAnswers.{h,cc}`)

- `fetch_from_file(path)` parses `papst_answers.txt`: a non-indented line is a
  song title (starts a `Reaction`); following indented lines are the reaction
  text(s). Keywords are extracted from the title by splitting on
  `space/tab/CR/LF/-/_/,`, lowercasing, and stripping quotes.
- `run()` every 1 min calls `fetch_last_played_chunks()`:
  - SELECTs `P_PLAY_QUEUE_CHUNKS` where `sermon_reaction_idx = 0` and
    `hist_an_zeit >= NOW() - 10 MINUTE`.
  - For each, `get_reaction_from_song(file, hist_an_user)` matches the played
    file's title keywords against the answer file; best keyword match wins (ties
    broken randomly). Before that, `UsernameResolver::resolve(file, user)` fills
    an empty actor from the file path/owner, so the chunk and the protocol entry
    both carry a username.
  - Writes a `SERMON` row (`action` = "`<user>` hat `<title>` abgespielt.",
    `reaction` = random intro + matched answer), then updates
    `chunk.sermon_reaction_idx` to the new SERMON `idx`.

### ButtonListener (`src/ButtonListener.{h,cc}`)

- Boost.Asio `udp_server` bound to `[network].UDPListenPort` (default 22000).
- `received_data()` splits incoming bytes on newline, then each line on `;`,
  then each `KEY=VALUE` pair, building a `std::map`. Unknown keys abort the row.
- `message_to_table()` fills a `BUTTON_QUEUE` (defaults `time_stamp`/`seq` to 0
  when missing, via `s2x<unsigned>(s, 0)`), stamps `hist_*`, and inserts.

### FetchButton (`src/FetchButton.{h,cc}`)

- `run()` refreshes the `USERS_ACTION` cache every 30 s (indexed by both
  `button_mac_address` and `username`) and polls `BUTTON_QUEUE` every 100 ms.
- `fetch_buttons()` selects up to 100 rows ordered by `hist_an_zeit, idx`. For each:
  - **Deduplicate** `ButtonPressed`/`ButtonReleased` events: the ESP32 sends every
    datagram twice, so `(MAC, SEQ, TIME)` identifies one physical event. The
    second copy is archived and dropped (`is_duplicate_button_event`).
  - Resolve the row's user: by `mac_address` if present, else by
    `hist_an_user` (username). Unresolvable rows and `Ping` rows are archived
    and dropped.
  - If the row has a `file`, resolve it against the user's `home_directory`:
    try the literal `file`, then `<home>/audio_random/<file>`, then
    `<home>/audio_main/<file>`. On success insert a `PLAY_QUEUE_CHUNKS` row and
    archive to `P_BUTTON_QUEUE`.
  - If no `file` and the action is `ButtonPressed`, just archive it.
  - If no `file` and the action is `ButtonReleased` (or legacy), call
    `handle_button_click()`: group consecutive releases into a `ClickBurst`
    (gap ≤ 600 ms from the button's `TIME`), archive the row, and defer playback.
    `flush_expired_click_bursts()` (called every poll) flushes bursts that have
    been quiet for 800 ms.
- `flush_click_burst()` plays the N-th **case-insensitively sorted** file from
  `<home>/audio_main/`, clamped to 1..5 and to the number of files. N is the
  number of clicks: single = 1st file, double = 2nd, … up to 5 clicks = 5th.
  `get_audio_main_files()` lists only regular files, converts each path to
  `std::wstring` with `Tools::Utf8Util::utf8toWString()` (from `utf8_util.h`),
  sorts with a wide case-insensitive comparator (`Tools::tolower`), logs the
  sorted list with 1-based click indices, then converts back to UTF-8 for the
  DB insert.
- `delete_button_queue_entry()` archives (except `Ping`) and deletes the active
  row.

### PlaySound (`src/PlaySound.{h,cc}`)

- SDL2_mixer: `SDL_Init(SDL_INIT_AUDIO)`, `Mix_Init(MP3|OGG)`,
  `Mix_OpenAudio(44100, AUDIO_S32SYS, 2, 640)`.
- `Music` (streaming, `Mix_Music`) and `Chunk` (`Mix_Chunk`, WAV) inner classes
  wrap the mixer calls. `run()` keeps two in-memory lists (`m_music`,
  `m_chunks`), advancing the front element when `finished()`.
- Chunk queue has drop rules: within 5 s of a chunk start drop everything newer;
  between 5–30 s drop the current chunk if a newer one is queued; after 30 s
  drop the current chunk. `Chunk::finished()` uses
  `Mix_Playing(channel) == 0` (SDL_mixer returns `1` while playing, `0` when
  done), so finished chunks are popped within one 300 ms poll — don't
  regress this to checking for `-1`, or every chunk lingers in the queue.

### FetchSound (`src/FetchSound.{h,cc}`)

- Every 100 ms: `fetch_music()` (skips if > 1 music queued) then `fetch_chunk()`.
- Each selects the oldest row, hands the file to `PlaySound`, then archives to
  the `P_*` table and deletes the active row.

### PlayAnimation (`src/PlayAnimation.{h,cc}`)

- `Animation` inner class `fork()`s and `execve()`s the command string
  `<python> <file>` (from `[animations].python`). Before forking it calls
  `APP.db.dispose()` (the child must not inherit the DB socket) and closes all
  FDs except stdin/stdout/stderr. `stop()` sends `SIGTERM` and `waitpid()`s.
- `run_once()`: if > 1 animations are queued, stop the front one and pop it; if
  exactly 1, play it. So a new animation replaces the running one.

### FetchAnimation (`src/FetchAnimation.{h,cc}`)

- Every 100 ms selects `PLAY_QUEUE_ANIMATION` ordered by `hist_an_zeit`.
- Skips work if `countAnimationsInQueue() > 1`. If nothing is running, plays
  row 0. If ≥ 2 rows exist, archives row 0 and plays row 1 (swap).

### FetchStats (`src/FetchStats.{h,cc}`)

- Every 1 min recomputes:
  - `totalactions` = sum of `count(idx)` over `P_PLAY_QUEUE_CHUNKS`,
    `P_PLAY_QUEUE_MUSIC`, `P_PLAY_QUEUE_ANIMATION`.
  - `user1..3` = top user by count in `P_BUTTON_QUEUE` for `PLAYSOUND`
    (website) and `ButtonReleased` (physical clicks). Rows without a
    `hist_an_user` are attributed through `USERS_ACTIONS.button_mac_address`.
  - `mostplayedsound` = most frequent `file` basename in `P_PLAY_QUEUE_CHUNKS`.
- Stat keys are the `FetchStats::*` constants (e.g. `TOTAL_ACTIONS =
  "totalactions"`).

### AutoStopFog (`src/AutoStopFog.{h,cc}`)

- Every 10 s checks `CONFIG` for `key='fog' AND value='1'`; if its
  `hist_ae_zeit` is older than 60 s, sets `value='0'`. This is how a fog toggle
  from the web (`change_fog.php`) is reverted automatically; the Python
  `ring_fog.py` also reconciles the physical relay against this value.

## Config sections (`config/`)

Each `ConfigSection*` derives `Configfile2::Section` and registers itself with a
static `SectionKey`. `Configfile2::createDefaultInstaceWithAllModules()` calls
all `registerSection()`. Values are declared with `CONFIG_SIMPLE_DECLARE_STR/INT`
and defaulted in the constructor; the INI file overrides them.

- `ConfigSectionDatabase` — `Host`, `UserName`, `Password`, `Instance`,
  `retry_db_timeout` (defaults: localhost / broker / broker / pope / 60).
- `ConfigSectionNetwork` — `UDPListenPort` (22000).
- `ConfigSectionAnimations` — `python` (`/home/papst/ypy/bin/python`).
- `ConfigSectionPopeReacts` — `answers` (`…/papst_answers.txt`).
- `ConfigSectionGlobal` — empty placeholder.

## db/ layer

- `ThreadedDatabase` (`db/database.{h,cc}`) wraps a `Database` connection with
  thread-safe disposal.
- `db/dbi.{h,cc}` provides `StdSqlSelect`, `StdSqlInsert`, `StdSqlUpdate` and
  the `DBInList`/`DBInArrayList` bind helpers; `%%table%%` in a SELECT string is
  replaced by the row struct's table name.
- `mysql_db.{h,cc}` is the MySQL/MariaDB backend; `oracle_db`/`odbc_db` are
  present but unused by this deployment.

## Sample DB strings (for grepping)

- Select pattern: `select %PLAY_QUEUE_CHUNKS from PLAY_QUEUE_CHUNKS order by hist_an_zeit`
- Archive pattern: copy to `P_*`, `idx=0`, `setHist(HIST_LO, ...)`,
  `delete from <active> where idx = N`.
