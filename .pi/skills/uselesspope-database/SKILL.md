---
name: uselesspope-database
description: How the UselessPope MariaDB tables connect — the broker-owned queue→archive pairs, the one declared USERS_ACTIONS→USERS foreign key, the logical SERMON↔P_PLAY_QUEUE_CHUNKS and BUTTON_QUEUE→USERS_ACTIONS links, and the BF1942 discovery→query→game-archive chain. Use when writing joins, migrations, or a www/UI query over the `papst` database.
---

# UselessPope database — table relationships

The schema lives in the broker repo: `create.sql` is generated from
`src/bindtypes.cc::create_sql()` and **is the authoritative source**. Most tables
are **not** connected by SQL foreign keys; the broker links them logically by a
column value. This skill maps every relationship, its kind, and its direction.

Throughout, every table begins with the six audit columns from `BASE`:
`idx` (auto-increment PK), `hist_an_zeit/hist_an_user`,
`hist_ae_zeit/hist_ae_user`, `hist_lo_zeit/hist_lo_user`.

## Relationship kinds

| Kind | Meaning |
|------|---------|
| **FK** | A real `FOREIGN KEY` constraint emitted by `create_sql()` |
| **Logical** | Join on a column value; no constraint, no cascade |
| **Snapshot** | A row copied into a `P_*` archive (new `idx`, no shared key) |
| **Standalone** | Key/value table with no relationships |

## Table groups

| Group | Tables | Owner |
|-------|--------|-------|
| Live queues (drained by the broker) | `BUTTON_QUEUE`, `PLAY_QUEUE_CHUNKS`, `PLAY_QUEUE_MUSIC`, `PLAY_QUEUE_ANIMATION` | broker |
| Archives (history) | `P_BUTTON_QUEUE`, `P_PLAY_QUEUE_CHUNKS`, `P_PLAY_QUEUE_MUSIC`, `P_PLAY_QUEUE_ANIMATION` | broker |
| People / config | `USERS_ACTIONS`, `CONFIG`, `STATS`, `SERMON` | broker |
| Users | `USERS` (+ `USERPROPERTIES`, `USERRIGHTS`, `STATUS_LEDS`) | `UselessPope-www` (not created by `create.sql`) |
| BF1942 discovery | `BF1942_SERVER` | broker (`Bf1942Discover`) |
| BF1942 live | `BF1942_STATUS`, `BF1942_PLAYER` | broker (`Bf1942Query`) |
| BF1942 history | `BF1942_GAME`, `P_BF1942_PLAYER` | broker (`Bf1942Query`) |

`create.sql` only *references* `USERS` (an index and the foreign key below); it
does not `CREATE TABLE USERS` — that table belongs to `UselessPope-www`.

## Diagram

```
                          ┌──────────────┐
                          │    USERS     │  (www)
                          │ username  PK │
                          └──────┬───────┘
                                 │ FK
                                 │ 1:N
                          ┌──────┴────────────┐
                          │   USERS_ACTIONS   │
                          │ username          │
                          │ button_mac_address│◄──────────┐
                          └───────────────────┘           │
                                                          │ logical (N:1)
   ┌──────────────┐   logical (mac)   ┌──────────────┐    │
   │ BUTTON_QUEUE │──────────────────►│ (via MAC)    │────┘
   └──────┬───────┘                   └──────────────┘
          │ snapshot
   ┌──────▼───────┐
   │P_BUTTON_QUEUE│
   └──────────────┘

   ┌───────────────────┐   snapshot   ┌──────────────────────┐
   │ PLAY_QUEUE_CHUNKS │─────────────►│ P_PLAY_QUEUE_CHUNKS  │
   └───────────────────┘              │ sermon_reaction_idx  │
                                      └──────────┬───────────┘
                                                 │ logical 1:1
                                                 ▼
                                          ┌────────────┐
                                          │   SERMON   │
                                          │ idx        │
                                          └────────────┘

   ┌───────────────┐  ┌────────────────┐  ┌────────────────┐
   │   CONFIG      │  │     STATS      │  │ PLAY_QUEUE_MUSIC │
   │ key/value     │  │ key/value      │  │ PLAY_QUEUE_ANIM. │  (snapshot → P_*)
   └───────────────┘  └────────────────┘  └────────────────┘
```

### BF1942 chain

```
                 Bf1942Discover (UDP 54915 beacon)
                              │ upsert by guid
                              ▼
                    ┌───────────────────┐
                    │   BF1942_SERVER   │
                    │ guid (unique)     │
                    │ ip_address        │◄──────────────┐
                    │ name, last_seen   │               │
                    └─────────┬─────────┘               │
                              │ logical: ip_address     │ logical
                              │ ↔ server_ip             │
             ┌────────────────┼─────────────────┐       │
             ▼                ▼                 ▼       │
   ┌──────────────┐  ┌────────────────┐  ┌──────────────┴──┐
   │BF1942_STATUS │  │ BF1942_PLAYER  │  │  BF1942_GAME    │
   │server_ip 1:1 │  │server_ip N:1   │  │ server_ip N:1   │
   │round state   │  │live roster     │  │ idx = game_id   │
   └──────────────┘  └───────┬────────┘  │ map, winner     │
                             │ snapshot  │ tickets, idle   │
                             │ at round  └────────┬────────┘
                             │ end               │ logical 1:N
                             ▼                   ▼
                    ┌──────────────────────────────────┐
                    │         P_BF1942_PLAYER          │
                    │ game_id → BF1942_GAME.idx        │
                    │ mapname, winning_team (denorm.)  │
                    │ player fields                    │
                    └──────────────────────────────────┘
```

## Relationship catalogue

### Declared foreign key (the only one)

| From | Column | To | Column | Cardinality |
|------|--------|----|--------|-------------|
| `USERS_ACTIONS` | `username` | `USERS` | `username` | N:1 |

Emitted as `ALTER TABLE USERS_ACTIONS ADD FOREIGN KEY (username) REFERENCES
USERS(username)`. `USERS` must exist before that statement runs.

### Logical links (no constraint)

| From | Column | To | Column | Cardinality | Notes |
|------|--------|----|--------|-------------|-------|
| `BUTTON_QUEUE` | `mac_address` | `USERS_ACTIONS` | `button_mac_address` | N:1 | The `Forkey` is **commented out** in `bindtypes.cc`; `FetchButton` resolves it in code. |
| `P_PLAY_QUEUE_CHUNKS` | `sermon_reaction_idx` | `SERMON` | `idx` | 1:1 | Set by `FetchAnswers` to the inserted `SERMON.idx`; `0` = not yet reacted. |
| `P_PLAY_QUEUE_CHUNKS` | `hist_an_user` | `USERS_ACTIONS` / `USERS` | `username` | N:1 | Text value, resolved from the file path/owner; `SERMON.action` also embeds `"<user> hat <title> abgespielt."`. |
| `BF1942_STATUS` | `server_ip` | `BF1942_SERVER` | `ip_address` | 1:1 | One live status row per discovered server. |
| `BF1942_PLAYER` | `server_ip` | `BF1942_SERVER` | `ip_address` | N:1 | Live roster per server. |
| `BF1942_GAME` | `server_ip` | `BF1942_SERVER` | `ip_address` | N:1 | One game per finished round. |
| `P_BF1942_PLAYER` | `game_id` | `BF1942_GAME` | `idx` | N:1 | Archived roster of that game. |

The query response carries **no beacon GUID**, so all BF1942 joins use the IP
(`BF1942_SERVER.ip_address` ↔ `*.server_ip`).

### Snapshot (archive) relationships

The broker never transfers `idx` across an active/archive pair; the `P_*` row is a
byte-copy with a new `idx` and a `HIST_LO` stamp. There is therefore **no key to
join**, only the copied content and timestamps.

| Active (drained) | Archive (history) | Extra columns on archive |
|------------------|-------------------|--------------------------|
| `PLAY_QUEUE_CHUNKS` | `P_PLAY_QUEUE_CHUNKS` | `sermon_reaction_idx` |
| `PLAY_QUEUE_MUSIC` | `P_PLAY_QUEUE_MUSIC` | — |
| `PLAY_QUEUE_ANIMATION` | `P_PLAY_QUEUE_ANIMATION` | — |
| `BUTTON_QUEUE` | `P_BUTTON_QUEUE` | — |
| `BF1942_PLAYER` (live) | `P_BF1942_PLAYER` | `game_id`, `mapname`, `winning_team` |

For BF1942 the snapshot is not a general queue drain: `Bf1942Query` copies the
live roster into `P_BF1942_PLAYER` and clears `BF1942_PLAYER` **when a round
ends** (map change / round-time reset / tickets reset), stamping all rows with
the new `game_id`.

### Standalone

`CONFIG` and `STATS` are independent `key`/`value` tables (settings and derived
counters). They have no links to any other table.

## Audit-column semantics

- `hist_an_*` — when the row was created (a queue arrived, a game was first seen,
  a player first appeared).
- `hist_ae_*` — last modification.
- `hist_lo_*` — when the work completed / the row was archived. In `P_*` tables
  this marks the archive moment; `FetchButton` intentionally preserves the
  original `hist_an_user` and does not re-stamp `hist_an`.

## Example joins

```sql
-- Sermon with the audio file that triggered it
SELECT s.*, c.file
FROM SERMON s
JOIN P_PLAY_QUEUE_CHUNKS c ON c.sermon_reaction_idx = s.idx
ORDER BY s.idx DESC;

-- Actions per user
SELECT a.*, u.*
FROM USERS_ACTIONS a
LEFT JOIN USERS u ON u.username = a.username;

-- Live button events resolved to a user
SELECT b.*, a.username
FROM BUTTON_QUEUE b
LEFT JOIN USERS_ACTIONS a ON a.button_mac_address = b.mac_address;

-- Live BF1942 scoreboard (players seen in the last minute)
SELECT p.player_name, p.slot, p.team, p.score, p.kills, p.deaths, p.ping
FROM BF1942_PLAYER p
WHERE p.server_ip = '10.0.0.7'
  AND p.last_seen >= NOW() - INTERVAL 60 SECOND
ORDER BY p.slot;

-- Server + live status + history
SELECT s.guid, s.name, s.ip_address,
       st.mapname, st.numplayers, st.maxplayers, st.tickets1, st.tickets2
FROM BF1942_SERVER s
LEFT JOIN BF1942_STATUS st ON st.server_ip = s.ip_address;

-- Finished game results
SELECT g.idx AS game_id, g.mapname, g.winning_team, g.tickets1, g.tickets2,
       p.player_name, p.team, p.score, p.kills, p.deaths
FROM BF1942_GAME g
JOIN P_BF1942_PLAYER p ON p.game_id = g.idx
ORDER BY g.idx DESC, p.slot;
```

## Gotchas

- **Only `USERS_ACTIONS→USERS` is enforced.** Everything else is logical; stale or
  missing parent rows will not be rejected. Join with `LEFT JOIN` when in doubt.
- **Deployment order.** Applying `create.sql` on a fresh DB requires `USERS` to
  exist first, or the `FOREIGN KEY` `ALTER` fails.
- **IP is the BF1942 join key.** `BF1942_SERVER` is keyed by `guid`; the query
  tables have `server_ip`, and the query response has no GUID. Correlate discovery
  and query rows via `BF1942_SERVER.ip_address = *.server_ip`.
- **`server_ip` vs `ip_address`.** `BF1942_STATUS`/`BF1942_PLAYER`/`BF1942_GAME`/
  `P_BF1942_PLAYER` use **`server_ip`**; `BF1942_SERVER` (discovery) uses
  **`ip_address`**. Mixing them up is the most common query bug here.
- **Active↔archive has no key.** To reconcile an archived `P_*` row with the live
  one, use the copied content (`file`, `player_name`, `mac`, …) plus
  `hist_an_zeit`/`hist_lo_zeit`.
- **`sermon_reaction_idx` direction.** Only `P_PLAY_QUEUE_CHUNKS` points at
  `SERMON`; `SERMON` does not store a chunk id. `0` means "no reaction yet".
- **`BF1942_PLAYER` is a live snapshot.** Rows for players who left keep an old
  `last_seen`; consumers filter by `last_seen`. On round end the table is cleared
  after its rows are archived into `P_BF1942_PLAYER`.
- **Denormalization in `P_BF1942_PLAYER`.** `mapname` and `winning_team` are
  copied per row (canonical values live in `BF1942_GAME`).
- **Reconnects and player totals.** `BF1942_PLAYER.score`/`kills`/`deaths` are
  cumulative for the current round. A BF1942 client owns its counters, so a
  crash/rejoin reports `0/0/0`; `Bf1942Query` keeps the earlier totals in
  `score_offset`/`kills_offset`/`deaths_offset` and adds the new session on top.
  The offsets are reset with the live roster at round end. They are copied into
  `P_BF1942_PLAYER` for reference; use the effective columns for results.
- **Idle games.** `BF1942_GAME.idle = 1` marks a round whose archived roster had
  no players or almost no activity (total `score` and `kills` both ≤ 20). The
  server ran while nobody was effectively logged in; the row is kept for the
  record but is not a real played game.
- **Not broker-owned:** `USERPROPERTIES`, `USERRIGHTS`, `STATUS_LEDS` exist in
  the live DB but are outside `create.sql`; treat them as `UselessPope-www` /
  `UselessPope-raspi` concerns.
