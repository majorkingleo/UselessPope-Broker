# PLAN: BF1942 live query (GameSpy/ASE status) in the broker

> Status: proposed plan. Reference implementation:
> `/home/papst/bf1942-adjust-monitor-settings/src_bf1942_query/bf1942_query.cc`
> and `/home/papst/bf1942-adjust-monitor-settings/docs/network-protocol.md` §5.
>
> Companion plan: `docs/PLAN_bf1942_discovery.md` (passive LAN beacon discovery).
> Discovery records *which* servers exist; this plan records *what is happening on
> them* (live scoreboard).

---

## 1. Objective

Extend the broker with a periodic **GameSpy/ASE query client** so it polls
Battlefield 1942 servers and records the live scoreboard — map, gametype, player
count and per-player **score / kills / deaths / ping / team** — in MariaDB for
`UselessPope-www` to render.

The standalone `bf1942_query` does exactly this as a CLI; this plan moves it into
the broker as a background thread.

## 2. What we integrate (facts from the reference)

`bf1942_query` sends the UDP request `\status\` to `<host>:<port>` and reads a
backslash-delimited `\key\value\...` response (possibly split over several
datagrams), terminated by `\final\` and/or `\queryid\<n.m>`.

Verified fields:

| Field | Meaning |
|-------|---------|
| `hostname`, `hostport` | server name, game port (14567) |
| `mapname`, `gametype` | current map / mode |
| `numplayers`, `maxplayers` | player counts |
| `tickets1`, `tickets2`, `roundTimeRemain` | round state |
| `playername_<i>` | **note the spelling** — not `player_<i>` |
| `score_<i>`, `kills_<i>`, `deaths_<i>`, `ping_<i>`, `team_<i>` | per player |
| `teamname_<i>` | team labels |

It exposes **live score / kills / deaths / ping / team**, but **not shots fired**
and **not history** — that lives only in the server event log
(`stats_collector/`).

Gotchas verified in §5 of the protocol doc:

- The player field is `playername_<n>`, and **`numplayers` can be smaller than the
  number of listed players** — scan `playername_<i>` until the first gap.
- `score` can be negative (a suicide is −2); `ping` is 0 until measured.
- The reference server (MK-VII) answers on **UDP 22000** (All-Seeing Eye port),
  not the classic GameSpy 23000; a server with querying disabled times out.
- The response may arrive in multiple datagrams; accumulate until `\final\` /
  `\queryid\`.
- **Not every server returns players in `\status\`.** The live server
  (`lan 2.1`, FH mod) answers `\status\` with server + rules only and exposes
  the player list under `\players\`. The client requests **both** and merges the
  player fields, so servers that include players in `\status\` (the reference
  server) keep working.

## 3. Decisions (recommendations)

1. **Storage:** a live **snapshot registry**, not a per-poll event log (a row per
   poll per player would grow unboundedly). Two new tables:
   - `BF1942_STATUS` — one row per server (keyed by `server_ip`), refreshed each
     poll: hostname, map, gametype, hostport, numplayers, maxplayers, last_seen.
   - `BF1942_PLAYER` — one row per (server, player name), refreshed each poll:
     score, kills, deaths, ping, team, slot, last_seen.
2. **Targets:** configurable hosts **plus** automatic targets from the discovery
   registry (`BF1942_SERVER.ip_address`). Empty static list ⇒ query everything
   discovery has seen. This reuses the companion plan with no duplicated config.
3. **Config placement:** extend the existing `[network]` section (consistent with
   `BF1942DiscoverPort` / `BF1942DiscoverEnabled`); no new section/file.
4. **Enabled by default?** Yes (`true`), with a kill-switch, matching discovery.
5. **Offline/stale handling:** the broker only records `last_seen`; it does not
   delete rows. `UselessPope-www` decides what is "live" from `last_seen` age
   (same policy as `BF1942_SERVER`).
6. **Linking:** attach query data to a discovered server by **IP** (the query
   response has no beacon GUID). `BF1942_STATUS.server_ip` /
   `BF1942_PLAYER.server_ip` are the join key.

## 4. Integration approach (broker idioms)

- New `BasicThread` subclass `Bf1942Query`, started from `main.cc` (like
  `Bf1942Discover` / `ButtonListener`).
- One thread polls all targets sequentially; each query is bounded by a timeout so
  one dead server cannot stall the cycle.
- POSIX UDP sockets ported directly from `bf1942_query.cc` (Linux only — the
  broker's target), since the reference code is already compact and proven. No
  new build dependency.
- DB access through the global `APP.db` with a dispose token.
- Follow the schema-sync rule (`create.sql`, `create_sql()`, `uselesspope-database`
  skill in `UselessPope-www`).
- Use the async logger (`common/logging.*`): a failed/timed-out query is a
  `CPPDEBUG` line, never an uncaught exception.

## 5. File-by-file changes

### 5.1 `src/bindtypes.h`

Add length constants:

```cpp
const unsigned BF1942_HOSTNAME_LEN = 128;
const unsigned BF1942_MAPNAME_LEN  = 64;
const unsigned BF1942_GAMETYPE_LEN = 32;
const unsigned BF1942_PLAYER_NAME_LEN = 64;
```

Add two structs (`BASE` gives the six `hist_*` audit columns):

```cpp
class BF1942_STATUS : public BASE
{
public:
	DBTypeVarChar	server_ip;     // query target IP (join key)
	DBTypeVarChar	hostname;
	DBTypeVarChar	mapname;
	DBTypeVarChar	gametype;
	DBTypeInt		hostport;
	DBTypeInt		numplayers;
	DBTypeInt		maxplayers;
	DBTypeDateTime	last_seen;
	BF1942_STATUS();
	// copy ctor + operator= mirroring the other structs
};

class BF1942_PLAYER : public BASE
{
public:
	DBTypeVarChar	server_ip;     // join key to BF1942_STATUS / BF1942_SERVER
	DBTypeVarChar	player_name;
	DBTypeInt		slot;          // scoreboard index <i>
	DBTypeInt		score;         // may be negative
	DBTypeInt		kills;
	DBTypeInt		deaths;
	DBTypeInt		ping;
	DBTypeInt		team;
	DBTypeDateTime	last_seen;
	BF1942_PLAYER();
	// copy ctor + operator= mirroring the other structs
};
```

### 5.2 `src/bindtypes.cc`

- Constructors (`BASE("BF1942_STATUS", this)` / `BASE("BF1942_PLAYER", this)`),
  with the column lengths above.
- In `create_sql()`: declare and emit both tables, plus lookup indexes:

```cpp
s += "ALTER TABLE `BF1942_STATUS` ADD INDEX IF NOT EXISTS `idx_BF1942_STATUS_server_ip`(`server_ip`);\n";
s += "ALTER TABLE `BF1942_PLAYER` ADD INDEX IF NOT EXISTS `idx_BF1942_PLAYER_server_player`(`server_ip`,`player_name`);\n";
```

### 5.3 `create.sql`

Add the two `CREATE TABLE` blocks (must equal `./broker -create-sql`). Columns
follow `BASE` then the fields above, matching the existing generated style.

### 5.4 `config/ConfigNetwork.h` / `.cc`

Extend the `network` section:

```cpp
CONFIG_SIMPLE_DECLARE_BOOL( BF1942QueryEnabled );
CONFIG_SIMPLE_DECLARE_INT(  BF1942QueryPort );      // default 22000 (ASE)
CONFIG_SIMPLE_DECLARE_INT(  BF1942QueryInterval );  // seconds, default 15
CONFIG_SIMPLE_DECLARE_INT(  BF1942QueryTimeout );   // seconds, default 3
CONFIG_SIMPLE_DECLARE_STR(  BF1942QueryHosts );     // "host[:port],..." ; empty = auto
```

Defaults: `enabled=true`, `port=22000`, `interval=15`, `timeout=3`,
`hosts=""` (auto-query discovered servers).

### 5.5 `src/Bf1942Query.h` / `src/Bf1942Query.cc` (new)

```cpp
#pragma once
#include "BasicThread.h"
#include <string>
#include <vector>

class Bf1942Query : public BasicThread
{
public:
	struct Target { std::string host; int port; };

private:
	int  m_default_port;
	int  m_interval_s;
	int  m_timeout_s;
	std::vector<Target> m_static_targets;

public:
	Bf1942Query( int default_port, int interval_s, int timeout_s,
	             const std::string & hosts );
	void run() override;

private:
	std::vector<Target> targets_from_db();
	void query_and_store( const Target & target );
	void store_status( const std::string & server_ip,
	                   const std::map<std::string,std::string> & fields );
	void store_players( const std::string & server_ip,
	                    const std::map<std::string,std::string> & fields );
};
```

Key pieces (port of `bf1942_query.cc`, Windows branches dropped):

- `parse_fields()` — verbatim backslash parser.
- `query_fields(host, port, timeout_s, request)` — one UDP request, accumulate
  datagrams until `\final\`/`\queryid\` or the `SO_RCVTIMEO` timeout; return the
  field map, or `std::nullopt` on timeout/no response.
- `fetch_snapshot(host, port, timeout_s)` — sends `\status\` (server + rules),
  then `\players\` (player rows), and folds the player fields in. Some server
  builds answer only server/rules for `\status\`; others include players there.
- `run()` — loop on `APP.quit_request`; each cycle build the target list
  (static ∪ `SELECT ip_address FROM BF1942_SERVER`), then `query_and_store()` each.
  Sleep `m_interval_s` in small slices so quit stays responsive.
- `query_and_store()` — parse fields → `store_status()` + `store_players()`;
  wrap in `try/catch` so a DB error is logged, never fatal.
- `store_players()` — `cap = maxplayers > 0 ? maxplayers : 128`; iterate `i` until
  `playername_<i>` is empty; upsert by `(server_ip, player_name)`.
- Upsert = `SELECT` by key → `UPDATE` (refresh fields + `last_seen`, stamp
  `HIST_AE`) or `INSERT` (stamp `HIST_AN|HIST_AE|HIST_LO`, user `"broker"`).
  Use the bind layer so player names are escaped. Commit once per target.

`targets_from_db()` fetches only one column, so it must **not** use the
`%TABLE` / `DBInList<DBBindType>()` struct bind (that binds the full column list
positionally and would mismatch a single-column SELECT). Use the raw per-thread
select instead:

```cpp
auto erg = APP.db->select( "SELECT `ip_address` FROM `BF1942_SERVER`" );
if( !erg ) return {};
std::vector<Target> out;
for( const auto & row : erg.row_list.values )   // vector<vector<string>>
    if( !row.empty() && !row[0].empty() )
        out.push_back( { row[0], m_default_port } );
return out;
```

`BF1942QueryHosts` parsing: split on `,`, then on the last `:` if present
(`host` / `host:port`), trim whitespace, default a missing port to
`m_default_port`, and dedupe against the discovered list so a host configured
both ways is queried once.

### 5.6 `src/main.cc`

```cpp
#include "Bf1942Query.h"

threads.emplace_back([&cfg_net]() {
	if( !cfg_net.isBF1942QueryEnabled() ) {
		return;
	}
	auto token = APP.db.get_dispose_token();
	Bf1942Query query( cfg_net.BF1942QueryPort,
	                   cfg_net.BF1942QueryInterval,
	                   cfg_net.BF1942QueryTimeout,
	                   cfg_net.BF1942QueryHosts );
	query.run();
});
```

### 5.7 `Makefile.am`

Add `src/Bf1942Query.h` / `src/Bf1942Query.cc` to `broker_SOURCES`. No new
`LDADD` (POSIX sockets need no extra library).

### 5.8 Optional CLI (recommended for testing)

Add a one-shot `-query-bf1942 HOST[:PORT]` flag that reuses the parser, prints the
scoreboard (or `-csv`) and exits — the same convenience as `-enqueue-*`. Useful
for verifying a server without starting the daemon.

### 5.9 `.pi/skills/uselesspope-broker/SKILL.md`

- Bump `## The ten worker threads` → `## The eleven worker threads` and add:

  ```
  | BF1942 query | `Bf1942Query` | Polls configured/discovered servers' GameSpy/ASE UDP port (`[network].BF1942QueryPort`, default 22000), parses the `\status\` scoreboard and upserts `BF1942_STATUS` + `BF1942_PLAYER` (keyed by `server_ip`). Disable with `[network].BF1942QueryEnabled=0`. |
  ```

- Add to the config table:

  ```
  | `network` | `BF1942QueryEnabled` | `true` | enable/disable the query thread |
  | `network` | `BF1942QueryPort` | `22000` | GameSpy/ASE query port |
  | `network` | `BF1942QueryInterval` | `15` | seconds between polls |
  | `network` | `BF1942QueryTimeout` | `3` | per-query UDP timeout |
  | `network` | `BF1942QueryHosts` | `""` | `host[:port],...`; empty = auto-query discovered servers |
  ```

- Mention `BF1942_STATUS` / `BF1942_PLAYER` in the schema-sync bullet.

## 6. Query / parse details

```
request : \status\                        (8 bytes, UDP)
response: \gamename\bfield1942\gamever\v1.61\hostname\MK-VII\hostport\14567\
          mapname\Bocage\numplayers\3\maxplayers\32\gametype\conquest\...
          \playername_0\[TK]LieutenantORF\score_0\0\kills_0\0\deaths_0\0\ping_0\0\team_0\2\
          ...
          \final\\queryid\202.1
```

Parsing rules:

1. Find the first `\` (some implementations prefix a byte), then split on `\` into
   `key,value` pairs.
2. A trailing key with no value is stored with an empty value.
3. Accumulate datagrams until `\final\` or `\queryid\` is seen, or the timeout.
4. Ignore keys that are not `playername_*`/`*_<n>` when building player rows.
5. Request `\status\` **and** `\players\`; merge the player fields only when the
   `\players\` reply contains `playername_0` (see §2).

## 7. Upsert logic

Per target / cycle:

- `BF1942_STATUS` keyed by `server_ip`:
  1. `SELECT ... WHERE server_ip = '<ip>'` — `escape()` the IP even though it
     normally comes from a trusted source (config/discovery).
  2. found → update hostname/mapname/gametype/hostport/numplayers/maxplayers,
     `last_seen = now()`, `HIST_AE`, `UPDATE`
  3. absent → new row, `HIST_AN|HIST_AE|HIST_LO` = `"broker"`, `INSERT`
- `BF1942_PLAYER` keyed by `(server_ip, player_name)`:
  - `SELECT ... WHERE server_ip = '<ip>' AND player_name = '<name>'` — `escape()`
    both values (player names are untrusted).
  - same upsert; `slot` = `<i>` for ordering, score/kills/deaths/ping/team from the
    fields, `last_seen = now()`.
- `commit()` once per target.

Players who left keep the row with an old `last_seen`; consumers filter
`last_seen >= now() - 2*interval`. (Alternative: delete the server's rows not in
the current snapshot for a strict live roster — noted as an option, not default.)

When `numplayers = 0` the server row still refreshes, but no `BF1942_PLAYER`
upsert runs — the stale player rows stay until the consumer filters them by
`last_seen`.

## 8. Design notes / gotchas

- **Port collision (important).** The broker's `ButtonListener` binds
  `[network].UDPListenPort` = **22000** to `0.0.0.0` **without** `SO_REUSEADDR`.
  If the BF1942 server runs on the *same* host, its ASE/query server also binds
  UDP 22000 → the two cannot coexist (a different port is required, not just
  address reuse). The query *client* only uses an ephemeral source port, so a
  *remote* server is unaffected — verify before enabling.
- **Port default.** The reference tool defaults to 23000, but the verified server
  answers on 22000 (ASE). Use 22000 as the broker default and keep it configurable.
- **Split responses.** Always accumulate until `\final\`/`\queryid\`; a single
  `recvfrom` is not guaranteed to hold the whole reply.
- **`numplayers` is unreliable** — scan `playername_<i>` until the first gap.
- **Negative score / zero ping** are valid.
- **Duplicate player names** on one server would merge under the
  `(server_ip, player_name)` key; `slot` is stored for order. Acceptable edge
  case; note it for the www side.
- **No GUID in the query response** — link to discovery by IP
  (`BF1942_STATUS.server_ip` ↔ `BF1942_SERVER.ip_address`).
- **Timeouts are normal** (query disabled or wrong port): log at `CPPDEBUG`, skip
  the target, keep the loop running.
- **Untrusted strings** (hostnames, player names) go through the DB bind layer
  (`StdSql*`), so escaping is handled; use `escape()` for any hand-written WHERE.
- **DB errors** must not escape the Asio/thread loop: catch per target, log, and
  continue (same rule as `Bf1942Discover`).
- **Schema sync:** add the two tables to the `uselesspope-database` skill in
  `UselessPope-www`; the DB user needs SELECT/INSERT/UPDATE on them.
- **Low cost:** default 15 s interval, 3 s timeout, tiny UDP payloads, sequential
  queries, no new dependencies, negligible CPU.
- **Timestamp helper.** `last_seen` must use the same `"YYYY-MM-DD HH:MM:SS"`
  format `BASE::setHist()` writes. `Bf1942Discover.cc` already has a local
  `now_string()`; factor it into a shared helper (e.g. in `bindtypes.*` or
  `common/`) instead of duplicating it a third time.
- **`BF1942_STATUS.hostname` duplicates `BF1942_SERVER.name`.** Either drop it and
  join on `server_ip`, or keep STATUS self-contained — decide and document.
- **Fields deliberately not stored:** `status`, `teamname_<i>`, `tickets1/2`,
  `roundTimeRemain`, `gamename`, `gamever`. Add `status`/`teamname` if the www
  scoreboard needs them.
- **Player-name length.** Confirm BF1942's actual name limit before fixing
  `BF1942_PLAYER_NAME_LEN = 64` (truncation would merge distinct players).
- **Discovery dependency.** `targets_from_db()` requires the `BF1942_SERVER`
  table (discovery plan). If it is missing, the `SELECT` throws; it is caught per
  cycle, but the dependency must be documented.
- **Index uniqueness.** The `(server_ip, player_name)` index is non-unique to
  match the existing `ADD INDEX IF NOT EXISTS` style (safe given the single query
  thread). A `UNIQUE` index would make the upsert self-defending — verify
  MariaDB's `ADD UNIQUE INDEX IF NOT EXISTS` support if chosen.

## 9. Build & verify

```bash
./reconfigure.sh && make
./broker -create-sql | grep -A14 BF1942_STATUS
./broker -create-sql | grep -A14 BF1942_PLAYER
# once the tables exist (mysql < create.sql or update_db.sql):
./broker -query-bf1942 10.0.0.7:22000     # one-shot, if the CLI flag is added
mysql -u papst -p papst -e "SELECT * FROM BF1942_STATUS; SELECT * FROM BF1942_PLAYER ORDER BY slot;"
```

Automated checks:

- Unit-test `parse_fields()` against the sample response in §6 (including a
  `playername_0` with spaces and a negative score).
- DB round-trip test (insert → `%TABLE` select → update → cleanup) like the
  discovery plan's `dbtest`.
- End-to-end: run `Bf1942Query` against a loopback UDP responder that replays a
  captured response, assert `BF1942_STATUS`/`BF1942_PLAYER` rows.

## 10. Acceptance criteria

- [ ] With `BF1942QueryEnabled=false`, the broker behaves exactly as before.
- [ ] A reachable server produces one `BF1942_STATUS` row and one `BF1942_PLAYER`
      row per listed player; `last_seen` refreshes each poll.
- [ ] `numplayers` smaller than the listed player count does not truncate players.
- [ ] Negative scores and zero pings are stored verbatim.
- [ ] An unreachable/disabled-query server logs once and the loop continues.
- [ ] `server_ip` joins `BF1942_STATUS`/`BF1942_PLAYER` to `BF1942_SERVER.ip_address`.
- [ ] `./broker -create-sql` matches `create.sql` for the new tables.
- [ ] No build warnings; `make` clean.

## 11. Open questions

1. **Static targets vs. discovery-only.** Recommend discovery-driven with an
   optional static `BF1942QueryHosts` override. Confirm.
2. **Roster semantics.** Keep departed players (filter by `last_seen`) vs. delete
   rows not present in the latest poll. Recommend keep + `last_seen`.
3. **Query port default** 22000 (ASE, verified) vs. 23000 (classic GameSpy).
   Recommend 22000, configurable.
4. **Poll interval** 15 s default; lower increases DB write volume, higher makes
   the www scoreboard laggier.
5. **Co-located server?** If BF1942 runs on this Pi, resolve the 22000 collision
   before enabling.
