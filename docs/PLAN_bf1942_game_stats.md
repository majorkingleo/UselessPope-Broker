# PLAN: automatic per-game statistics (round archive)

> Status: implemented (unit = one round; winner from final tickets with
> `winning_team = 0` fallback; denormalized map/winner in `P_BF1942_PLAYER`;
> always-on; 15 s poll).
> Goal: keep `BF1942_PLAYER` as the **live** scoreboard, and automatically
> archive every finished game into `P_BF1942_PLAYER` with a game id, the map,
> the player fields and the winning team. The final result is **mandatory** —
> every completed round must end up archived exactly once.

---

## 1. Objective

When a BF1942 round ends, snapshot the final per-player state into an archive
table, together with the map and the winning team, so `UselessPope-www` can
render historical game results.

Today the broker only keeps the **live** snapshot (`BF1942_STATUS` /
`BF1942_PLAYER`). It does not record round boundaries, the map per round, or the
winner.

## 2. Terminology & assumptions

- A **game = one round** (one played map). In conquest this is the natural unit:
  a round ends when one team's tickets reach 0 (or the time limit hits), a team
  wins, and the map then rotates.
- The query protocol gives the round state as `tickets1`, `tickets2`,
  `roundTimeRemain` and `status`. These are **currently dropped**; this plan
  starts storing them because they are the round-boundary/winner signals.
- Polling is every `BF1942QueryInterval` (default 15 s). Round detection is
  therefore **sampled**, not event-precise (see §8).

## 3. Current state (relevant bits)

- `BF1942_STATUS` (one row per server, keyed by `server_ip`): `server_ip`,
  `hostname`, `mapname`, `gametype`, `hostport`, `numplayers`, `maxplayers`,
  `last_seen`.
- `BF1942_PLAYER` (one row per server+player): `server_ip`, `player_name`,
  `slot`, `score`, `kills`, `deaths`, `ping`, `team`, `last_seen`.
- `Bf1942Query::query_and_store()` polls `\status\` + `\players\`, then upserts
  STATUS and PLAYERS. No round concept, no history, no winner.

## 4. Decisions (recommendations)

1. **Game = round on one map.** One `BF1942_GAME` header row per round; one
   `P_BF1942_PLAYER` row per player of that round.
2. **`BF1942_PLAYER` stays the live table.** It is cleared (archive-then-delete)
   when a new round starts, so each round accumulates a fresh roster.
3. **Round end is detected by comparing against the previous snapshot**
   (read the old `BF1942_STATUS` row before overwriting it — persistent across
   restarts, no extra in-memory state).
4. **Winner from tickets** — `tickets1` vs `tickets2` of the *last* snapshot of
   the ended round. Fall back to `winning_team = 0` (draw/unknown) if tickets
   are missing/equal; the archive row is still written (**result is mandatory**).
5. **Mandatory-result guarantee via ordering + transaction.** Finalize-and-archive
   happens **before** the new round's STATUS is overwritten and before the new
   players are upserted, and everything is committed once. If anything throws,
   STATUS is left unchanged, so the next poll re-detects the same transition and
   retries — a round is never silently dropped.
6. **Archive convention** (same as the other `P_*` tables): copy the active
   player row into `P_BF1942_PLAYER` (reset `idx` to 0, stamp `HIST_LO`), then
   `DELETE` it from `BF1942_PLAYER`.

## 5. Data model

### 5.1 Extend `BF1942_STATUS` (live, +5 columns)

```
round_time_remain  INT             // seconds left; resets => new round
tickets1           INT             // team 1 tickets
tickets2           INT             // team 2 tickets
status             INT             // GameSpy status field (optional signal)
round_started_at   DATETIME        // when the current round's map first appeared
```

### 5.2 New `BF1942_GAME` (header — one row per finished round)

```
server_ip    VARCHAR(15)
mapname      VARCHAR(64)
gametype     VARCHAR(32)
started_at   DATETIME
ended_at     DATETIME
winning_team INT             // 1 or 2; 0 = draw/unknown
tickets1     INT             // final observed
tickets2     INT
victory_type VARCHAR(16)     // "tickets" | "time" | "unknown" (best effort)
+ BASE audit columns
```

`idx` (auto-increment) is the **game id**.

### 5.3 New `P_BF1942_PLAYER : public BF1942_PLAYER` (archive)

Inherits all live player fields, adds:

```
game_id      INT             // -> BF1942_GAME.idx
mapname      VARCHAR(64)     // denormalized (requested)
winning_team INT             // denormalized (requested)
```

Indexes: `(game_id)` and `(game_id, player_name)`.

Denormalizing `mapname`/`winning_team` into the player rows is deliberate: it
keeps the www query trivial. The canonical copy is in `BF1942_GAME`.

## 6. Detection & finalization algorithm

Per target, inside `Bf1942Query::query_and_store()` (currently upsert-only):

1. `fields = fetch_snapshot(host, port, timeout)` — if no response, log and return.
2. Read the **old** `BF1942_STATUS` row for `server_ip` (if any).
3. Compute `round_ended`:
   - `old` exists AND `old.mapname` is non-empty AND one of:
     - `fields.mapname != old.mapname` (map rotation), **or**
     - `fields.roundTimeRemain > old.round_time_remain + margin` (round restarted
       on the same map), **or**
     - tickets reset: `old.tickets1 + old.tickets2` ≈ 0 and
       `fields.tickets1 + fields.tickets2` large.
4. If `round_ended`:
   a. Determine `winning_team` from **`old`**: `tickets1 > tickets2` → 1,
      `tickets2 > tickets1` → 2, else 0. `victory_type` from whether a team hit 0
      (`tickets`) or time ran out (`time`), else `unknown`.
   b. `INSERT` a `BF1942_GAME` row (server_ip, `old.mapname`, gametype,
      `old.round_started_at` → `started_at`, now → `ended_at`, winning_team,
      `old.tickets1/2`, victory_type). Read back the auto `idx` = `game_id`.
   c. `SELECT` all `BF1942_PLAYER` rows for `server_ip`; for each, `INSERT` into
      `P_BF1942_PLAYER` with `game_id`, `mapname = old.mapname`,
      `winning_team`, copied player fields, `idx` reset to 0, `HIST_LO` stamped.
   d. `DELETE FROM BF1942_PLAYER WHERE server_ip = ...` (new round starts empty).
5. Upsert `BF1942_STATUS` with the **new** fields; set `round_started_at = now()`
   if this is a new round, otherwise keep `old.round_started_at`.
6. Upsert the new round's players (from `fields`).
7. `commit()` once.

Ordering is the point: archive (4) strictly before overwrite (5) and player
upsert (6), so the archive captures only the *ended* round's roster and final
values.

On the very first poll for a server (`old` missing) there is nothing to
finalize — just insert.

## 7. Winner / team details

- `team_<n>` is 1 or 2; `tickets1`/`tickets2` map to those team ids. Winner is
  the team with more tickets at the end of the round.
- Optional: also store `teamname_0`/`teamname_1` (axis/allied) into `BF1942_GAME`
  when the server reports them, for human labels. Not required for the winner.
- Polling caveat: the "final" tickets are the last observed before the reset.
  If a round ends and the next map starts within one poll window, the previous
  snapshot's tickets are used — usually correct, but a 15 s window can see a
  team's tickets tick a few points off. Acceptable; see §8.

## 8. Robustness & gotchas

- **Mandatory result.** Because finalization (4) precedes the STATUS overwrite
  (5) and shares one `commit()`, a failure leaves the transition undetected, so
  the next poll retries. No round is dropped, only deferred.
- **Restart.** `BF1942_STATUS` persists, so a restart mid-round does not invent
  a round end (same map → no transition). If a round ended while the broker was
  down, the first poll sees a map change and archives the stale-but-present
  `BF1942_PLAYER` rows with their last-known scores.
- **Polling granularity.** A round that starts *and* ends entirely between two
  polls is invisible. Lower `BF1942QueryInterval` (e.g. 5 s) if needed; this
  raises DB write volume.
- **Winner unknown fallback.** If tickets are missing/equal, archive with
  `winning_team = 0`. The row is still written (result mandatory), the result is
  just marked unknown.
- **`status` field.** Its numeric enum isn't decoded; treat it as a secondary
  signal only, never the sole trigger.
- **Long names / duplicate names** — same constraints as the live table (keyed
  by `(server_ip, player_name)`; `slot` preserved for order).
- **Schema sync** with the `uselesspope-database` skill in `UselessPope-www`
  (the DB user needs SELECT/INSERT/DELETE on the new/changed tables).

## 9. File-by-file changes

- `src/bindtypes.h` — add `round_time_remain/tickets1/tickets2/status/round_started_at`
  to `BF1942_STATUS`; add `BF1942_GAME` struct; add `P_BF1942_PLAYER :
  BF1942_PLAYER` struct (with `game_id`, `mapname`, `winning_team`).
- `src/bindtypes.cc` — constructors; `create_sql()` entries + indexes
  (`idx_BF1942_GAME_server_ip`, `idx_P_BF1942_PLAYER_game`,
  `idx_P_BF1942_PLAYER_game_player`).
- `create.sql` — regenerated block (equals `./broker -create-sql`).
- `src/Bf1942Query.cc` — `store_status()` gains the round-state fields and the
  compare-before-overwrite logic; new `finalize_round(old, now)` performing
  steps 4a–4d; `query_and_store()` reordered per §6. Possibly a
  `store_round_state()` helper.
- `src/main.cc` — no change (no new thread/config).
- `.pi/skills/uselesspope-broker/SKILL.md` — mention the round archive tables and
  the new STATUS columns.
- `UselessPope-www` — schema skill update; consume `BF1942_GAME` +
  `P_BF1942_PLAYER` (join on `game_id`).

No new config keys are strictly required (the feature is always on). Optional:
a `BF1942ArchiveEnabled` kill-switch.

## 10. Build & verify

```bash
./reconfigure.sh && make
./broker -create-sql | grep -A16 BF1942_GAME
./broker -create-sql | grep -A16 P_BF1942_PLAYER
mysql < create.sql   # additive ALTER on BF1942_STATUS + 2 new tables
```

Automated checks (loopback responder, like the query plan's tests):

- Two successive polls with the same map → no `BF1942_GAME` row.
- Poll with a different map (and non-zero old tickets) → one `BF1942_GAME` row
  (correct map, `winning_team` from tickets) + one `P_BF1942_PLAYER` row per
  player, and the live `BF1942_PLAYER` cleared/rebuilt for the new map.
- Tickets `0` vs `200` → winner = team 2; `200` vs `0` → team 1; equal → 0.
- Round restart on the same map (`roundTimeRemain` increase) → finalize.
- Induce a DB error during finalize → STATUS not overwritten, next poll retries.

## 11. Acceptance criteria

- [ ] `BF1942_PLAYER` remains the live roster; it is reset at each new round.
- [ ] Every completed round produces exactly one `BF1942_GAME` row and one
      `P_BF1942_PLAYER` row per player, with `game_id`, `mapname`, `winning_team`.
- [ ] `winning_team` is correct from the final tickets; unknown only when tickets
      are absent/equal (and still archived).
- [ ] A round that ends while the broker is down is recovered on the next poll.
- [ ] `./broker -create-sql` matches `create.sql`.
- [ ] No build warnings; `make` clean.

## 12. Open questions

1. **Unit = round** (recommended) vs. a full match spanning map rotations?
2. **Winner fallback** — keep `winning_team = 0` for unknown, or require tickets
   to be present and skip archiving otherwise (contradicts "mandatory")?
3. **Poll interval** — keep 15 s (accept imprecise tickets) or lower to ~5 s for
   tighter end-of-round capture?
4. **Denormalize `mapname`/`winning_team` into `P_BF1942_PLAYER`** (as requested)
   vs. join-only via `game_id`. Plan assumes denormalized.
5. **Optional kill-switch** `BF1942ArchiveEnabled` — needed, or always-on is fine?
