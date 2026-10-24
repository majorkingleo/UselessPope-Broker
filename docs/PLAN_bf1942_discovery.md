# PLAN: BF1942 LAN server discovery in the broker

> Status: approved plan — all recommendations from the integration review are
> adopted. Reference implementation:
> `/home/papst/bf1942-adjust-monitor-settings/src_bf1942_discover/bf1942_discover.cc`
> and `/home/papst/bf1942-adjust-monitor-settings/docs/network-protocol.md`.

---

## 1. Objective

Teach the `broker` daemon to passively discover Battlefield 1942 game servers on
the LAN and record them in MariaDB so `UselessPope-www` can render a live server
list.

`bf1942_discover` (the reference tool) does this today as a standalone CLI that
prints each discovered server to stdout. This plan moves that behaviour into the
broker as a permanent background thread.

## 2. What we integrate (facts from the reference)

BF1942 servers broadcast a ~263-byte UDP datagram to `255.255.255.255:54915`
roughly once per second. The payload is mostly raw C++ state (including leaked
heap pointers) — it is **not** a text protocol. The only reliably parseable
fields are:

| Field       | Where in the datagram              | Example                                |
|-------------|------------------------------------|----------------------------------------|
| server name | NUL-terminated string at byte 1    | `Mk-VII`                               |
| GUID        | `{…}` substring somewhere in body  | `{50709c01-5d47-4dbc-8162-7ca9f2e8c5df}` |
| source IP   | from `recvfrom()`                  | `192.168.1.7`                          |

There is **no player, score or map data** in the beacon — it is a discovery
beacon only. (Player/score data lives in the ASE query, `bf1942_query`, which is
explicitly **out of scope** here.)

## 3. Decisions (all recommendations adopted)

1. **Storage shape:** a state **registry table** `BF1942_SERVER` — one row per
   server, upserted on each beacon, `last_seen` refreshed. **No** `P_*` archive
   table: nothing is drained, and history is expressed via `last_seen`.
2. **Config placement:** extend the existing `[network]` section (no new section
   or config file).
3. **Enabled by default:** yes, with an explicit kill-switch
   `network.BF1942DiscoverEnabled=0` so it can be disabled without a rebuild.
4. **Offline detection:** the broker only records `last_seen`; it does **not**
   expire servers. `UselessPope-www` decides "online vs. offline" from the
   `last_seen` age.

## 4. Integration approach (broker idioms)

- New `BasicThread` subclass `Bf1942Discover`, started from `main.cc`, exactly
  like `ButtonListener`.
- Reuse the existing Boost.Asio UDP-listener pattern (the broker already links
  Boost.Asio — no new dependency).
- DB access through the global `APP.db` (`Tools::ThreadedDatabase`) with a
  dispose token, matching every other worker thread.
- Follow the schema-sync rule: `create.sql`, `bindtypes.cc::create_sql()` and the
  `uselesspope-database` skill in `UselessPope-www` must stay in sync.

## 5. File-by-file changes

### 5.1 `src/bindtypes.h`

Add length constants next to the existing `*_LEN` block:

```cpp
const unsigned BF1942_GUID_LEN = 64;         // "{8-4-4-4-12}" = 38 chars, headroom
const unsigned BF1942_SERVER_NAME_LEN = 128;
```

Add the registry struct after the existing queue structs:

```cpp
class BF1942_SERVER : public BASE
{
public:
	DBTypeVarChar	name;         // server name (beacon byte 1..NUL)
	DBTypeVarChar	guid;         // "{...}" identifier (unique key)
	DBTypeVarChar	ip_address;   // source IP of the beacon
	DBTypeDateTime	last_seen;    // most recent beacon timestamp

public:
	BF1942_SERVER();
	BF1942_SERVER( const BF1942_SERVER & b )
	: BF1942_SERVER()
	{
		BASE::operator=(b);
	}

	BF1942_SERVER & operator=( const BF1942_SERVER & b )
	{
	  BASE::operator=(b);
	  return *this;
	}
};
```

### 5.2 `src/bindtypes.cc`

Constructor:

```cpp
BF1942_SERVER::BF1942_SERVER()
: BASE( "BF1942_SERVER", this ),
  name( this, "name", BF1942_SERVER_NAME_LEN ),
  guid( this, "guid", BF1942_GUID_LEN ),
  ip_address( this, "ip_address", IP_ADDRESS_LEN ),
  last_seen( this, "last_seen" )
{}
```

In `create_sql()`:
- declare `BF1942_SERVER bf1942_server;`
- emit `create_sql_statement( &bf1942_server, forkeys, add_drop_table );`
- add a lookup index on `guid` for the upsert:

```cpp
s += "ALTER TABLE `BF1942_SERVER` ADD INDEX IF NOT EXISTS `idx_BF1942_SERVER_guid`(`guid`);\n";
```

### 5.3 `create.sql`

Add the matching table (must equal `./broker -create-sql` output):

```sql
CREATE TABLE IF NOT EXISTS `BF1942_SERVER` (
 `idx` INT NOT NULL ,
`hist_an_zeit` DATETIME DEFAULT current_timestamp()NOT NULL ,
`hist_an_user` VARCHAR(20) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT ''NOT NULL ,
`hist_ae_zeit` DATETIME DEFAULT current_timestamp()NOT NULL ,
`hist_ae_user` VARCHAR(20) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT ''NOT NULL ,
`hist_lo_zeit` DATETIME DEFAULT current_timestamp()NOT NULL ,
`hist_lo_user` VARCHAR(20) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT ''NOT NULL ,
`name` VARCHAR(128) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT ''NOT NULL ,
`guid` VARCHAR(64) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT ''NOT NULL ,
`ip_address` VARCHAR(15) CHARACTER SET utf8 COLLATE utf8_general_ci DEFAULT ''NOT NULL ,
`last_seen` DATETIME DEFAULT current_timestamp()NOT NULL
) ENGINE = INNODB;
ALTER TABLE `BF1942_SERVER` ADD PRIMARY KEY IF NOT EXISTS ( `idx` );
ALTER TABLE `BF1942_SERVER` CHANGE `idx` `idx` INT( 11 ) NOT NULL AUTO_INCREMENT;
ALTER TABLE `BF1942_SERVER`  DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;
ALTER TABLE `BF1942_SERVER` ADD INDEX IF NOT EXISTS `idx_BF1942_SERVER_guid`(`guid`);
```

### 5.4 `config/ConfigNetwork.h` / `config/ConfigNetwork.cc`

Extend the `network` section (mirrors the existing `UDPListenPort`):

```cpp
CONFIG_SIMPLE_DECLARE_INT(  BF1942DiscoverPort );
CONFIG_SIMPLE_DECLARE_BOOL( BF1942DiscoverEnabled );
```

Constructor:

```cpp
ConfigSectionNetwork::ConfigSectionNetwork( const std::string & name_, Configfile2 *config_file_ )
: Section( name_, config_file_ ),
  UDPListenPort( "UDPListenPort", 22000 ),
  BF1942DiscoverPort( "BF1942DiscoverPort", 54915 ),
  BF1942DiscoverEnabled( "BF1942DiscoverEnabled", true )
{
	registerValue( &UDPListenPort );
	registerValue( &BF1942DiscoverPort );
	registerValue( &BF1942DiscoverEnabled );
}
```

### 5.5 `src/Bf1942Discover.h` / `src/Bf1942Discover.cc` (new)

```cpp
// Bf1942Discover.h
#pragma once
#include "BasicThread.h"

class Bf1942Discover : public BasicThread
{
private:
	unsigned m_port;

public:
	Bf1942Discover( unsigned port );
	void run() override;

private:
	void received_data( const std::string & data, const std::string & source_ip );
};
```

`run()` mirrors `ButtonListener` but the UDP callback delivers **raw bytes** plus
the **source IP** (the beacon is binary; `ButtonListener`'s text-stripping
helper must **not** be reused verbatim):

```cpp
void Bf1942Discover::run()
{
	boost::asio::io_context io_context;
	udp::socket socket( io_context, udp::endpoint( udp::v4(), m_port ) );
	socket.set_option( udp::socket::reuse_address( true ) );

	std::array<char, 1500> buffer {};
	udp::endpoint remote {};

	auto start_receive = [&]() {
		socket.async_receive_from(
			boost::asio::buffer( buffer ), remote,
			[&]( const boost::system::error_code & ec, std::size_t n ) {
				if( !ec && n > 0 ) {
					received_data(
						std::string( buffer.data(), n ),
						remote.address().to_string() );
				}
				start_receive();
			} );
	};
	start_receive();

	while( !APP.quit_request ) {
		io_context.run_for( 300ms );
	}
}
```

`received_data()` — parse and upsert (section 6):

```cpp
void Bf1942Discover::received_data( const std::string & data, const std::string & source_ip )
{
	Beacon b;
	if( !parse_beacon( data, b ) )
		return;

	const std::string key = !b.guid.empty() ? b.guid : ( b.name + "@" + source_ip );

	BF1942_SERVER existing;
	bool found = StdSqlSelect( *APP.db,
		Tools::format( "select %%%s from %s where `guid` = '%s'",
			existing.get_table_name(), existing.get_table_name(), escape( key ) ),
		DBInList<DBBindType>() >> existing ) > 0;

	if( found ) {
		existing.name       = b.name;
		existing.ip_address = source_ip;
		existing.last_seen  = now();          // see BASE::setHist time format
		existing.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
		if( !StdSqlUpdate( *APP.db, existing,
			Tools::format( "where `%s` = '%d'", existing.idx.get_name(), existing.idx.data ) ) ) {
			CPPDEBUG( Tools::format( "SqlError: '%s'", APP.db->get_error() ) );
		}
	} else {
		BF1942_SERVER srv;
		srv.name       = b.name;
		srv.guid       = key;
		srv.ip_address = source_ip;
		srv.last_seen  = now();
		srv.setHist( BASE::HIST_TYPE::HIST_AN, "broker" );
		srv.setHist( BASE::HIST_TYPE::HIST_AE, "broker" );
		srv.setHist( BASE::HIST_TYPE::HIST_LO, "broker" );
		if( !StdSqlInsert( *APP.db, srv ) ) {
			CPPDEBUG( Tools::format( "SqlError: '%s'", APP.db->get_error() ) );
		}
	}

	APP.db->commit();
}
```

`parse_beacon()` is ported verbatim from `bf1942_discover.cc` (name at byte
`1..NUL`, GUID via `memchr('{')`/`memchr('}')`), operating on the raw byte
string instead of `const char *`.

### 5.6 `src/main.cc`

```cpp
#include "Bf1942Discover.h"

// after the ButtonListener thread:
threads.emplace_back([&cfg_net]() {
	auto token = APP.db.get_dispose_token();
	if( cfg_net.BF1942DiscoverEnabled ) {
		Bf1942Discover discover( cfg_net.BF1942DiscoverPort );
		discover.run();
	}
});
```

### 5.7 `Makefile.am`

Add to `broker_SOURCES`:

```
src/Bf1942Discover.h \
src/Bf1942Discover.cc \
```

No new `CPPFLAGS`/`LDADD` (Boost.Asio already linked).

## 6. Upsert logic (per beacon)

Beacons repeat ~1/s per server; the upsert keeps the table at **one row per
active server** instead of one row per datagram.

1. `SELECT` by `guid` (fallback key `name@ip` when GUID absent).
2. Found → update `name`, `ip_address`, `last_seen`, stamp `HIST_AE`, `UPDATE`.
3. Not found → new row, stamp `HIST_AN|HIST_AE|HIST_LO` as `"broker"`, `INSERT`.
4. `commit()`.

## 7. Design notes / gotchas

- **Socket options:** bind `udp::v4()` on the port and set `reuse_address(true)`
  so a second listener (or another BF1942 process on the host) can still bind.
  Receiving broadcasts needs no `SO_BROADCAST` (that is send-only).
- **Binary payload:** do not `strip_view()` the datagram — it contains NUL bytes
  and leaked pointers; parse raw bytes only.
- **Source IP is required** (unlike `ButtonListener`, whose remote endpoint is
  commented out) — it is one of the three stored fields.
- **Dedupe / noise:** never insert a row per datagram; always upsert.
- **DB error handling:** follow the skill's guidance — prefer
  `throw STDERR_EXCEPTION(...)` for fatal DB errors in the new thread rather than
  silent `CPPDEBUG` (the code sketches above use `CPPDEBUG` only as a fallback;
  prefer throwing for hard failures).
- **Schema sync:** the new table must also be added to the `uselesspope-database`
  skill in `UselessPope-www`, and the DB user must hold
  `INSERT`/`SELECT`/`UPDATE` on `BF1942_SERVER`.

## 8. Build & verify

```bash
./reconfigure.sh && make
./broker -create-sql | grep -A14 BF1942_SERVER   # schema sanity
./broker -debug                                   # watch beacon debug lines
mysql -u papst -p papst -e "SELECT * FROM BF1942_SERVER ORDER BY last_seen DESC;"
```

## 9. Acceptance criteria

- [ ] `./broker` starts with no config changes; `[network].BF1942DiscoverPort=54915` default.
- [ ] A BF1942 server on the LAN appears in `BF1942_SERVER` with correct name/GUID/IP.
- [ ] `last_seen` updates on each beacon; table stays at one row per server.
- [ ] `network.BF1942DiscoverEnabled=0` disables the listener.
- [ ] `./broker -create-sql` matches `create.sql` for the new table.
- [ ] No build warnings introduced; `make` clean.

---

## 10. Step-by-step implementation — lower-cost path

If you want discovery working with the **smallest possible diff** first (no new
DB table, no schema migration, no new config section), follow the ordered steps
below. The listener only **logs** each discovered server (via `CPPDEBUG`, i.e.
`./broker -debug` / `nohup.out`). Upgrade to the full `BF1942_SERVER` registry
(§5–§7) later when `UselessPope-www` must render the list.

### Step 1 — new header `src/Bf1942Discover.h`

```cpp
#pragma once
#include "BasicThread.h"

class Bf1942Discover : public BasicThread
{
private:
	unsigned m_port;

public:
	explicit Bf1942Discover( unsigned port = 54915 );
	void run() override;
};
```

### Step 2 — new source `src/Bf1942Discover.cc`

Boost.Asio listener + `parse_beacon()` ported verbatim from the reference tool.
Binary-safe (no `strip_view()`), dedup via an in-memory `std::set<guid>`, and
**no DB access**:

```cpp
#include "Bf1942Discover.h"
#include <boost/asio.hpp>
#include <array>
#include <set>
#include <cstring>
#include <CpputilsDebug.h>
#include <format.h>
#include "App.h"

using namespace Tools;
using boost::asio::ip::udp;
using namespace std::chrono_literals;

namespace {
struct Beacon { std::string name; std::string guid; };

bool parse_beacon( const char * data, std::size_t len, Beacon & out ) {
	if( len < 2 ) return false;
	const std::size_t name_max = len - 1;
	std::size_t i = 0;
	while( i < name_max && data[1 + i] != '\0' ) ++i;
	out.name.assign( data + 1, i );
	const char * g = static_cast<const char *>( std::memchr( data, '{', len ) );
	if( g ) {
		const char * e = static_cast<const char *>( std::memchr( g, '}', len - (g - data) ) );
		if( e ) out.guid.assign( g, e - g + 1 );
	}
	return !out.name.empty();
}
} // namespace

Bf1942Discover::Bf1942Discover( unsigned port )
: BasicThread( "Bf1942Discover" ), m_port( port )
{}

void Bf1942Discover::run()
{
	boost::asio::io_context io_context;
	udp::socket socket( io_context, udp::endpoint( udp::v4(), m_port ) );
	socket.set_option( udp::socket::reuse_address( true ) );

	std::array<char, 1500> buffer {};
	udp::endpoint remote {};
	std::set<std::string> seen;

	auto start_receive = [&]() {
		socket.async_receive_from( boost::asio::buffer( buffer ), remote,
			[&]( const boost::system::error_code & ec, std::size_t n ) {
				if( !ec && n > 0 ) {
					Beacon b;
					if( parse_beacon( buffer.data(), n, b ) ) {
						const std::string key = !b.guid.empty() ? b.guid : ( b.name + "@" + remote.address().to_string() );
						if( seen.insert( key ).second ) {
							CPPDEBUG( Tools::format( "BF1942 server '%s' guid %s at %s", b.name, b.guid, remote.address().to_string() ) );
						}
					}
				}
				start_receive();
			} );
	};
	start_receive();

	while( !APP.quit_request )
		io_context.run_for( 300ms );
}
```

### Step 3 — hook into `src/main.cc`

```cpp
#include "Bf1942Discover.h"

// after the ButtonListener thread (no DB dispose token needed — no DB writes):
threads.emplace_back([]() {
	Bf1942Discover discover;   // default port 54915
	discover.run();
});
```

### Step 4 — register in `Makefile.am`

```
src/Bf1942Discover.h \
src/Bf1942Discover.cc \
```

### Step 5 — build & smoke test

```bash
./reconfigure.sh && make
./broker -debug | grep BF1942      # shows one line per discovered server
```

### Step 6 (optional) — config kill-switch

Add `BF1942DiscoverPort` / `BF1942DiscoverEnabled` to `[network]` exactly as in
§5.4, then replace the `main.cc` hook with:

```cpp
threads.emplace_back([&cfg_net]() {
	if( cfg_net.BF1942DiscoverEnabled ) {
		Bf1942Discover discover( cfg_net.BF1942DiscoverPort );
		discover.run();
	}
});
```

### Step 7 (optional) — persist without a new table

Swap the `CPPDEBUG` line for an insert into the **existing** `SERMON` table
(action = `"BF1942 server '<name>' entdeckt (<ip>)"`), reusing `bindtypes.h`
as-is. This needs no schema change, but note `SERMON` has different semantics
(action/reaction) — prefer the `BF1942_SERVER` table (§5–§7) for a clean
consumer-facing list.

### Upgrading to the full plan

When `UselessPope-www` needs the live list: add the `BF1942_SERVER` table per
§5.1–§5.3 and replace the `CPPDEBUG`/`SERMON` sink with the `BF1942_SERVER`
upsert in §6. Steps 1–4 stay unchanged.
