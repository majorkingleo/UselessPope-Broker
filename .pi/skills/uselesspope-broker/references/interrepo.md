# How the repositories connect

The "Useless Pope" is split across **four** GitHub repositories under
`majorkingleo`, all talking to the same physical Raspberry Pi and the same
MariaDB instance:

| Repo | Language | Role |
|------|----------|------|
| `UselessPope-www` | PHP / jQuery | Web control panel (login, dashboard, buttons). |
| **`UselessPope-Broker`** | C++ | **This repo** — daemon that drains queues and drives audio/animation processes. |
| `UselessPope-raspi` | Python | LED matrix animations + GPIO (fog, ring, brightness, motor). |
| `UselessPope-RemoteButton` | Arduino | Physical remote button that sends UDP datagrams. |

There is **no direct network link between the repos**. They are glued together by
two shared resources on the Raspberry Pi:

1. **The MariaDB database `papst`** (localhost, `/run/mysqld/mysqld.sock`) —
   the single source of truth for queues, config and stats.
2. **`~/.UselessPope-Broker.ini`** — shared DB credentials read by both the
   C++ broker and the Python scripts.

The authoritative schema lives in **this repo** (`create.sql`, printed by
`./broker -create-sql`), and is documented in the `uselesspope-database` skill
inside the `UselessPope-www` repo.

## Shared MariaDB `papst`

Every component writes/reads specific tables. The broker is the **consumer** of
the command queues and the **producer** of the results:

| Table | Written by | Read/consumed by |
|-------|-----------|------------------|
| `BUTTON_QUEUE` | www `play_file.php`; broker `ButtonListener` (from remote button UDP) | broker `FetchButton` |
| `P_BUTTON_QUEUE` | broker `FetchButton` | www (top-user stats via broker `FetchStats`) |
| `PLAY_QUEUE_ANIMATION` | www `change_animation.php`; raspi `ring_animation.py` | broker `FetchAnimation` |
| `P_PLAY_QUEUE_ANIMATION` | broker `FetchAnimation` | broker `FetchStats` |
| `PLAY_QUEUE_CHUNKS` | broker `FetchButton`, `-enqueue-chunk`, raspi `ring.py` | broker `FetchSound` |
| `P_PLAY_QUEUE_CHUNKS` | broker `FetchSound` | broker `FetchAnswers` (→ `SERMON`) |
| `PLAY_QUEUE_MUSIC` | broker `-enqueue-music`, raspi `ring_motor.py` | broker `FetchSound` |
| `P_PLAY_QUEUE_MUSIC` | broker `FetchSound` | broker `FetchStats` |
| `CONFIG` | www (`change_brightness`, `change_fog`, `change_animation`); raspi `config.py`; broker `insert_default_values` | everyone |
| `STATS` | raspi animations; broker `FetchStats` | www `get_uptime.php` |
| `SERMON` | broker `FetchAnswers` | www `log.php` |
| `STATUS_LEDS` | www `sim/` | www `sim/` (not the physical LEDs) |
| `USERS_ACTIONS` | (admin/seed) | broker `FetchButton`; www `get_sound_files.php` |
| `USERS`/`USERRIGHTS`/`USERPROPERTIES` | www `common_login/` | www |

## Broker ↔ UselessPope-www (web → queue → broker → result)

The web panel never runs code on the Pi directly; it just inserts rows:

1. **Sound button** → `lib/play_file.php` inserts `BUTTON_QUEUE`
   (`action=PLAYSOUND`, `file=<path>`, `mac_address` = user's remote-button MAC,
   `ip_address="website"`).
2. **Animation** → `lib/change_animation.php` increments `CONFIG.current_animation`,
   resolves `CONFIG.animation<N>` to the Python file path, inserts
   `PLAY_QUEUE_ANIMATION`.
3. **Fog** → `lib/change_fog.php` toggles `CONFIG.fog`.
4. **Brightness** → `lib/change_brightness.php` adjusts `CONFIG.brightness`
   (read live by the Python animations, not by the broker).

The broker then:

- `FetchButton` drains `BUTTON_QUEUE` → `PLAY_QUEUE_CHUNKS` → `FetchSound` →
  `PlaySound` (SDL2_mixer) → speakers.
- `FetchAnimation` drains `PLAY_QUEUE_ANIMATION` → `PlayAnimation` →
  `fork/execve <python> <file>`.
- `FetchAnswers` watches `P_PLAY_QUEUE_CHUNKS` and writes a witty `SERMON` line
  ("`<user>` hat `<sound>` abgespielt." → "Der Papst meint dazu: …").

Back to www:

- `log.php` renders `SERMON` (the "Protokoll der Predigt" iframe).
- `lib/get_uptime.php` renders `STATS` (uptime, rotations, top users,
  most-played sound, brightness) into the dashboard.
- The footer links to all four repos with line counts (`lib/lines_of_code.php`).

## Broker ↔ UselessPope-raspi (broker spawns the Python animations)

The broker is the **process manager** for the LED animations. It does not import
or link Python; it spawns the scripts as child processes:

- `CONFIG.animation<N>` stores absolute paths like
  `/home/papst/UselessPope-raspi/python/pope_default_rotating_color_wheel.py`.
  `PlayAnimation` builds `<[animations].python> <CONFIG.animation<N>>` and
  `execve`s it.
- The Python scripts then connect to the **same** `papst` DB (via
  `python/config.py`, which reads the same `~/.UselessPope-Broker.ini`) to poll
  `CONFIG.brightness`/`scroll_text` and write `STATS` counters
  (`umdrehungen`, `frequenz`, `snake_p1_wins`, …).

Reverse direction — raspi scripts call **into** the broker binary:

- `python/ring.py` shells out to
  `/home/papst/UselessPope-Broker/broker -enqueue-chunk <file>`.
- `python/ring_motor.py` shells out to
  `/home/papst/UselessPope-Broker/broker -enqueue-music <file>`.
- `python/ring_animation.py` inserts `PLAY_QUEUE_ANIMATION` directly (raw SQL)
  — the same table the broker drains.

This is why `python/config.py` and the broker share the INI file: both need the
same `papst` database to coordinate.

## Broker ↔ UselessPope-RemoteButton (UDP)

The Arduino remote button sends plain-text UDP datagrams to the broker's
`ButtonListener` (port 22000 by default):

```
TIME=650922;SEQ=161;MAC=D8:BC:38:FA:EF:20;IP=192.168.1.137;ACTION=ButtonPressed
```

The broker maps `MAC` → `USERS_ACTIONS.button_mac_address` to identify the user,
then plays their configured sound (`FetchButton`). `ACTION=Ping` is a heartbeat
and is discarded. This is the *only* network listener in the whole stack.

## End-to-end example: a guest presses "Hallelujah"

1. Browser → `lib/play_file.php` → `INSERT INTO BUTTON_QUEUE (action='PLAYSOUND', file=..., hist_*_user='<username>')`.
2. Broker `FetchButton` → resolves `USERS_ACTIONS` by `hist_an_user` → inserts
   `PLAY_QUEUE_CHUNKS (file=<audio_main|audio_random path>)` → archives
   `P_BUTTON_QUEUE`.
3. Broker `FetchSound` → `PlaySound.play_chunk()` → SDL2_mixer plays the WAV →
   archives `P_PLAY_QUEUE_CHUNKS`.
4. Broker `FetchAnswers` (≤ 1 min later) → matches the file title against
   `papst_answers.txt` → inserts `SERMON`.
5. Web `log.php` iframe shows the pope's reaction.

## When adding a feature, which repo to touch

| Change | Repo |
|--------|------|
| New web button / dashboard stat | `UselessPope-www` |
| New LED animation script | `UselessPope-raspi` (+ register `animation<N>` here in `src/main.cc` or `CONFIG`) |
| New queue-consumer, audio behaviour, reaction logic, UDP handling | **`UselessPope-Broker`** |
| New table / column / config key | **`UselessPope-Broker`** (`create.sql` + `bindtypes.h`), then update the other repos + `uselesspope-database` skill |
| Physical button behaviour | `UselessPope-RemoteButton` |
